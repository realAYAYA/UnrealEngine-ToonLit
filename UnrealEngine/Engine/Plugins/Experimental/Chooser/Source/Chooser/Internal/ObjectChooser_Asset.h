// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IObjectChooser.h"
#include "ObjectChooser_Asset.generated.h"

USTRUCT(DisplayName = "Asset")
struct CHOOSER_API FAssetChooser : public FObjectChooserBase
{
	GENERATED_BODY()
	
	// FObjectChooserBase interface
	virtual UObject* ChooseObject(FChooserEvaluationContext& Context) const final override;
public: 
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TObjectPtr<UObject> Asset;
};

// deprecated class for upgrading old data
UCLASS(ClassGroup = "LiveLink", deprecated)
class CHOOSER_API UDEPRECATED_ObjectChooser_Asset : public UObject, public IObjectChooser
{
	GENERATED_BODY()
	
public:
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const override
	{
		OutInstancedStruct.InitializeAs(FAssetChooser::StaticStruct());
		FAssetChooser& AssetChooser = OutInstancedStruct.GetMutable<FAssetChooser>();
		AssetChooser.Asset = Asset;
	}
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TObjectPtr<UObject> Asset;
};