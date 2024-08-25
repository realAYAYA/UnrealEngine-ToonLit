// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupControllerTag.h"

#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Models/DMXControlConsoleFaderGroupControllerModel.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroupControllerTag"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorFaderGroupControllerTag::Construct(const FArguments& InArgs, const TWeakPtr<FDMXControlConsoleFaderGroupControllerModel>& InFaderGroupControllerModel)
	{
		if (!ensureMsgf(InFaderGroupControllerModel.IsValid(), TEXT("Invalid fader group controller model, cannot create fader group controller tag correctly.")))
		{
			return;
		}

		WeakFaderGroupControllerModel = InFaderGroupControllerModel;

		constexpr float SlotWidth = 8.f;
		constexpr float SingleSlotHeight = 24.f;
		constexpr float OddSlotHeight = 6.f;
		constexpr float EvenSlotHeight = 3.f;

		const TSharedRef<SWidget> OddTagSlotImage =
			SNew(SImage)
			.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroupTag"))
			.ColorAndOpacity(this, &SDMXControlConsoleEditorFaderGroupControllerTag::GetFaderGroupControllerEditorColor);

		const TSharedRef<SWidget> EvenTagSlotImage =
			SNew(SImage)
			.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.DefaultBrush"));

		const auto CreateTagSlotLambda = [](float Width, float Height, TSharedRef<SWidget> SlotImage)
			{
				return
					SNew(SBox)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.MinDesiredWidth(Width)
					.MinDesiredHeight(Height)
					[
						SlotImage
					];
			};

		ChildSlot
			[
				SNew(SHorizontalBox)

				// Single Tag Slot
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerTag::GetSingleSlotTagVisibility))

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateTagSlotLambda(SlotWidth, SingleSlotHeight, OddTagSlotImage)
					]
				]

				// Multiple Tag Slot
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerTag::GetMultiSlotTagVisibility))

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateTagSlotLambda(SlotWidth, OddSlotHeight, OddTagSlotImage)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateTagSlotLambda(SlotWidth, EvenSlotHeight, EvenTagSlotImage)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateTagSlotLambda(SlotWidth, OddSlotHeight, OddTagSlotImage)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateTagSlotLambda(SlotWidth, EvenSlotHeight, EvenTagSlotImage)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						CreateTagSlotLambda(SlotWidth, OddSlotHeight, OddTagSlotImage)
					]
				]
			];
	}

	FSlateColor SDMXControlConsoleEditorFaderGroupControllerTag::GetFaderGroupControllerEditorColor() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		const UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerModel.IsValid() ? FaderGroupControllerModel->GetFaderGroupController() : nullptr;
		if (FaderGroupController)
		{
			return FaderGroupController->GetEditorColor();
		}

		return FLinearColor::White;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerTag::GetSingleSlotTagVisibility() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		const bool bIsVisible = FaderGroupControllerModel.IsValid() && FaderGroupControllerModel->HasSingleFaderGroup();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerTag::GetMultiSlotTagVisibility() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		const bool bIsVisible = FaderGroupControllerModel.IsValid() && !FaderGroupControllerModel->HasSingleFaderGroup();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE
