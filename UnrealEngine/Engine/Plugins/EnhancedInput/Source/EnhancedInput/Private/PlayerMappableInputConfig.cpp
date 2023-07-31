// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerMappableInputConfig.h"
#include "InputMappingContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerMappableInputConfig)

#define LOCTEXT_NAMESPACE "PlayerMappableInputConfig"

UPlayerMappableInputConfig::UPlayerMappableInputConfig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ResetToDefault();
}

void UPlayerMappableInputConfig::ResetToDefault()
{
}

#if WITH_EDITOR

EDataValidationResult UPlayerMappableInputConfig::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = Super::IsDataValid(ValidationErrors);

	if(ConfigName == NAME_None)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("AssetPath"), FText::FromString(GetPathName()));
		const FText NoNameError = FText::Format(LOCTEXT("NoNameError", "'{AssetPath}' does not have a valid ConfigName!"), Args);
		ValidationErrors.Emplace(NoNameError);
		
		Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
	}
	
	// Check that every context is valid and all player mappable mappings have names
	for (const TPair<TObjectPtr<UInputMappingContext>, int32>& ContextPair : Contexts)
	{
		if(const UInputMappingContext* IMC = ContextPair.Key.Get())
		{
			for(const FEnhancedActionKeyMapping& Mapping : IMC->GetMappings())
			{
				if(Mapping.bIsPlayerMappable && Mapping.PlayerMappableOptions.Name == NAME_None)
				{
					Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
					FFormatNamedArguments Args;
					Args.Add(TEXT("AssetPath"), FText::FromString(GetPathName()));
					Args.Add(TEXT("ActionName"), FText::FromString(Mapping.Action->GetPathName()));

					const FText NoMappingError = FText::Format(LOCTEXT("NoNamePlayerMappingError", "'{AssetPath}' has a player mappable key that has no name!"), Args);
					ValidationErrors.Emplace(NoMappingError);
				}
			}
		}
		else
		{
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
			
			FFormatNamedArguments Args;
			Args.Add(TEXT("AssetPath"), FText::FromString(GetPathName()));

			const FText NullContextError = FText::Format(LOCTEXT("NullContextError", "There is a null mapping context in '{AssetPath}'"), Args);
			ValidationErrors.Emplace(NullContextError);
		}
	}
	
	return CombineDataValidationResults(Result, EDataValidationResult::Valid);
}

#endif

void UPlayerMappableInputConfig::ForEachDefaultPlayerMappableKey(TFunctionRef<void(const FEnhancedActionKeyMapping&)> Operation) const
{
	for (const TPair<TObjectPtr<UInputMappingContext>, int32>& ContextPair: Contexts)
	{
		if(const UInputMappingContext* IMC = ContextPair.Key.Get())
		{
			for(const FEnhancedActionKeyMapping& Mapping : IMC->GetMappings())
			{
				if(Mapping.bIsPlayerMappable)
				{
					Operation(Mapping);
				}
			}
		}
	}
}

TArray<FEnhancedActionKeyMapping> UPlayerMappableInputConfig::GetPlayerMappableKeys() const
{
	TArray<FEnhancedActionKeyMapping> OutMappings;
	auto GatherMappings = [&OutMappings](const FEnhancedActionKeyMapping& Options)
	{
		OutMappings.Add(Options);
	};
	
	ForEachDefaultPlayerMappableKey(GatherMappings);

	return OutMappings;
}

FEnhancedActionKeyMapping UPlayerMappableInputConfig::GetMappingByName(const FName MappingName) const
{
	FEnhancedActionKeyMapping OutMapping;
	
	TArray<FEnhancedActionKeyMapping> MappableKeys = GetPlayerMappableKeys();
	FEnhancedActionKeyMapping* ExistingMapping = MappableKeys.FindByPredicate([MappingName](const FEnhancedActionKeyMapping& Mapping) { return Mapping.PlayerMappableOptions.Name == MappingName; });

	if(ExistingMapping)
	{
		OutMapping = *ExistingMapping;
	}
	return OutMapping;
}

TArray<FEnhancedActionKeyMapping> UPlayerMappableInputConfig::GetKeysBoundToAction(const UInputAction* InAction) const
{
	TArray<FEnhancedActionKeyMapping> OutMappings;
	auto GatherMappings = [&OutMappings, InAction](const FEnhancedActionKeyMapping& Options)
	{
		if(Options.Action == InAction)
		{
			OutMappings.Add(Options);
		}
	};
	
	ForEachDefaultPlayerMappableKey(GatherMappings);
	
	return OutMappings;
}

#undef LOCTEXT_NAMESPACE

