// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Rendering/RenderingCommon.h"
#include "SGraphPin.h"

class UMaterialGraphNode;
struct FStrataMaterialCompilationOutput;

struct GRAPHEDITOR_API FSubstrateWidget
{
	static const TSharedRef<SWidget> ProcessOperator(const FStrataMaterialCompilationOutput& CompilationOutput);
	static const TSharedRef<SWidget> ProcessOperator(const FStrataMaterialCompilationOutput& CompilationOutput, const FGuid& InGuid);
	static void GetPinColor(TSharedPtr<SGraphPin>& Out, const UMaterialGraphNode* InNode);
};