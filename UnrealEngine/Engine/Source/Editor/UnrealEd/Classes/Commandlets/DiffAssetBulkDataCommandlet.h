// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "DiffAssetBulkDataCommandlet.generated.h"

// Detailed comments in DiffAssetBulkDataCommandlet.cpp
UCLASS()
class UDiffAssetBulkDataCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface

private:

};
