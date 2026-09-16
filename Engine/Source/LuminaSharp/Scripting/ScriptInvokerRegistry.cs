using System;
using System.Collections.Generic;

namespace LuminaSharp;

// Where those generated invokers register themselves, keyed by declaring type and function name.
public static class ScriptInvokerRegistry
{
    private static readonly Dictionary<(Type Owner, string Function), nint> Invokers = new();

    // A managed function pointer, kept as nint so the registry needs no delegate object per binding.
    public static void Register(Type Owner, string Function, nint Invoker)
    {
        Invokers[(Owner, Function)] = Invoker;
    }

    internal static nint Find(Type Owner, string Function)
    {
        return Invokers.TryGetValue((Owner, Function), out nint Found) ? Found : 0;
    }

    // Holds user types and delegates over user methods, so a generation unload has to drop the whole table.
    internal static void Clear()
    {
        Invokers.Clear();
    }
}
