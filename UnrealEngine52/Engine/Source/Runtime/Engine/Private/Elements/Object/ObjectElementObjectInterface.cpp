// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Object/ObjectElementObjectInterface.h"
#include "Elements/Object/ObjectElementData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ObjectElementObjectInterface)

UObject* UObjectElementObjectInterface::GetObject(const FTypedElementHandle& InElementHandle)
{
	return ObjectElementDataUtil::GetObjectFromHandle(InElementHandle);
}

