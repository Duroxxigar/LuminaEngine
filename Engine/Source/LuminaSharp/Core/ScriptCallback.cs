using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Lumina;

namespace LuminaSharp;

// Builds the one-shot FScriptCallback a reflected native function takes, from an ordinary managed lambda.
public static class ScriptCallback
{
    // Tracked so a generation unload can clear a closure over user types before it pins the collectible ALC.
    private static readonly HashSet<IntPtr> Pending = new();

    public static FScriptCallback Of(Action Handler)
    {
        return Bind(_ => Handler());
    }

    // The payload is the entity the operation produced, or the null entity when it produced none.
    public static FScriptCallback Of(Action<Entity> Handler)
    {
        return Bind(Payload => Handler(new Entity((uint)Payload)));
    }

    private static FScriptCallback Bind(Action<ulong> Trampoline)
    {
        GCHandle Handle = GCHandle.Alloc(Trampoline);
        IntPtr Token = GCHandle.ToIntPtr(Handle);
        Pending.Add(Token);
        return new FScriptCallback { Token = (ulong)Token };
    }

    // Resolves the handle, frees it, and runs the closure once. Native reaches this by name.
    [ManagedExport]
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvStdcall) })]
    public static void InvokeScriptCallback(IntPtr Token, ulong Payload)
    {
        try
        {
            if (Token == IntPtr.Zero)
            {
                return;
            }

            GCHandle Handle = GCHandle.FromIntPtr(Token);
            Pending.Remove(Token);
            Action<ulong>? Trampoline = Handle.Target as Action<ulong>;
            Handle.Free();
            Trampoline?.Invoke(Payload);
        }
        catch (Exception Exception)
        {
            Interop.LogException(Exception);
        }
    }

    // Cleared but left allocated, so a later native completion still frees the handle and finds nothing to run.
    internal static void PurgeAll()
    {
        foreach (IntPtr Token in Pending)
        {
            GCHandle Handle = GCHandle.FromIntPtr(Token);
            if (Handle.IsAllocated)
            {
                Handle.Target = null;
            }
        }
        Pending.Clear();
    }
}
