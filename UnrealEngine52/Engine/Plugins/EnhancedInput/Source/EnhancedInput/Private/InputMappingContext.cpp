// Copyright Epic Games, Inc. All Rights Reserved.

#include "InputMappingContext.h"

#include "EnhancedInputLibrary.h"
#include "EnhancedInputModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputMappingContext)

#define LOCTEXT_NAMESPACE "InputMappingContext"

#if WITH_EDITOR
EDataValidationResult UInputMappingContext::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(ValidationErrors), EDataValidationResult::Valid);
	for (FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		Result = CombineDataValidationResults(Result, Mapping.IsDataValid(ValidationErrors));
	}
	return Result;
}
#endif	// WITH_EDITOR

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
