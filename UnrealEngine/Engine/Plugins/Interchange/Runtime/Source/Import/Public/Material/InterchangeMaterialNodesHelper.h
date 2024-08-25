// Copyright Epic Games, Inc. All Rights Reserved. 
#pragma once
#include "CoreMinimal.h"

class UInterchangeBaseNode;
class UInterchangeBaseNodeContainer;
class UInterchangeShaderGraphNode;

namespace UE::Interchange::Materials::Private
{
	struct FMaterialNodesHelper
	{
	public:
		static bool SetupScalarParameter(UInterchangeBaseNodeContainer& BaseNodeContainer, UInterchangeShaderGraphNode* ShaderGraphNode, const FString& InputName, const float& AttributeValue);
		static bool SetupVectorParameter(UInterchangeBaseNodeContainer& BaseNodeContainer, UInterchangeShaderGraphNode* ShaderGraphNode, const FString& InputName, const FLinearColor& AttributeValue);
	};
} 
