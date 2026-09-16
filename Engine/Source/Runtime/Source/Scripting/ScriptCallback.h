#pragma once

#include "Core/Object/ObjectMacros.h"
#include "Platform/GenericPlatform.h"
#include "ScriptCallback.generated.h"

namespace Lumina
{
    /** A one-shot script callback, carried as the GCHandle owning its closure so it crosses by value. */
    REFLECT()
    struct RUNTIME_API FScriptCallback
    {
        GENERATED_BODY()

        PROPERTY()
        uint64 Token = 0;

        NODISCARD bool IsBound() const { return Token != 0; }
    };

    namespace Scripting
    {
        /** Runs Callback once with Payload and frees its handle. Does nothing when nothing is bound. */
        RUNTIME_API void InvokeScriptCallback(FScriptCallback Callback, uint64 Payload);
    }
}
