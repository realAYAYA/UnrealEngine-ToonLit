// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"
#include "Param/ParamType.h"

class URigVMPin;
class URigVMEdGraphNode;

namespace UE::AnimNext::Editor
{

// A pin widget that allows picking using an AnimNext parameter picker
class SGraphPinParamName : public SGraphPin
{
	SLATE_BEGIN_ARGS(SGraphPinParamName)
		: _ModelPin(nullptr)
		, _GraphNode(nullptr)
	{}

	SLATE_ARGUMENT(URigVMPin*, ModelPin)

	SLATE_ARGUMENT(URigVMEdGraphNode*, GraphNode)

	SLATE_ARGUMENT(FAnimNextParamType, FilterType)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

private:
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;

	URigVMPin* ModelPin = nullptr;

	URigVMEdGraphNode* Node = nullptr;

	FAnimNextParamType FilterType;

	FAnimNextParamType CachedType;
};

}