#pragma once
#include "EntitySystem.h"
#include "Core/Object/ObjectMacros.h"
#include "SocketAttachmentSystem.generated.h"

namespace Lumina
{
    // Drives entities with an SSocketAttachmentComponent from their parent's animated skeletal pose.
    // Runs after the animation systems (PrePhysics/Low) so the frame's pose is final, and while paused
    // so editor preview and paused gameplay keep attachments glued.
    REFLECT()
    class RUNTIME_API SSocketAttachmentSystem : public CEntitySystem
    {
        GENERATED_BODY()
    public:

        void Configure() override;

        // Writes the attached entity's transform; reads the parent's mesh/pose. Defined in the .cpp.

        void OnUpdate() override;
    };
}
