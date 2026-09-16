#pragma once

#include "Containers/Name.h"
#include "Containers/String.h"
#include "Core/Object/FunctionLibrary.h"
#include "Core/Object/ObjectMacros.h"
#include "GameLibrary.generated.h"

namespace Lumina
{
    class CGameInstance;
    class CWorld;
    class CWorldSubsystem;

    /** Engine level session operations. There is one engine per process, so none of this takes a world. */
    REFLECT()
    class RUNTIME_API CGameLibrary : public CFunctionLibrary
    {
        GENERATED_BODY()

    public:

        /** The swap runs at the next frame start, so calling this mid tick is safe. */
        FUNCTION()
        static void OpenLevel(const FString& Url);

        /** Ends the play session in the editor and exits the process in a packaged game, both deferred. */
        FUNCTION()
        static void QuitGame();

        /** The one object that outlives a level change, so where state that survives travel belongs. */
        FUNCTION()
        static CGameInstance* GetGameInstance();

        /** Resolved by class name, so a C# subsystem and a C++ one are found through the same call. */
        FUNCTION()
        static CWorldSubsystem* GetSubsystem(CWorld* World, const FName& ClassName);
    };
}
