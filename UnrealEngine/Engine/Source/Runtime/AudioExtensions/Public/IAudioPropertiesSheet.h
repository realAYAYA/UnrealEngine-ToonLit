// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"

#include "IAudioPropertiesSheet.generated.h"


UCLASS(Abstract)
class AUDIOEXTENSIONS_API UAudioPropertySheetBaseAsset : public UObject
{
	GENERATED_BODY()

public: 
	virtual bool CopyToObjectProperties(TObjectPtr<UObject> TargetObject) { return false; };
};