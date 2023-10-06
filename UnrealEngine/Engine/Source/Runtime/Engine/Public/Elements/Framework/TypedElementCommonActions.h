// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Framework/TypedElementListProxy.h"
#include "Elements/Framework/TypedElementInterfaceCustomization.h"
#include "Elements/Interfaces/TypedElementWorldInterface.h"

#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

#include "TypedElementCommonActions.generated.h"

class UTypedElementSelectionSet;

/**
 * Customization used to allow asset editors (such as the level editor) to override the base behavior of common actions.
 */
class FTypedElementCommonActionsCustomization
{
public:
	virtual ~FTypedElementCommonActionsCustomization() = default;

	//~ See UTypedElementCommonActions for API docs
	ENGINE_API virtual bool DeleteElements(ITypedElementWorldInterface* InWorldInterface, TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& InDeletionOptions);
	ENGINE_API virtual void DuplicateElements(ITypedElementWorldInterface* InWorldInterface, TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, const FVector& InLocationOffset, TArray<FTypedElementHandle>& OutNewElements);
	ENGINE_API virtual void CopyElements(ITypedElementWorldInterface* InWorldInterface,TArrayView<const FTypedElementHandle> InElementHandles, FOutputDevice& Out);
	ENGINE_API virtual TSharedPtr<FWorldElementPasteImporter> GetPasteImporter(ITypedElementWorldInterface* InWorldInterface, const FTypedElementListConstPtr& InSelectedHandles, UWorld* InWorld);
};

/**
 * Utility to hold a typed element handle and its associated world interface and common actions customization.
 */
struct FTypedElementCommonActionsElement
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

USTRUCT(BlueprintType)
struct FTypedElementPasteOptions
{
	GENERATED_BODY()

	// Todo Copy And Paste should we consider supporting pasting with surface snapping?

	// Todo Copy And Paste should we add optional options to handle where to paste (Like some sort of per type paste context for example instance can go under existing actor/components or the partition mode)?

	// If provided the SelectionSet selection will only contains the newly added elements
	UPROPERTY(BlueprintReadWrite, Category = "TypedElementInterfaces|World|PasteOptions")
	TObjectPtr<UTypedElementSelectionSet> SelectionSetToModify;

	UPROPERTY(BlueprintReadWrite, Category = "TypedElementInterfaces|World|PasteOptions")
	bool bPasteAtLocation = false;

	UPROPERTY(BlueprintReadWrite, Category = "TypedElementInterfaces|World|PasteOptions")
	FVector PasteLocation = FVector::ZeroVector;

	// C++ Only. Allow for custom callbacks for some custom top level object in the T3D file
	TMap<FStringView, TFunction<TSharedRef<FWorldElementPasteImporter> ()>> ExtraCustomImport;
};

/**
 * A utility to handle higher-level common actions, but default via UTypedElementWorldInterface,
 * but asset editors can customize this behavior via FTypedElementCommonActionsCustomization.
 */
UCLASS(Transient, MinimalAPI)
class UTypedElementCommonActions : public UObject, public TTypedElementInterfaceCustomizationRegistry<FTypedElementCommonActionsCustomization>
{
	GENERATED_BODY()

public:
	/**
	 * Delete any elements from the given selection set that can be deleted.
	 * @note Internally this just calls DeleteNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	UFUNCTION(BlueprintCallable, Category="TypedElementFramework|Common")
	ENGINE_API bool DeleteSelectedElements(UTypedElementSelectionSet* SelectionSet, UWorld* World, const FTypedElementDeletionOptions& DeletionOptions);
	
	/**
	 * Delete any elements from the given list that can be deleted.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	ENGINE_API bool DeleteNormalizedElements(const FTypedElementListConstPtr& ElementListPtr, UWorld* World, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& DeletionOptions);

	/**
	 * Duplicate any elements from the given selection set that can be duplicated.
	 * @note Internally this just calls DuplicateNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	ENGINE_API TArray<FTypedElementHandle> DuplicateSelectedElements(const UTypedElementSelectionSet* SelectionSet, UWorld* World, const FVector& LocationOffset);
	
	/**
	 * Duplicate any elements from the given list that can be duplicated.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	ENGINE_API virtual TArray<FTypedElementHandle> DuplicateNormalizedElements(const FTypedElementListConstPtr& ElementListPtr, UWorld* World, const FVector& LocationOffset);

	/**
	 * Copy any elements from the given selection set that can be copied into the clipboard
	 * @note Internally this just calls CopyNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	UFUNCTION(BlueprintCallable, Category = "TypedElementFramework|Common")
	ENGINE_API bool CopySelectedElements(UTypedElementSelectionSet* SelectionSet);

	/**
	 * Copy any elements from the given selection set that can be copied into the string
	 * @note Internally this just calls CopyNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	UFUNCTION(BlueprintCallable, Category = "TypedElementFramework|Common")
	ENGINE_API bool CopySelectedElementsToString(UTypedElementSelectionSet* SelectionSet, FString& OutputString);

	/*
	 * Copy any elements from the given selection set that can be copied into the clipboard.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	ENGINE_API virtual bool CopyNormalizedElements(const FTypedElementListConstPtr& ElementListPtr, FString* OptionalOutputString = nullptr);

	/**
	 * Paste any elements from the given string or from the clipboard
	 * @note Internally this just calls PasteNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	ENGINE_API TArray<FTypedElementHandle> PasteElements(UTypedElementSelectionSet* SelectionSet, UWorld* World, const FTypedElementPasteOptions& PasteOption, const FString* OptionalInputString = nullptr);

	/**
	 * Paste any elements from the given string or from the clipboard
	  * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	ENGINE_API virtual TArray<FTypedElementHandle> PasteNormalizedElements(const FTypedElementListConstPtr& ElementListPtr, UWorld* World, const FTypedElementPasteOptions& PasteOptions, const FString* OptionalInputString = nullptr);

	/**
	 * Script Api
	 */

	/**
	 * Delete any elements from the given list that can be deleted.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	UFUNCTION(BlueprintCallable, Category="TypedElementFramework|Common")
	ENGINE_API bool DeleteNormalizedElements(const FScriptTypedElementListProxy ElementList, UWorld* World, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& DeletionOptions);

	/**
	 * Duplicate any elements from the given selection set that can be duplicated.
	 * @note Internally this just calls DuplicateNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	UFUNCTION(BlueprintCallable, DisplayName="Duplicate Selected Elements", Category = "TypedElementFramework|Common", meta=(ScriptName="DuplicateSelectedElements"))
	ENGINE_API TArray<FScriptTypedElementHandle> K2_DuplicateSelectedElements(const UTypedElementSelectionSet* SelectionSet, UWorld* World, const FVector& LocationOffset);
	
	/**
	 * Duplicate any elements from the given list that can be duplicated.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	UFUNCTION(BlueprintCallable, Category = "TypedElementFramework|Common")
	ENGINE_API TArray<FScriptTypedElementHandle> DuplicateNormalizedElements(const FScriptTypedElementListProxy ElementList, UWorld* World, const FVector& LocationOffset);
	
	/*
	 * Copy any elements from the given selection set that can be copied into the clipboard.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	UFUNCTION(BlueprintCallable, Category = "TypedElementFramework|Common")
	ENGINE_API bool CopyNormalizedElements(const FScriptTypedElementListProxy& ElementList);

	/*
	 * Copy any elements from the given selection set that can be copied into the clipboard.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	UFUNCTION(BlueprintCallable, Category = "TypedElementFramework|Common")
	ENGINE_API bool CopyNormalizedElementsToString(const FScriptTypedElementListProxy& ElementList, FString& OutputString);

	/**
	 * Paste any elements from the clipboard
	 * @note Internally this just calls PasteNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	UFUNCTION(BlueprintCallable, DisplayName="Paste Elements", Category = "TypedElementFramework|Common", meta=(ScriptName="PasteElements"))
	ENGINE_API TArray<FScriptTypedElementHandle> K2_PasteElements(UTypedElementSelectionSet* SelectionSet, UWorld* World, const FTypedElementPasteOptions& PasteOption);

	/**
	 * Paste any elements from the given string
	 * @note Internally this just calls PasteNormalizedElements on the result of UTypedElementSelectionSet::GetNormalizedSelection.
	 */
	UFUNCTION(BlueprintCallable, Category = "TypedElementFramework|Common")
	ENGINE_API TArray<FScriptTypedElementHandle> PasteElementsFromString(UTypedElementSelectionSet* SelectionSet, UWorld* World, const FTypedElementPasteOptions& PasteOption, const FString& InputString);

	/**
	 * Paste any elements from the clipboard
	  * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	UFUNCTION(BlueprintCallable, DisplayName="Paste Normalized Elements", Category = "TypedElementFramework|Common", meta=(ScriptName="PasteNormalizedElements"))
	ENGINE_API TArray<FScriptTypedElementHandle> K2_PasteNormalizedElements(const FScriptTypedElementListProxy& ElementList, UWorld* World, const FTypedElementPasteOptions& PasteOption);

	/**
	 * Paste any elements from the given string
	  * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList.
	 */
	UFUNCTION(BlueprintCallable, Category = "TypedElementFramework|Common")
	ENGINE_API TArray<FScriptTypedElementHandle> PasteNormalizedElementsFromString(const FScriptTypedElementListProxy& ElementList, UWorld* World, const FTypedElementPasteOptions& PasteOption, const FString& InputString);

private:
	/**
	 * Attempt to resolve the selection interface and common actions customization for the given element, if any.
	 */
	FTypedElementCommonActionsElement ResolveCommonActionsElement(const FTypedElementHandle& InElementHandle) const;
};

namespace TypedElementCommonActionsUtils
{
	/**
	 * Is the elements Copy and paste currently enabled?
	 */
	ENGINE_API bool IsElementCopyAndPasteEnabled();
}
