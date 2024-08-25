// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Templates/SharedPointer.h"
#include "AvaTagCollectionMenuContext.generated.h"

class IPropertyHandle;

UCLASS()
class UAvaTagCollectionMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<IPropertyHandle> TagCollectionHandleWeak;
};
