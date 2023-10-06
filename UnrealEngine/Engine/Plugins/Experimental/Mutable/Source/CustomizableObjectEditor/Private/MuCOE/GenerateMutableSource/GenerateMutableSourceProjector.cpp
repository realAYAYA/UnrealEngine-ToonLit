// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceProjector.h"

#include "MuCO/MutableProjectorTypeUtils.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/Nodes/CustomizableObjectNodeProjectorConstant.h"
#include "MuCOE/Nodes/CustomizableObjectNodeProjectorParameter.h"
#include "MuR/Parameters.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


mu::NodeProjectorPtr GenerateMutableSourceProjector(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)
	
	CheckNumOutputs(*Pin, GenerationContext);
	
	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceProjector), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeProjector*>(Generated->Node.get());
	}

	mu::NodeProjectorPtr Result;
	
	if (const UCustomizableObjectNodeProjectorConstant* TypedNodeConst = Cast<UCustomizableObjectNodeProjectorConstant>(Node))
	{
		mu::NodeProjectorConstantPtr ProjectorNode = new mu::NodeProjectorConstant();
		Result = ProjectorNode;

		const mu::PROJECTOR_TYPE ProjectorType = ProjectorUtils::GetEquivalentProjectorType(TypedNodeConst->Value.ProjectionType);
		switch (ProjectorType) 
		{ 
			case mu::PROJECTOR_TYPE::PLANAR:
				ProjectorNode->SetValue(
					ProjectorType,
					TypedNodeConst->Value.Position[0], TypedNodeConst->Value.Position[1], TypedNodeConst->Value.Position[2],
					TypedNodeConst->Value.Direction[0], TypedNodeConst->Value.Direction[1], TypedNodeConst->Value.Direction[2],
					TypedNodeConst->Value.Up[0], TypedNodeConst->Value.Up[1], TypedNodeConst->Value.Up[2],
					TypedNodeConst->Value.Scale[0], TypedNodeConst->Value.Scale[1], TypedNodeConst->Value.Scale[2],
					TypedNodeConst->Value.Angle);
				break;
			
			case mu::PROJECTOR_TYPE::CYLINDRICAL:
				{
					// Apply strange swizzle for scales
					// TODO: try to avoid this
					const float Radius = FMath::Abs(TypedNodeConst->Value.Scale[0] / 2.0f);
					const float Height = TypedNodeConst->Value.Scale[2];
					ProjectorNode->SetValue(
						ProjectorType,
						TypedNodeConst->Value.Position[0], TypedNodeConst->Value.Position[1], TypedNodeConst->Value.Position[2],
						-TypedNodeConst->Value.Direction[0], -TypedNodeConst->Value.Direction[1], -TypedNodeConst->Value.Direction[2],
						-TypedNodeConst->Value.Up[0], -TypedNodeConst->Value.Up[1], -TypedNodeConst->Value.Up[2],
						-Height, Radius, Radius,
						TypedNodeConst->Value.Angle);
				}
				break;
			
			case mu::PROJECTOR_TYPE::WRAPPING:
				ProjectorNode->SetValue(
					ProjectorType,
					TypedNodeConst->Value.Position[0], TypedNodeConst->Value.Position[1], TypedNodeConst->Value.Position[2],
					TypedNodeConst->Value.Direction[0], TypedNodeConst->Value.Direction[1], TypedNodeConst->Value.Direction[2],
					TypedNodeConst->Value.Up[0], TypedNodeConst->Value.Up[1], TypedNodeConst->Value.Up[2],
					TypedNodeConst->Value.Scale[0], TypedNodeConst->Value.Scale[1], TypedNodeConst->Value.Scale[2],
					TypedNodeConst->Value.Angle);
				break;
			
			case mu::PROJECTOR_TYPE::COUNT: 
			default:
				checkNoEntry();
		}
	}

	else if (const UCustomizableObjectNodeProjectorParameter* TypedNodeParam = Cast<UCustomizableObjectNodeProjectorParameter>(Node))
	{
		mu::NodeProjectorParameterPtr ProjectorNode = new mu::NodeProjectorParameter();
		Result = ProjectorNode;

		GenerationContext.AddParameterNameUnique(Node, TypedNodeParam->ParameterName);

		ProjectorNode->SetName(StringCast<ANSICHAR>(*TypedNodeParam->ParameterName).Get());
		ProjectorNode->SetUid(StringCast<ANSICHAR>(*GenerationContext.GetNodeIdUnique(Node).ToString()).Get());
		switch ((int)TypedNodeParam->DefaultValue.ProjectionType)
		{
		case 0:
			ProjectorNode->SetDefaultValue(
				mu::PROJECTOR_TYPE::PLANAR,
				TypedNodeParam->DefaultValue.Position[0], TypedNodeParam->DefaultValue.Position[1], TypedNodeParam->DefaultValue.Position[2],
				TypedNodeParam->DefaultValue.Direction[0], TypedNodeParam->DefaultValue.Direction[1], TypedNodeParam->DefaultValue.Direction[2],
				TypedNodeParam->DefaultValue.Up[0], TypedNodeParam->DefaultValue.Up[1], TypedNodeParam->DefaultValue.Up[2],
				TypedNodeParam->DefaultValue.Scale[0], TypedNodeParam->DefaultValue.Scale[1], TypedNodeParam->DefaultValue.Scale[2],
				TypedNodeParam->DefaultValue.Angle);
			break;

		case 1:
		{
			// Apply strange swizzle for scales
			// TODO: try to avoid this
			float Radius = FMath::Abs(TypedNodeParam->DefaultValue.Scale[0] / 2.0f);
			float Height = TypedNodeParam->DefaultValue.Scale[2];
			ProjectorNode->SetDefaultValue(
				mu::PROJECTOR_TYPE::CYLINDRICAL,
				TypedNodeParam->DefaultValue.Position[0], TypedNodeParam->DefaultValue.Position[1], TypedNodeParam->DefaultValue.Position[2],
				-TypedNodeParam->DefaultValue.Direction[0], -TypedNodeParam->DefaultValue.Direction[1], -TypedNodeParam->DefaultValue.Direction[2],
				-TypedNodeParam->DefaultValue.Up[0], -TypedNodeParam->DefaultValue.Up[1], -TypedNodeParam->DefaultValue.Up[2],
				-Height, Radius, Radius,
				TypedNodeParam->DefaultValue.Angle);
			break;
		}

		case 2:
			ProjectorNode->SetDefaultValue(
				mu::PROJECTOR_TYPE::WRAPPING,
				TypedNodeParam->DefaultValue.Position[0], TypedNodeParam->DefaultValue.Position[1], TypedNodeParam->DefaultValue.Position[2],
				TypedNodeParam->DefaultValue.Direction[0], TypedNodeParam->DefaultValue.Direction[1], TypedNodeParam->DefaultValue.Direction[2],
				TypedNodeParam->DefaultValue.Up[0], TypedNodeParam->DefaultValue.Up[1], TypedNodeParam->DefaultValue.Up[2],
				TypedNodeParam->DefaultValue.Scale[0], TypedNodeParam->DefaultValue.Scale[1], TypedNodeParam->DefaultValue.Scale[2],
				TypedNodeParam->DefaultValue.Angle);
			break;

		default:
			// Not implemented.
			check(false);
		}

		GenerationContext.ParameterUIDataMap.Add(TypedNodeParam->ParameterName, FParameterUIData(
			TypedNodeParam->ParameterName,
			TypedNodeParam->ParamUIMetadata,
			EMutableParameterType::Projector));
	}

	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	GenerationContext.GeneratedNodes.Add(Node);

	if (Result)
	{
		Result->SetMessageContext(Node);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

