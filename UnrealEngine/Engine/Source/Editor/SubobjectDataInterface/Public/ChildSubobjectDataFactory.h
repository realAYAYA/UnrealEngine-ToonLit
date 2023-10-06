// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "SubobjectDataFactory.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

struct FSubobjectData;

/**
* This is the default subobject data factory that will provide a simple 
*/
class FChildSubobjectDataFactory : public ISubobjectDataFactory
{
public:

	// Begin ISubobjectDataFactory interface
	virtual FName GetID() const { return TEXT("ChildSubobjectFactory"); }
	virtual TSharedPtr<FSubobjectData> CreateSubobjectData(const FCreateSubobjectParams& Params) override;
	virtual bool ShouldCreateSubobjectData(const FCreateSubobjectParams& Params) const override;
	// End ISubobjectDataFactory
};