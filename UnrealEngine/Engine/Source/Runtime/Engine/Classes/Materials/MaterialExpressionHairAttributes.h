// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionHairAttributes.generated.h"

UCLASS(MinimalAPI)
class UMaterialExpressionHairAttributes : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** If enabled, this nodes outputs a tangent space tangent, otherwise it outputs a world space tangent. */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionHairAttributes)
	uint32 bUseTangentSpace : 1;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};
