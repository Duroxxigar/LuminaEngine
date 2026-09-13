#include "RuntimePCH.h"
#include "ScriptClass.h"

#include "Core/Reflection/Type/LuminaTypes.h"

IMPLEMENT_INTRINSIC_CLASS(CScriptClass, CClass, RUNTIME_API)

namespace Lumina
{
    CClass* CScriptClass::GetMetaClass() const
    {
        return StaticClass();
    }

    bool CScriptClass::ConstructScriptProperties(void* Object) const
    {
        if (Object == nullptr || ScriptProperties.empty())
        {
            return false;
        }

        uint8* Base = static_cast<uint8*>(Object);
        for (FProperty* Property : ScriptLifecycleProperties)
        {
            Property->ConstructValue(Base + Property->Offset);
        }

        if (const CObject* Defaults = GetDefaultObjectIfCreated(); Defaults != nullptr && Defaults != Object)
        {
            const uint8* DefaultBase = reinterpret_cast<const uint8*>(Defaults);
            for (FProperty* Property : ScriptProperties)
            {
                Property->CopyCompleteValue(Base + Property->Offset, DefaultBase + Property->Offset);
            }
        }
        return true;
    }

    void CScriptClass::DestructScriptProperties(void* Object) const
    {
        if (Object == nullptr)
        {
            return;
        }
        for (FProperty* Property : ScriptLifecycleProperties)
        {
            Property->DestructValue(static_cast<uint8*>(Object) + Property->Offset);
        }
    }
}
namespace Lumina
{
    const FFunction* CScriptClass::FindScriptOverride(const FName& Name) const
    {
        const auto It = ScriptOverrideFunctions.find(Name);
        return It != ScriptOverrideFunctions.end() ? It->second : nullptr;
    }
}
