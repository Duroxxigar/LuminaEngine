#pragma once

#include "Core/Object/Object.h"
#include "Core/Object/ObjectMacros.h"
#include "FunctionLibrary.generated.h"

namespace Lumina
{
    // Base for stateless helpers that take their subject as an argument instead of owning it as a method.
    REFLECT()
    class RUNTIME_API CFunctionLibrary : public CObject
    {
        GENERATED_BODY()
    };
}
