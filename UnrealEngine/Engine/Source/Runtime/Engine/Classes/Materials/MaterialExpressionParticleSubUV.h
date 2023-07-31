// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MaterialExpressionParticleSubUV.generated.h"

UCLASS(MinimalAPI)
class UMaterialExpressionParticleSubUV : public UMaterialExpressionTextureSample
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionParticleSubUV)
	uint32 bBlend:1;


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual int32 GetWidth() const override;	
	virtual int32 GetLabelPadding() override { return 8; }
#endif
	//~ End UMaterialExpression Interface
};



