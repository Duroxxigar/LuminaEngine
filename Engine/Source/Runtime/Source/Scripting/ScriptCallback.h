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

        // Runs Callback with Payload and keeps its handle, for a binding that fires more than once.
        RUNTIME_API void InvokeScriptCallbackRepeating(FScriptCallback Callback, uint64 Payload);

        // Frees a repeating callback's handle; a one-shot one frees itself when it fires.
        RUNTIME_API void ReleaseScriptCallback(FScriptCallback Callback);

        // Packs a float payload, since the wire carries one uint64 and the closure knows its own shape.
        FORCEINLINE uint64 PackFloatPayload(float Value)
        {
            uint32 Bits;
            Memory::Memcpy(&Bits, &Value, sizeof(Bits));
            return (uint64)Bits;
        }
    }

    /** Owns a repeating callback's handle, so whatever captured it releases on destruction. */
    struct RUNTIME_API FScriptCallbackOwner
    {
        FScriptCallback Callback;

        explicit FScriptCallbackOwner(FScriptCallback In)
            : Callback(In)
        {
        }

        ~FScriptCallbackOwner() { Scripting::ReleaseScriptCallback(Callback); }

        LE_NO_COPYMOVE(FScriptCallbackOwner);

        void Invoke(uint64 Payload) const { Scripting::InvokeScriptCallbackRepeating(Callback, Payload); }
    };
}
