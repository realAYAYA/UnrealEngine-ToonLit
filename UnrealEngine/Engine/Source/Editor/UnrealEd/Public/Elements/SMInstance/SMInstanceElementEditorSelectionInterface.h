// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/SMInstance/SMInstanceElementSelectionInterface.h"
#include "SMInstanceElementEditorSelectionInterface.generated.h"

UCLASS()
class UNREALED_API USMInstanceElementEditorSelectionInterface : public USMInstanceElementSelectionInterface
{
	GENERATED_BODY()

public:
	virtual bool IsElementSelected(const FTypedElementHandle& InElementHandle, const FTypedElementListConstPtr& InSelectionSet, const FTypedElementIsSelectedOptions& InSelectionOptions) override;
	virtual bool SelectElement(const FTypedElementHandle& InElementHandle, const FTypedElementListPtr& InSelectionSet, const FTypedElementSelectionOptions& InSelectionOptions) override;
	virtual bool DeselectElement(const FTypedElementHandle& InElementHandle, const FTypedElementListPtr& InSelectionSet, const FTypedElementSelectionOptions& InSelectionOptions) override;
	virtual bool ShouldPreventTransactions(const FTypedElementHandle& InElementHandle) override;
	virtual TUniquePtr<ITypedElementTransactedElement> CreateTransactedElementImpl() override;
};
