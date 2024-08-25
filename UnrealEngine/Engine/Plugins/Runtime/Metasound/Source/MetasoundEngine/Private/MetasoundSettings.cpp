// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundSettings.h"
#include "Algo/Count.h"

#if WITH_EDITORONLY_DATA

namespace MetaSoundSettingsPrivate
{
	/** Generate new name for the item. **/
	static FName GenerateNextName(const TCHAR* InBaseName)
	{
		const TArray<FName> Names = UMetaSoundQualityHelper::GetQualityList();	
		FString NewName = InBaseName;
		
		// Start adding a postfix starting at 1.
		for (int32 Postfix=1; Names.Contains(*NewName); ++Postfix)
		{
			NewName = FString::Format(TEXT("{0} {1}"), { InBaseName, Postfix });
		}
		return FName(*NewName);
	}
	
	/** When adding a quality new entry in the editor call this **/	
	static void OnCreateNewQualitySettings(FMetaSoundQualitySettings& InNewItem)
	{
		InNewItem.Name = GenerateNextName(TEXT("New Quality"));
		InNewItem.UniqueId = FGuid::NewGuid();
	}

	static void OnDuplicateQualitySettings(FMetaSoundQualitySettings& InNewlyDuplicatedItem)
	{
		InNewlyDuplicatedItem.Name = GenerateNextName(*FString::Printf(TEXT("%s (Duplicate)"), *InNewlyDuplicatedItem.Name.ToString()));
		InNewlyDuplicatedItem.UniqueId = FGuid::NewGuid();
	}

	static void OnPasteQualitySettings(FMetaSoundQualitySettings& InNewlyPastedItem)
	{
		InNewlyPastedItem.Name = GenerateNextName(*FString::Printf(TEXT("%s (Copied)"), *InNewlyPastedItem.Name.ToString()));
		InNewlyPastedItem.UniqueId = FGuid::NewGuid();
	}
	static void OnRenameQualitySettings(FMetaSoundQualitySettings& InRenamed)
	{
		// Prevent 'None' as an option.
		if (InRenamed.Name.IsNone())
		{
			InRenamed.Name = GenerateNextName(TEXT("New Quality"));
		}
		
		// More than one?
		else if (Algo::Count(UMetaSoundQualityHelper::GetQualityList(), InRenamed.Name) > 1)
		{
			// add something unique to the name.
			InRenamed.Name = GenerateNextName(*InRenamed.Name.ToString());
		}	
	}
	
}//MetaSoundSettingsPrivate

void UMetaSoundSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PostEditChangeChainProperty)
{
	const int32 QualityItemIndex = PostEditChangeChainProperty.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(UMetaSoundSettings, QualitySettings));

	// Quality item changed..
	if (QualityItemIndex != INDEX_NONE && QualitySettings.IsValidIndex(QualityItemIndex))
	{
		FMetaSoundQualitySettings& Item = QualitySettings[QualityItemIndex];

		// Name has changed.
		if (PostEditChangeChainProperty.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FMetaSoundQualitySettings, Name))
		{
			MetaSoundSettingsPrivate::OnRenameQualitySettings(Item);
		}
		
		// Array change.
		else if (PostEditChangeChainProperty.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSettings, QualitySettings))
		{
			// Add
			if (PostEditChangeChainProperty.ChangeType == EPropertyChangeType::ArrayAdd)
			{
				MetaSoundSettingsPrivate::OnCreateNewQualitySettings(Item);
			}
			// Duplicate.
			else if (PostEditChangeChainProperty.ChangeType == EPropertyChangeType::Duplicate)
			{
				MetaSoundSettingsPrivate::OnDuplicateQualitySettings(Item);
			}
		}	
	}
	
	// Handle pasting separately as we might not have a valid index in the case of pasting when quality array is empty.
	if (PostEditChangeChainProperty.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMetaSoundSettings, QualitySettings))
	{
		// Paste...
		if (PostEditChangeChainProperty.ChangeType == EPropertyChangeType::ValueSet)
		{
			const int32 IndexOfPastedItem = QualityItemIndex != INDEX_NONE ? QualityItemIndex : 0;
			if (QualitySettings.IsValidIndex(IndexOfPastedItem))
			{
				FMetaSoundQualitySettings& PastedItem = QualitySettings[IndexOfPastedItem];
				MetaSoundSettingsPrivate::OnPasteQualitySettings(PastedItem);
			}
		}
	}

	Super::PostEditChangeChainProperty(PostEditChangeChainProperty);
}

void UMetaSoundSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{	
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	DenyListCacheChangeID++;
}

FName UMetaSoundSettings::GetQualitySettingPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UMetaSoundSettings, QualitySettings);
}

#endif //WITH_EDITORONLY_DATA

TArray<FName> UMetaSoundQualityHelper::GetQualityList()
{
	TArray<FName> Names;

#if WITH_EDITORONLY_DATA
	if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
	{
		Algo::Transform(Settings->GetQualitySettings(), Names, [](const FMetaSoundQualitySettings& Quality) -> FName
		{
			return Quality.Name;
		});
	}
#endif //WITH_EDITORONLY_DATA

	return Names;
}
