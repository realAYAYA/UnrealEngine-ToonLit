// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TG_SystemTypes.h"


#include "TG_Parameter.generated.h"




USTRUCT()
struct FTG_ParameterInfo
{
	GENERATED_BODY()
public:
	FTG_Id Id;
	FName Name;
};

UCLASS()
class UTG_Parameters : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "TextureGraph", meta=(EditFixedOrder, ShowOnlyInnerProperties))
	TArray<FTG_ParameterInfo> Parameters;

	TObjectPtr<class UTG_Graph> TextureGraph;

};

