// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Rendering/RenderingCommon.h"
#include "SGraphPin.h"

class UMaterialGraphNode;
struct FSubstrateMaterialCompilationOutput;

struct GRAPHEDITOR_API FSubstrateWidget
{
	static const TSharedRef<SWidget> ProcessOperator(const FSubstrateMaterialCompilationOutput& CompilationOutput);
	static const TSharedRef<SWidget> ProcessOperator(const FSubstrateMaterialCompilationOutput& CompilationOutput, const TArray<FGuid>& InGuid);
	static void GetPinColor(TSharedPtr<SGraphPin>& Out, const UMaterialGraphNode* InNode);
	static FLinearColor GetConnectionColor();
	static bool HasInputSubstrateType(const UEdGraphPin* InPin);
	static bool HasOutputSubstrateType(const UEdGraphPin* InPin);
};