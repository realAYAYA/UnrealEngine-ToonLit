// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetNodes/SGraphNodeK2Base.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UK2Node;

class GRAPHEDITOR_API SGraphNodeK2Default : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2Default){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node* InNode);
};
