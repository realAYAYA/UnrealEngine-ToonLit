// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamInputProfileCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "VCamInputSettings.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "VCamInputProfileCustomization"

FVCamInputProfileCustomization::FVCamInputProfileCustomization()
{
	BuildProfileComboList();
}

TSharedRef<IPropertyTypeCustomization> FVCamInputProfileCustomization::MakeInstance()
{
	return MakeShared<FVCamInputProfileCustomization>();
}

void FVCamInputProfileCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.
		NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		];

	// We only want the dropdown list outside of the settings class as the settings class is the thing
	// defining the presets we use for the dropdown
	const bool bInSettingsClass = PropertyHandle->GetOuterBaseClass() == UVCamInputSettings::StaticClass();

	if (!bInSettingsClass)
	{
		HeaderRow.
			ValueContent()
			.MaxDesiredWidth(0.f)
			[
				SAssignNew(ProfileComboBox, SComboBox< TSharedPtr<FString> >)
				.OptionsSource(&ProfileComboList)
				.OnGenerateWidget(this, &FVCamInputProfileCustomization::MakeProfileComboWidget)
				.OnSelectionChanged(this, &FVCamInputProfileCustomization::OnProfileChanged)
				.OnComboBoxOpening(this, &FVCamInputProfileCustomization::BuildProfileComboList)
				.IsEnabled(this, &FVCamInputProfileCustomization::IsProfileEnabled)
				.ContentPadding(2)
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &FVCamInputProfileCustomization::GetProfileComboBoxContent)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ToolTipText(this, &FVCamInputProfileCustomization::GetProfileComboBoxContent)
				]
			];
	}
}

void FVCamInputProfileCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	CachedStructPropertyHandle = StructPropertyHandle;
	
	// Retrieve structure's child properties
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren( NumChildren );	
	const FName MappableKeyOverridesPropertyName = GET_MEMBER_NAME_CHECKED(FVCamInputProfile, MappableKeyOverrides);
	for(uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> ChildProperty = StructPropertyHandle->GetChildHandle(ChildIndex);
		ChildBuilder.AddProperty(ChildProperty.ToSharedRef());

		if (ChildProperty->GetProperty() && ChildProperty->GetProperty()->GetFName() == MappableKeyOverridesPropertyName)
		{
			MappableKeyOverridesHandle = ChildProperty;
		}
	}
}

void FVCamInputProfileCustomization::BuildProfileComboList()
{
	const UVCamInputSettings* VCamInputSettings = GetDefault<UVCamInputSettings>();

	if (!VCamInputSettings)
	{
		return;
	}
	
	const TArray<FName>& Presets = VCamInputSettings->GetInputProfileNames();
	
	ProfileComboList.Empty(Presets.Num() + 1);
	ProfileComboList.Add(MakeShared<FString>(TEXT("Custom...")));

	// put all presets in the list
	for (const FName& Preset : Presets)
	{
		ProfileComboList.Add(MakeShared<FString>(Preset.ToString()));
	}
}

bool FVCamInputProfileCustomization::IsProfileEnabled() const
{
	if (MappableKeyOverridesHandle.IsValid())
	{
		return MappableKeyOverridesHandle->IsEditable() && FSlateApplication::Get().GetNormalExecutionAttribute().Get();
	}
	return false;
}

void FVCamInputProfileCustomization::OnProfileChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	const UVCamInputSettings* VCamInputSettings = GetDefault<UVCamInputSettings>();

	if (!VCamInputSettings)
	{
		return;
	}
	
	// Ignore any changes that come from code
	if (SelectInfo != ESelectInfo::Direct && CachedStructPropertyHandle.IsValid() && MappableKeyOverridesHandle.IsValid())
	{
		const FString NewPresetName = *NewSelection.Get();

		if (const FVCamInputProfile* InputProfile = VCamInputSettings->InputProfiles.Find(FName(NewPresetName)))
		{
			const FScopedTransaction Transaction(LOCTEXT("ChangeVCamInputProfile", "Change VCam Input Profile"));
			
			FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(CachedStructPropertyHandle->GetProperty());
			FMapProperty* MapProperty = CastFieldChecked<FMapProperty>(MappableKeyOverridesHandle->GetProperty());

			TArray<UObject*> OuterObjects;
			MappableKeyOverridesHandle->GetOuterObjects(OuterObjects);

			CachedStructPropertyHandle->NotifyPreChange();
			
			for (UObject* OuterObject : OuterObjects)
			{				
				void* StructValuePtr = StructProperty->ContainerPtrToValuePtr<void*>(OuterObject);
				void* MapValuePtr = MapProperty->ContainerPtrToValuePtr<void*>(StructValuePtr);
				
				FScriptMapHelper MapHelper(MapProperty, MapValuePtr);

				MapHelper.EmptyValues();

				for (const TPair<FName, FKey>& MappingPair : InputProfile->MappableKeyOverrides)
				{
					MapHelper.AddPair(&MappingPair.Key, &MappingPair.Value);
				}
			}

			CachedStructPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			CachedStructPropertyHandle->NotifyFinishedChangingProperties();
		}
	}
}

TSharedRef<SWidget> FVCamInputProfileCustomization::MakeProfileComboWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*InItem))
		.Font(IDetailLayoutBuilder::GetDetailFont());
}

FText FVCamInputProfileCustomization::GetProfileComboBoxContent() const
{
	// just test one variable for multiple selection
	void* StructValueData;
	if (CachedStructPropertyHandle->GetValueData(StructValueData) == FPropertyAccess::Result::MultipleValues)
	{
		// multiple selection
		return LOCTEXT("MultipleValues", "Multiple Values");
	}
	const TSharedPtr<FString> ProfileString = GetProfileString(StructValueData);
	return ProfileString.IsValid() ? FText::AsCultureInvariant(*ProfileString) : LOCTEXT("NoProfileFound", "Error");
}

TSharedPtr<FString> FVCamInputProfileCustomization::GetProfileString(void* StructValueData) const
{
	FVCamInputProfile* InputProfilePtr = static_cast<FVCamInputProfile*>(StructValueData);
	const UVCamInputSettings* VCamInputSettings = GetDefault<UVCamInputSettings>();

	if (InputProfilePtr && VCamInputSettings)
	{
		for (const TPair<FName, FVCamInputProfile>& InputProfilePair : VCamInputSettings->InputProfiles)
		{
			const FName& InputProfileName = InputProfilePair.Key;
			const FVCamInputProfile& InputProfileValue = InputProfilePair.Value;
			if (InputProfileValue == *InputProfilePtr)
			{
				const TSharedPtr<FString>* FoundString = ProfileComboList.FindByPredicate([InputProfileString = InputProfileName.ToString()](const TSharedPtr<FString> ListEntry)
				{
					return ListEntry.IsValid() ? *ListEntry == InputProfileString : false;
				});
				
				if (FoundString)
				{
					return *FoundString;
				}
				break;
			}
		}
	}
	return ProfileComboList[0];
}

#undef LOCTEXT_NAMESPACE