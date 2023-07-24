// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMaterialEditorStrataWidget.h"

#include "EditorWidgetsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditor.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMaterialEditorStrataWidget"

DEFINE_LOG_CATEGORY_STATIC(LogMaterialEditorStrataWidget, Log, All);

void SMaterialEditorStrataWidget::Construct(const FArguments& InArgs, TWeakPtr<FMaterialEditor> InMaterialEditorPtr)
{
	MaterialEditorPtr = InMaterialEditorPtr;

	ButtonApplyToPreview = SNew(SButton)
		.HAlign(HAlign_Center)
		.OnClicked(this, &SMaterialEditorStrataWidget::OnButtonApplyToPreview)
		.Text(LOCTEXT("ButtonApplyToPreview", "Apply to preview"));

	CheckBoxForceFullSimplification = SNew(SCheckBox)
		.Padding(5.0f)
		.ToolTipText(LOCTEXT("CheckBoxForceFullSimplificationToolTip", "This will force full simplification of the material."));	// Just a test, needs to be more explicit

	if (Strata::IsStrataEnabled())
	{
		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SWrapBox)
					.UseAllottedSize(true)
					+SWrapBox::Slot()
					.Padding(5.0f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						CheckBoxForceFullSimplification->AsShared()
					]
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(16.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::White)
					.ShadowColorAndOpacity(FLinearColor::Black)
					.ShadowOffset(FVector2D::UnitVector)
					.Text(LOCTEXT("FullsimplificationLabel", "Full simplification"))
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 0.0f)
			[
				SNew(SWrapBox)
				.UseAllottedSize(true)
				+SWrapBox::Slot()
				.Padding(5.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					ButtonApplyToPreview->AsShared()
				]
			]
		];
	}
	else
	{
		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 0.0f)
			[
				SNew(SWrapBox)
				.UseAllottedSize(true)
				+SWrapBox::Slot()
				.Padding(5.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::Yellow)
					.ShadowColorAndOpacity(FLinearColor::Black)
					.ShadowOffset(FVector2D::UnitVector)
					.Text(LOCTEXT("SubstrateWidgetNotEnable", "Substrate is not enabled."))
				]
			]
		];
	}
}

TSharedRef<SWidget> SMaterialEditorStrataWidget::GetContent()
{
	return SharedThis(this);
}

SMaterialEditorStrataWidget::~SMaterialEditorStrataWidget()
{
}

void SMaterialEditorStrataWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
}

FReply SMaterialEditorStrataWidget::OnButtonApplyToPreview()
{
	if (MaterialEditorPtr.IsValid())
	{
		UMaterialInterface* MaterialInterface = MaterialEditorPtr.Pin()->GetMaterialInterface();

		FStrataCompilationConfig StrataCompilationConfig;
		StrataCompilationConfig.bFullSimplify = CheckBoxForceFullSimplification->IsChecked();
		MaterialInterface->SetStrataCompilationConfig(StrataCompilationConfig);

		MaterialInterface->ForceRecompileForRendering();
	}



	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
