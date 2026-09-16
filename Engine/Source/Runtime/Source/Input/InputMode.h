#pragma once

#include "Core/Object/ObjectMacros.h"
#include "InputMode.generated.h"

namespace Lumina
{
    REFLECT()
    enum class EInputMode : uint8
    {
        Game,
        UI,
        GameAndUI,
    };

    inline const char* InputModeToString(EInputMode Mode)
    {
        switch (Mode)
        {
        case EInputMode::Game:      return "Game";
        case EInputMode::UI:        return "UI";
        case EInputMode::GameAndUI: return "GameAndUI";
        }
        return "Game";
    }
}
