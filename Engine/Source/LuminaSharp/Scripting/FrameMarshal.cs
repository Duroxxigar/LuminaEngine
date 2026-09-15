using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;

namespace LuminaSharp;

// A frame is a container like an object is, so a blittable slot is copied in place and the rest is marshalled.
internal static unsafe class FrameMarshal
{
    internal enum EAccess
    {
        Blittable,
        Bool,
        String,
        Object,
        StructView,
        Optional,
        ContainerView,
    }

    internal readonly struct FSlot
    {
        internal FSlot(EAccess Access, int Offset, IntPtr Property, Type Declared, FValueCopier? Copier,
            FViewFactory? View = null)
        {
            this.Access = Access;
            this.Offset = Offset;
            this.Property = Property;
            this.Declared = Declared;
            this.Copier = Copier;
            this.View = View;
        }

        internal readonly EAccess       Access;
        internal readonly int           Offset;
        internal readonly IntPtr        Property;
        internal readonly Type          Declared;
        internal readonly FValueCopier? Copier;
        internal readonly FViewFactory? View;
    }

    // A container view is two words, the instance and its ops table, so one factory serves every closed form.
    internal sealed class FViewFactory
    {
        internal FViewFactory(ConstructorInfo Constructor, IntPtr Ops)
        {
            this.Constructor = Constructor;
            this.Ops = Ops;
        }

        private readonly ConstructorInfo Constructor;
        private readonly IntPtr          Ops;

        internal object Create(nint Address)
        {
            return Constructor.Invoke(new object[] { Address, (nint)Ops });
        }
    }

    // The generic read and write for one value type, closed over that type once and reused per call.
    internal sealed class FValueCopier
    {
        internal FValueCopier(Func<nint, object> Read, Action<nint, object?> Write, int Size)
        {
            this.Read = Read;
            this.Write = Write;
            this.Size = Size;
        }

        internal readonly Func<nint, object>    Read;
        internal readonly Action<nint, object?> Write;
        internal readonly int                   Size;
    }

    private static readonly Dictionary<Type, FValueCopier?> CopierByType = new();

    private static readonly Dictionary<Type, ConstructorInfo?> ViewConstructorByType = new();

    private static readonly MethodInfo ReadValueMethod =
        typeof(FrameMarshal).GetMethod(nameof(ReadValue), BindingFlags.NonPublic | BindingFlags.Static)!;

    private static readonly MethodInfo WriteValueMethod =
        typeof(FrameMarshal).GetMethod(nameof(WriteValue), BindingFlags.NonPublic | BindingFlags.Static)!;

    private static readonly MethodInfo SizeOfMethod =
        typeof(Unsafe).GetMethod(nameof(Unsafe.SizeOf), BindingFlags.Public | BindingFlags.Static)!;

    private static readonly MethodInfo HasReferencesMethod =
        typeof(RuntimeHelpers).GetMethod(nameof(RuntimeHelpers.IsReferenceOrContainsReferences),
            BindingFlags.Public | BindingFlags.Static)!;

    // C# in is by-ref but read-only, so only out and ref are slots the caller reads back.
    internal static EScriptParamFlags DirectionOf(ParameterInfo Parameter)
    {
        if (!Parameter.ParameterType.IsByRef || (Parameter.IsIn && !Parameter.IsOut))
        {
            return EScriptParamFlags.None;
        }

        return Parameter.IsOut
            ? EScriptParamFlags.OutParam
            : EScriptParamFlags.OutParam | EScriptParamFlags.RefParam;
    }

    // Resolved once per signature, so a mismatch drops the call instead of reading whatever is at the offset.
    internal static bool TryBind(IntPtr Property, Type Declared, string Where, out FSlot Slot)
    {
        Slot = default;

        // A frame slot holds the value, so out, ref and in all bind as the type behind the reference.
        if (Declared.IsByRef)
        {
            Declared = Declared.GetElementType()!;
        }

        if (Property == IntPtr.Zero)
        {
            Debug.LogError($"{Where}. The call frame has no slot for a {Declared.Name}, so the call is dropped.");
            return false;
        }

        int Offset = Native.PropertyOffset(Property);
        int Width = Native.PropertySize(Property);

        if (Offset < 0)
        {
            Debug.LogError($"{Where}. The {Declared.Name} slot has no offset, so the call is dropped.");
            return false;
        }

        if (Declared == typeof(bool))
        {
            Slot = new FSlot(EAccess.Bool, Offset, Property, Declared, null);
            return true;
        }

        // Ahead of the blittable path, which would otherwise bind the nullable itself and copy its flag byte.
        if (Nullable.GetUnderlyingType(Declared) is Type Payload)
        {
            return TryBindOptional(Property, Declared, Payload, Offset, Where, out Slot);
        }

        if (Declared == typeof(string))
        {
            Slot = new FSlot(EAccess.String, Offset, Property, Declared, null);
            return true;
        }

        if (typeof(NativeObject).IsAssignableFrom(Declared))
        {
            Slot = new FSlot(EAccess.Object, Offset, Property, Declared, null);
            return true;
        }

        if (typeof(NativeStruct).IsAssignableFrom(Declared))
        {
            Slot = new FSlot(EAccess.StructView, Offset, Property, Declared, null);
            return true;
        }

        if (IsContainerView(Declared))
        {
            return TryBindView(Property, Declared, Offset, Where, out Slot);
        }

        FValueCopier? Copier = ResolveCopier(Declared);
        if (Copier == null)
        {
            Debug.LogError($"{Where}. {Declared.Name} cannot be moved through a call frame, so the call is "
                + "dropped. Supported are numbers, bool, enums, string, engine struct mirrors such as "
                + "FVector3 and FName, entity handles, object references and component views.");
            return false;
        }

        if (Copier.Size != Width)
        {
            Debug.LogError($"{Where}. {Declared.Name} is {Copier.Size} bytes in C# but its frame slot is "
                + $"{Width}, so the call is dropped rather than writing past it.");
            return false;
        }

        Slot = new FSlot(EAccess.Blittable, Offset, Property, Declared, Copier);
        return true;
    }

    internal static object? Read(IntPtr Frame, in FSlot Slot)
    {
        nint Address = (nint)Frame + Slot.Offset;

        switch (Slot.Access)
        {
            case EAccess.Blittable:
                return Slot.Copier!.Read(Address);

            case EAccess.Bool:
                return Unsafe.ReadUnaligned<byte>((void*)Address) != 0;

            // An FString is read in place, exactly as a blittable one is; there is no crossing for it.
            case EAccess.String:
                return NativeMarshal.ReadString(Address);

            case EAccess.Object:
            {
                IntPtr Object = Native.PropGetObject(Frame, Slot.Property);
                return Object == IntPtr.Zero ? null : NativeObjectMarshal.FromHandleOfType(Object, Slot.Declared);
            }

            // The frame outlives the call, which is the whole window this view over its slot is valid for.
            case EAccess.StructView:
                return Activator.CreateInstance(Slot.Declared, (IntPtr)Address);

            // Native hands back a null payload for an unset optional, which is the null a nullable wants.
            case EAccess.Optional:
            {
                IntPtr Payload = Native.PropOptionalGetValue(Frame, Slot.Property);
                return Payload == IntPtr.Zero ? null : Slot.Copier!.Read(Payload);
            }

            case EAccess.ContainerView:
                return Slot.View!.Create(Address);
        }

        return null;
    }

    internal static void Write(IntPtr Frame, in FSlot Slot, object? Value)
    {
        nint Address = (nint)Frame + Slot.Offset;

        switch (Slot.Access)
        {
            case EAccess.Blittable:
                Slot.Copier!.Write(Address, Value);
                return;

            case EAccess.Bool:
                Unsafe.WriteUnaligned((void*)Address, (byte)(Value is true ? 1 : 0));
                return;

            // Assigned rather than written in place, since the frame's string owns its memory.
            case EAccess.String:
                Native.PropSetString(Frame, Slot.Property, Value as string ?? string.Empty);
                return;

            case EAccess.Object:
                Native.PropSetObject(Frame, Slot.Property, NativeObjectMarshal.ToHandle(Value as NativeObject));
                return;

            // The property owns the copy, since only it knows whether the struct's members own memory.
            case EAccess.StructView:
                Native.PropCopyStruct(Frame, Slot.Property, NativeObjectMarshal.ToHandle(Value as NativeStruct));
                return;

            case EAccess.Optional:
            {
                if (Value == null)
                {
                    Native.PropOptionalReset(Frame, Slot.Property);
                    return;
                }

                // Long-aligned, since the setter reads the payload back through its own type.
                long* Scratch = stackalloc long[(Slot.Copier!.Size + 7) / 8];
                Slot.Copier.Write((nint)Scratch, Value);
                Native.PropOptionalSetValue(Frame, Slot.Property, (IntPtr)Scratch);
                return;
            }
        }
    }

    // A container view aliases the slot rather than copying it, so the callee's edits are already in the frame.
    internal static bool IsViewOnly(in FSlot Slot)
    {
        return Slot.Access == EAccess.ContainerView;
    }

    // A copier closes over a user type, so leaving one cached would pin the generation being unloaded.
    internal static void Reset()
    {
        lock (CopierByType)
        {
            CopierByType.Clear();
        }

        lock (ViewConstructorByType)
        {
            ViewConstructorByType.Clear();
        }
    }

    private static bool IsContainerView(Type Declared)
    {
        if (!Declared.IsGenericType)
        {
            return false;
        }

        Type Definition = Declared.GetGenericTypeDefinition();
        return Definition == typeof(global::Lumina.TVector<>)
            || Definition == typeof(global::Lumina.THashMap<,>);
    }

    private static bool TryBindView(IntPtr Property, Type Declared, int Offset, string Where, out FSlot Slot)
    {
        Slot = default;

        bool bMap = Declared.GetGenericTypeDefinition() == typeof(global::Lumina.THashMap<,>);
        IntPtr Ops = bMap ? Native.PropMapOps(Property) : Native.PropVectorOps(Property);

        if (Ops == IntPtr.Zero)
        {
            Debug.LogError($"{Where}. {Declared.Name} needs a {(bMap ? "map" : "vector")} frame slot and this "
                + "one is not, so the call is dropped.");
            return false;
        }

        ConstructorInfo? Constructor = ResolveViewConstructor(Declared);
        if (Constructor == null)
        {
            Debug.LogError($"{Where}. {Declared.Name} has no view constructor to bind, so the call is dropped.");
            return false;
        }

        Slot = new FSlot(EAccess.ContainerView, Offset, Property, Declared, null, new FViewFactory(Constructor, Ops));
        return true;
    }

    private static ConstructorInfo? ResolveViewConstructor(Type Declared)
    {
        lock (ViewConstructorByType)
        {
            if (ViewConstructorByType.TryGetValue(Declared, out ConstructorInfo? Cached))
            {
                return Cached;
            }

            ConstructorInfo? Constructor = Declared.GetConstructor(new[] { typeof(nint), typeof(nint) });
            ViewConstructorByType[Declared] = Constructor;
            return Constructor;
        }
    }

    // A nullable crosses as its payload, matching how a TOptional property already binds.
    private static bool TryBindOptional(IntPtr Property, Type Declared, Type Payload, int Offset, string Where,
        out FSlot Slot)
    {
        Slot = default;

        IntPtr Inner = Native.PropOptionalInner(Property);
        if (Inner == IntPtr.Zero)
        {
            Debug.LogError($"{Where}. {Declared.Name} needs an optional frame slot and this one is not, so the "
                + "call is dropped.");
            return false;
        }

        FValueCopier? Copier = ResolveCopier(Payload);
        if (Copier == null)
        {
            Debug.LogError($"{Where}. {Payload.Name} cannot be the payload of an optional crossing a call "
                + "frame, so the call is dropped.");
            return false;
        }

        int Width = Native.PropertySize(Inner);
        if (Copier.Size != Width)
        {
            Debug.LogError($"{Where}. {Payload.Name} is {Copier.Size} bytes in C# but the optional's payload "
                + $"is {Width}, so the call is dropped rather than writing past it.");
            return false;
        }

        Slot = new FSlot(EAccess.Optional, Offset, Property, Declared, Copier);
        return true;
    }

    private static object ReadValue<T>(nint Address) where T : unmanaged
    {
        return Unsafe.ReadUnaligned<T>((void*)Address);
    }

    private static void WriteValue<T>(nint Address, object? Value) where T : unmanaged
    {
        Unsafe.WriteUnaligned((void*)Address, Value is T Typed ? Typed : default);
    }

    private static FValueCopier? ResolveCopier(Type Declared)
    {
        lock (CopierByType)
        {
            if (CopierByType.TryGetValue(Declared, out FValueCopier? Cached))
            {
                return Cached;
            }

            FValueCopier? Copier = BuildCopier(Declared);
            CopierByType[Declared] = Copier;
            return Copier;
        }
    }

    // Null for a type that has to be marshalled rather than copied.
    private static FValueCopier? BuildCopier(Type Declared)
    {
        if (!Declared.IsValueType || Declared == typeof(void) || Declared.ContainsGenericParameters
            || Declared.IsByRefLike || Declared.IsPointer)
        {
            return null;
        }

        try
        {
            // Reflection does not enforce ReadValue's unmanaged constraint, so this is the real gate.
            if ((bool)HasReferencesMethod.MakeGenericMethod(Declared).Invoke(null, null)!)
            {
                return null;
            }

            var Read = (Func<nint, object>)ReadValueMethod.MakeGenericMethod(Declared)
                .CreateDelegate(typeof(Func<nint, object>));

            var Write = (Action<nint, object?>)WriteValueMethod.MakeGenericMethod(Declared)
                .CreateDelegate(typeof(Action<nint, object?>));

            int Size = (int)SizeOfMethod.MakeGenericMethod(Declared).Invoke(null, null)!;

            return new FValueCopier(Read, Write, Size);
        }
        catch (Exception Exception)
        {
            Debug.LogError($"{Declared.Name} could not be bound as a blittable call-frame value. {Exception.Message}");
            return null;
        }
    }
}
