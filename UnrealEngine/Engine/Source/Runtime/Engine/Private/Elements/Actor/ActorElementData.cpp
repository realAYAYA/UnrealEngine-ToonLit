// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Actor/ActorElementData.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "UObject/Stack.h"

UE_DEFINE_TYPED_ELEMENT_DATA_RTTI(FActorElementData);

namespace ActorElementDataUtil
{

AActor* GetActorFromHandle(const FTypedElementHandle& InHandle, const bool bSilent)
{
	const FActorElementData* ActorElement = InHandle.GetData<FActorElementData>(bSilent);
	return ActorElement ? ActorElement->Actor : nullptr;
}

AActor* GetActorFromHandleChecked(const FTypedElementHandle& InHandle)
{
	const FActorElementData& ActorElement = InHandle.GetDataChecked<FActorElementData>();
	return ActorElement.Actor;
}

} // namespace ActorElementDataUtil
