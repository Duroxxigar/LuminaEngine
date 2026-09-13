#pragma once

#include "Containers/FunctionRef.h"
#include "Core/Reflection/Type/ObjectReferenceVisitor.h"
#include "Platform/GenericPlatform.h"

namespace Lumina
{
    /**
     * A holder of CObject references the reflected object graph cannot reach on its own: an ECS registry,
     * an engine-owned pointer, an editor selection.
     *
     * Registering one is how a subsystem exposes what it points at without anything else knowing it exists,
     * which is what replaces a hard-coded evacuate/restore pair per holder.
     */
    class IObjectReferenceProvider
    {
    public:

        virtual ~IObjectReferenceProvider() = default;

        /** Named only so a failure to repoint can say which holder it came from. */
        virtual const char* GetReferenceProviderName() const = 0;

        virtual void VisitObjectReferences(FObjectReferenceVisitor::FSlotFunc Func) = 0;
    };

    /** The registered holders, shared by everything that rewrites the object graph. */
    struct RUNTIME_API FObjectReferenceProviders
    {
        static void Register(IObjectReferenceProvider* Provider);
        static void Unregister(IObjectReferenceProvider* Provider);

        /** Returns how many providers were visited. */
        static int32 ForEach(TFunctionRef<void(IObjectReferenceProvider&)> Func);
    };
}
