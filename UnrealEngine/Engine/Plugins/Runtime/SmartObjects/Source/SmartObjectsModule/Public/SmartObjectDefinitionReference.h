// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBag.h"
#include "SmartObjectDefinitionReference.generated.h"

class USmartObjectDefinition;

/**
 * Struct to hold reference to a SmartObjectDefinition asset along with values to parameterized it.
 */
USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectDefinitionReference
{
	GENERATED_BODY()

	bool operator==(const FSmartObjectDefinitionReference& RHS) const
	{
		return SmartObjectDefinition == RHS.SmartObjectDefinition
			&& Parameters.Identical(&RHS.Parameters, 0)
			&& PropertyOverrides == RHS.PropertyOverrides; 
	}

	bool operator!=(const FSmartObjectDefinitionReference& RHS) const
	{
		return !(*this == RHS);
	}

	/** @return true if the reference is set. */
	bool IsValid() const
	{
		return SmartObjectDefinition != nullptr;
	}
	
	/** Sets the SmartObject Definition asset and synchronize parameters. */
	void SetSmartObjectDefinition(USmartObjectDefinition* NewSmartObjectDefinition)
	{
		SmartObjectDefinition = NewSmartObjectDefinition;
		SyncParameters();
	}

	/** @return const pointer to the referenced SmartObject Definition asset. */
	const USmartObjectDefinition* GetSmartObjectDefinition() const
	{
		return SmartObjectDefinition;
	}

	/** @return pointer to the referenced SmartObject Definition asset. */
	USmartObjectDefinition* GetMutableSmartObjectDefinition()
	{
		return SmartObjectDefinition;
	}

	/** @return reference to the parameters for the referenced SmartObject Definition asset. */
	const FInstancedPropertyBag& GetParameters() const
	{
		ConditionallySyncParameters();
		return Parameters;
	}

	/** @return reference to the parameters for the referenced SmartObject Definition asset. */
	FInstancedPropertyBag& GetMutableParameters()
	{
		ConditionallySyncParameters();
		return Parameters;
	}

	/**
	 * Enforce self parameters to be compatible with those exposed by the selected SmartObject Definition asset.
	 */
	void SyncParameters();

	/**
	 * Indicates if current parameters are compatible with those available in the selected SmartObject Definition asset.
	 * @return true when parameters requires to be synced to be compatible with those available in the selected SmartObject Definition asset, false otherwise.
	 */
	bool RequiresParametersSync() const;

	/** Sync parameters to match the asset if required. */
	void ConditionallySyncParameters() const;

	/** @return true if the property of specified ID is overridden. */
	bool IsPropertyOverridden(const FGuid PropertyID) const
	{
		return PropertyOverrides.Contains(PropertyID);
	}

	/** Sets the override status of specified property by ID. */
	void SetPropertyOverridden(const FGuid PropertyID, const bool bIsOverridden);

	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

protected:
	UPROPERTY(EditAnywhere, Category = "")
	TObjectPtr<USmartObjectDefinition> SmartObjectDefinition = nullptr;

	UPROPERTY(EditAnywhere, Category = "", meta = (FixedLayout))
	FInstancedPropertyBag Parameters;

	/** Array of overridden properties. Non-overridden properties will inherit the values from the SmartObjectDefintion default parameters. */
	UPROPERTY(EditAnywhere, Category = "")
	TArray<FGuid> PropertyOverrides;

	friend class FSmartObjectDefinitionReferenceDetails;
};

template<>
struct TStructOpsTypeTraits<FSmartObjectDefinitionReference> : public TStructOpsTypeTraitsBase2<FSmartObjectDefinitionReference>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
