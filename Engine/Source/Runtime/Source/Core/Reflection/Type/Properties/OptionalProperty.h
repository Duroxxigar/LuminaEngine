#pragma once

#include "Core/Reflection/Type/LuminaTypes.h"
#include "Memory/SmartPtr.h"

namespace Lumina
{
    /** Reflection wrapper for TOptional<T>. Wire format: bool engaged + optional payload. */
    class LUMINA_VISIBLE_TYPE FOptionalProperty : public FProperty
    {
    public:
        explicit FOptionalProperty(const FOptionalPropertyParams* Params)
            : FProperty(Params)
            , HasValueFn(Params->HasValueFn)
            , GetValueFn(Params->GetValueFn)
            , SetValueFn(Params->SetValueFn)
            , ResetFn   (Params->ResetFn)
            , ConstructContainerFn(Params->ConstructContainerFn)
            , DestructContainerFn (Params->DestructContainerFn)
            , ContainerContext    (Params->ContainerContext)
        {
        }

        DECLARE_FPROPERTY(EPropertyTypeFlags::Optional)

        /** Inner property (payload type) installed via ConstructProperties. */
        void AddProperty(FProperty* Property) override { Inner = Property; }

        FProperty* GetInternalProperty() const { return Inner; }

        bool  HasValue(const void* InContainer) const { return HasValueFn(InContainer); }
        void* GetValue(void* InContainer) const       { return GetValueFn(InContainer); }
        void  SetValue(void* InContainer, const void* InValue) const { SetValueFn(InContainer, InValue); }
        void  Reset(void* InContainer) const          { ResetFn(InContainer); }

        // Routed through the ops rather than a type test, so a TOptional<T> and a script optional are one path.
        void ConstructValue(void* Value) const override { if (ConstructContainerFn) { ConstructContainerFn(Value, ContainerContext); } }
        void DestructValue(void* Value) const override  { if (DestructContainerFn)  { DestructContainerFn(Value, ContainerContext); } }
        bool OwnsStorage() const override { return ConstructContainerFn != nullptr; }

        RUNTIME_API void Serialize(FArchive& Ar, void* Value) override;
        RUNTIME_API void SerializeItem(IStructuredArchive::FSlot Slot, void* Value, void const* Defaults) override;

        /** Compares engaged-state then payload via Inner; Copy mirrors engaged state. */
        RUNTIME_API bool Identical(const void* ValueA, const void* ValueB) const override;
        RUNTIME_API void CopyCompleteValue(void* Dst, const void* Src) const override;

    private:

        OptionalHasValuePtr     HasValueFn;
        OptionalGetValuePtr     GetValueFn;
        OptionalSetValuePtr     SetValueFn;
        OptionalResetPtr        ResetFn;

        void (*ConstructContainerFn)(void*, const void*) = nullptr;
        void (*DestructContainerFn)(void*, const void*) = nullptr;
        const void*             ContainerContext = nullptr;

        FProperty*              Inner = nullptr;
    };
}
