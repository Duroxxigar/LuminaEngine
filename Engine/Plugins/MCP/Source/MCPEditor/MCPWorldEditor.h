#pragma once

#include "LuminaEditor.h"
#include "UI/EditorUI.h"
#include "UI/Tools/WorldEditorTool.h"

namespace Lumina::MCP
{
    // Shared because several tool files reached for the open world editor with an identical copy each,
    // which the unity build then saw as one redefinition after another.
    inline FWorldEditorTool* FindWorldEditor()
    {
        if (GEditorEngine == nullptr)
        {
            return nullptr;
        }

        FEditorUI* UI = static_cast<FEditorUI*>(GEditorEngine->GetDevelopmentToolsUI());
        return UI != nullptr ? UI->FindTool<FWorldEditorTool>() : nullptr;
    }
}
