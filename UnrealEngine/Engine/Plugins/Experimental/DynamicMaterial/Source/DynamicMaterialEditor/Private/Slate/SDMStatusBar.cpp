// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SDMStatusBar.h"
#include "Components/DMMaterialSlot.h"
#include "DetailLayoutBuilder.h"
#include "DynamicMaterialEditorStyle.h"
#include "Model/DynamicMaterialModel.h"
#include "Slate/SDMEditor.h"
#include "Slate/SDMSlot.h"
#include "SlateOptMacros.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMStatusBar"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDMStatusBar::Construct(const FArguments& InArgs, const TWeakObjectPtr<UDynamicMaterialModel>& InEditorModel, const TSharedRef<SDMEditor>& InEditorWidget)
{
	SetCanTick(true);

	ensure(InEditorModel.IsValid());
	EditorModelWeak = InEditorModel;
	EditorWidgetWeak = InEditorWidget;

	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.BorderImage(FDynamicMaterialEditorStyle::GetBrush("Border.Top"))
		.BorderBackgroundColor(FLinearColor(1, 1, 1, 0.05f))
		[
			SNew(SWrapBox)
			.HAlign(HAlign_Left)
			.InnerSlotPadding(FVector2D(5.0f))
			.UseAllottedSize(true)
			+ CreateStatsWrapBoxEntry(
				TAttribute<FText>::CreateSP(this, &SDMStatusBar::GetNumPixelShaderInstructionsText), 
				LOCTEXT("NumPixelShaderInstructions_ToolTip", "Pixel shader instruction count")
			)
			+ CreateStatsWrapBoxEntry(
				TAttribute<FText>::CreateSP(this, &SDMStatusBar::GetNumVertexShaderInstructionsText), 
				LOCTEXT("NumVertexShaderInstructions_ToolTip", "Vertex shader instruction count")
			)
			+ CreateStatsWrapBoxEntry(
				TAttribute<FText>::CreateSP(this, &SDMStatusBar::GetNumSamplersText), 
				LOCTEXT("NumSamplers_ToolTip", "Sampler count")
			)
		]
	];
}

SWrapBox::FSlot::FSlotArguments SDMStatusBar::CreateStatsWrapBoxEntry(TAttribute<FText> InText, const FText& InTooltipText)
{
	const FLinearColor SeparatorColor = FLinearColor(1, 1, 1, 0.1f);

	return 
		MoveTemp(
			SWrapBox::Slot()
			.Padding(0.0f, 2.0f, 0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				.ToolTipText(InTooltipText)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.BorderBackgroundColor(SeparatorColor)
					.BorderImage(FDynamicMaterialEditorStyle::GetBrush("Border.Left"))
					
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(5.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "SmallText")
					.Text(InText)
				]
			]
		);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText SDMStatusBar::GetNumMaterialSlotsText() const
{
	return FText::Format(LOCTEXT("NumMaterialSlots_Text", "Material Slots {0}"), CachedSlotCount);
}

FText SDMStatusBar::GetNumTotalLayersText() const
{
	return FText::Format(LOCTEXT("NumTotalLayers_Text", "Total Layers {0}"), CachedCurrentLayerCount);
}

FText SDMStatusBar::GetNumCurrentSlotLayersText() const
{
	return FText::Format(LOCTEXT("NumCurrentSlotLayers_Text", "Layers {0}"), CachedTotalLayerCount);
}

FText SDMStatusBar::GetNumPixelShaderInstructionsText() const
{
	return FText::Format(LOCTEXT("NumPixelShaderInstructions_Text", "PS Instructions {0}"), CachedMaterialStats.NumPixelShaderInstructions);
}

FText SDMStatusBar::GetNumVertexShaderInstructionsText() const
{
	return FText::Format(LOCTEXT("NumVertexShaderInstructions_Text", "VS Instructions {0}"), CachedMaterialStats.NumVertexShaderInstructions);
}

FText SDMStatusBar::GetNumSamplersText() const
{
	return FText::Format(LOCTEXT("NumSamplers_Text", "Samplers {0}"), CachedMaterialStats.NumSamplers);
}

FText SDMStatusBar::GetNumPixelTextureSamplesText() const
{
	return FText::Format(LOCTEXT("NumPixelTextureSamples_Text", "PT Samples {0}"), CachedMaterialStats.NumPixelTextureSamples);
}

FText SDMStatusBar::GetNumVertexTextureSamplesText() const
{
	return FText::Format(LOCTEXT("NumVertexTextureSamples_Text", "VT Samples {0}"), CachedMaterialStats.NumVertexTextureSamples);
}

FText SDMStatusBar::GetNumVirtualTextureSamplesText() const
{
	return FText::Format(LOCTEXT("NumVirtualTextureSamples_Text", "Virtual Texture Samples {0}"), CachedMaterialStats.NumVirtualTextureSamples);
}

FText SDMStatusBar::GetNumUVScalarsText() const
{
	return FText::Format(LOCTEXT("NumUVScalars_Text", "UV Scalars {0}"), CachedMaterialStats.NumUVScalars);
}

FText SDMStatusBar::GetNumInterpolatorScalarsText() const
{
	return FText::Format(LOCTEXT("NumInterpolatorScalars_Text", "Interpolator Scalars {0}"), CachedMaterialStats.NumInterpolatorScalars);
}

#undef LOCTEXT_NAMESPACE
