// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IObjectChooser.h"
#include "ObjectChooser_Class.generated.h"

USTRUCT(DisplayName = "Class")
struct CHOOSER_API FClassChooser : public FObjectChooserBase
{
	GENERATED_BODY()
	
	// FObjectChooserBase interface
	virtual UObject* ChooseObject(FChooserEvaluationContext& Context) const final override;
public: 
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TObjectPtr<UClass> Class;
};
