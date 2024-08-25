// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionAbsorptionMediumMaterialOutput.generated.h"

/** Material output expression for setting absorption properties of solid refractive glass (for the Path Tracer Only). */
UCLASS(MinimalAPI, collapsecategories, hidecategories = Object)
class UMaterialExpressionAbsorptionMediumMaterialOutput : public UMaterialExpressionCustomOutput
{
	GENERATED_UCLASS_BODY()

	/** Input for the transmittance color seen after travelling a distance of 100 units into the object. Valid range is [0,1]. */
	UPROPERTY()
	FExpressionInput TransmittanceColor;

public:
#if WITH_EDITOR
	//~ Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
	//~ End UMaterialExpression Interface
#endif

	//~ Begin UMaterialExpressionCustomOutput Interface
	virtual int32 GetNumOutputs() const override;
	virtual FString GetFunctionName() const override;
	virtual FString GetDisplayName() const override;
#if WITH_EDITOR
	virtual UE::Shader::EValueType GetCustomOutputType(int32 OutputIndex) const override;
#endif
	//~ End UMaterialExpressionCustomOutput Interface
};
