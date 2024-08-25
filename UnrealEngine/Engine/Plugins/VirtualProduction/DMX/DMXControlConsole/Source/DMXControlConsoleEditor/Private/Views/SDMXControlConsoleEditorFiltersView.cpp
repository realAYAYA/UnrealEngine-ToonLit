// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFiltersView.h"

#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorToolbar.h"
#include "Editor.h"
#include "Filters/SFilterSearchBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleFilterModel.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"
#include "Widgets/SDMXControlConsoleEditorFilterButton.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFiltersView"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorFiltersView::Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleEditorToolbar> EditorToolbar, UDMXControlConsoleEditorModel* InEditorModel)
	{
		checkf(InEditorModel, TEXT("Invalid control console editor model, can't constuct filters view correctly."));
		EditorModel = InEditorModel;
		WeakToolbarPtr = EditorToolbar;

		if (UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData())
		{
			ControlConsoleData->GetOnDMXLibraryChanged().AddSP(this, &SDMXControlConsoleEditorFiltersView::RequestRefresh);
			ControlConsoleData->GetOnDMXLibraryReloaded().AddSP(this, &SDMXControlConsoleEditorFiltersView::RequestRefresh);
		}

		if (UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel->GetControlConsoleEditorData())
		{
			ControlConsoleEditorData->GetOnUserFiltersChanged().AddSP(this, &SDMXControlConsoleEditorFiltersView::UpdateUserFilterButtons);
		}

		ChildSlot
			[
				SNew(SScrollBox)
				.Orientation(EOrientation::Orient_Vertical)
					
				+SScrollBox::Slot()
				.AutoSize()
				.Padding(8.f)
				[
					SNew(SVerticalBox)
						
					// User filters section
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 8.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.f, 0.f)
						[
							SNew(SDMXControlConsoleEditorExpandArrowButton)
							.IsExpanded(true)
							.OnExpandClicked(this, &SDMXControlConsoleEditorFiltersView::OnSetFiltersBoxVisibility, EDMXControlConsoleFilterCategory::User)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.f, 2.f, 2.f, 0.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DMXEditorFiltersView_UserFilters", "User"))
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f)
					[
						SAssignNew(UserFiltersBox, SWrapBox)
						.UseAllottedSize(true)
						.Orientation(EOrientation::Orient_Horizontal)
						.Visibility(EVisibility::Collapsed)
						.InnerSlotPadding(FVector2D(4.f, 4.f))
					]

					//Attribute Name filters section
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 8.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)
						
						+SHorizontalBox::Slot()
						.Padding(2.f, 0.f)
						.AutoWidth()
						[
							SNew(SDMXControlConsoleEditorExpandArrowButton)
							.IsExpanded(true)
							.OnExpandClicked(this, &SDMXControlConsoleEditorFiltersView::OnSetFiltersBoxVisibility, EDMXControlConsoleFilterCategory::AttributeName)
						]

						+ SHorizontalBox::Slot()
						.Padding(2.f, 2.f, 2.f, 0.f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DMXEditorFiltersView_AttributeNameFilters","Attribute Names"))
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
					]

					+ SVerticalBox::Slot()
					.Padding(4.f)
					.AutoHeight()
					[
						SAssignNew(AttributeNameFiltersBox, SWrapBox)
						.UseAllottedSize(true)
						.Orientation(EOrientation::Orient_Horizontal)
						.Visibility(EVisibility::Collapsed)
						.InnerSlotPadding(FVector2D(4.f, 4.f))
					]

					//Universe ID filters section
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 8.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.f, 0.f)
						[
							SNew(SDMXControlConsoleEditorExpandArrowButton)
							.IsExpanded(true)
							.OnExpandClicked(this, &SDMXControlConsoleEditorFiltersView::OnSetFiltersBoxVisibility, EDMXControlConsoleFilterCategory::UniverseID)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.f, 2.f, 2.f, 0.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DMXEditorFiltersView_UniverseIDFilters", "Universe ID"))
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f)
					[
						SAssignNew(UniverseIDFiltersBox, SWrapBox)
						.UseAllottedSize(true)
						.Orientation(EOrientation::Orient_Horizontal)
						.Visibility(EVisibility::Collapsed)
						.InnerSlotPadding(FVector2D(4.f, 4.f))
					]

					// Fixture ID filters section
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 8.f, 0.f, 4.f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.f, 0.f)
						[
							SNew(SDMXControlConsoleEditorExpandArrowButton)
							.IsExpanded(true)
							.OnExpandClicked(this, &SDMXControlConsoleEditorFiltersView::OnSetFiltersBoxVisibility, EDMXControlConsoleFilterCategory::FixtureID)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.f, 2.f, 2.f, 0.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DMXEditorFiltersView_FixtureIDFilters", "FID"))
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f)
					[
						SAssignNew(FixtureIDFiltersBox, SWrapBox)
						.UseAllottedSize(true)
						.Orientation(EOrientation::Orient_Horizontal)
						.Visibility(EVisibility::Collapsed)
						.InnerSlotPadding(FVector2D(4.f, 4.f))
					]
				]
			];

		UpdateFilterButtons();
	}

	void SDMXControlConsoleEditorFiltersView::RequestRefresh()
	{
		if (!RefreshFiltersViewTimerHandle.IsValid())
		{
			RefreshFiltersViewTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXControlConsoleEditorFiltersView::UpdateFilterButtons));
		}
	}

	void SDMXControlConsoleEditorFiltersView::UpdateFilterButtons()
	{
		RefreshFiltersViewTimerHandle.Invalidate();

		FilterModels.Reset();

		UpdateUserFilterButtons();
		UpdateAttributeNameFilterButtons();
		UpdateUniverseIDFilterButtons();
		UpdateFixtureIDFilterButtons();
	}

	void SDMXControlConsoleEditorFiltersView::UpdateUserFilterButtons()
	{
		UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!ControlConsoleEditorData || !UserFiltersBox.IsValid())
		{
			return;
		}

		// Remove only user filters from the filter models array
		FilterModels.RemoveAll(
			[](const TSharedPtr<FDMXControlConsoleFilterModel>& UserFilterModel)
			{
				return UserFilterModel.IsValid() && UserFilterModel->IsUserFilter();
			});

		UserFiltersBox->ClearChildren();
		const TArray<FDMXControlConsoleEditorUserFilter>& UserFilters = ControlConsoleEditorData->GetUserFilters();
		for (const FDMXControlConsoleEditorUserFilter& UserFilter : UserFilters)
		{
			constexpr bool bIsUserFilter = true;
			const TSharedRef<FDMXControlConsoleFilterModel> FilterModel = MakeShared<FDMXControlConsoleFilterModel>(EditorModel, UserFilter.FilterLabel, UserFilter.FilterString, UserFilter.FilterColor, bIsUserFilter);
			FilterModel->GetOnEnableStateChanged().BindSP(this, &SDMXControlConsoleEditorFiltersView::OnFilterStateChanged);
			FilterModels.Add(FilterModel);
			
			UserFiltersBox->AddSlot()
			.FillLineWhenSizeLessThan(100.f)
			.Padding(2.f)
			[
				SNew(SDMXControlConsoleEditorFilterButton, FilterModel)
				.OnDisableAllFilters(this, &SDMXControlConsoleEditorFiltersView::OnDisableAllFilters)
			];

			const bool bIsUserFilterEnabled = UserFilter.bIsEnabled;
			if (bIsUserFilterEnabled)
			{
				FilterModel->SetIsEnabled(bIsUserFilterEnabled);
			}
		}
	}

	void SDMXControlConsoleEditorFiltersView::UpdateAttributeNameFilterButtons()
	{
		UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!ControlConsoleEditorData || !AttributeNameFiltersBox.IsValid())
		{
			return;
		}

		const FLinearColor AttributeNameFiltersColor = FStyleColors::AccentRed.GetSpecifiedColor();

		AttributeNameFiltersBox->ClearChildren();
		const TArray<FString>& AttributeNameFilters = ControlConsoleEditorData->GetAttributeNameFilters();
		for (const FString& AttributeNameFilter : AttributeNameFilters)
		{
			const TSharedRef<FDMXControlConsoleFilterModel> FilterModel = MakeShared<FDMXControlConsoleFilterModel>(EditorModel, AttributeNameFilter, AttributeNameFilter, AttributeNameFiltersColor);
			FilterModel->GetOnEnableStateChanged().BindSP(this, &SDMXControlConsoleEditorFiltersView::OnFilterStateChanged);
			FilterModels.Add(FilterModel);
			
			AttributeNameFiltersBox->AddSlot()
			.FillLineWhenSizeLessThan(100.f)
			.Padding(2.f)
			[
				SNew(SDMXControlConsoleEditorFilterButton, FilterModel)
				.OnDisableAllFilters(this, &SDMXControlConsoleEditorFiltersView::OnDisableAllFilters)
			];
		}
	}

	void SDMXControlConsoleEditorFiltersView::UpdateUniverseIDFilterButtons()
	{
		UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!ControlConsoleEditorData || !UniverseIDFiltersBox.IsValid())
		{
			return;
		}

		const FLinearColor UniverseIDFiltersColor = FStyleColors::AccentBlue.GetSpecifiedColor();

		UniverseIDFiltersBox->ClearChildren();
		const TArray<FString>& UniverseIDFilters = ControlConsoleEditorData->GetUniverseIDFilters();
		for (const FString& UniverseIDFilter : UniverseIDFilters)
		{
			const TSharedRef<FDMXControlConsoleFilterModel> FilterModel = MakeShared<FDMXControlConsoleFilterModel>(EditorModel, UniverseIDFilter, UniverseIDFilter, UniverseIDFiltersColor);
			FilterModel->GetOnEnableStateChanged().BindSP(this, &SDMXControlConsoleEditorFiltersView::OnFilterStateChanged);
			FilterModels.Add(FilterModel);
			
			UniverseIDFiltersBox->AddSlot()
			.FillLineWhenSizeLessThan(100.f)
			.Padding(2.f)
			[
				SNew(SDMXControlConsoleEditorFilterButton, FilterModel)
				.OnDisableAllFilters(this, &SDMXControlConsoleEditorFiltersView::OnDisableAllFilters)
			];
		}
	}

	void SDMXControlConsoleEditorFiltersView::UpdateFixtureIDFilterButtons()
	{
		UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!ControlConsoleEditorData || !FixtureIDFiltersBox.IsValid())
		{
			return;
		}

		const FLinearColor FixtureIDFiltersColor = FStyleColors::AccentYellow.GetSpecifiedColor();

		FixtureIDFiltersBox->ClearChildren();
		const TArray<FString>& FixtureIDFilters = ControlConsoleEditorData->GetFixtureIDFilters();
		for (const FString& FixtureIDFilter : FixtureIDFilters)
		{
			const TSharedRef<FDMXControlConsoleFilterModel> FilterModel = MakeShared<FDMXControlConsoleFilterModel>(EditorModel, FixtureIDFilter, FixtureIDFilter, FixtureIDFiltersColor);
			FilterModel->GetOnEnableStateChanged().BindSP(this, &SDMXControlConsoleEditorFiltersView::OnFilterStateChanged);
			FilterModels.Add(FilterModel);
			
			FixtureIDFiltersBox->AddSlot()
			.FillLineWhenSizeLessThan(100.f)
			.Padding(2.f)
			[
				SNew(SDMXControlConsoleEditorFilterButton, FilterModel)
				.OnDisableAllFilters(this, &SDMXControlConsoleEditorFiltersView::OnDisableAllFilters)
			];
		}
	}

	void SDMXControlConsoleEditorFiltersView::OnFilterStateChanged(TSharedPtr<FDMXControlConsoleFilterModel> FilterModel)
	{
		const TSharedPtr<FDMXControlConsoleEditorToolbar> Toolbar = WeakToolbarPtr.Pin();
		UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!FilterModel.IsValid() || !Toolbar || !ControlConsoleEditorData)
		{
			return;
		}

		const TSharedPtr<SFilterSearchBox>& FilterSearchBox = Toolbar->GetFilterSearchBox();
		if (!FilterSearchBox)
		{
			return;
		}

		// If the filter is a user filter, update its state
		const FString& FilterLabel = FilterModel->GetFilterLabel();
		FDMXControlConsoleEditorUserFilter* UserFilter = ControlConsoleEditorData->FindUserFilter(FilterLabel);
		if (UserFilter)
		{
			UserFilter->bIsEnabled = FilterModel->IsEnabled();
		}

		// Update the search box text string
		FString NewSearchBoxString = FilterSearchBox->GetText().ToString();

		const FString& FilterString = FilterModel->GetFilterString();
		if (FilterModel->IsEnabled() && NewSearchBoxString.IsEmpty())
		{
			NewSearchBoxString = FilterString;
		}
		else if (FilterModel->IsEnabled() && !NewSearchBoxString.Contains(FilterString))
		{
			NewSearchBoxString = FString::Format(TEXT("{0}, {1}"), { NewSearchBoxString, FilterString });
		}
		else if (!FilterModel->IsEnabled() && NewSearchBoxString.Contains(FilterString))
		{
			const TArray<FString> StringsToReplace = { TEXT(", ") + FilterString, FilterString + TEXT(", "), FilterString };
			for (const FString& StringToReplace : StringsToReplace)
			{
				NewSearchBoxString = NewSearchBoxString.Replace(*StringToReplace, TEXT(""));
			}
		}

		FilterSearchBox->SetText(FText::FromString(NewSearchBoxString.ToLower()));
		FSlateApplication::Get().SetUserFocus(0, FilterSearchBox.ToSharedRef());
	}

	void SDMXControlConsoleEditorFiltersView::OnDisableAllFilters()
	{
		for (const TSharedPtr<FDMXControlConsoleFilterModel>& FilterModel : FilterModels)
		{
			if (FilterModel.IsValid())
			{
				FilterModel->SetIsEnabled(false);
			}
		}
	}

	void SDMXControlConsoleEditorFiltersView::OnSetFiltersBoxVisibility(bool bIsNotVisible, EDMXControlConsoleFilterCategory FilterCategory)
	{
		const EVisibility NewVisibility = bIsNotVisible ? EVisibility::Collapsed : EVisibility::Visible;

		switch (FilterCategory)
		{
		case EDMXControlConsoleFilterCategory::User:
			if (UserFiltersBox.IsValid())
			{
				UserFiltersBox->SetVisibility(NewVisibility);
			}
			break;
		case EDMXControlConsoleFilterCategory::AttributeName:
			if (AttributeNameFiltersBox.IsValid())
			{
				AttributeNameFiltersBox->SetVisibility(NewVisibility);
			}
			break;
		case EDMXControlConsoleFilterCategory::UniverseID:
			if (UniverseIDFiltersBox.IsValid())
			{
				UniverseIDFiltersBox->SetVisibility(NewVisibility);
			}
			break;
		case EDMXControlConsoleFilterCategory::FixtureID:
			if (FixtureIDFiltersBox.IsValid())
			{
				FixtureIDFiltersBox->SetVisibility(NewVisibility);
			}
			break;
		default:
			checkf(0, TEXT("Undefined enum value"));
			break;
		}
	}
}

#undef LOCTEXT_NAMESPACE
