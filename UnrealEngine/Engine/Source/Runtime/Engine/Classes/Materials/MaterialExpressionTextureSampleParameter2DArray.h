// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "MaterialExpressionTextureSampleParameter2DArray.generated.h"

UCLASS(collapsecategories, hidecategories = Object, MinimalAPI)
class UMaterialExpressionTextureSampleParameter2DArray : public UMaterialExpressionTextureSampleParameter
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	// Begin UMaterialExpression Interface
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	// End UMaterialExpression Interface
#endif

	// Begin UMaterialExpressionTextureSampleParameter Interface
#if WITH_EDITOR
	virtual bool TextureIsValid( UTexture* InTexture, FString& OutMessage) override;
#endif
	virtual const TCHAR* GetRequirements();
	// End UMaterialExpressionTextureSampleParameter Interface
};