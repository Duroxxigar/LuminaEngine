#pragma once

#include "Containers/String.h"
#include "Containers/Vector.h"
#include "Core/Object/ObjectMacros.h"
#include "MCPAssetTools.h"

#include "MCPAssetObjectTools.generated.h"

namespace Lumina
{
    REFLECT()
    struct MCPEDITOR_API SAssetClassInfo
    {
        GENERATED_BODY()

        /** Pass this to assets.create as Class. */
        PROPERTY()
        FString Name;

        /** Content browser grouping, such as Gameplay or Materials. */
        PROPERTY()
        FString Category;

        /** True when the content browser would ask questions before creating one, which assets.create skips. */
        PROPERTY()
        bool bHasCreationDialogue = false;
    };

    REFLECT()
    struct MCPEDITOR_API SListAssetClassesParams
    {
        GENERATED_BODY()

        /** Only classes whose name or category contains this. Empty lists every one of them. */
        PROPERTY()
        FString Contains;
    };

    REFLECT()
    struct MCPEDITOR_API SListAssetClassesResult
    {
        GENERATED_BODY()

        PROPERTY()
        TVector<SAssetClassInfo> Classes;
    };

    REFLECT()
    struct MCPEDITOR_API SCreateAssetParams
    {
        GENERATED_BODY()

        /** Asset class name, from assets.list_classes. */
        PROPERTY()
        FString ClassName;

        /** Name without an extension; the .lasset suffix is added. */
        PROPERTY()
        FString Name;

        /** Existing folder under /Game/Content to create it in. */
        PROPERTY()
        FString Folder;
    };

    REFLECT()
    struct MCPEDITOR_API SDescribeAssetParams
    {
        GENERATED_BODY()

        /** GUID of the asset, from assets.search. */
        PROPERTY()
        FString Asset;
    };

    REFLECT()
    struct MCPEDITOR_API SDescribeAssetResult
    {
        GENERATED_BODY()

        PROPERTY()
        FString Name;

        PROPERTY()
        FString Path;

        PROPERTY()
        FString ClassName;

        PROPERTY()
        FString Guid;

        /** Every reflected field as JSON, in the shape assets.set_property takes back. */
        PROPERTY()
        FString Properties;

        /** Field paths that cannot be written, so a call on one of them is not worth making. */
        PROPERTY()
        TVector<FString> UnwritableFields;

        /** Assets that reference this one, which assets.delete would leave dangling. */
        PROPERTY()
        TVector<SAssetInfo> Referencers;
    };

    REFLECT()
    struct MCPEDITOR_API SSetAssetPropertyParams
    {
        GENERATED_BODY()

        /** GUID of the asset, from assets.search. */
        PROPERTY()
        FString Asset;

        /** Field path on the asset, such as RowStructName or Settings.Radius or Layers[2].Name. */
        PROPERTY()
        FString Path;

        /** The new value as JSON, so 12.5 or "Torch" or true or {"X":1,"Y":0,"Z":0}. */
        PROPERTY(RawJson)
        FString Value;
    };

    REFLECT()
    struct MCPEDITOR_API SAssetPropertyResult
    {
        GENERATED_BODY()

        /** What the field held before the change, as JSON. */
        PROPERTY()
        FString Previous;

        /** What it holds now, read back after applying. */
        PROPERTY()
        FString Current;

        /** True when an open editor recorded the edit, so editor.undo can reach it. */
        PROPERTY()
        bool bUndoable = false;
    };

    REFLECT()
    struct MCPEDITOR_API SDeleteAssetParams
    {
        GENERATED_BODY()

        /** GUID of the asset, from assets.search. */
        PROPERTY()
        FString Asset;

        /** Delete even when other assets still reference it, leaving those references dangling. */
        PROPERTY()
        bool bForce = false;
    };

    REFLECT()
    struct MCPEDITOR_API SDeleteAssetResult
    {
        GENERATED_BODY()

        PROPERTY()
        FString Path;

        /** True when the file is gone. False with Referencers listed means bForce was needed. */
        PROPERTY()
        bool bDeleted = false;

        PROPERTY()
        TVector<SAssetInfo> Referencers;
    };

    namespace MCP
    {
        // Tools that treat an asset as a reflected object, whatever its class.
        void RegisterAssetObjectTools(FStringView Owner);
    }
}
