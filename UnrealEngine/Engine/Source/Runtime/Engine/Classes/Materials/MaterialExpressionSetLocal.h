// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionSetLocal.generated.h"

UCLASS(collapsecategories, hidecategories = Object, MinimalAPI, meta = (MaterialControlFlow))
class UMaterialExpressionSetLocal : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionExecOutput Exec;

	UPROPERTY()
	FExpressionInput Value;

	UPROPERTY(EditAnywhere, Category = MaterialLocals)
	FName LocalName;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool HasExecInput() override { return true; }
	virtual bool GenerateHLSLStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope) const override;
#endif
	//~ End UMaterialExpression Interface
};
