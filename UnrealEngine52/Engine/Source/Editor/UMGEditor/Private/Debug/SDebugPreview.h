// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FWidgetBlueprintEditor;
class UPlayer;
class UUserWidget;

namespace UE::UMG
{

class SDebugPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDebugPreview) {}
	SLATE_END_ARGS()

	~SDebugPreview();

	void Construct(const FArguments& Args, TSharedPtr<FWidgetBlueprintEditor> InWidgetBlueprintEditor);

private:

	TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor;
	TObjectPtr<UUserWidget> CreatedWidget;
};

} // namespace UE::UMG