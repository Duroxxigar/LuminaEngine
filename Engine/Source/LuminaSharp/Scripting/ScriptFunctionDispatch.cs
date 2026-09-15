using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace LuminaSharp;

/// <summary>
/// The single entry point every script-declared reflected function is dispatched through.
/// </summary>
/// <remarks>
/// One entry point serves all of them, which is why native hands it the function being called. Arguments are
/// read back out of the call frame through the function's own parameters: a frame is a container like an
/// object is, so the property accessors already exported address it without any marshalling of its own.
/// </remarks>
public static unsafe class ScriptFunctionDispatch
{
    // Keyed by the function AND the type, because a re-mint frees the old FFunction and the arena can hand
    // its address straight back to a new one; on the pointer alone that would silently dispatch the old method.
    private static readonly Dictionary<(IntPtr Function, Type Type), FBound> BoundByFunction = new();

    private readonly struct FBound
    {
        public FBound(MethodInfo Method, FrameMarshal.FSlot[] Parameters, bool[]? WriteBack,
            FrameMarshal.FSlot Return, bool bHasReturn)
        {
            this.Method = Method;
            this.Parameters = Parameters;
            this.WriteBack = WriteBack;
            this.Return = Return;
            this.bHasReturn = bHasReturn;
        }

        public readonly MethodInfo            Method;
        public readonly FrameMarshal.FSlot[]  Parameters;

        // Null when nothing is by-ref, which is the ordinary signature and skips the write-back pass.
        public readonly bool[]?               WriteBack;
        public readonly FrameMarshal.FSlot    Return;
        public readonly bool                  bHasReturn;
    }

    [ManagedExport]
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    public static void InvokeScriptFunction(IntPtr Instance, IntPtr Function, IntPtr Frame)
    {
        try
        {
            if (Instance == IntPtr.Zero || Function == IntPtr.Zero)
            {
                return;
            }

            if (GCHandle.FromIntPtr(Instance).Target is not object Target)
            {
                return;
            }

            if (!TryBind(Target.GetType(), Function, out FBound Bound))
            {
                return;
            }

            int Count = Bound.Parameters.Length;
            object?[] Arguments = Count == 0 ? Array.Empty<object>() : new object?[Count];

            for (int Index = 0; Index < Count; ++Index)
            {
                Arguments[Index] = FrameMarshal.Read(Frame, Bound.Parameters[Index]);
            }

            object? Result = Bound.Method.Invoke(Target, Arguments);

            // Invoke assigns an out or ref argument back into the array, which the caller reads off the frame.
            if (Bound.WriteBack != null)
            {
                for (int Index = 0; Index < Count; ++Index)
                {
                    if (Bound.WriteBack[Index])
                    {
                        FrameMarshal.Write(Frame, Bound.Parameters[Index], Arguments[Index]);
                    }
                }
            }

            if (Bound.bHasReturn)
            {
                FrameMarshal.Write(Frame, Bound.Return, Result);
            }
        }
        catch (Exception Exception)
        {
            Interop.LogException(Exception);
        }
    }

    // Resolved once per function. A signature the frame and the method disagree about is reported here rather
    // than read as whatever happened to be at the offset.
    private static bool TryBind(Type Type, IntPtr Function, out FBound Bound)
    {
        var Key = (Function, Type);
        if (BoundByFunction.TryGetValue(Key, out Bound))
        {
            return Bound.Method != null;
        }

        Bound = default;

        string Name = Native.FunctionGetName(Function);
        MethodInfo? Method = Type.GetMethod(Name,
            BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);

        if (Method == null)
        {
            Debug.LogError($"Script function '{Name}' is reflected on {Type.Name} but the method is gone; the call is dropped.");
            BoundByFunction[Key] = default;
            return false;
        }

        ParameterInfo[] Signature = Method.GetParameters();
        int Count = Native.FunctionParamCount(Function);

        IntPtr ReturnParam = Native.FunctionReturnParam(Function);
        bool bHasReturn = ReturnParam != IntPtr.Zero;

        if (Count != Signature.Length)
        {
            Debug.LogError($"Script function '{Name}' on {Type.Name} takes {Signature.Length} arguments but its frame describes {Count}; the call is dropped.");
            BoundByFunction[Key] = default;
            return false;
        }

        bool bMethodReturns = Method.ReturnType != typeof(void);
        if (bHasReturn != bMethodReturns)
        {
            Debug.LogError($"Script function '{Name}' on {Type.Name} returns {Method.ReturnType.Name} but its frame {(bHasReturn ? "describes a return value" : "describes none")}; the call is dropped.");
            BoundByFunction[Key] = default;
            return false;
        }

        FrameMarshal.FSlot[] Parameters = new FrameMarshal.FSlot[Signature.Length];
        bool[]? WriteBack = null;

        for (int Index = 0; Index < Signature.Length; ++Index)
        {
            string Where = $"Script function '{Name}' on {Type.Name}, argument '{Signature[Index].Name}'";
            if (!FrameMarshal.TryBind(Native.FunctionParamAt(Function, Index), Signature[Index].ParameterType,
                    Where, out Parameters[Index]))
            {
                BoundByFunction[Key] = default;
                return false;
            }

            // A view writes through to the slot as the callee edits it, so a ref one needs no copy back.
            if (FrameMarshal.DirectionOf(Signature[Index]) != EScriptParamFlags.None
                && !FrameMarshal.IsViewOnly(Parameters[Index]))
            {
                WriteBack ??= new bool[Signature.Length];
                WriteBack[Index] = true;
            }
        }

        FrameMarshal.FSlot Return = default;
        if (bHasReturn && !FrameMarshal.TryBind(ReturnParam, Method.ReturnType,
                $"Script function '{Name}' on {Type.Name}, return value", out Return))
        {
            BoundByFunction[Key] = default;
            return false;
        }

        // A view borrows storage it does not own, so there is nothing for it to hand back by value.
        if (bHasReturn && FrameMarshal.IsViewOnly(Return))
        {
            Debug.LogError($"Script function '{Name}' on {Type.Name} returns {Method.ReturnType.Name}, a view over storage it does not own; the call is dropped. Take it as a parameter and fill it in place instead.");
            BoundByFunction[Key] = default;
            return false;
        }

        Bound = new FBound(Method, Parameters, WriteBack, Return, bHasReturn);
        BoundByFunction[Key] = Bound;
        return true;
    }

    /// <summary>
    /// Dropped on hot reload. A bound entry holds a MethodInfo from the generation being unloaded, which
    /// roots its declaring type and so pins the collectible load context: left in place this does not just
    /// go stale, it stops the generation unloading at all.
    /// </summary>
    internal static void Reset()
    {
        BoundByFunction.Clear();
        FrameMarshal.Reset();
    }
}
