#include "AgentCorePCH.h"
#include "Agent/AgentObjectEdit.h"

#include "Agent/AgentPropertyPath.h"
#include "Agent/AgentReflectionUtils.h"
#include "Agent/AgentToolMarshal.h"
#include "Containers/StringFormat.h"
#include "Core/Object/Package/Package.h"
#include "Core/Reflection/Type/LuminaTypes.h"
#include "Core/Reflection/Type/Properties/ArrayProperty.h"
#include "Core/Reflection/Type/Properties/StructProperty.h"
#include "LuminaEditor.h"
#include "UI/EditorUI.h"
#include "UI/Tools/EditorTool.h"

namespace Lumina::Agent
{
    namespace
    {
        FEditorTool* FindOpenEditorFor(CObject* Owner)
        {
            if (GEditorEngine == nullptr || Owner == nullptr)
            {
                return nullptr;
            }

            FEditorUI* UI = static_cast<FEditorUI*>(GEditorEngine->GetDevelopmentToolsUI());
            return UI != nullptr ? UI->FindAssetEditor(Owner) : nullptr;
        }

        FString Dump(const nlohmann::json& Value)
        {
            return FString(Value.dump().c_str());
        }

        void CollectUnwritable(CStruct* Struct, FStringView Prefix, int32 Depth, TVector<FString>& OutPaths)
        {
            if (Struct == nullptr || Depth > 8)
            {
                return;
            }

            for (CStruct* Current : Detail::CollectStructChain(Struct))
            {
                Current->ForEachProperty<FProperty>([&](FProperty* Property)
                {
                    if (Property == nullptr)
                    {
                        return;
                    }

                    const FString Name(Property->GetPropertyName().ToString().c_str());
                    const FString Path = Prefix.empty() ? Name : Lumina::Format("{}.{}", Prefix, Name);

                    switch (Property->GetType())
                    {
                    case EPropertyTypeFlags::Delegate:
                        OutPaths.push_back(Path);
                        break;

                    case EPropertyTypeFlags::Struct:
                        CollectUnwritable(static_cast<FStructProperty*>(Property)->GetStruct(), FStringView(Path), Depth + 1, OutPaths);
                        break;

                    case EPropertyTypeFlags::Vector:
                        if (FProperty* Inner = static_cast<FArrayProperty*>(Property)->GetInternalProperty();
                            Inner != nullptr && Inner->GetType() == EPropertyTypeFlags::Struct)
                        {
                            CollectUnwritable(static_cast<FStructProperty*>(Inner)->GetStruct(),
                                FStringView(Lumina::Format("{}[]", Path)), Depth + 1, OutPaths);
                        }
                        break;

                    default:
                        break;
                    }
                });
            }
        }
    }

    FObjectEditResult SetObjectProperty(CStruct* Root, void* RootData, CObject* Owner, FStringView Path,
        const nlohmann::json& Value, FName Label)
    {
        FObjectEditResult Result;

        FResolvedProperty Target;
        if (!ResolvePropertyPath(Root, RootData, Path, Target, Result.Error))
        {
            return Result;
        }

        // Checked before anything is recorded, so a bad value costs no snapshot.
        if (const FMarshalResult Checked = ValidatePropertyValue(Value, Target.Property, Path); !Checked.IsValid())
        {
            Result.Error = Checked.Error;
            return Result;
        }

        nlohmann::json Before;
        if (WriteProperty(Target.Property, Target.ValuePtr, Before).IsValid())
        {
            Result.Previous = Dump(Before);
        }

        FMarshalResult Applied;
        const auto Mutate = [&]()
        {
            Applied = ReadProperty(Value, Target.Property, Target.ValuePtr, Path);
            if (Applied.IsValid() && Owner != nullptr)
            {
                Owner->PostPropertyChange(Target.Property);
            }
        };

        if (FEditorTool* Editor = FindOpenEditorFor(Owner))
        {
            Editor->RunObjectTransacted(Label, Owner, Mutate);
            Result.bUndoable = true;
        }
        else
        {
            Mutate();
        }

        if (!Applied.IsValid())
        {
            Result.Error = Applied.Error;
            return Result;
        }

        if (Owner != nullptr && Owner->GetPackage() != nullptr)
        {
            Owner->GetPackage()->MarkDirty();
        }

        nlohmann::json After;
        if (WriteProperty(Target.Property, Target.ValuePtr, After).IsValid())
        {
            Result.Current = Dump(After);
        }

        return Result;
    }

    void CollectUnwritableFields(CStruct* Root, FStringView Prefix, TVector<FString>& OutPaths)
    {
        CollectUnwritable(Root, Prefix, 0, OutPaths);
    }
}
