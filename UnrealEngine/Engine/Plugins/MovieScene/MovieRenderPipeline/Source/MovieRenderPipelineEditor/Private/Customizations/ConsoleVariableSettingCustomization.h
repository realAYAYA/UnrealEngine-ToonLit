// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomization.h"
#include "IDetailGroup.h"
#include "MoviePipelineConsoleVariableSetting.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Sections/MovieSceneConsoleVariableTrackInterface.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SMoviePipelineEditor"

/** Customize how properties in UMoviePipelineConsoleVariableSetting appear in the details panel. */
class FConsoleVariablesSettingDetailsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FConsoleVariablesSettingDetailsCustomization>();
	}

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		IDetailCategoryBuilder& SettingsCategory = DetailBuilder.EditCategory(TEXT("Settings"));
		const TSharedRef<IPropertyHandle> ConsoleVariablePresetsHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMoviePipelineConsoleVariableSetting, ConsoleVariablePresets));

		// Customize how the console variable presets array looks. Each preset is shown as a group, and that group's children
		// are the console variables contained within the preset.
		const TSharedRef<FDetailArrayBuilder> PropertyBuilder = MakeShareable(new FDetailArrayBuilder(ConsoleVariablePresetsHandle));
		PropertyBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FConsoleVariablesSettingDetailsCustomization::GeneratePresetGroup, &DetailBuilder));
		SettingsCategory.AddCustomBuilder(PropertyBuilder, false);

		// Regenerate the preset details when the preset array is updated
		ConsoleVariablePresetsHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda([&DetailBuilder]()
		{
			DetailBuilder.ForceRefreshDetails();
		}));
	}
	//~ End IDetailCustomization interface

	UMoviePipelineConsoleVariableSetting* GetCVarSettingFromHandle(const TSharedPtr<IPropertyHandle>& PropertyHandle) const
	{
		if (!PropertyHandle.IsValid() || !PropertyHandle->IsValidHandle())
		{
			return nullptr;
		}
		
		TArray<UObject*> OuterObjects;
		PropertyHandle->GetOuterObjects(OuterObjects);

		if (OuterObjects.Num() != 1)
		{
			return nullptr;
		}

		// Get the outermost cvar setting object
		return Cast<UMoviePipelineConsoleVariableSetting>(OuterObjects[0]);
	}

	void GeneratePresetGroup(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout)
	{
		static const FText ConsoleVariableOverrideText = LOCTEXT("CreateConsoleVariableOverride", "Override Console Variable");
		static const FText ConsoleVariableDisabledText = LOCTEXT("DisabledConsoleVariable", "This console variable is disabled.");

		// Add the preset asset chooser as the group header
		FStringView PropName = ElementProperty->GetPropertyPath();
		IDetailGroup& Group = ChildrenBuilder.AddGroup(FName(PropName), FText::FromStringView(PropName));
		Group.HeaderRow()
		[
			ElementProperty->CreatePropertyValueWidget()
		];

		UMoviePipelineConsoleVariableSetting* CVarSetting = GetCVarSettingFromHandle(ElementProperty);
		if (!CVarSetting)
		{
			return;
		}

		const TArray<TScriptInterface<IMovieSceneConsoleVariableTrackInterface>>& CVarPresets = CVarSetting->ConsoleVariablePresets;
		if (!CVarPresets.IsValidIndex(ElementIndex))
		{
			return;
		}

		const TScriptInterface<IMovieSceneConsoleVariableTrackInterface>& CVarPreset = CVarPresets[ElementIndex];
		if (!CVarPreset)
		{
			return;
		}

		const bool bOnlyIncludeChecked = false;
		TArray<TTuple<FString, FString>> CVars;
		CVarPreset->GetConsoleVariablesForTrack(bOnlyIncludeChecked, CVars);

		// Show every console variable that's included in this preset; each console variable gets its own row
		for (const TTuple<FString, FString>& CVar : CVars)
		{
			// Each console variable gets a menu that allows the user to create an override outside of the preset
			FMenuBuilder CreateOverrideMenu(true, nullptr, nullptr, true);
			FUIAction AddOverrideAction(
				FExecuteAction::CreateLambda([CVar, CVarSetting]()
				{
					CVarSetting->AddConsoleVariable(CVar.Key, 0.f);
				})
			);
			CreateOverrideMenu.AddMenuEntry(ConsoleVariableOverrideText, FText::GetEmpty(), FSlateIcon(), AddOverrideAction);
			
			Group.AddWidgetRow()
			.WholeRowContent()
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.Padding(5, 0)
				.FillWidth(0.75f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.IsEnabled(CVarPreset->IsConsoleVariableEnabled(CVar.Key))
					.Font(DetailLayout->GetDetailFont())
					.Text(FText::FromString(CVar.Key))
					.ToolTipText(!CVarPreset->IsConsoleVariableEnabled(CVar.Key) ? ConsoleVariableDisabledText : FText::GetEmpty())
				]
				
				+ SHorizontalBox::Slot()
				.Padding(0, 0)
				.FillWidth(0.25f)
				.VAlign(VAlign_Center)
				[
					SNew(SEditableTextBox)
					.IsEnabled(false)
					.Font(DetailLayout->GetDetailFont())
					.Text(FText::FromString(CVar.Value))
				]
				
				+ SHorizontalBox::Slot()
				.Padding(5, 0)
				.AutoWidth()
				[
					SNew(SComboButton)
					.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
					.ForegroundColor(FSlateColor::UseForeground())
					.HasDownArrow(true)
					.MenuContent()
					[
						CreateOverrideMenu.MakeWidget()
					]
				]
			];
		}
	}
};

#undef LOCTEXT_NAMESPACE