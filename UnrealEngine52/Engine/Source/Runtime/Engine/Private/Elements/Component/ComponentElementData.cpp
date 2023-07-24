// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Component/ComponentElementData.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "UObject/Stack.h"

UE_DEFINE_TYPED_ELEMENT_DATA_RTTI(FComponentElementData);

namespace ComponentElementDataUtil
{

UActorComponent* GetComponentFromHandle(const FTypedElementHandle& InHandle, const bool bSilent)
{
	const FComponentElementData* ComponentElement = InHandle.GetData<FComponentElementData>(bSilent);
	return ComponentElement ? ComponentElement->Component : nullptr;
}

UActorComponent* GetComponentFromHandleChecked(const FTypedElementHandle& InHandle)
{
	const FComponentElementData& ComponentElement = InHandle.GetDataChecked<FComponentElementData>();
	return ComponentElement.Component;
}

} // namespace ComponentElementDataUtil
