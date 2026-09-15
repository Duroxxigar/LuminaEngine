#pragma once

#include "Containers/String.h"
#include "Containers/StringFormat.h"
#include "Containers/StringView.h"
#include "Core/Object/Class.h"
#include "Core/Object/Object.h"

namespace Lumina::Agent
{
    // Loads the asset a GUID string names; false with a reason when it is malformed or names nothing.
    NODISCARD AGENTCORE_API bool ResolveAssetObject(FStringView Guid, CObject*& OutObject, FString& OutError);

    // Loads the asset a GUID string names; false with a reason when it is malformed or names nothing.
    template<typename T>
    NODISCARD bool ResolveAsset(FStringView Guid, T*& OutAsset, FString& OutError)
    {
        CObject* Object = nullptr;
        if (!ResolveAssetObject(Guid, Object, OutError))
        {
            return false;
        }

        if (!Object->IsA<T>())
        {
            OutError = Lumina::Format("'{}' is a {}, not a {}.", Guid, Object->GetClass()->GetName(), T::StaticClass()->GetName());
            return false;
        }

        OutAsset = static_cast<T*>(Object);
        return true;
    }

    // Finds a reflected class by name, optionally requiring it to derive from Base.
    NODISCARD AGENTCORE_API CClass* FindClassByName(FStringView Name, CClass* Base, FString& OutError);
}
