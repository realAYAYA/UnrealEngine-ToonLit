// Copyright Epic Games, Inc. All Rights Reserved.

#include "InputMappingContext.h"

#include "EnhancedInputLibrary.h"
#include "EnhancedInputModule.h"
#include "PlayerMappableKeySettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputMappingContext)

#define LOCTEXT_NAMESPACE "InputMappingContext"

namespace UE::EnhancedInput
{
	static const FName PlayerMappableOptionsHaveBeenUpgradedname = TEXT("__EnhancedInput_Internal_HasBeenUpgraded__");
}

#if WITH_EDITOR
EDataValidationResult UInputMappingContext::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		Result = CombineDataValidationResults(Result, Mapping.IsDataValid(Context));
	}
	return Result;
}
#endif	// WITH_EDITOR

void UInputMappingContext::PostLoad()
{
	Super::PostLoad();
	
#if WITH_EDITORONLY_DATA
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	
	for (FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		// If this was player mappable, then set the behavior to override the action and populate it with the same data
		if (Mapping.bIsPlayerMappable && Mapping.PlayerMappableOptions.Name != UE::EnhancedInput::PlayerMappableOptionsHaveBeenUpgradedname)
		{
			Mapping.SettingBehavior = EPlayerMappableKeySettingBehaviors::OverrideSettings;

			// If something was player mappable before then it probably didnt have a default object here
			if (!Mapping.PlayerMappableKeySettings)
			{
				FString NewObjectName;
				EObjectFlags MaskedOuterFlags = RF_Public | RF_Transactional;
				Mapping.PlayerMappableKeySettings = NewObject<UPlayerMappableKeySettings>(this, UPlayerMappableKeySettings::StaticClass(), *NewObjectName, MaskedOuterFlags, nullptr);
			}

			check(Mapping.PlayerMappableKeySettings);

			Mapping.PlayerMappableKeySettings->Metadata = Mapping.PlayerMappableOptions.Metadata;

			// If the name was set already, use that
			if (Mapping.PlayerMappableKeySettings->Name.IsValid())
			{
				Mapping.PlayerMappableKeySettings->Name = Mapping.PlayerMappableOptions.Name;
			}
			// Otherwise let's generate something so that the data is valid
			else
			{
				FName UniqueName = MakeUniqueObjectName(this, UInputMappingContext::StaticClass(), Mapping.Action.GetFName());
				Mapping.PlayerMappableKeySettings->Name = UniqueName;
			}

			Mapping.PlayerMappableKeySettings->DisplayName = Mapping.PlayerMappableOptions.DisplayName;
			Mapping.PlayerMappableKeySettings->DisplayCategory = Mapping.PlayerMappableOptions.DisplayCategory;

			// Set the name of the old player mappable options so that we know it has already been upgraded, and we don't do it again
			// on the next post load.
			Mapping.PlayerMappableOptions.Name = UE::EnhancedInput::PlayerMappableOptionsHaveBeenUpgradedname;
		}		
	}
	
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif	// WITH_EDITORONLY_DATA
}

FEnhancedActionKeyMapping& UInputMappingContext::MapKey(const UInputAction* Action, FKey ToKey)
{
	IEnhancedInputModule::Get().GetLibrary()->RequestRebuildControlMappingsUsingContext(this);
	return Mappings.Add_GetRef(FEnhancedActionKeyMapping(Action, ToKey));
}

void UInputMappingContext::UnmapKey(const UInputAction* Action, FKey Key)
{
	int32 MappingIdx = Mappings.IndexOfByPredicate([&Action, &Key](const FEnhancedActionKeyMapping& Other) { return Other.Action == Action && Other.Key == Key; });
	if (MappingIdx != INDEX_NONE)
	{
		Mappings.RemoveAtSwap(MappingIdx);	// TODO: Preserve order?
		IEnhancedInputModule::Get().GetLibrary()->RequestRebuildControlMappingsUsingContext(this);
	}
}

void UInputMappingContext::UnmapAllKeysFromAction(const UInputAction* Action)
{
	int32 Found = Mappings.RemoveAllSwap([&Action](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == Action; });
	if (Found > 0)
	{
		IEnhancedInputModule::Get().GetLibrary()->RequestRebuildControlMappingsUsingContext(this);
	}
}

void UInputMappingContext::UnmapAll()
{
	if (Mappings.Num())
	{
		Mappings.Empty();
		IEnhancedInputModule::Get().GetLibrary()->RequestRebuildControlMappingsUsingContext(this);
	}
}

#undef LOCTEXT_NAMESPACE
