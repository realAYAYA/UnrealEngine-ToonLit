// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceProjector.h"

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeProjectorConstant.h"
#include "MuCOE/Nodes/CustomizableObjectNodeProjectorParameter.h"
#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeProjector.h"
#include "Templates/Casts.h"

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

		switch ((int)TypedNodeConst->Value.ProjectionType)
		{
		case 0:
			ProjectorNode->SetValue(
				mu::PROJECTOR_TYPE::PLANAR,
				TypedNodeConst->Value.Position[0], TypedNodeConst->Value.Position[1], TypedNodeConst->Value.Position[2],
				TypedNodeConst->Value.Direction[0], TypedNodeConst->Value.Direction[1], TypedNodeConst->Value.Direction[2],
				TypedNodeConst->Value.Up[0], TypedNodeConst->Value.Up[1], TypedNodeConst->Value.Up[2],
				TypedNodeConst->Value.Scale[0], TypedNodeConst->Value.Scale[1], TypedNodeConst->Value.Scale[2],
				TypedNodeConst->Value.Angle);
			break;

		case 1:
		{
			// Apply strange swizzle for scales
			// TODO: try to avoid this
			float Radius = FMath::Abs(TypedNodeConst->Value.Scale[0] / 2.0f);
			float Height = TypedNodeConst->Value.Scale[2];
			ProjectorNode->SetValue(
				mu::PROJECTOR_TYPE::CYLINDRICAL,
				TypedNodeConst->Value.Position[0], TypedNodeConst->Value.Position[1], TypedNodeConst->Value.Position[2],
				-TypedNodeConst->Value.Direction[0], -TypedNodeConst->Value.Direction[1], -TypedNodeConst->Value.Direction[2],
				-TypedNodeConst->Value.Up[0], -TypedNodeConst->Value.Up[1], -TypedNodeConst->Value.Up[2],
				-Height, Radius, Radius,
				TypedNodeConst->Value.Angle);
			break;
		}

		case 2:
			ProjectorNode->SetValue(
				mu::PROJECTOR_TYPE::WRAPPING,
				TypedNodeConst->Value.Position[0], TypedNodeConst->Value.Position[1], TypedNodeConst->Value.Position[2],
				TypedNodeConst->Value.Direction[0], TypedNodeConst->Value.Direction[1], TypedNodeConst->Value.Direction[2],
				TypedNodeConst->Value.Up[0], TypedNodeConst->Value.Up[1], TypedNodeConst->Value.Up[2],
				TypedNodeConst->Value.Scale[0], TypedNodeConst->Value.Scale[1], TypedNodeConst->Value.Scale[2],
				TypedNodeConst->Value.Angle);
			break;

		default:
			// Not implemented.
			check(false);
		}

	}

	else if (const UCustomizableObjectNodeProjectorParameter* TypedNodeParam = Cast<UCustomizableObjectNodeProjectorParameter>(Node))
	{
		mu::NodeProjectorParameterPtr ProjectorNode = new mu::NodeProjectorParameter();
		Result = ProjectorNode;

		GenerationContext.AddParameterNameUnique(Node, TypedNodeParam->ParameterName);

		ProjectorNode->SetName(TCHAR_TO_ANSI(*TypedNodeParam->ParameterName));
		ProjectorNode->SetUid(TCHAR_TO_ANSI(*GenerationContext.GetNodeIdUnique(Node).ToString()));
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

