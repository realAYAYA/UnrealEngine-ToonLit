// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLinkParametersDialog.h"
#include "SParameterPicker.h"
#include "Param/ParameterPickerArgs.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SLinkParametersDialog"

namespace UE::AnimNext::Editor
{

class SParameterToLink : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SParameterToLink) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FParameterBindingReference& InBindingReference)
	{
		BindingReference = InBindingReference;
		
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(5.0f, 3.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromName(BindingReference.Parameter))
				.ToolTipText(FText::FromName((BindingReference.Parameter)))
			]
		];
	}

	FParameterBindingReference BindingReference;
};

void SLinkParametersDialog::Construct(const FArguments& InArgs)
{
	FParameterPickerArgs ParameterPickerArgs;
	ParameterPickerArgs.bShowUnboundParameters = false;
	ParameterPickerArgs.OnGetParameterBindings = &OnGetParameterBindings;
	ParameterPickerArgs.OnSelectionChanged = FSimpleDelegate::CreateSP(this, &SLinkParametersDialog::HandleSelectionChanged);
	ParameterPickerArgs.OnFilterParameter = InArgs._OnFilterParameter;

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Link Parameters"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(500.f, 500.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SBox)
			.Padding(5.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SParameterPicker)
					.Args(ParameterPickerArgs)
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.MinDesiredHeight(40.0f)
					.Padding(5.0f)
					[
						SAssignNew(QueuedParametersBox, SWrapBox)
						.UseAllottedWidth(true)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
						.Text(LOCTEXT("LinkButton", "Link"))
						.ToolTipText(LOCTEXT("LinkButtonTooltip", "Link the selected parameters to the current parameter block"))
						.OnClicked_Lambda([this]()
						{
							RequestDestroyWindow();
							return FReply::Handled();
						})
					]
					+SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.Text(LOCTEXT("CancelButton", "Cancel"))
						.ToolTipText(LOCTEXT("CancelButtonTooltip", "Cancel adding linked parameters"))
						.OnClicked_Lambda([this]()
						{
							bCancelPressed = true;
							RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
			]
		]);
}

bool SLinkParametersDialog::ShowModal(TArray<FParameterBindingReference>& OutParameters)
{
	FSlateApplication::Get().AddModalWindow(SharedThis(this), FGlobalTabmanager::Get()->GetRootWindow());

	if(!bCancelPressed)
	{
		OnGetParameterBindings.ExecuteIfBound(OutParameters);
		return true;
	}
	return false;
}

void SLinkParametersDialog::HandleSelectionChanged()
{
	QueuedParametersBox->ClearChildren();

	TArray<FParameterBindingReference> ParameterBindings;
	OnGetParameterBindings.ExecuteIfBound(ParameterBindings);
	for(const FParameterBindingReference& ParameterBinding : ParameterBindings)
	{
		QueuedParametersBox->AddSlot()
		.Padding(2.0f)
		[
			SNew(SParameterToLink, ParameterBinding)
		];
	}
}

}

#undef LOCTEXT_NAMESPACE