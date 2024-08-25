// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectDefinition.h"
#include "SmartObjectSettings.h"
#include "Misc/EnumerateRange.h"
#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#include "WorldConditions/WorldCondition_SmartObjectActorTagQuery.h"
#include "WorldConditions/SmartObjectWorldConditionObjectTagQuery.h"
#include "SmartObjectUserComponent.h"
#include "Engine/SCS_Node.h"
#include "Misc/DataValidation.h"
#include "SmartObjectPropertyHelpers.h"
#include "Interfaces/ITargetPlatform.h"
#endif

#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectDefinition)

#define LOCTEXT_NAMESPACE "SmartObjectDefinition"

namespace UE::SmartObject
{
	const FVector DefaultSlotSize(40, 40, 90);

	namespace Delegates
	{
#if WITH_EDITOR
		FOnParametersChanged OnParametersChanged;
#endif	
	} // Delegates

} // UE::SmartObject


const FSmartObjectDefinitionDataHandle FSmartObjectDefinitionDataHandle::Invalid(INDEX_NONE);
const FSmartObjectDefinitionDataHandle FSmartObjectDefinitionDataHandle::Root(RootIndex);
const FSmartObjectDefinitionDataHandle FSmartObjectDefinitionDataHandle::Parameters(ParametersIndex);


USmartObjectDefinition::USmartObjectDefinition(const FObjectInitializer& ObjectInitializer): UDataAsset(ObjectInitializer)
{
	UserTagsFilteringPolicy = GetDefault<USmartObjectSettings>()->DefaultUserTagsFilteringPolicy;
	ActivityTagsMergingPolicy = GetDefault<USmartObjectSettings>()->DefaultActivityTagsMergingPolicy;
	WorldConditionSchemaClass = GetDefault<USmartObjectSettings>()->DefaultWorldConditionSchemaClass;
}

#if WITH_EDITOR

EDataValidationResult USmartObjectDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> ValidationErrors;
	Validate(&ValidationErrors);

	for (const FText& Error : ValidationErrors)
	{
		Context.AddError(Error);
	}
	
	return CombineDataValidationResults(Result, bValid.GetValue() ? EDataValidationResult::Valid : EDataValidationResult::Invalid);
}

TSubclassOf<USmartObjectSlotValidationFilter> USmartObjectDefinition::GetPreviewValidationFilterClass() const
{
	if (PreviewData.UserActorClass.IsValid())
	{
		if (const UClass* UserActorClass = PreviewData.UserActorClass.Get())
		{
			// Try to get smart object user component added in the BP.
			if (const UBlueprintGeneratedClass* UserBlueprintClass = Cast<UBlueprintGeneratedClass>(UserActorClass))
			{
				const TArray<USCS_Node*>& Nodes = UserBlueprintClass->SimpleConstructionScript->GetAllNodes();
				for (USCS_Node* Node : Nodes)
				{
					UActorComponent* Component = Node->GetActualComponentTemplate(const_cast<UBlueprintGeneratedClass*>(UserBlueprintClass));
					if (const USmartObjectUserComponent* UserComponent = Cast<USmartObjectUserComponent>(Component))
					{
						return UserComponent->GetValidationFilter();
					}
				}
			}
			
			// Try to get the component from the CDO (e.g. added as default object in C++).
			if (const AActor* UserActor = Cast<AActor>(UserActorClass->GetDefaultObject()))
			{
				if (const USmartObjectUserComponent* UserComponent = UserActor->GetComponentByClass<USmartObjectUserComponent>())
				{
					return UserComponent->GetValidationFilter();
				}
			}
		}
		return nullptr;
	}

	if (PreviewData.UserValidationFilterClass.IsValid())
	{
		return PreviewData.UserValidationFilterClass.Get();
	}
	
	return nullptr;
}

#endif // WITH_EDITOR

bool USmartObjectDefinition::Validate(TArray<FText>* ErrorsToReport) const
{
	bValid = false;

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

	bValid = ErrorsToReport == nullptr || ErrorsToReport->IsEmpty();
	return bValid.GetValue();
}

FBox USmartObjectDefinition::GetBounds() const
{
	FBox BoundingBox(ForceInitToZero);
	for (const FSmartObjectSlotDefinition& Slot : GetSlots())
	{
		BoundingBox += FVector(Slot.Offset) + UE::SmartObject::DefaultSlotSize;
		BoundingBox += FVector(Slot.Offset) - UE::SmartObject::DefaultSlotSize;
	}
	return BoundingBox;
}

void USmartObjectDefinition::GetSlotActivityTags(const int32 SlotIndex, FGameplayTagContainer& OutActivityTags) const
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

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TOptional<FTransform> USmartObjectDefinition::GetSlotTransform(const FTransform& OwnerTransform, const FSmartObjectSlotIndex SlotIndex) const
{
	TOptional<FTransform> Transform;

	if (ensureMsgf(Slots.IsValidIndex(SlotIndex), TEXT("Requesting slot transform for an out of range index: %s"), *LexToString(SlotIndex)))
	{
		const FSmartObjectSlotDefinition& Slot = Slots[SlotIndex];
		Transform = FTransform(FRotator(Slot.Rotation), FVector(Slot.Offset)) * OwnerTransform;
	}

	return Transform;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FTransform USmartObjectDefinition::GetSlotWorldTransform(const int32 SlotIndex, const FTransform& OwnerTransform) const
{
	if (ensureMsgf(Slots.IsValidIndex(SlotIndex), TEXT("Requesting slot transform for an out of range index: %s"), *LexToString(SlotIndex)))
	{
		const FSmartObjectSlotDefinition& Slot = Slots[SlotIndex];
		return FTransform(FRotator(Slot.Rotation), FVector(Slot.Offset)) * OwnerTransform;
	}
	return OwnerTransform;
}

const USmartObjectBehaviorDefinition* USmartObjectDefinition::GetBehaviorDefinition(const int32 SlotIndex,
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

bool USmartObjectDefinition::FindSlotAndDefinitionDataIndexByID(const FGuid ID, int32& OutSlotIndex, int32& OutDefinitionDataIndex) const
{
	OutSlotIndex = INDEX_NONE;
	OutDefinitionDataIndex = INDEX_NONE;
	
	// First try to find direct match on a slot.
	for (TConstEnumerateRef<const FSmartObjectSlotDefinition> SlotDefinition : EnumerateRange(Slots))
	{
		if (SlotDefinition->ID == ID)
		{
			OutSlotIndex = SlotDefinition.GetIndex();
			return true;
		}

		// Next try to find slot index based on definition data.
		const int32 DefinitionDataIndex = SlotDefinition->DefinitionData.IndexOfByPredicate([&ID](const FSmartObjectDefinitionDataProxy& DataProxy)
		{
			return DataProxy.ID == ID;
		});
		if (DefinitionDataIndex != INDEX_NONE)
		{
			OutSlotIndex = SlotDefinition.GetIndex();
			OutDefinitionDataIndex = DefinitionDataIndex;
			return true;
		}
	}

	return false;
}

void USmartObjectDefinition::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FSmartObjectEditPropertyPath ChangePropertyPath(PropertyChangedEvent);

	static const FSmartObjectEditPropertyPath ParametersPath(USmartObjectDefinition::StaticClass(), TEXT("Parameters"));
	static const FSmartObjectEditPropertyPath SlotsPath(USmartObjectDefinition::StaticClass(), TEXT("Slots"));
	static const FSmartObjectEditPropertyPath WorldConditionSchemaClassPath(USmartObjectDefinition::StaticClass(), TEXT("WorldConditionSchemaClass"));
	static const FSmartObjectEditPropertyPath SlotsDefinitionDataPath(USmartObjectDefinition::StaticClass(), TEXT("Slots.DefinitionData"));

	// Ensure unique Slot ID on added or duplicated items.
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd
		|| PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
	{
		if (ChangePropertyPath.IsPathExact(SlotsPath))
		{
			const int32 SlotIndex = ChangePropertyPath.GetPropertyArrayIndex(SlotsPath);
			if (Slots.IsValidIndex(SlotIndex))
			{
				FSmartObjectSlotDefinition& SlotDefinition = Slots[SlotIndex];
				SlotDefinition.ID = FGuid::NewGuid();
				SlotDefinition.SelectionPreconditions.SetSchemaClass(WorldConditionSchemaClass);
				
				// Set new IDs to all duplicated data too
				for (FSmartObjectDefinitionDataProxy& DataProxy : SlotDefinition.DefinitionData)
				{
					DataProxy.ID = FGuid::NewGuid();
				}
			}
		}

		if (ChangePropertyPath.IsPathExact(SlotsDefinitionDataPath))
		{
			const int32 SlotIndex = ChangePropertyPath.GetPropertyArrayIndex(SlotsPath);
			if (Slots.IsValidIndex(SlotIndex))
			{
				FSmartObjectSlotDefinition& SlotDefinition = Slots[SlotIndex];
				const int32 DataIndex = ChangePropertyPath.GetPropertyArrayIndex(SlotsDefinitionDataPath);
				if (SlotDefinition.DefinitionData.IsValidIndex(DataIndex))
				{
					FSmartObjectDefinitionDataProxy& DataProxy = SlotDefinition.DefinitionData[DataIndex];
					DataProxy.ID = FGuid::NewGuid();
				}
			}
		}
	}

	// Anything in the parameters change, notify.
	if (ChangePropertyPath.ContainsPath(ParametersPath))
	{
		UpdateBindingPaths();
		UE::SmartObject::Delegates::OnParametersChanged.Broadcast(*this);
	}

	// Anything in the slots changed, update references.
	if (ChangePropertyPath.ContainsPath(SlotsPath))
	{
		UpdateSlotReferences();
	}

	// If schema changes, update preconditions too.
	if (ChangePropertyPath.IsPathExact(WorldConditionSchemaClassPath))
	{
		for (FSmartObjectSlotDefinition& Slot : Slots)
		{
			Slot.SelectionPreconditions.SetSchemaClass(WorldConditionSchemaClass);
			Slot.SelectionPreconditions.Initialize(this);
		}
	}

#if WITH_EDITOR
	UpdateBindingDataHandles();
#endif	
	
	Validate();
}

void USmartObjectDefinition::PreSave(FObjectPreSaveContext SaveContext)
{
	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
		Slot.SelectionPreconditions.Initialize(this);
	}

	UpdateSlotReferences();
	Super::PreSave(SaveContext);

#if WITH_EDITOR

	UpdateBindingDataHandles();

	if (SaveContext.IsCooking()
		&& SaveContext.GetTargetPlatform()->IsClientOnly()
		&& GetDefault<USmartObjectSettings>()->bShouldExcludePreConditionsOnDedicatedClient
		&& !HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		FObjectSaveOverride ObjSaveOverride;

		// Add path to the conditions within the main definition
		FProperty* OverrideProperty = FindFProperty<FProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, Preconditions));
		check(OverrideProperty);
		FPropertySaveOverride PropOverride;
		PropOverride.PropertyPath = FFieldPath(OverrideProperty);
		PropOverride.bMarkTransient = true;
		
		ObjSaveOverride.PropOverrides.Add(PropOverride);

		// Add path to the conditions within the slot definition struct
		OverrideProperty = FindFProperty<FProperty>(FSmartObjectSlotDefinition::StaticStruct(), GET_MEMBER_NAME_CHECKED(FSmartObjectSlotDefinition, SelectionPreconditions));
		check(OverrideProperty);
		PropOverride.PropertyPath = FFieldPath(OverrideProperty);
		ObjSaveOverride.PropOverrides.Add(PropOverride);

		SaveContext.AddSaveOverride(this, ObjSaveOverride);
	}

#endif // WITH_EDITOR
	
}

void USmartObjectDefinition::UpdateSlotReferences()
{
	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
		for (FSmartObjectDefinitionDataProxy& DataProxy : Slot.DefinitionData)
		{
			if (!DataProxy.Data.IsValid())
			{
				continue;
			}
			const UScriptStruct* ScriptStruct = DataProxy.Data.GetScriptStruct();
			uint8* Memory = DataProxy.Data.GetMutableMemory();
			
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

void USmartObjectDefinition::UpdateBindingPaths()
{
	for (auto It = PropertyBindings.CreateIterator(); It; ++It)
	{
		if (!UpdateAndValidatePath(It->TargetPath)
			|| !UpdateAndValidatePath(It->SourcePath))
		{
			It.RemoveCurrentSwap();
		}
	}
	
	ApplyParameters();
}

bool USmartObjectDefinition::UpdateAndValidatePath(FPropertyBindingPath& Path)
{
	FPropertyBindingDataView DataView;
	if (!GetDataViewByID(Path.GetStructID(), DataView))
	{
		return false;
	}
	if (!Path.UpdateSegmentsFromValue(DataView))
	{
		return false;
	}
	return true;
}


#endif // WITH_EDITOR

void USmartObjectDefinition::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	EnsureValidGuids();
#endif	
}

void USmartObjectDefinition::PostLoad()
{
	Super::PostLoad();

	// Fill in missing world condition schema for old data.
	if (!WorldConditionSchemaClass)
	{
		WorldConditionSchemaClass = GetDefault<USmartObjectSettings>()->DefaultWorldConditionSchemaClass;
	}

	if (Preconditions.GetSchemaClass().Get() == nullptr)
	{
		Preconditions.SetSchemaClass(WorldConditionSchemaClass);
	}

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!ObjectTagFilter.IsEmpty())
	{
		FWorldCondition_SmartObjectActorTagQuery NewActorTagQueryCondition;
		NewActorTagQueryCondition.TagQuery = ObjectTagFilter;
		Preconditions.AddCondition(FWorldConditionEditable(0, EWorldConditionOperator::And, FConstStructView::Make(NewActorTagQueryCondition)));
		ObjectTagFilter.Clear();
		UE_ASSET_LOG(LogSmartObject, Log, this, TEXT("Deprecated object tag filter has been replaced by a %s precondition to validate tags on the smart object actor."
			" If the intent was to validate against instance runtime tags then the condition should be replaced by %s."),
			*FWorldCondition_SmartObjectActorTagQuery::StaticStruct()->GetName(),
			*FSmartObjectWorldConditionObjectTagQuery::StaticStruct()->GetName());
	}

	if (PreviewClass_DEPRECATED.IsValid())
	{
		PreviewData.ObjectActorClass = PreviewClass_DEPRECATED;
		PreviewClass_DEPRECATED.Reset();
	}
	if (PreviewMeshPath_DEPRECATED.IsValid())
	{
		PreviewData.ObjectMeshPath = PreviewMeshPath_DEPRECATED;
		PreviewMeshPath_DEPRECATED.Reset();
	}

	for (TEnumerateRef<FSmartObjectSlotDefinition> Slot : EnumerateRange(Slots))
	{
		if (Slot->Data_DEPRECATED.Num() > 0)
		{
			Slot->DefinitionData.Reserve(Slot->Data_DEPRECATED.Num());

			for (TEnumerateRef<const FInstancedStruct> Data : EnumerateRange(Slot->Data_DEPRECATED))
			{
				FSmartObjectDefinitionDataProxy& DataProxy = Slot->DefinitionData.AddDefaulted_GetRef();
				DataProxy.Data.InitializeAsScriptStruct(Data->GetScriptStruct(), Data->GetMemory());

				static FName DataProxyName(TEXT("DataProxy"));
				const uint32 Hashes[] = {
					GetTypeHash(DataProxyName),
					GetTypeHash(Slot.GetIndex()),
					GetTypeHash(Data.GetIndex())
				}; 
				const uint64 Hash = CityHash64((const char*)Hashes, sizeof Hashes);
				DataProxy.ID = FGuid::NewDeterministicGuid(GetPathName(), Hash);
			}
			Slot->Data_DEPRECATED.Reset();
		}
	}	
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	EnsureValidGuids();
	UpdateBindingDataHandles();
#endif	

	Preconditions.Initialize(this);
	
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
		if (Slot.SelectionPreconditions.GetSchemaClass().Get() == nullptr)
		{
			Slot.SelectionPreconditions.SetSchemaClass(WorldConditionSchemaClass);
		}

		Slot.SelectionPreconditions.Initialize(this);
	}
	
#if WITH_EDITOR
	UpdateSlotReferences();
	UpdateBindingPaths();

	Validate();
#endif	
}

USmartObjectDefinition* USmartObjectDefinition::GetAssetVariation(const FInstancedPropertyBag& VariationParameters)
{
	// If no parameters, return this asset.
	if (!VariationParameters.IsValid())
	{
		return this;
	}

	// Remove unused variations
	for (auto It = Variations.CreateIterator(); It; ++It)
	{
		if (!It->DefinitionAsset.IsValid())
		{
			It.RemoveCurrentSwap();
		}
	}
	
	// Expect correct bag if provided.
	UPropertyBag* VariationParametersBag = const_cast<UPropertyBag*>(VariationParameters.GetPropertyBagStruct());
	if (!VariationParametersBag || VariationParametersBag != Parameters.GetPropertyBagStruct())
	{
		UE_LOG(LogSmartObject, Error, TEXT("%hs %s: Expecting matching variation parameters."), __FUNCTION__, *GetFullNameSafe(this));
		return nullptr;
	}

	// Calculate hash of the parameters, will be used to look up an existing variation.
	TArray<uint8> Data;
    FMemoryWriter Writer(Data);
    FObjectAndNameAsStringProxyArchive WriterProxy(Writer, /*bInLoadIfFindFails*/true);
	VariationParametersBag->SerializeItem(WriterProxy, const_cast<uint8*>(VariationParameters.GetValue().GetMemory()), /* Defaults */ nullptr);

	const uint64 VariationParametersHash = CityHash64((const char*)Data.GetData(), Data.Num());

	const FSmartObjectDefinitionAssetVariation* ExistingVariation = Variations.FindByPredicate([VariationParametersHash](const FSmartObjectDefinitionAssetVariation& Variation)
	{
		return Variation.ParametersHash == VariationParametersHash;
	});
	if (ExistingVariation)
	{
		return ExistingVariation->DefinitionAsset.Get();
	}

	// Not the same, create a new one.
	const FName UniqueName = MakeUniqueObjectName(
		GetTransientPackage(),
		USmartObjectDefinition::StaticClass(),
		FName(FString::Printf(TEXT("%s_Var%llx"), *GetNameSafe(this), VariationParametersHash))
	);

	USmartObjectDefinition* AssetVariation = DuplicateObject(this, GetTransientPackage(), UniqueName);
	check(AssetVariation);

	// Apply parameters
	AssetVariation->Parameters = VariationParameters;
	AssetVariation->ApplyParameters();

	// Keep track of variations.
	Variations.Emplace(AssetVariation, VariationParametersHash);
	
	return AssetVariation;
}

void USmartObjectDefinition::ApplyParameters()
{
	// Do property copies
	for (const FSmartObjectDefinitionPropertyBinding& Binding : PropertyBindings)
	{
		FPropertyBindingDataView SourceDataView;
		if (!GetDataView(Binding.SourceDataHandle, SourceDataView))
		{
			UE_LOG(LogSmartObject, Error, TEXT("%hs %s: Could not find data view for property copy source %s."), __FUNCTION__, *GetFullNameSafe(this), *Binding.SourcePath.ToString());
			continue;
		}

		FPropertyBindingDataView TargetDataView;
		if (!GetDataView(Binding.TargetDataHandle, TargetDataView))
		{
			UE_LOG(LogSmartObject, Error, TEXT("%hs %s: Could not find data view for property copy target %s."), __FUNCTION__, *GetFullNameSafe(this), *Binding.TargetPath.ToString());
			continue;
		}
		
		CopyProperty(SourceDataView, Binding.SourcePath, TargetDataView, Binding.TargetPath);
	}
}

bool USmartObjectDefinition::CopyProperty(FPropertyBindingDataView SourceDataView, const FPropertyBindingPath& SourcePath, FPropertyBindingDataView TargetDataView, const FPropertyBindingPath& TargetPath)
{
	TArray<FPropertyBindingPathIndirection> SourceIndirections;
	if (!SourcePath.ResolveIndirectionsWithValue(SourceDataView, SourceIndirections))
	{
		return false;
	}

	TArray<FPropertyBindingPathIndirection> TargetIndirections;
	if (!TargetPath.ResolveIndirectionsWithValue(TargetDataView, TargetIndirections))
	{
		return false;
	}

	const FProperty* SourceLeafProperty = SourceIndirections.Last().GetProperty();
	const FProperty* TargetLeafProperty = TargetIndirections.Last().GetProperty();
		
	if (!SourceLeafProperty
		|| !TargetLeafProperty
		|| !ArePropertiesCompatible(SourceLeafProperty, TargetLeafProperty))
	{
		return false;
	}

	const void* SourceAddress = SourceIndirections.Last().GetPropertyAddress();
	void* TargetAddress = TargetIndirections.Last().GetMutablePropertyAddress();

	if (SourceAddress && TargetAddress)
	{
		TargetLeafProperty->CopyCompleteValue(TargetAddress, SourceAddress);
	}

	return true;
}


bool USmartObjectDefinition::ArePropertiesCompatible(const FProperty* SourceProperty, const FProperty* TargetProperty)
{
	if (SourceProperty == TargetProperty)
	{
		return true;
	}

	if (SourceProperty == nullptr || TargetProperty == nullptr)
	{
		return true;
	}

	// Special case for object properties since InPropertyA->SameType(InPropertyB) requires both properties to be of the exact same class.
	// In our case we want to be able to bind a source property if its class is a child of the target property class.
	const FObjectPropertyBase* SourceObjectProperty = CastField<const FObjectPropertyBase>(SourceProperty);
	const FObjectPropertyBase* TargetObjectProperty = CastField<const FObjectPropertyBase>(TargetProperty);
	if (SourceObjectProperty && TargetObjectProperty)
	{
		return SourceObjectProperty->PropertyClass->IsChildOf(TargetObjectProperty->PropertyClass);
	}

	if (SourceProperty->SameType(TargetProperty))
	{
		return true;
	}

	return false;
}

#if WITH_EDITOR
void USmartObjectDefinition::EnsureValidGuids()
{
	if (!RootID.IsValid())
	{
		static FName RootName(TEXT("RootID"));
		RootID = FGuid::NewDeterministicGuid(GetPathName(), GetTypeHash(RootName));
	}
	if (!ParametersID.IsValid())
	{
		static FName DataProxyName(TEXT("ParametersID"));
		ParametersID = FGuid::NewDeterministicGuid(GetPathName(), GetTypeHash(DataProxyName));
	}
}

void USmartObjectDefinition::UpdateBindingDataHandles()
{
	for (FSmartObjectDefinitionPropertyBinding& Binding : PropertyBindings)
	{
		Binding.SourceDataHandle = GetDataHandleByID(Binding.SourcePath.GetStructID());
		Binding.TargetDataHandle = GetDataHandleByID(Binding.TargetPath.GetStructID());
	}
}

void USmartObjectDefinition::AddPropertyBinding(const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath)
{
	FPropertyBindingPath ValidatedSourcePath = SourcePath;
	if (!UpdateAndValidatePath(ValidatedSourcePath))
	{
		return;
	}

	FPropertyBindingPath ValidatedTargetPath = TargetPath;
	if (!UpdateAndValidatePath(ValidatedSourcePath))
	{
		return;
	}
	
	RemovePropertyBindings(TargetPath);

	PropertyBindings.Emplace(ValidatedSourcePath, ValidatedTargetPath);

	UpdateBindingPaths();
	UpdateBindingDataHandles();
}

void USmartObjectDefinition::RemovePropertyBindings(const FPropertyBindingPath& TargetPath)
{
	PropertyBindings.RemoveAll([&TargetPath](const FSmartObjectDefinitionPropertyBinding& Binding)
	{
		return Binding.GetTargetPath() == TargetPath;
	});
	
	UpdateBindingDataHandles();
}

const FPropertyBindingPath* USmartObjectDefinition::GetPropertyBindingSource(const FPropertyBindingPath& TargetPath)
{
	const FSmartObjectDefinitionPropertyBinding* Binding = PropertyBindings.FindByPredicate([&TargetPath](const FSmartObjectDefinitionPropertyBinding& Binding)
	{
		return Binding.GetTargetPath() == TargetPath;
	});
	return Binding ? &Binding->GetSourcePath() : nullptr; 
}

void USmartObjectDefinition::GetAccessibleStructs(const FGuid TargetStructID, TArray<FBindableStructDesc>& OutStructDescs)
{
	FBindableStructDesc& ParametersDesc = OutStructDescs.AddDefaulted_GetRef();
	ParametersDesc.Name = FName(TEXT("Parameters"));
	ParametersDesc.ID = ParametersID;
	ParametersDesc.Struct = Parameters.GetPropertyBagStruct();
}

bool USmartObjectDefinition::GetDataViewByID(const FGuid StructID, FPropertyBindingDataView& OutDataView)
{
	if (StructID == ParametersID)
	{
		OutDataView = FPropertyBindingDataView(Parameters.GetMutableValue());
		return true;
	}
	if (StructID == RootID)
	{
		OutDataView = FPropertyBindingDataView(this);
		return true;
	}

	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
		if (StructID == Slot.ID)
		{
			OutDataView = FPropertyBindingDataView(FStructView::Make(Slot));
			return true;
		}
		for (FSmartObjectDefinitionDataProxy& DataProxy : Slot.DefinitionData)
		{
			if (StructID == DataProxy.ID)
			{
				OutDataView = FPropertyBindingDataView(DataProxy.Data.GetScriptStruct(), DataProxy.Data.GetMutableMemory());
				return true;
			}
		}
	}
	
	return false;
}

bool USmartObjectDefinition::GetStructDescByID(const FGuid StructID, FBindableStructDesc& OutDesc)
{
	if (StructID == ParametersID)
	{
		OutDesc = FBindableStructDesc(FName(TEXT("Parameters")), Parameters.GetMutableValue().GetScriptStruct(), ParametersID);
		return true;
	}
	if (StructID == RootID)
	{
		OutDesc = FBindableStructDesc(FName(TEXT("Root")), StaticClass(), RootID);
		return true;
	}

	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
		if (StructID == Slot.ID)
		{
			OutDesc = FBindableStructDesc(Slot.Name, TBaseStructure<FSmartObjectSlotDefinition>::Get(), Slot.ID);
			return true;
		}
		for (FSmartObjectDefinitionDataProxy& DataProxy : Slot.DefinitionData)
		{
			if (StructID == DataProxy.ID)
			{
				FString DataName = Slot.Name.ToString();
				const UScriptStruct* ScriptStruct = DataProxy.Data.GetScriptStruct(); 
				if (ScriptStruct)
				{
					DataName += TEXT(" ");
					DataName += ScriptStruct->GetDisplayNameText().ToString();
				}
				OutDesc = FBindableStructDesc(FName(DataName), ScriptStruct, DataProxy.ID);
				return true;
			}
		}
	}
	
	return false;
}

FSmartObjectDefinitionDataHandle USmartObjectDefinition::GetDataHandleByID(const FGuid StructID)
{
	if (StructID == ParametersID)
	{
		return FSmartObjectDefinitionDataHandle::Parameters;
	}
	if (StructID == RootID)
	{
		return FSmartObjectDefinitionDataHandle::Root;
	}

	for (const TEnumerateRef<const FSmartObjectSlotDefinition> Slot : EnumerateRange(Slots))
	{
		if (StructID == Slot->ID)
		{
			return FSmartObjectDefinitionDataHandle(Slot.GetIndex());
		}
		for (const TEnumerateRef<const FSmartObjectDefinitionDataProxy> DataProxy : EnumerateRange(Slot->DefinitionData))
		{
			if (StructID == DataProxy->ID)
			{
				return FSmartObjectDefinitionDataHandle(Slot.GetIndex(), DataProxy.GetIndex());
			}
		}
	}
	
	return {};
}

FGuid USmartObjectDefinition::GetDataRootID() const
{
	return RootID;
}

bool USmartObjectDefinition::AddParameterAndBindingFromPropertyPath(const FPropertyBindingPath& TargetPath)
{
	if (TargetPath.IsPathEmpty())
	{
		return false;
	}

	FPropertyBindingDataView TargetDataView;
	if (!GetDataViewByID(TargetPath.GetStructID(), TargetDataView))
	{
		return false;
	}

	FBindableStructDesc TargetDesc;
	if (!GetStructDescByID(TargetPath.GetStructID(), TargetDesc))
	{
		return false;
	}

	TArray<FPropertyBindingPathIndirection> TargetIndirections;
	if (!TargetPath.ResolveIndirectionsWithValue(TargetDataView, TargetIndirections))
	{
		return false;
	}

	// Add new property
	const FProperty* TargetLeafProperty = TargetIndirections.Last().GetProperty();

	const FString NewNameString = TargetDesc.Name.ToString() + TEXT(" ") + TargetLeafProperty->GetDisplayNameText().ToString();
	const FName NewPropertyName(NewNameString);
	
	Parameters.AddProperty(NewPropertyName, TargetLeafProperty);

	const FPropertyBindingPath SourcePath(ParametersID, NewPropertyName);

	// Copy the current value to the newly created parameter.
	FPropertyBindingDataView SourceDataView;
	if (GetDataViewByID(SourcePath.GetStructID(), SourceDataView))
	{
		// Note: source/target reversed intentionally.
		CopyProperty(TargetDataView, TargetPath, SourceDataView, SourcePath);
	}

	// Add binding
	AddPropertyBinding(SourcePath, TargetPath);

	// Update UI
	UE::SmartObject::Delegates::OnParametersChanged.Broadcast(*this);
	
	return true;
}

#endif // WITH_EDITOR

bool USmartObjectDefinition::GetDataView(const FSmartObjectDefinitionDataHandle DataHandle, FPropertyBindingDataView& OutDataView)
{
	if (!DataHandle.IsSlotValid())
	{
		return false;
	}
	
	if (DataHandle.IsParameters())
	{
		OutDataView = FPropertyBindingDataView(Parameters.GetMutableValue());
		return true;
	}
	if (DataHandle.IsRoot())
	{
		OutDataView = FPropertyBindingDataView(this);
		return true;
	}

	const int32 SlotIndex = DataHandle.GetSlotIndex();
	if (Slots.IsValidIndex(SlotIndex))
	{
		FSmartObjectSlotDefinition& Slot = Slots[SlotIndex];

		if (DataHandle.IsDataValid())
		{
			// Slot data definition
			const int32 DataDefinitionIndex = DataHandle.GetDataIndex();
			if (Slot.DefinitionData.IsValidIndex(DataDefinitionIndex))
			{
				FSmartObjectDefinitionDataProxy& DataProxy = Slot.DefinitionData[DataDefinitionIndex];
				OutDataView = FPropertyBindingDataView(DataProxy.Data.GetScriptStruct(), DataProxy.Data.GetMutableMemory());
				return true;
			}
		}
		else
		{
			// Just a slot
			OutDataView = FPropertyBindingDataView(FStructView::Make(Slot));
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
