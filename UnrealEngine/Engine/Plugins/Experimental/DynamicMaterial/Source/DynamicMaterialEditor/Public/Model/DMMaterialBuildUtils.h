// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Model/IDMMaterialBuildUtilsInterface.h"

struct FDMMaterialBuildState;

struct DYNAMICMATERIALEDITOR_API FDMMaterialBuildUtils : public IDMMaterialBuildUtilsInterface
{
	FDMMaterialBuildUtils(FDMMaterialBuildState& InBuildState);

	UMaterialExpression* CreateDefaultExpression() const;

	virtual UMaterialExpression* CreateExpression(TSubclassOf<UMaterialExpression> InExpressionClass, const FString& InComment,
		UObject* InAsset = nullptr) const override;

	virtual UMaterialExpression* CreateExpressionParameter(TSubclassOf<UMaterialExpression> InExpressionClass, FName InParameterName,
		const FString& InComment, UObject* InAsset = nullptr) const override;

	template<typename InExpressionClass
		UE_REQUIRES(std::derived_from<InExpressionClass, UMaterialExpression>)>
	InExpressionClass* CreateExpression(const FString& InComment, UObject* InAsset = nullptr) const
	{
		return Cast<InExpressionClass>(CreateExpression(TSubclassOf<UMaterialExpression>(InExpressionClass::StaticClass()), InComment, InAsset));
	}

	template<typename InExpressionClass
		UE_REQUIRES(std::derived_from<InExpressionClass, UMaterialExpression>)>
	InExpressionClass* CreateExpressionParameter(FName InParameterName, const FString& InComment, UObject* InAsset = nullptr) const
	{
		return Cast<InExpressionClass>(CreateExpressionParameter(TSubclassOf<UMaterialExpression>(InExpressionClass::StaticClass()), InParameterName, InComment, InAsset));
	}

	virtual TArray<UMaterialExpression*> CreateExpressionInputs(const TArray<FDMMaterialStageConnection>& InInputConnectionMap,
		int32 InStageSourceInputIdx, const TArray<UDMMaterialStageInput*>& InStageInputs, int32& OutOutputIndex, 
		int32& OutOutputChannel) const override;

	virtual TArray<UMaterialExpression*> CreateExpressionInput(UDMMaterialStageInput* InInput) const override;

	virtual UMaterialExpressionComponentMask* CreateExpressionBitMask(UMaterialExpression* InExpression, int32 InOutputIndex,
		int32 InOutputChannels) const override;

	virtual UMaterialExpressionAppendVector* CreateExpressionAppend(UMaterialExpression* InExpressionA, int32 InOutputIndexA, 
		UMaterialExpression* InExpressionB, int32 InOutputIndexB) const override;

	virtual void UpdatePreviewMaterial(UMaterialExpression* InLastExpression, int32 InOutputIndex, int32 InOutputChannel,
		int32 InSize) const override;

protected:
	FDMMaterialBuildState& BuildState;
};
