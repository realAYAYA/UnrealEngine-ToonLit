// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFilterButton.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layout/WidgetPath.h"
#include "Models/DMXControlConsoleFilterModel.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFilterButton"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorFilterButton::Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleFilterModel>& InFilterModel)
	{
		WeakFilterModel = InFilterModel;

		OnDisableAllFiltersDelegate = InArgs._OnDisableAllFilters;

		ChildSlot
			[
				SNew(SBox)
				.MinDesiredWidth(120.f)
				[
					SNew(SBorder)
					.Padding(1.0f)
					.BorderImage(FAppStyle::Get().GetBrush("FilterBar.FilterBackground"))
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("FilterBar.FilterImage"))
							.ColorAndOpacity(this, &SDMXControlConsoleEditorFilterButton::GetFilterButtonColor)
							.IsEnabled(this, &SDMXControlConsoleEditorFilterButton::IsEnabled)
						]

						+ SHorizontalBox::Slot()
						.Padding(FMargin(4.f, 1.f, 4.f, 1.f))
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.ToolTipText(this, &SDMXControlConsoleEditorFilterButton::GetFilterStringAsText)
							.OnClicked(this, &SDMXControlConsoleEditorFilterButton::OnFilterButtonClicked)
							[
								SNew(STextBlock)
								.Text(this, &SDMXControlConsoleEditorFilterButton::GetFilterLabelAsText)
								.IsEnabled(this, &SDMXControlConsoleEditorFilterButton::IsEnabled)
							]
						]
					]
				]
			];
	}

	FReply SDMXControlConsoleEditorFilterButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			const FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, GenerateFilterButtonMenuWidget(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}

		return FReply::Handled();
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFilterButton::GenerateFilterButtonMenuWidget()
	{
		constexpr bool bShouldCloseWindowAfterClosing = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

		MenuBuilder.BeginSection("FilterOptions", LOCTEXT("FilterContextHeading", "Filter Options"));
		{
			// Enable/disable filter options
			const auto EnableStateMenuEntryLambda = [this, &MenuBuilder](const FText& Label, const FText& ToolTip, bool bEnable)
				{
					MenuBuilder.AddMenuEntry
					(
						Label,
						ToolTip,
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFilterButton::SetIsFilterEnabled, bEnable))
					);
				};
			
			bool bEnableState = true;
			EnableStateMenuEntryLambda
			(
				FText::Format(LOCTEXT("EnableFilter", "Enable: {0}"), GetFilterLabelAsText()),
				LOCTEXT("EnableFilterTooltip", "Enable this filter from the list."),
				bEnableState
			);

			bEnableState = false;
			EnableStateMenuEntryLambda
			(
				FText::Format(LOCTEXT("DisableFilter", "Disable: {0}"), GetFilterLabelAsText()),
				LOCTEXT("DisableFilterTooltip", "Disable this filter from the list."),
				bEnableState
			);

			// Remove filter option
			MenuBuilder.AddMenuEntry
			(
				FText::Format(LOCTEXT("RemoveFilter", "Remove: {0}"), GetFilterLabelAsText()),
				LOCTEXT("RemoveFilterTooltip", "Remove this filter from the list."),
				FSlateIcon(),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFilterButton::OnRemoveFilter),
					FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFilterButton::IsUserFilter),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateSP(this, &SDMXControlConsoleEditorFilterButton::IsUserFilter)
				)
			);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("BulkFilterOptions", LOCTEXT("BulkFilterContextHeading", "Bulk Filter Options"));
		{
			// Disable all filters but this option
			MenuBuilder.AddMenuEntry
			(
				FText::Format(LOCTEXT("DisableAllButThisFilter", "Disable All But This: {0}"), GetFilterLabelAsText()),
				LOCTEXT("DisableAllButThisFilterTooltip", "Disable all filter from the list but this."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFilterButton::OnDisableAllFiltersButThis))
			);

			// Disable all filters option
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("DisableAllFilters", "Disable All Filters"),
				LOCTEXT("DisableAllFiltersTooltip", "Disable all filters from the list."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFilterButton::OnDisableAllFilters))
			);
		}

		return MenuBuilder.MakeWidget();
	}

	bool SDMXControlConsoleEditorFilterButton::IsUserFilter() const
	{
		return WeakFilterModel.IsValid() && WeakFilterModel.Pin()->IsUserFilter();
	}

	bool SDMXControlConsoleEditorFilterButton::IsFilterEnabled() const
	{
		return WeakFilterModel.IsValid() && WeakFilterModel.Pin()->IsEnabled();
	}

	void SDMXControlConsoleEditorFilterButton::SetIsFilterEnabled(bool bEnable)
	{
		if (WeakFilterModel.IsValid())
		{
			WeakFilterModel.Pin()->SetIsEnabled(bEnable);
		}
	}

	FText SDMXControlConsoleEditorFilterButton::GetFilterLabelAsText() const
	{
		return WeakFilterModel.IsValid() ? FText::FromString(WeakFilterModel.Pin()->GetFilterLabel()) : FText::GetEmpty();
	}

	FText SDMXControlConsoleEditorFilterButton::GetFilterStringAsText() const
	{
		return WeakFilterModel.IsValid() ? FText::FromString(WeakFilterModel.Pin()->GetFilterString()) : FText::GetEmpty();
	}

	FReply SDMXControlConsoleEditorFilterButton::OnFilterButtonClicked()
	{
		if (WeakFilterModel.IsValid())
		{
			const bool bIsFilterEnabled = IsFilterEnabled();
			WeakFilterModel.Pin()->SetIsEnabled(!bIsFilterEnabled);
		}

		return FReply::Handled();
	}

	void SDMXControlConsoleEditorFilterButton::OnRemoveFilter() const
	{
		if (WeakFilterModel.IsValid())
		{
			WeakFilterModel.Pin()->RemoveFilter();
		}
	}

	void SDMXControlConsoleEditorFilterButton::OnDisableAllFiltersButThis()
	{
		if (WeakFilterModel.IsValid())
		{
			OnDisableAllFilters();
			SetIsFilterEnabled(true);
		}
	}

	void SDMXControlConsoleEditorFilterButton::OnDisableAllFilters() const
	{
		OnDisableAllFiltersDelegate.ExecuteIfBound();
	}

	FSlateColor SDMXControlConsoleEditorFilterButton::GetFilterButtonColor() const
	{
		const TSharedPtr<FDMXControlConsoleFilterModel> FilterModel = WeakFilterModel.Pin();
		return 
			FilterModel.IsValid() && FilterModel->IsEnabled() ? 
			FilterModel->GetFilterColor() : 
			FAppStyle::Get().GetSlateColor("Colors.Recessed");
	}
}

#undef LOCTEXT_NAMESPACE
