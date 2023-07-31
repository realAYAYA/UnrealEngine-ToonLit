// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementCommonActions.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Framework/TypedElementUtil.h"

#include "UObject/GCObjectScopeGuard.h"
#include "UObject/Stack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TypedElementCommonActions)

bool FTypedElementCommonActionsCustomization::DeleteElements(ITypedElementWorldInterface* InWorldInterface, TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& InDeletionOptions)
{
	return InWorldInterface->DeleteElements(InElementHandles, InWorld, InSelectionSet, InDeletionOptions);
}

void FTypedElementCommonActionsCustomization::DuplicateElements(ITypedElementWorldInterface* InWorldInterface, TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, const FVector& InLocationOffset, TArray<FTypedElementHandle>& OutNewElements)
{
	InWorldInterface->DuplicateElements(InElementHandles, InWorld, InLocationOffset, OutNewElements);
}


bool UTypedElementCommonActions::DeleteSelectedElements(UTypedElementSelectionSet* SelectionSet, UWorld* World, const FTypedElementDeletionOptions& DeletionOptions)
{
	if (!SelectionSet)
	{
		FFrame::KismetExecutionMessage(TEXT("SelectionSet is null."), ELogVerbosity::Error);
		return false;
	}

	if (!World)
	{
		FFrame::KismetExecutionMessage(TEXT("World is null."), ELogVerbosity::Error);
		return false;
	}

	FTypedElementListRef NormalizedElements = SelectionSet->GetNormalizedSelection(FTypedElementSelectionNormalizationOptions());
	return DeleteNormalizedElements(NormalizedElements, World, SelectionSet, DeletionOptions);
}

bool UTypedElementCommonActions::DeleteNormalizedElements(const FTypedElementListConstPtr& ElementListPtr, UWorld* World, UTypedElementSelectionSet* SelectionSet, const FTypedElementDeletionOptions& DeletionOptions)
{
	bool bSuccess = false;

	if (ElementListPtr)
	{
		TMap<FTypedHandleTypeId, TArray<FTypedElementHandle>> ElementsToDeleteByType;
		TypedElementUtil::BatchElementsByType(ElementListPtr.ToSharedRef(), ElementsToDeleteByType);

		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		UTypedElementRegistry::FDisableElementDestructionOnGC GCGuard(Registry);

		for (const auto& ElementsByTypePair : ElementsToDeleteByType)
		{
			FTypedElementCommonActionsCustomization* CommonActionsCustomization = GetInterfaceCustomizationByTypeId(ElementsByTypePair.Key);
			ITypedElementWorldInterface* WorldInterface = Registry->GetElementInterface<ITypedElementWorldInterface>(ElementsByTypePair.Key);
			if (CommonActionsCustomization && WorldInterface)
			{
				bSuccess |= CommonActionsCustomization->DeleteElements(WorldInterface, ElementsByTypePair.Value, World, SelectionSet, DeletionOptions);
			}
		}
	}
	
	return bSuccess;
}

TArray<FTypedElementHandle> UTypedElementCommonActions::DuplicateSelectedElements(const UTypedElementSelectionSet* SelectionSet, UWorld* World, const FVector& LocationOffset)
{
	FTypedElementListRef NormalizedElements = SelectionSet->GetNormalizedSelection(FTypedElementSelectionNormalizationOptions());
	return DuplicateNormalizedElements(NormalizedElements, World, LocationOffset);
}

TArray<FTypedElementHandle> UTypedElementCommonActions::DuplicateNormalizedElements(const FTypedElementListConstPtr& ElementListPtr, UWorld* World, const FVector& LocationOffset)
{
	TArray<FTypedElementHandle> NewElements;
	if (ElementListPtr)
	{
		NewElements.Reserve(ElementListPtr->Num());

		TMap<FTypedHandleTypeId, TArray<FTypedElementHandle>> ElementsToDuplicateByType;
		TypedElementUtil::BatchElementsByType(ElementListPtr.ToSharedRef(), ElementsToDuplicateByType);

		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		for (const auto& ElementsByTypePair : ElementsToDuplicateByType)
		{
			FTypedElementCommonActionsCustomization* CommonActionsCustomization = GetInterfaceCustomizationByTypeId(ElementsByTypePair.Key);
			ITypedElementWorldInterface* WorldInterface = Registry->GetElementInterface<ITypedElementWorldInterface>(ElementsByTypePair.Key);
			if (CommonActionsCustomization && WorldInterface)
			{
				CommonActionsCustomization->DuplicateElements(WorldInterface, ElementsByTypePair.Value, World, LocationOffset, NewElements);
			}
		}
	}
	
	return NewElements;
}

bool UTypedElementCommonActions::DeleteNormalizedElements(const FScriptTypedElementListProxy ElementList, UWorld* World, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& DeletionOptions)
{
	FTypedElementListPtr NativeList = UE::TypedElementFramework::ConvertToNativeTypedElementList(ElementList.GetElementList());
	if (!NativeList)
	{
		FFrame::KismetExecutionMessage(TEXT("ElementList is in a invalid state."), ELogVerbosity::Error);
		return false;
	}

	if (!World)
	{
		FFrame::KismetExecutionMessage(TEXT("World is null."), ELogVerbosity::Error);
		return false;
	}

	return DeleteNormalizedElements(NativeList, World, InSelectionSet, DeletionOptions);
}

TArray<FScriptTypedElementHandle> UTypedElementCommonActions::K2_DuplicateSelectedElements(const UTypedElementSelectionSet* SelectionSet, UWorld* World, const FVector& LocationOffset)
{
	if (!SelectionSet)
	{
		FFrame::KismetExecutionMessage(TEXT("SelectionSet is null."), ELogVerbosity::Error);
		return {};
	}

	if (!World)
	{
		FFrame::KismetExecutionMessage(TEXT("World is null."), ELogVerbosity::Error);
		return {};
	}

	return TypedElementUtil::ConvertToScriptElementArray(DuplicateSelectedElements(SelectionSet, World, LocationOffset), SelectionSet->GetElementList()->GetRegistry());
}

TArray<FScriptTypedElementHandle> UTypedElementCommonActions::DuplicateNormalizedElements(const FScriptTypedElementListProxy ElementList, UWorld* World, const FVector& LocationOffset)
{
	FTypedElementListPtr NativeList = UE::TypedElementFramework::ConvertToNativeTypedElementList(ElementList.GetElementList());
	if (!NativeList)
	{
		FFrame::KismetExecutionMessage(TEXT("ElementList is in a invalid state."), ELogVerbosity::Error);
		return {};
	}

	if (!World)
	{
		FFrame::KismetExecutionMessage(TEXT("World is null."), ELogVerbosity::Error);
		return {};
	}

	return TypedElementUtil::ConvertToScriptElementArray(DuplicateNormalizedElements(NativeList, World, LocationOffset), NativeList->GetRegistry());
}

FTypedElementCommonActionsElement UTypedElementCommonActions::ResolveCommonActionsElement(const FTypedElementHandle& InElementHandle) const
{
	return InElementHandle
		? FTypedElementCommonActionsElement(UTypedElementRegistry::GetInstance()->GetElement<ITypedElementWorldInterface>(InElementHandle), GetInterfaceCustomizationByTypeId(InElementHandle.GetId().GetTypeId()))
		: FTypedElementCommonActionsElement();
}

