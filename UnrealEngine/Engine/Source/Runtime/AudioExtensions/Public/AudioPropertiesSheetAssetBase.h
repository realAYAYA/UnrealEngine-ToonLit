// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"

#include "AudioPropertiesSheetAssetBase.generated.h"

class UAudioPropertiesBindings;

UCLASS(Abstract)
class AUDIOEXTENSIONS_API UAudioPropertiesSheetAssetBase : public UObject
{
	GENERATED_BODY()

public: 
	virtual bool CopyToObjectProperties(TObjectPtr<UObject> TargetObject, const TObjectPtr<const UAudioPropertiesBindings> PropertiesBindings) const { return false; };
};