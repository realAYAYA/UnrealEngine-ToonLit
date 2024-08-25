// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "Field/FieldSystemTypes.h"
#include "MaterialExpressionSamplePhysicsField.generated.h"

/**
 * Material expresions to sample the global field
 */

UCLASS()
class UMaterialExpressionSamplePhysicsVectorField : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to current world position if not specified"))
	FExpressionInput WorldPosition;

	/** Defines the reference space for the WorldPosition input. */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionSamplePhysicsVectorField)
	EPositionOrigin WorldPositionOriginType = EPositionOrigin::Absolute;

	/** Target Type to be accessed */
	UPROPERTY(EditAnywhere, Category = "Field Setup", meta = (DisplayName = "Target Type", ShowAsInputPin = "Advanced"))
	TEnumAsByte<EFieldVectorType> FieldTarget;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS()
class UMaterialExpressionSamplePhysicsScalarField : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to current world position if not specified"))
	FExpressionInput WorldPosition;

	/** Defines the reference space for the WorldPosition input. */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionSamplePhysicsScalarField)
	EPositionOrigin WorldPositionOriginType = EPositionOrigin::Absolute;

	/** Target Type to be accessed */
	UPROPERTY(EditAnywhere, Category = "Field Setup", meta = (DisplayName = "Target Type", ShowAsInputPin = "Advanced"))
	TEnumAsByte<EFieldScalarType> FieldTarget;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS()
class UMaterialExpressionSamplePhysicsIntegerField : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to current world position if not specified"))
	FExpressionInput WorldPosition;

	/** Defines the reference space for the WorldPosition input. */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionSamplePhysicsIntegerField)
	EPositionOrigin WorldPositionOriginType = EPositionOrigin::Absolute;

	/** Target Type to be accessed */
	UPROPERTY(EditAnywhere, Category = "Field Setup", meta = (DisplayName = "Target Type", ShowAsInputPin = "Advanced"))
	TEnumAsByte<EFieldIntegerType> FieldTarget;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
#endif
	//~ End UMaterialExpression Interface
};
