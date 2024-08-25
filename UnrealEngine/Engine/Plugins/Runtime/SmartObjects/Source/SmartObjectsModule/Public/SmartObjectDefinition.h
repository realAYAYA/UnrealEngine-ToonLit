// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Math/Box.h"
#include "WorldConditionQuery.h"
#include "WorldConditions/SmartObjectWorldConditionSchema.h"
#include "SmartObjectTypes.h"
#include "PropertyBag.h"
#include "PropertyBindingPath.h"
#include "SmartObjectDefinition.generated.h"

struct FSmartObjectSlotIndex;
class UGameplayBehaviorConfig;
class USmartObjectSlotValidationFilter;
class USmartObjectDefinition;
enum class ESmartObjectTagFilteringPolicy: uint8;
enum class ESmartObjectTagMergingPolicy: uint8;


namespace UE::SmartObject::Delegates
{
#if WITH_EDITOR

	/** Called in editor when parameters for a specific SmartObjectDefinition changes. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnParametersChanged, const USmartObjectDefinition& /*SmartObjectDefinition*/);
	extern SMARTOBJECTSMODULE_API FOnParametersChanged OnParametersChanged;

#endif
}; //

/** Indicates how Tags from slots and parent object are combined to be evaluated by a TagQuery from a find request. */
UENUM()
enum class ESmartObjectSlotShape : uint8
{
	Circle,
	Rectangle
};

/**
 * Abstract class that can be extended to bind a new type of behavior framework
 * to the smart objects by defining the required definition.
 */
UCLASS(Abstract, NotBlueprintable, EditInlineNew, CollapseCategories, HideDropdown)
class SMARTOBJECTSMODULE_API USmartObjectBehaviorDefinition : public UObject
{
	GENERATED_BODY()
};

/** Helper struct for definition data, which allows to identify items based on GUID in editor (even empty ones). */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectDefinitionDataProxy
{
	GENERATED_BODY()

	FSmartObjectDefinitionDataProxy() = default;

	template<typename T, typename = std::enable_if_t<std::is_base_of_v<FSmartObjectDefinitionData, std::decay_t<T>>>>
	static FSmartObjectDefinitionDataProxy Make(const T& Struct)
	{
		FSmartObjectDefinitionDataProxy NewProxy;
		NewProxy.Data.InitializeAsScriptStruct(TBaseStructure<T>::Get(), reinterpret_cast<const uint8*>(&Struct));
#if WITH_EDITORONLY_DATA
		NewProxy.ID = FGuid::NewGuid();
#endif
		return NewProxy;
	}
	
	UPROPERTY(EditDefaultsOnly, Category = "Slot", meta = (ExcludeBaseStruct))
	TInstancedStruct<FSmartObjectDefinitionData> Data;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "Slot", meta = (Hidden))
	FGuid ID;
#endif	
};

using FSmartObjectSlotDefinitionDataProxy UE_DEPRECATED(5.4, "Deprecated struct. Please use FSmartObjectDefinitionDataProxy instead.") = FSmartObjectDefinitionDataProxy;

/**
 * Persistent and sharable definition of a smart object slot.
 */
USTRUCT(BlueprintType)
struct SMARTOBJECTSMODULE_API FSmartObjectSlotDefinition
{
	GENERATED_BODY()

	// Macro needed to avoid deprecation errors with "Data" being copied or created in the default methods
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FSmartObjectSlotDefinition() = default;
	FSmartObjectSlotDefinition(const FSmartObjectSlotDefinition&) = default;
	FSmartObjectSlotDefinition(FSmartObjectSlotDefinition&&) = default;
	FSmartObjectSlotDefinition& operator=(const FSmartObjectSlotDefinition&) = default;
	FSmartObjectSlotDefinition& operator=(FSmartObjectSlotDefinition&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Returns a reference to the definition data of the specified type.
	 * Method will fail a check if the slot definition doesn't contain the given type.
	 */
	template<typename T>
	const T& GetDefinitionData() const
	{
		static_assert(TIsDerivedFrom<T, FSmartObjectDefinitionData>::IsDerived,
					"Given struct doesn't represent a valid definition data type. Make sure to inherit from FSmartObjectDefinitionData or one of its child-types.");

		for (const FSmartObjectDefinitionDataProxy& DataProxy : DefinitionData)
		{
			if (DataProxy.Data.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return DataProxy.Data.Get<T>();
			}
		}
		checkf(false, TEXT("Failed to find slot definition data"));
		return nullptr;
	}

	/**
	 * Returns a pointer to the definition data of the specified type.
	 * Method will return null if the slot doesn't contain the given type.
	 */
	template<typename T>
	const T* GetDefinitionDataPtr() const
	{
		static_assert(TIsDerivedFrom<T, FSmartObjectDefinitionData>::IsDerived,
					"Given struct doesn't represent a valid definition data type. Make sure to inherit from FSmartObjectDefinitionData or one of its child-types.");

		for (const FSmartObjectDefinitionDataProxy& DataProxy : DefinitionData)
		{
			if (DataProxy.Data.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return DataProxy.Data.GetPtr<T>();
			}
		}

		return nullptr;
	}
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "Slot")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Slot", meta = (DisplayName = "Color"))
	FColor DEBUG_DrawColor = FColor::Yellow;

	UPROPERTY(EditAnywhere, Category = "Slot", meta = (DisplayName = "Shape"))
	ESmartObjectSlotShape DEBUG_DrawShape = ESmartObjectSlotShape::Circle;
	
	UPROPERTY(EditAnywhere, Category = "Slot", meta = (DisplayName = "Size"))
	float DEBUG_DrawSize = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Slot", meta = (Hidden))
	FGuid ID;
#endif // WITH_EDITORONLY_DATA

	/** Offset relative to the parent object where the slot is located. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Slot")
	FVector3f Offset = FVector3f::ZeroVector;

	/** Rotation relative to the parent object. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Slot")
	FRotator3f Rotation = FRotator3f::ZeroRotator;
	
	/** Whether the slot is enable initially. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Slot")
	bool bEnabled = true;

	/** This slot is available only for users matching this query. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Slot")
	FGameplayTagQuery UserTagFilter;

	/**
	 * Tags identifying this slot's use case. Can be used while looking for slots supporting given activity.
	 * Depending on the tag filtering policy these tags can override the parent object's tags
	 * or be combined with them while applying filters from requests.
	 */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Slot")
	FGameplayTagContainer ActivityTags;

	/** Initial runtime tags. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Slot")
	FGameplayTagContainer RuntimeTags;

	/** Preconditions that must pass for the slot to be selected. */
	UPROPERTY(EditDefaultsOnly, Category = "Slot")
	FWorldConditionQueryDefinition SelectionPreconditions;

	/**
	 * All available definitions associated to this slot.
	 * This allows multiple frameworks to provide their specific behavior definition to the slot.
	 * Note that there should be only one definition of each type since the first one will be selected.
	 */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Slot", Instanced)
	TArray<TObjectPtr<USmartObjectBehaviorDefinition>> BehaviorDefinitions;

	/** Custom definition data items (struct inheriting from SmartObjecDefinitionData) that can be added to the slot definition and accessed through a FSmartObjectSlotView */
	UPROPERTY(EditDefaultsOnly, Category = "Slot")
	TArray<FSmartObjectDefinitionDataProxy> DefinitionData;
	
#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.4, "Use DefinitionData instead.")
	UPROPERTY()
	TArray<FInstancedStruct> Data_DEPRECATED;
#endif	
};


/**
 * Data used for previewing in the Smart Object editor. 
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectDefinitionPreviewData
{
	GENERATED_BODY()
	
	/** Actor class used as the object for previewing the definition in the asset editor. */
	UPROPERTY(EditDefaultsOnly, Category = "Object Preview")
	TSoftClassPtr<AActor> ObjectActorClass;

	/** Path of the static mesh used as the object for previewing the definition in the asset editor. */
	UPROPERTY(EditDefaultsOnly, Category = "Object Preview", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	FSoftObjectPath ObjectMeshPath;

	/** Actor class used for previewing the smart object user actor in the asset editor. */
	UPROPERTY(EditDefaultsOnly, Category = "User Preview")
	TSoftClassPtr<AActor> UserActorClass;

	/** Validation filter used for previewing the smart object user in the asset editor. */
	UPROPERTY(EditDefaultsOnly, Category = "User Preview")
	TSoftClassPtr<USmartObjectSlotValidationFilter> UserValidationFilterClass;
};

/** Used internally by USmartObjectDefinition to point to a specific piece of data. */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectDefinitionDataHandle
{
	GENERATED_BODY()

	static const FSmartObjectDefinitionDataHandle Invalid;
	static const FSmartObjectDefinitionDataHandle Root;
	static const FSmartObjectDefinitionDataHandle Parameters;

	FSmartObjectDefinitionDataHandle() = default;
	
	explicit FSmartObjectDefinitionDataHandle(const int32 InSlotIndex, const int32 InDataIndex = INDEX_NONE)
	{
		check(InSlotIndex < InvalidIndex || InSlotIndex == INDEX_NONE);
		check(InDataIndex < InvalidIndex || InDataIndex == INDEX_NONE);
		SlotIndex = InSlotIndex == INDEX_NONE ? InvalidIndex : (uint16)InSlotIndex;
		DataIndex = InDataIndex == INDEX_NONE ? InvalidIndex : (uint16)InDataIndex;
	}

	FSmartObjectDefinitionDataHandle& operator=(const FSmartObjectDefinitionDataHandle& Other)
	{
		SlotIndex = Other.SlotIndex;
		DataIndex = Other.DataIndex;
		return *this;
	}

	bool operator==(const FSmartObjectDefinitionDataHandle& Other) const
	{
		return SlotIndex == Other.SlotIndex && DataIndex == Other.DataIndex;
	}

	bool operator!=(const FSmartObjectDefinitionDataHandle& Other) const
	{
		return !(*this == Other);
	}

	bool IsSlotValid() const
	{
		return SlotIndex != InvalidIndex;
	}

	bool IsDataValid() const
	{
		return DataIndex != InvalidIndex;
	}

	bool IsRoot() const
	{
		return SlotIndex == RootIndex;
	}

	bool IsParameters() const
	{
		return SlotIndex == ParametersIndex;
	}

	int32 GetSlotIndex() const
	{
		return SlotIndex == InvalidIndex ? INDEX_NONE : SlotIndex;
	}

	int32 GetDataIndex() const
	{
		return DataIndex == InvalidIndex ? INDEX_NONE : DataIndex;
	}
	
protected:

	static constexpr uint16 InvalidIndex = MAX_uint16;
	static constexpr uint16 RootIndex = MAX_uint16 - 1;
	static constexpr uint16 ParametersIndex = MAX_uint16 - 2;
	
	UPROPERTY()
	uint16 SlotIndex = InvalidIndex;
	
	UPROPERTY()
	uint16 DataIndex = InvalidIndex;
};

/** Used internally by USmartObjectDefinition to store a property binding. */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectDefinitionPropertyBinding
{
	GENERATED_BODY()

	FSmartObjectDefinitionPropertyBinding() = default;

	FSmartObjectDefinitionPropertyBinding(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
		: SourcePath(InSourcePath)
		, TargetPath(InTargetPath)
	{
	}

	const FPropertyBindingPath& GetSourcePath() const
	{
		return SourcePath;
	}

	const FPropertyBindingPath& GetTargetPath() const
	{
		return TargetPath;
	}

protected:
	UPROPERTY()
	FPropertyBindingPath SourcePath;

	UPROPERTY()
	FPropertyBindingPath TargetPath;

	UPROPERTY()
	FSmartObjectDefinitionDataHandle SourceDataHandle;

	UPROPERTY()
	FSmartObjectDefinitionDataHandle TargetDataHandle;

	friend USmartObjectDefinition;
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
	const USmartObjectBehaviorDefinition* GetBehaviorDefinition(const int32 SlotIndex, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass) const;

	/** @return Preconditions that must pass for the object to be found/used. */
	const FWorldConditionQueryDefinition& GetPreconditions() const { return Preconditions; }

	/** @return mutable Preconditions that must pass for the object to be found/used. */
	FWorldConditionQueryDefinition& GetMutablePreconditions() { return Preconditions; }

	/** @return a view on all the slot definitions */
	TConstArrayView<FSmartObjectSlotDefinition> GetSlots() const { return Slots; }

	/** @return slot definition stored at a given index */
	const FSmartObjectSlotDefinition& GetSlot(const int32 Index) const { return Slots[Index]; }

	/** @return mutable slot definition stored at a given index */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject")
	FSmartObjectSlotDefinition& GetMutableSlot(const int32 Index) { return Slots[Index]; }

	/** @return True if specified slot index is valid. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject")
	bool IsValidSlotIndex(const int32 SlotIndex) const { return Slots.IsValidIndex(SlotIndex); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject", meta=(DisplayName="Get Smart Object Slot Definitions"))
	const TArray<FSmartObjectSlotDefinition>& K2_GetSlots() const { return Slots; }

#if WITH_EDITOR
	/** Returns a view on all the slot definitions */
	TArrayView<FSmartObjectSlotDefinition> GetMutableSlots() { return Slots; }

	/** @return validation filter class for preview. */
	TSubclassOf<USmartObjectSlotValidationFilter> GetPreviewValidationFilterClass() const;

	/** @return Index of the slot that has the specified ID, or INDEX_NONE if not found. */
	int32 FindSlotByID(const FGuid ID) const;

	/**
	 * Returns slot and definition data indices the ID represents.
	 * @param ID ID of the slots or definition data to find
	 * @param OutSlotIndex Index of the slot the ID points to
	 * @param OutDefinitionDataIndex Index of the definition data the ID points to, or INDEX_NONE, if ID points directly to a slot.
	 * @return true if ID matches data in the definition. */
	bool FindSlotAndDefinitionDataIndexByID(const FGuid ID, int32& OutSlotIndex, int32& OutDefinitionDataIndex) const;
#endif

	/** Return bounds encapsulating all slots */
	UFUNCTION(BlueprintCallable, Category="SmartObject")
	FBox GetBounds() const;

	/** Adds and returns a reference to a defaulted slot (used for testing purposes) */
	FSmartObjectSlotDefinition& DebugAddSlot() { return Slots.AddDefaulted_GetRef(); }

	/**
	 * Returns the transform (in world space) of the given slot index.
	 * @param OwnerTransform Transform (in world space) of the slot owner.
	 * @param SlotIndex Index within the list of slots.
	 * @return Transform (in world space) of the slot associated to SlotIndex.
	 * @note Method will ensure on invalid slot index.
	 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.3, "Please use GetSlotWorldTransform() instead.")
	TOptional<FTransform> GetSlotTransform(const FTransform& OwnerTransform, const FSmartObjectSlotIndex SlotIndex) const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Returns the transform (in world space) of the given slot index.
	 * @param OwnerTransform Transform (in world space) of the slot owner.
	 * @param SlotIndex Index within the list of slots.
	 * @return Transform (in world space) of the slot associated to SlotIndex, or OwnerTransform if index is invalid.
	 * @note Method will ensure on invalid slot index.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject")
	FTransform GetSlotWorldTransform(const int32 SlotIndex, const FTransform& OwnerTransform) const;

	/**
	 * Fills the provided GameplayTagContainer with the activity tags associated to the slot according to the tag merging policy.
	 * @param SlotIndex	Index of the slot for which the tags are requested
	 * @param OutActivityTags Tag container to fill with the activity tags associated to the slot
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject", meta = (DisplayName="Get Slot Activity Tags (By Index)", ScriptName="GetSlotActivityTagsByIndex", AutoCreateRefTerm = "OutActivityTags"))
	void GetSlotActivityTags(const int32 SlotIndex, FGameplayTagContainer& OutActivityTags) const;

	/**
	 * Fills the provided GameplayTagContainer with the activity tags associated to the slot according to the tag merging policy.
	 * @param SlotDefinition Definition of the slot for which the tags are requested
	 * @param OutActivityTags Tag container to fill with the activity tags associated to the slot
	 */
	void GetSlotActivityTags(const FSmartObjectSlotDefinition& SlotDefinition, FGameplayTagContainer& OutActivityTags) const;

	/** Returns the tag query to run on the user tags provided by a request to accept this definition */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject")
	const FGameplayTagQuery& GetUserTagFilter() const { return UserTagFilter; }

	/** Sets the tag query to run on the user tags provided by a request to accept this definition */
	UFUNCTION(BlueprintCallable, Category="SmartObject")
	void SetUserTagFilter(const FGameplayTagQuery& InUserTagFilter) { UserTagFilter = InUserTagFilter; }

	/** Returns the tag query to run on the runtime tags of a smart object instance to accept it */
	UE_DEPRECATED(5.2, "Use FWorldCondition_SmartObjectActorTagQuery or FSmartObjectWorldConditionObjectTagQuery in Preconditions instead.")
	const FGameplayTagQuery& GetObjectTagFilter() const { static FGameplayTagQuery Dummy; return Dummy; }

	/** Sets the tag query to run on the runtime tags of a smart object instance to accept it */
	UE_DEPRECATED(5.2, "Use FWorldCondition_SmartObjectActorTagQuery or FSmartObjectWorldConditionObjectTagQuery in Preconditions instead.")
	void SetObjectTagFilter(const FGameplayTagQuery& InObjectTagFilter) {}

	/** Returns the list of tags describing the activity associated to this definition */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject")
	const FGameplayTagContainer& GetActivityTags() const { return ActivityTags; }

	/** Sets the list of tags describing the activity associated to this definition */
	void SetActivityTags(const FGameplayTagContainer& InActivityTags) { ActivityTags = InActivityTags; }

	/** Returns the tag filtering policy that should be applied on User tags by this definition */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObject")
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
	UE_DEPRECATED(5.3, "Use ObjectActorClass in PreviewData instead.")
	UPROPERTY()
	TSoftClassPtr<AActor> PreviewClass_DEPRECATED;

	/** Path of the static mesh used for previewing the definition in the asset editor. */
	UE_DEPRECATED(5.3, "Use ObjectMeshPath in PreviewData instead.")
	UPROPERTY()
	FSoftObjectPath PreviewMeshPath_DEPRECATED;

	/** Actor class used for previewing the user in the asset editor. */
	UPROPERTY()
	FSmartObjectDefinitionPreviewData PreviewData;
#endif // WITH_EDITORONLY_DATA

	const USmartObjectWorldConditionSchema* GetWorldConditionSchema() const { return WorldConditionSchemaClass.GetDefaultObject(); }
	const TSubclassOf<USmartObjectWorldConditionSchema>& GetWorldConditionSchemaClass() const { return WorldConditionSchemaClass; }

	/**
	 * Returns a reference to the definition data of the specified type.
	 * Method will fail a check if the slot definition doesn't contain the given type.
	 */
	template<typename T>
	const T& GetDefinitionData() const
	{
		static_assert(TIsDerivedFrom<T, FSmartObjectDefinitionData>::IsDerived,
					"Given struct doesn't represent a valid definition data type. Make sure to inherit from FSmartObjectDefinitionData or one of its child-types.");

		for (const FSmartObjectDefinitionDataProxy& DataProxy : DefinitionData)
		{
			if (DataProxy.Data.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return DataProxy.Data.Get<T>();
			}
		}
		
		checkf(false, TEXT("Failed to find definition data"));
		return nullptr;
	}

	/**
	 * Returns a pointer to the definition data of the specified type.
	 * Method will return null if the slot doesn't contain the given type.
	 */
	template<typename T>
	const T* GetDefinitionDataPtr() const
	{
		static_assert(TIsDerivedFrom<T, FSmartObjectDefinitionData>::IsDerived,
					"Given struct doesn't represent a valid definition data type. Make sure to inherit from FSmartObjectDefinitionData or one of its child-types.");

		for (const FSmartObjectDefinitionDataProxy& DataProxy : DefinitionData)
		{
			if (DataProxy.Data.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return DataProxy.Data.GetPtr<T>();
			}
		}

		return nullptr;
	}

	/** @return reference to definition default parameters. */
	const FInstancedPropertyBag& GetDefaultParameters() const
	{
		return Parameters;
	}

	/**
	 * Returns a variation of this asset with specified parameters applied.
	 * The variations are cached, and if a variation with same parameters is already in use, the existing asset is returned.
	 * @return Pointer to an asset variation.
	*/
	USmartObjectDefinition* GetAssetVariation(const FInstancedPropertyBag& Parameters);

	static bool ArePropertiesCompatible(const FProperty* SourceProperty, const FProperty* TargetProperty);

#if WITH_EDITOR
	void AddPropertyBinding(const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath);
	void RemovePropertyBindings(const FPropertyBindingPath& TargetPath);
	const FPropertyBindingPath* GetPropertyBindingSource(const FPropertyBindingPath& TargetPath);
	void GetAccessibleStructs(const FGuid TargetStructID, TArray<FBindableStructDesc>& OutStructDescs);
	bool GetDataViewByID(const FGuid StructID, FPropertyBindingDataView& OutDataView);
	bool GetStructDescByID(const FGuid StructID, FBindableStructDesc& OutDesc);
	FGuid GetDataRootID() const;
	bool AddParameterAndBindingFromPropertyPath(const FPropertyBindingPath& TargetPath);
#endif // WITH_EDITOR

protected:

#if WITH_EDITOR
	void UpdateSlotReferences();
	void UpdateBindingPaths();
	bool UpdateAndValidatePath(FPropertyBindingPath& Path);
	FSmartObjectDefinitionDataHandle GetDataHandleByID(const FGuid StructID);

	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

private:
	/** Finds first behavior definition of a given class in the provided list of definitions. */
	static const USmartObjectBehaviorDefinition* GetBehaviorDefinitionByType(const TArray<USmartObjectBehaviorDefinition*>& BehaviorDefinitions, const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass);

	void ApplyParameters();
	bool CopyProperty(FPropertyBindingDataView SourceDataView, const FPropertyBindingPath& SourcePath, FPropertyBindingDataView TargetDataView, const FPropertyBindingPath& TargetPath);
	bool GetDataView(const FSmartObjectDefinitionDataHandle DataHandle, FPropertyBindingDataView& OutDataView);

#if WITH_EDITOR
	void EnsureValidGuids();
	void UpdateBindingDataHandles();
#endif // WITH_EDITOR

	/** Used internally by USmartObjectDefinition to store a variation of a definition asset. */
	struct FSmartObjectDefinitionAssetVariation
	{
		FSmartObjectDefinitionAssetVariation() = default;
	
		FSmartObjectDefinitionAssetVariation(USmartObjectDefinition* InDefinitionAsset, uint64 InParametersHash)
			: DefinitionAsset(InDefinitionAsset)
			, ParametersHash(InParametersHash)
		{
		}
	
		/** Pointer to the asset variation which has the parameters applied to it. Stored as weak pointer, so that we can prune variations which are not used anymore. */
		TWeakObjectPtr<USmartObjectDefinition> DefinitionAsset = nullptr;

		/** Hash of the variation properties. */
		uint64 ParametersHash = 0;
	};
	
	/** Variations of the asset based on provided parameters, created on demand via GetAssetVariation(). */
	TArray<FSmartObjectDefinitionAssetVariation> Variations;
	
	/** Parameters for the SmartObject definition */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", meta = (NoBinding))
	FInstancedPropertyBag Parameters;

	/** Binding ID for the parameters. */
	UPROPERTY()
	FGuid ParametersID;

	/** Binding ID for the whole asset. */
	UPROPERTY()
	FGuid RootID;

	/** Property bindings. */
	UPROPERTY()
	TArray<FSmartObjectDefinitionPropertyBinding> PropertyBindings;
	
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
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", meta = (NoBinding))
	FWorldConditionQueryDefinition Preconditions;

private:
	/** Tags identifying this Smart Object's use case. Can be used while looking for objects supporting given activity */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	FGameplayTagContainer ActivityTags;

	/** Custom definition data items (struct inheriting from SmartObjectDefinitionData) for the whole Smart Object. */
	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", meta=(DisallowedStructs="/Script/SmartObjectsModule.SmartObjectSlotAnnotation"))
	TArray<FSmartObjectDefinitionDataProxy> DefinitionData;

	UPROPERTY(EditDefaultsOnly, Category = "SmartObject", AdvancedDisplay, meta = (NoBinding))
	TSubclassOf<USmartObjectWorldConditionSchema> WorldConditionSchemaClass;
	
	/** Indicates how Tags from slots and parent object are combined to be evaluated by a TagQuery from a find request. */
	UPROPERTY(EditAnywhere, Category = "SmartObject", AdvancedDisplay, meta = (NoBinding))
	ESmartObjectTagMergingPolicy ActivityTagsMergingPolicy;

	/** Indicates how TagQueries from slots and parent object will be processed against User Tags from a find request. */
	UPROPERTY(EditAnywhere, Category = "SmartObject", AdvancedDisplay, meta = (NoBinding))
	ESmartObjectTagFilteringPolicy UserTagsFilteringPolicy;
	
	mutable TOptional<bool> bValid;

	friend class FSmartObjectSlotReferenceDetails;
	friend class FSmartObjectViewModel;
	friend class FSmartObjectAssetToolkit;
	friend class FSmartObjectDefinitionDetails;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "SmartObjectTypes.h"
#endif
