// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPathTracingRayTypeSwitch.generated.h"

UCLASS()
class UMaterialExpressionPathTracingRayTypeSwitch : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Used for camera rays (or for non-path traced shading) */
	UPROPERTY()
	FExpressionInput Main;

	/** Used by the path tracer on shadow rays (this only applies for non-opaque blend modes) */
	UPROPERTY()
	FExpressionInput Shadow;

	/** Used by the path tracer on indirect diffuse rays */
	UPROPERTY()
	FExpressionInput IndirectDiffuse;

	/** Used by the path tracer on indirect specular rays */
	UPROPERTY()
	FExpressionInput IndirectSpecular;

	/** Used by the path tracer on indirect volume rays */
	UPROPERTY()
	FExpressionInput IndirectVolume;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override { return MCT_Unknown; }
	virtual uint32 GetOutputType(int32 OutputIndex) override { return MCT_Unknown; }
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};
