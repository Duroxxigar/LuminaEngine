#pragma once

#include "Containers/Vector.h"
#include "Scripting/ScriptExports.h"

namespace Lumina
{
    class CStruct;
}

namespace Lumina::Scripting
{
    // Writes FScriptPropertyEntry values into a CScriptStruct buffer by walking the struct's FProperties.

    RUNTIME_API void WriteValuesToStruct(const CStruct* Layout, void* Buffer, const TVector<FScriptPropertyEntry>& Values);
}
