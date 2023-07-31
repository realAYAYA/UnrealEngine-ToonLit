// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLandscapeVisibilityMask.generated.h"

class UTexture;
struct FMaterialParameterInfo;

UCLASS(collapseCategories, hideCategories=Object)
class LANDSCAPE_API UMaterialExpressionLandscapeVisibilityMask : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

public:

	static FName ParameterName;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	/**
	 * Gets the landscape layer names
	 */
	virtual void GetLandscapeLayerNames(TArray<FName>& OutLayers) const override;
#endif
	virtual UObject* GetReferencedTexture() const override;
	virtual bool CanReferenceTexture() const override { return true; }
	//~ End UMaterialExpression Interface
};



