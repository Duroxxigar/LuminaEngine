#include "AnimGraphNode_GetObjectParameter.h"
#include "UI/Tools/NodeGraph/Animation/AnimationGraphCompiler.h"

namespace Lumina
{
    FString CAnimGraphNode_GetObjectParameter::GetNodeTitleText() const
    {
        return ParameterName.IsNone() ? FString(GetNodeDisplayName()) : FString("Get ") + ParameterName.ToString();
    }

    void CAnimGraphNode_GetObjectParameter::BuildNode()
    {
        ObjectPin = CreateAnimPin("Object", ENodePinDirection::Output, EAnimPinType::Object);
    }

    void CAnimGraphNode_GetObjectParameter::GenerateBytecode(FAnimationGraphCompiler& Compiler)
    {
        Compiler.ValidateObjectParameterKey(ParameterName, ObjectType, this);

        if (ParameterName.IsNone())
        {
            EdNodeGraph::FError NodeWarning;
            NodeWarning.Name        = "Unbound Object Parameter";
            NodeWarning.Description = "Get Object Parameter has no field assigned, so it will always evaluate to nothing.";
            NodeWarning.Node        = this;
            Compiler.AddWarning(NodeWarning);
        }

        const int32 ParamIndex = Compiler.AddObjectParameter(ParameterName, ObjectType);
        const uint16 ObjectReg = Compiler.EmitLoadObjectParam((uint16)ParamIndex);

        Compiler.SetPinRegister(ObjectPin, ObjectReg);
    }
}
