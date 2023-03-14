// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "GLTFMaterialAnalysis.h"

#include "Materials/MaterialInstance.h"

#include "GLTFMaterialAnalyzer.generated.h"

UCLASS(NotBlueprintType, Transient)
class GLTFMATERIALANALYZER_API UGLTFMaterialAnalyzer : public UMaterialInstance
{
	GENERATED_BODY()

public:

	static void AnalyzeMaterialPropertyEx(const UMaterialInterface* InMaterial, const EMaterialProperty& InProperty, const FString& InCustomOutput, FGLTFMaterialAnalysis& OutAnalysis);
	
private:

	void ResetToDefaults();

	UMaterialExpressionCustomOutput* GetCustomOutputExpression() const;

	virtual FMaterialResource* GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel) override;

	virtual int32 CompilePropertyEx(FMaterialCompiler* Compiler, const FGuid& AttributeID) override;

	virtual bool IsPropertyActive(EMaterialProperty InProperty) const override;

	EMaterialProperty Property = MP_MAX;
	FString CustomOutput;

	// ReSharper disable once CppUE4ProbableMemoryIssuesWithUObject
	UMaterialInterface* Material = nullptr;

	FGLTFMaterialAnalysis* Analysis = nullptr;
};
