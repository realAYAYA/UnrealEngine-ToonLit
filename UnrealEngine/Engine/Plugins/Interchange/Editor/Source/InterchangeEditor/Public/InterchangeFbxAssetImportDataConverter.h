// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeAssetImportData.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeFbxAssetImportDataConverter.generated.h"

UCLASS()
class INTERCHANGEEDITOR_API UInterchangeFbxAssetImportDataConverter : public UInterchangeAssetImportDataConverterBase
{
	GENERATED_BODY()

public:
	virtual bool ConvertImportData(UObject* Object, const FString& ToExtension) const override;
	virtual bool ConvertImportData(const UObject* SourceImportData, UObject** DestinationImportDataClass) const override;
};
