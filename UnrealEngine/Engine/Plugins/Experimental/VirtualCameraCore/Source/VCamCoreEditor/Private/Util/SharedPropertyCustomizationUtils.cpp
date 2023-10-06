// Copyright Epic Games, Inc. All Rights Reserved.

#include "SharedPropertyCustomizationUtils.h"

#include "Customizations/StateSwitcher/SStringSelectionComboBox.h"
#include "UI/Switcher/VCamStateSwitcherWidget.h"

#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "VCamCoreEditor::StateSwitcher"

namespace UE::VCamCoreEditor::Private::StateSwitcher
{
	void CustomizeCurrentState(UVCamStateSwitcherWidget& StateSwitcher, IDetailPropertyRow& DetailPropertyRow, TSharedRef<IPropertyHandle> CurrentStatePropertyHandle, const FSlateFontInfo& RegularFont)
	{
		TWeakObjectPtr<UVCamStateSwitcherWidget> WeakStateSwitcher = &StateSwitcher;
		DetailPropertyRow
			.CustomWidget()
			.NameContent()
			[
				CurrentStatePropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)

				// I don't expect this to ever show since UVCamStateSwitcherWidget SHOULD handle incorrect states
				+SHorizontalBox::Slot()
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.DesiredSizeOverride(FVector2D{ 24.f, 24.f })
					.Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor"))
					.ToolTipText(LOCTEXT("StateNotFound", "This state was not found on the target widget. Was it removed?"))
					.Visibility_Lambda([WeakStateSwitcher]()
					{
						if (WeakStateSwitcher.IsValid())
						{
							return WeakStateSwitcher->GetStates().Contains(WeakStateSwitcher->GetCurrentState())
								? EVisibility::Collapsed
								: EVisibility::Visible;
						}
						return EVisibility::Collapsed;
					})
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SStringSelectionComboBox)
					.ToolTipText(CurrentStatePropertyHandle->GetToolTipText())
					.SelectedItem_Lambda([CurrentStatePropertyHandle]()
					{
						FName Name;
						CurrentStatePropertyHandle->GetValue(Name);
						return Name.ToString();
					})
					.ItemList_Lambda([WeakStateSwitcher]() -> TArray<FString>
					{
						if (WeakStateSwitcher.IsValid())
						{
							TArray<FString> States;
							Algo::Transform(WeakStateSwitcher->GetStates(), States, [](const FName& Name){ return Name.ToString(); });
							return States;
						}
						return {};
					})
					.OnItemSelected_Lambda([CurrentStatePropertyHandle](const FString& SelectedItem)
					{
						CurrentStatePropertyHandle->SetValue(FName(*SelectedItem));
					})
					.Font(RegularFont)
				]
			];
	}
}

#undef LOCTEXT_NAMESPACE