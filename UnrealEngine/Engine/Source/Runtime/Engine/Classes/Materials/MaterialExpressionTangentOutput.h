// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "MaterialExpressionTangentOutput.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionTangentOutput : public UMaterialExpressionCustomOutput
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "true"))
	FExpressionInput Input;

#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetInputType(int32 InputIndex) override { return MCT_Float; }
	virtual UE::Shader::EValueType GetCustomOutputType(int32 OutputIndex) const override { return UE::Shader::EValueType::Float3; }
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif

	virtual int32 GetNumOutputs() const override { return 1; }
	virtual FString GetFunctionName() const override { return TEXT("GetTangentOutput"); }
	virtual FString GetDisplayName() const override { return TEXT("CustomEyeTangent"); }
};
