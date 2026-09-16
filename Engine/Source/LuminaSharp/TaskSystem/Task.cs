using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Lumina;

namespace LuminaSharp;

// What CTaskLibrary cannot express, so the parallel fan-out plus a disposable wrapper over its raw handle.
public static unsafe partial class Task
{
    // Maps to Lumina::ETaskPriority (High=0, Medium=1, Low=2). Bodies use Medium.
    private const int PriorityMedium = 1;

    // A body runs on a fiber worker, so blocking, awaiting or re-entering the task system corrupts the CLR.
    public static void ParallelFor(int Count, Action<int> Body)
    {
        if (Count <= 0)
        {
            return;
        }

        // ParallelFor blocks, so the GCHandle is valid for the whole native call; free it in finally.
        GCHandle Gc = GCHandle.Alloc(Body);
        try
        {
            NativeParallelFor(
                (uint)Count,
                0,
                (IntPtr)(delegate* unmanaged[Cdecl]<void*, uint, uint, uint, void>)&ParallelThunk,
                GCHandle.ToIntPtr(Gc),
                PriorityMedium);
        }
        finally
        {
            Gc.Free();
        }
    }

    // Runs one chunk over [Start, End). Never throws across the boundary.
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void ParallelThunk(void* Ctx, uint Start, uint End, uint Thread)
    {
        try
        {
            Action<int>? Body = GCHandle.FromIntPtr((IntPtr)Ctx).Target as Action<int>;
            if (Body == null)
            {
                return;
            }

            for (uint i = Start; i < End; i++)
            {
                Body((int)i);
            }
        }
        catch (Exception Exception)
        {
            Interop.LogException(Exception);
        }
    }

    // Wraps CTaskLibrary.Run so a using block releases the native handle rather than the caller remembering.
    public static TaskHandle Run(Action Body)
    {
        return new TaskHandle(CTaskLibrary.Run(ScriptCallback.Of(Body), PriorityMedium));
    }

    [NativeCall(Module = "Runtime", EntryPoint = "LuminaSharp_Task_ParallelFor")]
    private static partial void NativeParallelFor(uint Num, uint MinRange, IntPtr Thunk, IntPtr Ctx, int Priority);
}

// A value type cannot guard itself, so a second Dispose double-frees the native completion handle.
public readonly struct TaskHandle : IDisposable
{
    internal readonly ulong Handle;

    internal TaskHandle(ulong Handle)
    {
        this.Handle = Handle;
    }

    public bool IsValid => Handle != 0;

    // Blocks until the task has completed.
    public void Wait()
    {
        if (Handle != 0)
        {
            CTaskLibrary.Wait(Handle);
        }
    }

    public void Dispose()
    {
        if (Handle != 0)
        {
            CTaskLibrary.Release(Handle);
        }
    }
}
