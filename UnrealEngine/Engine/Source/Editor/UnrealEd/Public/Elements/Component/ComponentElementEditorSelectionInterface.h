// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Component/ComponentElementSelectionInterface.h"
#include "Elements/Framework/TypedElementListFwd.h"
#include "Templates/UniquePtr.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ComponentElementEditorSelectionInterface.generated.h"

class ITypedElementTransactedElement;
class UActorComponent;
class UObject;
struct FTypedElementHandle;
struct FTypedElementIsSelectedOptions;

UCLASS()
class UNREALED_API UComponentElementEditorSelectionInterface : public UComponentElementSelectionInterface
{
	GENERATED_BODY()

public:
	virtual bool IsElementSelected(const FTypedElementHandle& InElementHandle, const FTypedElementListConstPtr& InSelectionSet, const FTypedElementIsSelectedOptions& InSelectionOptions) override;
	virtual bool ShouldPreventTransactions(const FTypedElementHandle& InElementHandle) override;
	virtual TUniquePtr<ITypedElementTransactedElement> CreateTransactedElementImpl() override;

	static bool IsComponentSelected(const UActorComponent* InComponent, FTypedElementListConstRef InSelectionSet, const FTypedElementIsSelectedOptions& InSelectionOptions);
};
