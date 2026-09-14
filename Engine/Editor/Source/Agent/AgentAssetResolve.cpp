#include "EditorPCH.h"
#include "Agent/AgentAssetResolve.h"

#include "Containers/StringFormat.h"
#include "Core/Object/ObjectCore.h"
#include "GUID/GUID.h"

namespace Lumina::Agent
{
    bool ResolveAssetObject(FStringView Guid, CObject*& OutObject, FString& OutError)
    {
        const TOptional<FGuid> Parsed = FGuid::TryParse(Guid);
        if (!Parsed.IsSet())
        {
            OutError = Lumina::Format("'{}' is not a GUID.", Guid);
            return false;
        }

        OutObject = StaticLoadObject(*Parsed);
        if (OutObject == nullptr)
        {
            OutError = Lumina::Format("No asset with GUID {} could be loaded.", Guid);
            return false;
        }

        return true;
    }

    CClass* FindClassByName(FStringView Name, CClass* Base, FString& OutError)
    {
        if (Name.empty())
        {
            OutError = "A class name is needed.";
            return nullptr;
        }

        CClass* Class = FindObject<CClass>(FName(Name));
        if (Class == nullptr)
        {
            OutError = Lumina::Format("No class is called {}.", Name);
            return nullptr;
        }

        if (Base != nullptr && !Class->IsChildOf(Base))
        {
            OutError = Lumina::Format("{} does not derive from {}.", Name, Base->GetName());
            return nullptr;
        }

        return Class;
    }
}
