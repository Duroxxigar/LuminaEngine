#include "RuntimePCH.h"

#include "GameplayProfilerLibrary.h"

#include "Core/Profiler/GameplayProfiler.h"

namespace Lumina
{
    void CGameplayProfilerLibrary::BeginScope(const FString& Name)
    {
        if (!Name.empty())
        {
            FGameplayProfiler::Get().BeginScope(FStringView(Name.c_str(), Name.size()));
        }
    }

    void CGameplayProfilerLibrary::EndScope()
    {
        FGameplayProfiler::Get().EndScope();
    }

    bool CGameplayProfilerLibrary::IsProfilerEnabled()
    {
        return FGameplayProfiler::Get().IsEnabled();
    }
}
