// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Framework/TypedElementListProxy.h"
#include "Elements/Framework/TypedElementInterfaceCustomization.h"
#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "TypedElementCommonActions.generated.h"

class UTypedElementSelectionSet;

/**
 * Customization used to allow asset editors (such as the level editor) to override the base behavior of common actions.
 */
class ENGINE_API FTypedElementCommonActionsCustomization
{
public:
	virtual ~FTypedElementCommonActionsCustomization() = default;

	//~ See UTypedElementCommonActions for API docs
	virtual bool DeleteElements(ITypedElementWorldInterface* InWorldInterface, TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& InDeletionOptions);
	virtual void DuplicateElements(ITypedElementWorldInterface* InWorldInterface, TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, const FVector& InLocationOffset, TArray<FTypedElementHandle>& OutNewElements);
};

/**
 * Utility to hold a typed element handle and its associated world interface and common actions customization.
 */
struct ENGINE_API FTypedElementCommonActionsElement
{
public:
	FTypedElementCommonActionsElement() = default;

	FTypedElementCommonActionsElement(TTypedElement<ITypedElementWorldInterface> InElementWorldHandle, FTypedElementCommonActionsCustomization* InCommonActionsCustomization)
		: ElementWorldHandle(MoveTemp(InElementWorldHandle))
		, CommonActionsCustomization(InCommonActionsCustomization)
	{
	}

	FTypedElementCommonActionsElement(const FTypedElementCommonActionsElement&) = default;
	FTypedElementCommonActionsElement& operator=(const FTypedElementCommonActionsElement&) = default;

	FTypedElementCommonActionsElement(FTypedElementCommonActionsElement&&) = default;
	FTypedElementCommonActionsElement& operator=(FTypedElementCommonActionsElement&&) = default;

	FORCEINLINE explicit operator bool() const
	{
		return IsSet();
	}

	FORCEINLINE bool IsSet() const
	{
		return ElementWorldHandle.IsSet()
			&& CommonActionsCustomization;
	}

	//~ See UTypedElementCommonActions for API docs

private:
	TTypedElement<ITypedElementWorldInterface> ElementWorldHandle;
	FTypedElementCommonActionsCustomization* CommonActionsCustomization = nullptr;
};

/**
 * A utility to handle higher-level common actions, but default via UTypedElementWorldInterface,
 * but asset editors can customize this behavior via FTypedElementCommonActionsCustomization.
 */
UCLASS(Transient)
class ENGINE_API UTypedElementCommonActions : public UObject, public TTypedElementInterfaceCustomizationRegistry<FTypedElementCommonActionsCustomization>
{
	GENERATED_BODY()

public:
	/**
	 * Delete any elements from the given selection set that can be deleted.
	 * @note Internally this just calls DeleteNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	UFUNCTION(BlueprintCallable, Category="TypedElementFramework|Common")
	bool DeleteSelectedElements(UTypedElementSelectionSet* SelectionSet, UWorld* World, const FTypedElementDeletionOptions& DeletionOptions);
	
	/**
	 * Delete any elements from the given list that can be deleted.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	bool DeleteNormalizedElements(const FTypedElementListConstPtr& ElementListPtr, UWorld* World, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& DeletionOptions);

	/**
	 * Duplicate any elements from the given selection set that can be duplicated.
	 * @note Internally this just calls DuplicateNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	TArray<FTypedElementHandle> DuplicateSelectedElements(const UTypedElementSelectionSet* SelectionSet, UWorld* World, const FVector& LocationOffset);
	
	/**
	 * Duplicate any elements from the given list that can be duplicated.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	TArray<FTypedElementHandle> DuplicateNormalizedElements(const FTypedElementListConstPtr& ElementListPtr, UWorld* World, const FVector& LocationOffset);


	/**
	 * Script Api
	 */

	/**
	 * Delete any elements from the given list that can be deleted.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	UFUNCTION(BlueprintCallable, Category="TypedElementFramework|Common")
	bool DeleteNormalizedElements(const FScriptTypedElementListProxy ElementList, UWorld* World, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& DeletionOptions);

	/**
	 * Duplicate any elements from the given selection set that can be duplicated.
	 * @note Internally this just calls DuplicateNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	UFUNCTION(BlueprintCallable, DisplayName="Duplicate Selected Elements", Category = "TypedElementFramework|Common", meta=(ScriptName="DuplicateSelectedElements"))
	TArray<FScriptTypedElementHandle> K2_DuplicateSelectedElements(const UTypedElementSelectionSet* SelectionSet, UWorld* World, const FVector& LocationOffset);
	
	/**
	 * Duplicate any elements from the given list that can be duplicated.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	UFUNCTION(BlueprintCallable, Category = "TypedElementFramework|Common")
	TArray<FScriptTypedElementHandle> DuplicateNormalizedElements(const FScriptTypedElementListProxy ElementList, UWorld* World, const FVector& LocationOffset);

private:
	/**
	 * Attempt to resolve the selection interface and common actions customization for the given element, if any.
	 */
	FTypedElementCommonActionsElement ResolveCommonActionsElement(const FTypedElementHandle& InElementHandle) const;
};
