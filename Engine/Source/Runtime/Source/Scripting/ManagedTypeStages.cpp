#include "RuntimePCH.h"

#include "ManagedTypeRegistry.h"
#include "ScriptDataStruct.h"
#include "ScriptableObject.h"
#include "DotNet/ManagedRenderScene.h"

namespace Lumina::Scripting
{
    namespace
    {
        // Minted classes are reused by name across reloads, with each instance rebinding through its bridge.
        // First, because everything after it reads the classes this produces.
        class FScriptableClassStage final : public IManagedTypeCompiler
        {
        public:

            const char* GetName() const override { return "ScriptableClasses"; }

            void Compile(TSpan<const FManagedTypeDefinition> Definitions) override
            {
                FScriptableRegistry::RefreshMintedClasses(Definitions);
            }
        };

        // After the class minting, since both read the generation that just loaded and a data struct may
        // name a class this reload only just produced.
        class FDataStructStage final : public IManagedTypeCompiler
        {
        public:

            const char* GetName() const override { return "DataStructs"; }

            void Compile(TSpan<const FManagedTypeDefinition>) override
            {
                FScriptDataStructRegistry::Get().Refresh();
            }
        };

        // Last: it rebuilds the renderers that were torn down before the load, so it must not run until
        // every type those renderers can reach exists again. Registered last also puts its PreUnload first,
        // which is the order the teardown needs: the proxies must go before the types they were built from.
        class FRenderSceneStage final : public IManagedTypeCompiler
        {
        public:

            const char* GetName() const override { return "RenderScenes"; }

            void PreUnload() override
            {
                DotNet::ManagedRenderScenes::PreScriptUnload();
            }

            // A failed compile keeps the previous generation, so only the renderers need putting back.
            void UnloadAborted() override
            {
                DotNet::ManagedRenderScenes::PostScriptLoad();
            }

            void Compile(TSpan<const FManagedTypeDefinition>) override
            {
                DotNet::ManagedRenderScenes::PostScriptLoad();
            }
        };

    }

    // Registration order is execution order, which is the whole point of declaring them. Idempotent against
    // the registry's current contents rather than against a one-shot latch, so it reinstalls after a reset.
    void RegisterBuiltInManagedTypeStages()
    {
        static FScriptableClassStage ScriptableClasses;
        static FDataStructStage      DataStructs;
        static FRenderSceneStage     RenderScenes;

        FManagedTypeRegistry& Registry = FManagedTypeRegistry::Get();
        Registry.Register(&ScriptableClasses);
        Registry.Register(&DataStructs);
        Registry.Register(&RenderScenes);
    }
}
