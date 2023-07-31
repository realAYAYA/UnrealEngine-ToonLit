// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputPlatformSettings.h"
#include "InputMappingContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnhancedInputPlatformSettings)

#define LOCTEXT_NAMESPACE "EnhancedInputPlatformSettings"

//////////////////////////////////////////////////////////////////////////
// UEnhancedInputPlatformData

#if WITH_EDITOR
EDataValidationResult UEnhancedInputPlatformData::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = Super::IsDataValid(ValidationErrors);

	for (const TPair<TObjectPtr<const UInputMappingContext>, TObjectPtr<const UInputMappingContext>> Pair : MappingContextRedirects)
	{
		if (!Pair.Key)
		{
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
			
			FFormatNamedArguments Args;
			Args.Add(TEXT("AssetPath"), FText::FromString(GetPathName()));
			const FText NullKeyError = FText::Format(LOCTEXT("NullKeyError", "'{AssetPath}' does not have a valid key in the MappingContextRedirects!"), Args);
			ValidationErrors.Emplace(NullKeyError);
		}
		
		if (!Pair.Value)
		{
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
			
			FFormatNamedArguments Args;
			Args.Add(TEXT("AssetPath"), FText::FromString(GetPathName()));
			const FText NullValueError = FText::Format(LOCTEXT("NullValueError", "'{AssetPath}' does not have a valid value in the MappingContextRedirects!"), Args);
			ValidationErrors.Emplace(NullValueError);
		}
	}

	return Result;
}
#endif	// WITH_EDITOR

const UInputMappingContext* UEnhancedInputPlatformData::GetContextRedirect(UInputMappingContext* InContext) const
{
	if (const TObjectPtr<const UInputMappingContext>* RedirectToIMC = MappingContextRedirects.Find(InContext))
	{
		return RedirectToIMC->Get();
	}
	return InContext;
}

//////////////////////////////////////////////////////////////////////////
// UEnhancedInputPlatformSettings

void UEnhancedInputPlatformSettings::PostLoad()
{
	Super::PostLoad();

	InputDataClasses.Reset();
	LoadInputDataClasses();
}

#if WITH_EDITOR
void UEnhancedInputPlatformSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Reset the cached data classes so that they get re-evaluated the next time we want to load them
	// This ensures that we always have valid cached values during PIE sessions 
	InputDataClasses.Reset();
}
#endif	// WITH_EDITOR

void UEnhancedInputPlatformSettings::GetAllMappingContextRedirects(OUT TMap<TObjectPtr<const UInputMappingContext>, TObjectPtr<const UInputMappingContext>>& OutRedirects)
{
	ForEachInputData([&OutRedirects](const UEnhancedInputPlatformData& Data)
	{
		OutRedirects.Append(Data.GetMappingContextRedirects());
	});
}

void UEnhancedInputPlatformSettings::ForEachInputData(TFunctionRef<void(const UEnhancedInputPlatformData&)> Predicate)
{
	LoadInputDataClasses();
	
	for (TSubclassOf<UEnhancedInputPlatformData> InputDataClass : InputDataClasses)
	{
		if (const UEnhancedInputPlatformData* Data = InputDataClass.GetDefaultObject())
		{
			Predicate(*Data);
		}
	}
}

void UEnhancedInputPlatformSettings::LoadInputDataClasses()
{
	if (InputData.Num() != InputDataClasses.Num())
	{
		for (TSoftClassPtr<UEnhancedInputPlatformData> InputDataPtr : InputData)
		{
			if (TSubclassOf<UEnhancedInputPlatformData> InputDataClass = InputDataPtr.LoadSynchronous())
			{
				InputDataClasses.Add(InputDataClass);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
