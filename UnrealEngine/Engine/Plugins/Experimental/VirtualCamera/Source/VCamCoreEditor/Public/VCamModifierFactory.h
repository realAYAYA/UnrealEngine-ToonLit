// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VCamObjectWithInputFactory.h"

#include "VCamModifierFactory.generated.h"

UCLASS()
class VCAMCOREEDITOR_API UVCamModifierFactory : public UVCamObjectWithInputFactory
{
	GENERATED_BODY()
public:
	UVCamModifierFactory();
	
	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTip() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	//~ Begin UFactory Interface
	
};