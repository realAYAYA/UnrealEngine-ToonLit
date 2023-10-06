// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectIdentifier.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectIdentifier)


FCustomizableObjectIdPair::FCustomizableObjectIdPair(FString ObjectGroupName, FString ObjectName)
	: CustomizableObjectGroupName(ObjectGroupName)
	, CustomizableObjectName(ObjectName)
{
}

FCustomizableObjectIdentifier::FCustomizableObjectIdentifier()
{
}

FCustomizableObjectIdentifier::FCustomizableObjectIdentifier(FString ObjectGroupName, FString ObjectName)
	: CustomizableObjectGroupName(ObjectGroupName)
	, CustomizableObjectName(ObjectName)
{
}

