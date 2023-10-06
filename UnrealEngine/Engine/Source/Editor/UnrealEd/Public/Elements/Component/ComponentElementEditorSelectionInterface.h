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

UCLASS(MinimalAPI)
class UComponentElementEditorSelectionInterface : public UComponentElementSelectionInterface
{
	GENERATED_BODY()

public:
	UNREALED_API virtual bool IsElementSelected(const FTypedElementHandle& InElementHandle, const FTypedElementListConstPtr& InSelectionSet, const FTypedElementIsSelectedOptions& InSelectionOptions) override;
	UNREALED_API virtual bool ShouldPreventTransactions(const FTypedElementHandle& InElementHandle) override;
	UNREALED_API virtual TUniquePtr<ITypedElementTransactedElement> CreateTransactedElementImpl() override;

	static UNREALED_API bool IsComponentSelected(const UActorComponent* InComponent, FTypedElementListConstRef InSelectionSet, const FTypedElementIsSelectedOptions& InSelectionOptions);
};
