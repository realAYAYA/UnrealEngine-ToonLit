// Copyright Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// UCurveLinearColorAtlasFactory
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "CurveLinearColorAtlasFactory.generated.h"

UCLASS(hidecategories=(Object, Texture))
class UCurveLinearColorAtlasFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	/** Width of the texture render target */
	UPROPERTY(meta=(ToolTip="Width of the texture render target"))
	int32 Width;

	/** Height of the texture render target */
	UPROPERTY(meta=(ToolTip="Height of the texture render target"))
	int32 Height;

	/** Pixel format of the texture render target */
	UPROPERTY(meta=(ToolTip="Pixel format of the texture render target"))
	uint8 Format;

	virtual FText GetDisplayName() const override;

	uint32 GetMenuCategories() const override;

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	
};



