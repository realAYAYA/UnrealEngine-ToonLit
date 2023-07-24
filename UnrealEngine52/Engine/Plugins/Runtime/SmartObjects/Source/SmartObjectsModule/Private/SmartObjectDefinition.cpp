// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectDefinition.h"
#include "SmartObjectSettings.h"
#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#include "WorldConditions/WorldCondition_SmartObjectActorTagQuery.h"
#include "WorldConditions/SmartObjectWorldConditionObjectTagQuery.h"
#endif


#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectDefinition)

#define LOCTEXT_NAMESPACE "SmartObjectDefinition"

namespace UE::SmartObject
{
	const FVector DefaultSlotSize(40, 40, 90);
}

USmartObjectDefinition::USmartObjectDefinition(const FObjectInitializer& ObjectInitializer): UDataAsset(ObjectInitializer)
{
	UserTagsFilteringPolicy = GetDefault<USmartObjectSettings>()->DefaultUserTagsFilteringPolicy;
	ActivityTagsMergingPolicy = GetDefault<USmartObjectSettings>()->DefaultActivityTagsMergingPolicy;
	WorldConditionSchemaClass = GetDefault<USmartObjectSettings>()->DefaultWorldConditionSchemaClass;
}

#if WITH_EDITOR

EDataValidationResult USmartObjectDefinition::IsDataValid(TArray<FText>& ValidationErrors)
{
	const EDataValidationResult Result = Super::IsDataValid(ValidationErrors);

	Validate(&ValidationErrors);
	
	return CombineDataValidationResults(Result, bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid);
}

#endif // WITH_EDITOR

bool USmartObjectDefinition::Validate(TArray<FText>* ErrorsToReport) const
{
	bValid = false;
	if (Slots.Num() == 0)
	{
		if (ErrorsToReport)
		{
			ErrorsToReport->Emplace(LOCTEXT("MissingSlotError", "Need to provide at least one slot definition"));
		}
		else
		{
			return false;
		}
	}

	// Detect null entries in default definitions
	int32 NullEntryIndex;
	if (DefaultBehaviorDefinitions.Find(nullptr, NullEntryIndex))
	{
		if (ErrorsToReport)
		{
			ErrorsToReport->Emplace(FText::Format(LOCTEXT("NullDefaultBehaviorEntryError", "Null entry found at index {0} in default behavior definition list"), NullEntryIndex));
		}
		else
		{
			return false;
		}
	}

	// Detect null entries in slot definitions
	for (int i = 0; i < Slots.Num(); ++i)
	{
		const FSmartObjectSlotDefinition& Slot = Slots[i];
		if (Slot.BehaviorDefinitions.Find(nullptr, NullEntryIndex))
		{
			if (ErrorsToReport)
			{
				ErrorsToReport->Emplace(FText::Format(LOCTEXT("NullSlotBehaviorEntryError", "Null entry found at index {0} in default behavior definition list"), NullEntryIndex));
			}
			else
			{
				return false;
			}
		}
	}

	// Detect missing definitions in slots if no default one are provided
	if (DefaultBehaviorDefinitions.Num() == 0)
	{
		for (int i = 0; i < Slots.Num(); ++i)
		{
			const FSmartObjectSlotDefinition& Slot = Slots[i];
			if (Slot.BehaviorDefinitions.Num() == 0)
			{
				if (ErrorsToReport)
				{
					ErrorsToReport->Emplace(FText::Format(LOCTEXT("MissingSlotBehaviorError", "Slot at index {0} needs to provide a behavior definition since there is no default one in the SmartObject definition"), i));
				}
				else
				{
					return false;
				}
			}
		}
	}

	bValid = true;
	return true;
}

FBox USmartObjectDefinition::GetBounds() const
{
	FBox BoundingBox(ForceInitToZero);
	for (const FSmartObjectSlotDefinition& Slot : GetSlots())
	{
		BoundingBox += Slot.Offset + UE::SmartObject::DefaultSlotSize;
		BoundingBox += Slot.Offset - UE::SmartObject::DefaultSlotSize;
	}
	return BoundingBox;
}

void USmartObjectDefinition::GetSlotActivityTags(const FSmartObjectSlotIndex& SlotIndex, FGameplayTagContainer& OutActivityTags) const
{
	if (ensureMsgf(Slots.IsValidIndex(SlotIndex), TEXT("Requesting activity tags for an out of range slot index: %s"), *LexToString(SlotIndex)))
	{
		GetSlotActivityTags(Slots[SlotIndex], OutActivityTags);
	}
}

void USmartObjectDefinition::GetSlotActivityTags(const FSmartObjectSlotDefinition& SlotDefinition, FGameplayTagContainer& OutActivityTags) const
{
	OutActivityTags = ActivityTags;

	if (ActivityTagsMergingPolicy == ESmartObjectTagMergingPolicy::Combine)
	{
		OutActivityTags.AppendTags(SlotDefinition.ActivityTags);
	}
	else if (ActivityTagsMergingPolicy == ESmartObjectTagMergingPolicy::Override && !SlotDefinition.ActivityTags.IsEmpty())
	{
		OutActivityTags = SlotDefinition.ActivityTags;
	}
}

TOptional<FTransform> USmartObjectDefinition::GetSlotTransform(const FTransform& OwnerTransform, const FSmartObjectSlotIndex SlotIndex) const
{
	TOptional<FTransform> Transform;

	if (ensureMsgf(Slots.IsValidIndex(SlotIndex), TEXT("Requesting slot transform for an out of range index: %s"), *LexToString(SlotIndex)))
	{
		const FSmartObjectSlotDefinition& Slot = Slots[SlotIndex];
		Transform = FTransform(Slot.Rotation, Slot.Offset) * OwnerTransform;
	}

	return Transform;
}


const USmartObjectBehaviorDefinition* USmartObjectDefinition::GetBehaviorDefinition(const FSmartObjectSlotIndex& SlotIndex,
																					const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass) const
{
	const USmartObjectBehaviorDefinition* Definition = nullptr;
	if (Slots.IsValidIndex(SlotIndex))
	{
		Definition = GetBehaviorDefinitionByType(Slots[SlotIndex].BehaviorDefinitions, DefinitionClass);
	}

	if (Definition == nullptr)
	{
		Definition = GetBehaviorDefinitionByType(DefaultBehaviorDefinitions, DefinitionClass);
	}

	return Definition;
}


const USmartObjectBehaviorDefinition* USmartObjectDefinition::GetBehaviorDefinitionByType(const TArray<USmartObjectBehaviorDefinition*>& BehaviorDefinitions,
																				 const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass)
{
	USmartObjectBehaviorDefinition* const* BehaviorDefinition = BehaviorDefinitions.FindByPredicate([&DefinitionClass](const USmartObjectBehaviorDefinition* SlotBehaviorDefinition)
		{
			return SlotBehaviorDefinition != nullptr && SlotBehaviorDefinition->GetClass()->IsChildOf(*DefinitionClass);
		});

	return BehaviorDefinition != nullptr ? *BehaviorDefinition : nullptr;
}

#if WITH_EDITOR
int32 USmartObjectDefinition::FindSlotByID(const FGuid ID) const
{
	const int32 Slot = Slots.IndexOfByPredicate([&ID](const FSmartObjectSlotDefinition& Slot) { return Slot.ID == ID; });
	return Slot;
}

void USmartObjectDefinition::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FProperty* Property = PropertyChangedEvent.Property;
	if (Property == nullptr)
	{
		return;
	}
	const FProperty* MemberProperty = nullptr;
	if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode())
	{
		MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
	}
	if (MemberProperty == nullptr)
	{
		return;
	}

	// Ensure unique Slot ID on added or duplicated items.
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd
		|| PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
	{
		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, Slots))
		{
			const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(MemberProperty->GetFName().ToString());
			if (Slots.IsValidIndex(ArrayIndex))
			{
				FSmartObjectSlotDefinition& Slot = Slots[ArrayIndex];
				Slot.ID = FGuid::NewGuid();
				Slot.SelectionPreconditions.SchemaClass = WorldConditionSchemaClass;
			}
		}
	}

	// Anything in the slots changed, update references.
	if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, Slots))
	{
		UpdateSlotReferences();
	}

	// If schema changes, update preconditions too.
	if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, WorldConditionSchemaClass))
	{
		for (FSmartObjectSlotDefinition& Slot : Slots)
		{
			Slot.SelectionPreconditions.SchemaClass = WorldConditionSchemaClass;
			Slot.SelectionPreconditions.Initialize(*this);
		}
	}

	Validate();
}

void USmartObjectDefinition::PreSave(FObjectPreSaveContext SaveContext)
{
	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
		Slot.SelectionPreconditions.Initialize(*this);
	}

	UpdateSlotReferences();
	Super::PreSave(SaveContext);
}

void USmartObjectDefinition::UpdateSlotReferences()
{
	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
		for (FInstancedStruct& Data : Slot.Data)
		{
			if (!Data.IsValid())
			{
				continue;
			}
			const UScriptStruct* ScriptStruct = Data.GetScriptStruct();
			uint8* Memory = Data.GetMutableMemory();
			
			for (TFieldIterator<FProperty> It(ScriptStruct); It; ++It)
			{
				if (const FStructProperty* StructProp = CastField<FStructProperty>(*It))
				{
					if (StructProp->Struct == TBaseStructure<FSmartObjectSlotReference>::Get())
					{
						FSmartObjectSlotReference& Ref = *StructProp->ContainerPtrToValuePtr<FSmartObjectSlotReference>(Memory);
						const int32 Index = FindSlotByID(Ref.GetSlotID());
						Ref.SetIndex(Index);
					}
				}
			}
		}
	}
}

#endif // WITH_EDITOR

void USmartObjectDefinition::PostLoad()
{
	Super::PostLoad();

	// Fill in missing world condition schema for old data.
	if (!WorldConditionSchemaClass)
	{
		WorldConditionSchemaClass = GetDefault<USmartObjectSettings>()->DefaultWorldConditionSchemaClass;
	}

	if (!Preconditions.SchemaClass)
	{
		Preconditions.SchemaClass = WorldConditionSchemaClass;
	}

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!ObjectTagFilter.IsEmpty())
	{
		FWorldCondition_SmartObjectActorTagQuery NewActorTagQueryCondition;
		NewActorTagQueryCondition.TagQuery = ObjectTagFilter;
		Preconditions.EditableConditions.Emplace(0, EWorldConditionOperator::And, FConstStructView::Make(NewActorTagQueryCondition));
		ObjectTagFilter.Clear();
		UE_ASSET_LOG(LogSmartObject, Log, this, TEXT("Deprecated object tag filter has been replaced by a %s precondition to validate tags on the smart object actor."
			" If the intent was to validate against instance runtime tags then the condition should be replaced by %s."),
			*FWorldCondition_SmartObjectActorTagQuery::StaticStruct()->GetName(),
			*FSmartObjectWorldConditionObjectTagQuery::StaticStruct()->GetName());
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif	

	Preconditions.Initialize(*this);
	
	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
#if WITH_EDITOR
		// Fill in missing slot ID for old data.
		if (!Slot.ID.IsValid())
		{
			Slot.ID = FGuid::NewGuid();
		}
#endif
		// Fill in missing world condition schema for old data.
		if (!Slot.SelectionPreconditions.SchemaClass)
		{
			Slot.SelectionPreconditions.SchemaClass = WorldConditionSchemaClass;
		}

		Slot.SelectionPreconditions.Initialize(*this);
	}
	
#if WITH_EDITOR
	UpdateSlotReferences();

	Validate();
#endif	
}

#undef LOCTEXT_NAMESPACE
