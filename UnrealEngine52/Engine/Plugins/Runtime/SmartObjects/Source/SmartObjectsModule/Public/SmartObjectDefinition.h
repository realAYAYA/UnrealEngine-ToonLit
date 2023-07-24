// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "MassEntityTypes.h"
#include "Engine/DataAsset.h"
#include "Math/Box.h"
#include "WorldConditionQuery.h"
#include "WorldConditions/SmartObjectWorldConditionSchema.h"
#include "SmartObjectDefinition.generated.h"

struct FSmartObjectSlotIndex;

class UGameplayBehaviorConfig;
enum class ESmartObjectTagFilteringPolicy: uint8;
enum class ESmartObjectTagMergingPolicy: uint8;

/**
 * Abstract class that can be extended to bind a new type of behavior framework
 * to the smart objects by defining the required definition.
 */
UCLASS(Abstract, NotBlueprintable, EditInlineNew, CollapseCategories, HideDropdown)
class SMARTOBJECTSMODULE_API USmartObjectBehaviorDefinition : public UObject
{
	GENERATED_BODY()
};

/**
 * Persistent and sharable definition of a smart object slot.
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectSlotDefinition
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "SmartObject", meta = (DisplayName = "Color"))
	FColor DEBUG_DrawColor = FColor::Yellow;

	UPROPERTY(EditAnywhere, Category = "SmartObject", meta = (Hidden))
	FGuid ID;
#endif // WITH_EDITORONLY_DATA

	/** Whether the slot is enable initially. */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	bool bEnabled = true;

	/** Initial runtime tags. */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FGameplayTagContainer RuntimeTags;

	/** This slot is available only for users matching this query. */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FGameplayTagQuery UserTagFilter;

	/**
	 * Tags identifying this slot's use case. Can be used while looking for slots supporting given activity.
	 * Depending on the tag filtering policy these tags can override the parent object's tags
	 * or be combined with them while applying filters from requests.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FGameplayTagContainer ActivityTags;

	/** Preconditions that must pass for the slot to be selected. */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FWorldConditionQueryDefinition SelectionPreconditions;

	/** Offset relative to the parent object where the slot is located. */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FVector Offset = FVector::ZeroVector;

	/** Rotation relative to the parent object. */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Custom data (struct inheriting from SmartObjectSlotDefinitionData) that can be added to the slot definition and accessed through a FSmartObjectSlotView */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", meta = (BaseStruct = "/Script/SmartObjectsModule.SmartObjectSlotDefinitionData", ExcludeBaseStruct))
	TArray<FInstancedStruct> Data;

	/**
	 * All available definitions associated to this slot.
	 * This allows multiple frameworks to provide their specific behavior definition to the slot.
	 * Note that there should be only one definition of each type since the first one will be selected.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", Instanced)
	TArray<TObjectPtr<USmartObjectBehaviorDefinition>> BehaviorDefinitions;
};


/**
 * SmartObject definition asset. Contains sharable information that can be used by multiple SmartObject instances at runtime.
 */
UCLASS(BlueprintType, Blueprintable, CollapseCategories)
class SMARTOBJECTSMODULE_API USmartObjectDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	explicit USmartObjectDefinition(const FObjectInitializer& ObjectInitializer);

	/**
	 * Retrieves a specific type of behavior definition for a given slot.
	 * When the slot doesn't provide one or if the provided index is not valid
	 * the search will look in the object default definitions.
	 *
	 * @param SlotIndex			Index of the slot for which the definition is requested
	 * @param DefinitionClass	Type of the requested behavior definition
	 * @return The behavior definition found or null if none are available for the requested type.
	 */
	const USmartObjectBehaviorDefinition* GetBehaviorDefinition(const FSmartObjectSlotIndex& SlotIndex, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass) const;

	/** @return Preconditions that must pass for the object to be found/used. */
	const FWorldConditionQueryDefinition& GetPreconditions() const { return Preconditions; }

	/** @return mutable Preconditions that must pass for the object to be found/used. */
	FWorldConditionQueryDefinition& GetMutablePreconditions() { return Preconditions; }

	/** @return a view on all the slot definitions */
	TConstArrayView<FSmartObjectSlotDefinition> GetSlots() const { return Slots; }

	/** @return slot definition stored at a given index */
	const FSmartObjectSlotDefinition& GetSlot(const int32 Index) const { return Slots[Index]; }

	/** @return mutable slot definition stored at a given index */
	FSmartObjectSlotDefinition& GetMutableSlot(const int32 Index) { return Slots[Index]; }

	/** @return True if specified slot index is valid. */
	bool IsValidSlotIndex(const int32 SlotIndex) const { return Slots.IsValidIndex(SlotIndex); } 

#if WITH_EDITOR
	/** Returns a view on all the slot definitions */
	TArrayView<FSmartObjectSlotDefinition> GetMutableSlots() { return Slots; }
#endif

	/** Return bounds encapsulating all slots */
	FBox GetBounds() const;

	/** Adds and returns a reference to a defaulted slot (used for testing purposes) */
	FSmartObjectSlotDefinition& DebugAddSlot() { return Slots.AddDefaulted_GetRef(); }

	/**
	 * Returns the transform (in world space) of the given slot index.
	 * @param OwnerTransform Transform (in world space) of the slot owner.
	 * @param SlotIndex Index within the list of slots.
	 * @return Transform (in world space) of the slot associated to SlotIndex.
	 * @note Method will ensure on invalid invalid index.
	 */
	TOptional<FTransform> GetSlotTransform(const FTransform& OwnerTransform, const FSmartObjectSlotIndex SlotIndex) const;

	/**
	 * Fills the provided GameplayTagContainer with the activity tags associated to the slot according to the tag merging policy.
	 * @param SlotIndex	Index of the slot for which the tags are requested
	 * @param OutActivityTags Tag container to fill with the activity tags associated to the slot
	 */
	void GetSlotActivityTags(const FSmartObjectSlotIndex& SlotIndex, FGameplayTagContainer& OutActivityTags) const;

	/**
	 * Fills the provided GameplayTagContainer with the activity tags associated to the slot according to the tag merging policy.
	 * @param SlotDefinition Definition of the slot for which the tags are requested
	 * @param OutActivityTags Tag container to fill with the activity tags associated to the slot
	 */
	void GetSlotActivityTags(const FSmartObjectSlotDefinition& SlotDefinition, FGameplayTagContainer& OutActivityTags) const;

	/** Returns the tag query to run on the user tags provided by a request to accept this definition */
	const FGameplayTagQuery& GetUserTagFilter() const { return UserTagFilter; }

	/** Sets the tag query to run on the user tags provided by a request to accept this definition */
	void SetUserTagFilter(const FGameplayTagQuery& InUserTagFilter) { UserTagFilter = InUserTagFilter; }

	/** Returns the tag query to run on the runtime tags of a smart object instance to accept it */
	UE_DEPRECATED(5.2, "Use FWorldCondition_SmartObjectActorTagQuery or FSmartObjectWorldConditionObjectTagQuery in Preconditions instead.")
	const FGameplayTagQuery& GetObjectTagFilter() const { static FGameplayTagQuery Dummy; return Dummy; }

	/** Sets the tag query to run on the runtime tags of a smart object instance to accept it */
	UE_DEPRECATED(5.2, "Use FWorldCondition_SmartObjectActorTagQuery or FSmartObjectWorldConditionObjectTagQuery in Preconditions instead.")
	void SetObjectTagFilter(const FGameplayTagQuery& InObjectTagFilter) {}

	/** Returns the list of tags describing the activity associated to this definition */
	const FGameplayTagContainer& GetActivityTags() const { return ActivityTags; }

	/** Sets the list of tags describing the activity associated to this definition */
	void SetActivityTags(const FGameplayTagContainer& InActivityTags) { ActivityTags = InActivityTags; }

	/** Returns the tag filtering policy that should be applied on User tags by this definition */
	ESmartObjectTagFilteringPolicy GetUserTagsFilteringPolicy() const { return UserTagsFilteringPolicy; }

	/** Sets the tag filtering policy to apply on User tags by this definition */
	void SetUserTagsFilteringPolicy(const ESmartObjectTagFilteringPolicy InUserTagsFilteringPolicy) { UserTagsFilteringPolicy = InUserTagsFilteringPolicy; }

	/** Returns the tag merging policy to apply on Activity tags from this definition */
	ESmartObjectTagMergingPolicy GetActivityTagsMergingPolicy() const { return ActivityTagsMergingPolicy; }

	/** Sets the tag merging policy to apply on Activity tags from this definition */
	void SetActivityTagsMergingPolicy(const ESmartObjectTagMergingPolicy InActivityTagsMergingPolicy) { ActivityTagsMergingPolicy = InActivityTagsMergingPolicy; }

	/**
	 *	Performs validation for the current definition. The method will return on the first error encountered by default
	 *	but could go through all validations and report all errors (e.g. when saving the asset errors are reported to the user).
	 *	An object using an invalid definition will not be registered in the simulation.
	 *	The result of the validation is stored until next validation and can be retrieved using `IsValid`.
	 *	@param ErrorsToReport Optional list of error messages that could be provided to report them
	 *	@return true if the definition is valid
	 */
	bool Validate(TArray<FText>* ErrorsToReport = nullptr) const;

	/** Provides a description of the definition */
	friend FString LexToString(const USmartObjectDefinition& Definition)
	{
		return FString::Printf(TEXT("NumSlots=%d NumDefs=%d HasUserFilter=%s HasPreConditions=%s"),
			Definition.Slots.Num(),
			Definition.DefaultBehaviorDefinitions.Num(),
			*LexToString(!Definition.UserTagFilter.IsEmpty()),
			*LexToString(Definition.Preconditions.IsValid()));
	}

	/** Returns result of the last validation if `Validate` was called; unset otherwise. */
	TOptional<bool> IsValid() const { return bValid; }

#if WITH_EDITORONLY_DATA
	/** Actor class used for previewing the definition in the asset editor. */
	UPROPERTY()
	TSoftClassPtr<AActor> PreviewClass;

	/** Path of the static mesh used for previewing the definition in the asset editor. */
	UPROPERTY()
	FSoftObjectPath PreviewMeshPath;
#endif // WITH_EDITORONLY_DATA

	const USmartObjectWorldConditionSchema* GetWorldConditionSchema() const { return WorldConditionSchemaClass.GetDefaultObject(); }
	const TSubclassOf<USmartObjectWorldConditionSchema>& GetWorldConditionSchemaClass() const { return WorldConditionSchemaClass; }
	
protected:

#if WITH_EDITOR
	/** @return Index of the slot that has the specified ID, or INDEX_NONE if not found. */
	int32 FindSlotByID(const FGuid ID) const;

	void UpdateSlotReferences();

	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif // WITH_EDITOR

	virtual void PostLoad() override;

private:
	/** Finds first behavior definition of a given class in the provided list of definitions. */
	static const USmartObjectBehaviorDefinition* GetBehaviorDefinitionByType(const TArray<USmartObjectBehaviorDefinition*>& BehaviorDefinitions, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass);

	/**
	 * Where SmartObject's user needs to stay to be able to activate it. These
	 * will be used by AI to approach the object. Locations are relative to object's location.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	TArray<FSmartObjectSlotDefinition> Slots;

	/** List of behavior definitions of different types provided to SO's user if the slot does not provide one. */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", Instanced)
	TArray<TObjectPtr<USmartObjectBehaviorDefinition>> DefaultBehaviorDefinitions;

	/** This object is available if user tags match this query; always available if query is empty. */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FGameplayTagQuery UserTagFilter;

#if WITH_EDITORONLY_DATA
	/** This object is available if instance tags match this query; always available if query is empty. */
	UE_DEPRECATED(5.2, "FWorldCondition_SmartObjectActorTagQuery or FSmartObjectWorldConditionObjectTagQuery used in Preconditions instead.")
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use FWorldCondition_SmartObjectActorTagQuery or FSmartObjectWorldConditionObjectTagQuery in Preconditions instead."))
	FGameplayTagQuery ObjectTagFilter;
#endif // WITH_EDITORONLY_DATA

	/** Preconditions that must pass for the object to be found/used. */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FWorldConditionQueryDefinition Preconditions;

private:
	/** Tags identifying this Smart Object's use case. Can be used while looking for objects supporting given activity */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FGameplayTagContainer ActivityTags;

	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", AdvancedDisplay)
	TSubclassOf<USmartObjectWorldConditionSchema> WorldConditionSchemaClass;
	
	/** Indicates how Tags from slots and parent object are combined to be evaluated by a TagQuery from a find request. */
	UPROPERTY(EditAnywhere, Category = "SmartObject", AdvancedDisplay)
	ESmartObjectTagMergingPolicy ActivityTagsMergingPolicy;

	/** Indicates how TagQueries from slots and parent object will be processed against User Tags from a find request. */
	UPROPERTY(EditAnywhere, Category = "SmartObject", AdvancedDisplay)
	ESmartObjectTagFilteringPolicy UserTagsFilteringPolicy;
	
	mutable TOptional<bool> bValid;

	friend class FSmartObjectSlotReferenceDetails;
};

/**
 * Mass Fragment used to share slot definition between slot instances.
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectSlotDefinitionFragment : public FMassSharedFragment
{
	GENERATED_BODY()

	FSmartObjectSlotDefinitionFragment() = default;
	explicit FSmartObjectSlotDefinitionFragment(const USmartObjectDefinition& InObjectDefinition, const FSmartObjectSlotDefinition& InSlotDefinition)
		: SmartObjectDefinition(&InObjectDefinition), SlotDefinition(&InSlotDefinition) {}

	/** Pointer to the parent object definition to preserve slot definition pointer validity. */
	UPROPERTY(Transient)
	TObjectPtr<const USmartObjectDefinition> SmartObjectDefinition = nullptr;

	/** Pointer to the slot definition contained by the SmartObject definition. */
	const FSmartObjectSlotDefinition* SlotDefinition = nullptr;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "SmartObjectTypes.h"
#endif
