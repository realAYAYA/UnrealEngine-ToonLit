// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnvQueryGenerator_SmartObjects.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvQueryItemType_SmartObject.h"
#include "Engine/Engine.h"


#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_SmartObjects::UEnvQueryGenerator_SmartObjects()
{
	ItemType = UEnvQueryItemType_SmartObject::StaticClass();

	QueryBoxExtent.Set(2000,2000, 500);
	QueryOriginContext = UEnvQueryContext_Querier::StaticClass();
}

void UEnvQueryGenerator_SmartObjects::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (QueryOwner == nullptr)
	{	
		return;
	}
		
	UWorld* World = GEngine->GetWorldFromContextObject(QueryOwner, EGetWorldErrorMode::LogAndReturnNull);
	USmartObjectSubsystem* SmartObjectSubsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(World);

	if (SmartObjectSubsystem == nullptr)
	{
		return;
	}

	TArray<FSmartObjectRequestResult> FoundSlots;
	TArray<FVector> OriginLocations;
	QueryInstance.PrepareContext(QueryOriginContext, OriginLocations);
	int32 NumberOfSuccessfulQueries = 0;

	for (const FVector& Origin : OriginLocations)
	{
		FBox QueryBox(Origin - QueryBoxExtent, Origin + QueryBoxExtent);
		FSmartObjectRequest Request(QueryBox, SmartObjectRequestFilter);

		// @todo note that with this approach, if there's more than one Origin being used for generation we can end up 
		// with duplicates in AllResults
		NumberOfSuccessfulQueries += SmartObjectSubsystem->FindSmartObjects(Request, FoundSlots, Cast<AActor>(QueryOwner)) ? 1 : 0;
	}

	if (NumberOfSuccessfulQueries > 1)
	{
		FoundSlots.Sort([](const FSmartObjectRequestResult& A, const FSmartObjectRequestResult& B)
		{
			return A.SlotHandle < B.SlotHandle;
		});
	}

	TArray<FSmartObjectSlotEQSItem> AllResults;
	AllResults.Reserve(FoundSlots.Num());

	FSmartObjectSlotHandle PreviousSlotHandle;

	for (const FSmartObjectRequestResult& SlotResult : FoundSlots)
	{
		if (NumberOfSuccessfulQueries > 1 && SlotResult.SlotHandle == PreviousSlotHandle)
		{
			// skip duplicates
			continue;
		}
		PreviousSlotHandle = SlotResult.SlotHandle;

		if (bOnlyClaimable == false || SmartObjectSubsystem->CanBeClaimed(SlotResult.SlotHandle))
		{
			const FTransform& SlotTransform = SmartObjectSubsystem->GetSlotTransformChecked(SlotResult.SlotHandle);
			AllResults.Emplace(SlotTransform.GetLocation(), SlotResult.SmartObjectHandle, SlotResult.SlotHandle);
		}
	}

	QueryInstance.AddItemData<UEnvQueryItemType_SmartObject>(AllResults);
}

FText UEnvQueryGenerator_SmartObjects::GetDescriptionTitle() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("DescribeContext"), UEnvQueryTypes::DescribeContext(QueryOriginContext));
	return FText::Format(LOCTEXT("DescriptionGenerateSmartObjects", "Smart Object slots around {DescribeContext}"), Args);
};

FText UEnvQueryGenerator_SmartObjects::GetDescriptionDetails() const
{
	FFormatNamedArguments Args;

	FTextBuilder DescBuilder;

	if (!SmartObjectRequestFilter.UserTags.IsEmpty()
		|| !SmartObjectRequestFilter.ActivityRequirements.IsEmpty()
		|| !SmartObjectRequestFilter.BehaviorDefinitionClasses.IsEmpty()
		|| SmartObjectRequestFilter.Predicate)
	{
		DescBuilder.AppendLine(LOCTEXT("SmartObjectFilterHeader", "Smart Object Filter:"));
		if (!SmartObjectRequestFilter.UserTags.IsEmpty())
		{
			DescBuilder.AppendLineFormat(LOCTEXT("SmartObjectFilterUserTags", "\tUser Tags: {0}"), FText::FromString(SmartObjectRequestFilter.UserTags.ToString()));
		}
		
		if (!SmartObjectRequestFilter.ActivityRequirements.IsEmpty())
		{
			DescBuilder.AppendLineFormat(LOCTEXT("SmartObjectFilterActivityRequirements", "\tActivity Requirements: {0}")
				, FText::FromString(SmartObjectRequestFilter.ActivityRequirements.GetDescription()));
		}

		if (!SmartObjectRequestFilter.BehaviorDefinitionClasses.IsEmpty())
		{
			if (SmartObjectRequestFilter.BehaviorDefinitionClasses.Num() == 1)//SmartObjectRequestFilter.BehaviorDefinitionClass)
			{
				DescBuilder.AppendLineFormat(LOCTEXT("SmartObjectFilterBehaviorDefinitionClass", "\tBehavior Definition Class: {0}")
					, FText::FromString(GetNameSafe(SmartObjectRequestFilter.BehaviorDefinitionClasses[0])));
			}
			else
			{
				DescBuilder.AppendLine(LOCTEXT("SmartObjectFilterBehaviorDefinitionClassed", "\tBehavior Definition Classes:"));
				for (int i = 0; i < SmartObjectRequestFilter.BehaviorDefinitionClasses.Num(); ++i)
				{
					const TSubclassOf<USmartObjectBehaviorDefinition>& BehaviorClass = SmartObjectRequestFilter.BehaviorDefinitionClasses[i];
					DescBuilder.AppendLineFormat(LOCTEXT("SmartObjectFilterBehaviorDefinitionClassesElement", "\t\t[{0}]: {1}")
						, i, FText::FromString(GetNameSafe(SmartObjectRequestFilter.BehaviorDefinitionClasses[i])));
				}
			}
		}
		
		if (SmartObjectRequestFilter.Predicate)
		{
			DescBuilder.AppendLine(LOCTEXT("SmartObjectFilterPredicate", "\tWith Predicate function"));
		}
	}

	Args.Add(TEXT("QUERYEXTENT"), FText::FromString(FString::Printf(TEXT("[%.1f, %.1f, %.1f]"), float(QueryBoxExtent.X), float(QueryBoxExtent.Y), float(QueryBoxExtent.Z))));
	Args.Add(TEXT("CLAIMABLE"), FText::FromString(bOnlyClaimable ? TEXT("Claimable") : TEXT("All")));
	
	DescBuilder.AppendLineFormat(LOCTEXT("SmartObjectGeneratorParamsDescription", "Query Extent: {QUERYEXTENT}\nSlots: {CLAIMABLE}"), Args);

	return DescBuilder.ToText();
}

#undef LOCTEXT_NAMESPACE
