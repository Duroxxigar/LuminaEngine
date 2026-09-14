#pragma once

#include "Containers/String.h"
#include "Containers/Vector.h"
#include "Core/Object/ObjectMacros.h"

#include "MCPDataTableTools.generated.h"

namespace Lumina
{
    REFLECT()
    struct MCPEDITOR_API SDataTableColumnInfo
    {
        GENERATED_BODY()

        PROPERTY()
        FString Name;

        /** Reflected type name, such as Float or Name or Map. */
        PROPERTY()
        FString Type;

        /** False for a field datatable.set_row_property cannot write. */
        PROPERTY()
        bool bWritable = true;
    };

    REFLECT()
    struct MCPEDITOR_API SDescribeDataTableParams
    {
        GENERATED_BODY()

        /** GUID of the table, from assets.search. */
        PROPERTY()
        FString Asset;
    };

    REFLECT()
    struct MCPEDITOR_API SDescribeDataTableResult
    {
        GENERATED_BODY()

        /** Struct every row is an instance of; empty when the table has none picked yet. */
        PROPERTY()
        FString RowStruct;

        PROPERTY()
        int32 RowCount = 0;

        PROPERTY()
        TVector<FString> RowNames;

        PROPERTY()
        TVector<SDataTableColumnInfo> Columns;

        /** Every row struct a table could use, for assets.set_property on RowStructName. */
        PROPERTY()
        TVector<FString> RowStructs;
    };

    REFLECT()
    struct MCPEDITOR_API SGetRowParams
    {
        GENERATED_BODY()

        PROPERTY()
        FString Asset;

        /** Row name, from datatable.describe. */
        PROPERTY()
        FString Row;
    };

    REFLECT()
    struct MCPEDITOR_API SGetRowResult
    {
        GENERATED_BODY()

        PROPERTY()
        FString Row;

        /** Every field as JSON, in the shape datatable.set_row_property takes back. */
        PROPERTY()
        FString Value;

        /** Field paths that cannot be written. */
        PROPERTY()
        TVector<FString> UnwritableFields;
    };

    REFLECT()
    struct MCPEDITOR_API SSetRowPropertyParams
    {
        GENERATED_BODY()

        PROPERTY()
        FString Asset;

        PROPERTY()
        FString Row;

        /** Field path inside the row, such as MaxStackSize or RaceMeshes or Meshes[0].Socket. */
        PROPERTY()
        FString Path;

        /** The new value as JSON. */
        PROPERTY(RawJson)
        FString Value;
    };

    REFLECT()
    struct MCPEDITOR_API SRowPropertyResult
    {
        GENERATED_BODY()

        /** What the field held before the change, as JSON. */
        PROPERTY()
        FString Previous;

        /** What it holds now, read back after applying. */
        PROPERTY()
        FString Current;

        /** True when the table's open editor recorded the edit, so editor.undo can reach it. */
        PROPERTY()
        bool bUndoable = false;
    };

    REFLECT()
    struct MCPEDITOR_API SAddRowParams
    {
        GENERATED_BODY()

        PROPERTY()
        FString Asset;

        /** Wanted row name. Empty or taken names get a numbered suffix. */
        PROPERTY()
        FString Name;

        /** Existing row to copy fields from, applied before Value. */
        PROPERTY()
        FString CopyFrom;

        /** Fields to set on the new row, as a JSON object. */
        PROPERTY(RawJson)
        FString Value;
    };

    REFLECT()
    struct MCPEDITOR_API SAddRowResult
    {
        GENERATED_BODY()

        /** The name the row ended up with. */
        PROPERTY()
        FString Row;

        PROPERTY()
        int32 Index = 0;
    };

    REFLECT()
    struct MCPEDITOR_API SRemoveRowParams
    {
        GENERATED_BODY()

        PROPERTY()
        FString Asset;

        PROPERTY()
        FString Row;
    };

    REFLECT()
    struct MCPEDITOR_API SRemoveRowResult
    {
        GENERATED_BODY()

        PROPERTY()
        bool bRemoved = false;

        PROPERTY()
        int32 RemainingCount = 0;
    };

    REFLECT()
    struct MCPEDITOR_API SRenameRowParams
    {
        GENERATED_BODY()

        PROPERTY()
        FString Asset;

        PROPERTY()
        FString Row;

        PROPERTY()
        FString NewName;
    };

    REFLECT()
    struct MCPEDITOR_API SRenameRowResult
    {
        GENERATED_BODY()

        PROPERTY()
        FString Row;
    };

    REFLECT()
    struct MCPEDITOR_API SImportCsvParams
    {
        GENERATED_BODY()

        PROPERTY()
        FString Asset;

        /** CSV content, not a path: the first line is the header and its first column is the row name. */
        PROPERTY()
        FString Text;
    };

    REFLECT()
    struct MCPEDITOR_API SImportCsvResult
    {
        GENERATED_BODY()

        PROPERTY()
        int32 RowCount = 0;

        PROPERTY()
        int32 Skipped = 0;

        /** Per-cell problems the import worked around. */
        PROPERTY()
        TVector<FString> Errors;

        /** Header columns the row struct has no field for. */
        PROPERTY()
        TVector<FString> UnknownColumns;
    };

    REFLECT()
    struct MCPEDITOR_API SExportCsvParams
    {
        GENERATED_BODY()

        PROPERTY()
        FString Asset;
    };

    REFLECT()
    struct MCPEDITOR_API SExportCsvResult
    {
        GENERATED_BODY()

        /** CSV text in the shape datatable.import_csv takes back. */
        PROPERTY()
        FString Text;
    };

    namespace MCP
    {
        // Row-level access to CDataTable, whose rows are instanced structs reflection alone cannot reach.
        void RegisterDataTableTools(FStringView Owner);
    }
}
