// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "SavePackageUtilitiesCommandlet.generated.h"

/*
 * Commandlet used to validate the package saving mechanism. 
 * It can currently compare two back to back saves of a package (or folder of packages)
 */
UCLASS()
class USavePackageUtilitiesCommandlet
	: public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

private:
	void InitParameters(const FString& Params);

	TArray<FString> PackageNames;
	class ITargetPlatform* TargetPlatform = nullptr;
};
