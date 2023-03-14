// Copyright Epic Games, Inc. All Rights Reserved.

#include "FilterPresets.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FilterPresets)

#define LOCTEXT_NAMESPACE "FilterPreset"

void USharedFilterPresetContainer::GetSharedUserPresets(TArray<TSharedPtr<IFilterPreset>>& OutPresets)
{
	for (FFilterData& FilterData : SharedPresets)
	{
		OutPresets.Add(MakeShared<FUserFilterPreset>(FilterData.Name, FilterData));
	}
}

void USharedFilterPresetContainer::AddFilterData(const FFilterData& InFilterData)
{
	USharedFilterPresetContainer* SharedPresetsContainer = GetMutableDefault<USharedFilterPresetContainer>();
	SharedPresetsContainer->SharedPresets.Add(InFilterData);
}

bool USharedFilterPresetContainer::RemoveFilterData(const FFilterData& InFilterData)
{
	USharedFilterPresetContainer* SharedPresetsContainer = GetMutableDefault<USharedFilterPresetContainer>();
	return SharedPresetsContainer->SharedPresets.RemoveSingle(InFilterData) == 1;
}

void USharedFilterPresetContainer::Save()
{
	USharedFilterPresetContainer* SharedPresetsContainer = GetMutableDefault<USharedFilterPresetContainer>();
	SharedPresetsContainer->TryUpdateDefaultConfigFile();
}

void UEngineFilterPresetContainer::GetEnginePresets(TArray<TSharedPtr<IFilterPreset>>& OutPresets)
{
	for (FFilterData& FilterData : EnginePresets)
	{
		OutPresets.Add(MakeShared<FEngineFilterPreset>(FilterData.Name, FilterData));
	}
}

void ULocalFilterPresetContainer::GetUserPresets(TArray<TSharedPtr<IFilterPreset>>& OutPresets)
{
	for (FFilterData& FilterData : UserPresets)
	{
		const bool bIsLocal = true;
		OutPresets.Add(MakeShared<FUserFilterPreset>(FilterData.Name, FilterData, bIsLocal));
	}
}

void ULocalFilterPresetContainer::AddFilterData(const FFilterData& InFilterData)
{
	ULocalFilterPresetContainer* LocalPresetsContained = GetMutableDefault<ULocalFilterPresetContainer>();
	LocalPresetsContained->UserPresets.Add(InFilterData);
}

bool ULocalFilterPresetContainer::RemoveFilterData(const FFilterData& InFilterData)
{
	ULocalFilterPresetContainer* LocalPresetsContained = GetMutableDefault<ULocalFilterPresetContainer>();
	return LocalPresetsContained->UserPresets.RemoveSingle(InFilterData) == 1;
}

void ULocalFilterPresetContainer::Save()
{
	ULocalFilterPresetContainer* LocalPresetsContainer = GetMutableDefault<ULocalFilterPresetContainer>();
	LocalPresetsContainer->SaveConfig();
}

void FFilterPresetHelpers::CreateNewPreset(const TArray<TSharedPtr<ITraceObject>>& InObjects)
{
	ULocalFilterPresetContainer* LocalPresetsContainer = GetMutableDefault<ULocalFilterPresetContainer>();
	USharedFilterPresetContainer* SharedPresetsContainer = GetMutableDefault<USharedFilterPresetContainer>();

	FFilterData& NewUserFilter = LocalPresetsContainer->UserPresets.AddDefaulted_GetRef();

	bool bFoundValidName = false;
	int32 ProfileAppendNum = LocalPresetsContainer->UserPresets.Num();
	FString NewFilterName;
	while (!bFoundValidName)
	{
		NewFilterName = FString::Printf(TEXT("UserPreset_%i"), ProfileAppendNum);

		bool bValidName = true;
		for (const FFilterData& Filter : LocalPresetsContainer->UserPresets)
		{
			if (Filter.Name == NewFilterName)
			{
				bValidName = false;
				break;
			}
		}

		for (const FFilterData& Filter : SharedPresetsContainer->SharedPresets)
		{
			if (Filter.Name == NewFilterName)
			{
				bValidName = false;
				break;
			}
		}

		if (!bValidName)
		{
			++ProfileAppendNum;
		}

		bFoundValidName = bValidName;
	}

	NewUserFilter.Name = NewFilterName;

	TArray<FString> Names;
	ExtractEnabledObjectNames(InObjects, Names);
	NewUserFilter.AllowlistedNames = Names;

	ULocalFilterPresetContainer::Save();
}

bool FFilterPresetHelpers::CanModifySharedPreset()
{
	const USharedFilterPresetContainer* SharedContainer = GetDefault<USharedFilterPresetContainer>();
	return !FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*SharedContainer->GetDefaultConfigFilename());
}

void FFilterPresetHelpers::ExtractEnabledObjectNames(const TArray<TSharedPtr<ITraceObject>>& InObjects, TArray<FString>& OutNames)
{
	for (const TSharedPtr<ITraceObject>& Object : InObjects)
	{
		if (!Object->IsFiltered())
		{
			OutNames.Add(Object->GetName());
		}

		TArray<TSharedPtr<ITraceObject>> ChildObjects;
		Object->GetChildren(ChildObjects);
		if(ChildObjects.Num())
		{
			ExtractEnabledObjectNames(ChildObjects, OutNames);
		}
	}
}

bool FUserFilterPreset::CanDelete() const
{
	return true;
}

bool FUserFilterPreset::Delete()
{
	bool bRemoved = false;

	if (IsLocal())
	{
		bRemoved = ULocalFilterPresetContainer::RemoveFilterData(FilterData);
		ULocalFilterPresetContainer::Save();
	}
	else
	{
		bRemoved = USharedFilterPresetContainer::RemoveFilterData(FilterData);
		USharedFilterPresetContainer::Save();
	}

	return ensure(bRemoved);
}

bool FUserFilterPreset::MakeShared()
{
	ensure(IsLocal());

	USharedFilterPresetContainer::AddFilterData(FilterData);
	ensure(ULocalFilterPresetContainer::RemoveFilterData(FilterData));
		
	USharedFilterPresetContainer::Save();
	ULocalFilterPresetContainer::Save();
	
	return true;
}

bool FUserFilterPreset::MakeLocal()
{
	ensure(!IsLocal());

	ULocalFilterPresetContainer::AddFilterData(FilterData);
	ensure(USharedFilterPresetContainer::RemoveFilterData(FilterData));
	
	USharedFilterPresetContainer::Save();
	ULocalFilterPresetContainer::Save();

	return true;
}

bool FUserFilterPreset::IsLocal() const
{
	return bIsLocalPreset;
}

void FUserFilterPreset::Save(const TArray<TSharedPtr<ITraceObject>>& InObjects)
{
	TArray<FString> Names;
	FFilterPresetHelpers::ExtractEnabledObjectNames(InObjects, Names);

	FilterData.AllowlistedNames = Names;
	
	USharedFilterPresetContainer::Save();
	ULocalFilterPresetContainer::Save();
}

void FUserFilterPreset::Save()
{
	USharedFilterPresetContainer::Save();
	ULocalFilterPresetContainer::Save();
}

FString FFilterPreset::GetName() const
{
	return Name;
}

FText FFilterPreset::GetDisplayText() const
{
	return FText::FromString(Name);
}

FText FFilterPreset::GetDescription() const
{
	return FText::FormatOrdered(LOCTEXT("FilterPresetDescriptionFormat", "Name: {0}\nType: {1}"), FText::FromString(Name), CanDelete() ? (IsLocal() ? LOCTEXT("LocalPreset", "Local") : LOCTEXT("SharedPreset", "Shared")) : LOCTEXT("EnginePreset", "Engine"));
}

void FFilterPreset::GetAllowlistedNames(TArray<FString>& OutNames) const
{
	OutNames.Append(FilterData.AllowlistedNames);
}

bool FFilterPreset::CanDelete() const
{
	return false;
}

void FFilterPreset::Rename(const FString& InNewName)
{	
	Name = InNewName;
	FilterData.Name = InNewName;

	Save();
}

bool FFilterPreset::Delete()
{
	return false;
}

bool FFilterPreset::MakeShared()
{
	return false;
}

bool FFilterPreset::MakeLocal()
{
	return false;
}

bool FFilterPreset::IsLocal() const
{
	return false;
}

#undef LOCTEXT_NAMESPACE // "FilterPreset"

