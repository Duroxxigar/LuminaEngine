#include "RuntimePCH.h"

#include "ScriptCallback.h"

#include "Scripting/DotNet/DotNetHost.h"

namespace Lumina::Scripting
{
    void InvokeScriptCallback(FScriptCallback Callback, uint64 Payload)
    {
        if (!Callback.IsBound())
        {
            return;
        }

        // Lives in LuminaSharp.dll rather than a script assembly, so the resolved pointer survives a reload.
        static DotNet::TManagedExport<void (*)(void*, uint64)> Invoke("InvokeScriptCallback");
        if (auto* Fn = Invoke.Get())
        {
            Fn(reinterpret_cast<void*>(Callback.Token), Payload);
        }
    }
}
