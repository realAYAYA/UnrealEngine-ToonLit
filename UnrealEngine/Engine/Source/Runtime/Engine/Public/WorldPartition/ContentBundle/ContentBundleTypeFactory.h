// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "ContentBundleTypeFactory.generated.h"

class FContentBundleClient;
class UContentBundleDescriptor;

UCLASS(MinimalAPI)
class UContentBundleTypeFactory : public UObject
{
	GENERATED_BODY()

public:
	ENGINE_API virtual TSharedPtr<FContentBundleClient> CreateClient(const UContentBundleDescriptor* Descriptor, const FString& ClientDisplayName);
};
