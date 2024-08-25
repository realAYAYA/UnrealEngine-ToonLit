// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionCustom.generated.h"

struct FPropertyChangedEvent;

UENUM()
enum ECustomMaterialOutputType : int
{
	CMOT_Float1,
	CMOT_Float2,
	CMOT_Float3,
	CMOT_Float4,
	CMOT_MaterialAttributes,
	CMOT_MAX,
};

USTRUCT()
struct FCustomInput
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=CustomInput)
	FName InputName;

	UPROPERTY()
	FExpressionInput Input;
};

USTRUCT()
struct FCustomOutput
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomOutput)
	FName OutputName;

	UPROPERTY(EditAnywhere, Category = CustomOutput)
	TEnumAsByte<enum ECustomMaterialOutputType> OutputType = ECustomMaterialOutputType::CMOT_Float1;
};

USTRUCT()
struct FCustomDefine
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomInput)
	FString DefineName;

	UPROPERTY(EditAnywhere, Category = CustomInput)
	FString DefineValue;
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionCustom : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustom, meta=(MultiLine=true))
	FString Code;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustom)
	TEnumAsByte<enum ECustomMaterialOutputType> OutputType = ECustomMaterialOutputType::CMOT_Float1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustom)
	FString Description;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustom)
	TArray<struct FCustomInput> Inputs;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionCustom)
	TArray<struct FCustomOutput> AdditionalOutputs;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustom)
	TArray<struct FCustomDefine> AdditionalDefines;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustom)
	TArray<FString> IncludeFilePaths;

	UPROPERTY(VisibleAnywhere, Category=MaterialExpressionCustom)
	bool ShowCode;

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void RebuildOutputs();
#endif // WITH_EDITOR
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	//~ End UObject Interface.
	
	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FText GetCreationName() const override { return FText::FromString(TEXT("Custom")); }
	virtual TArrayView<FExpressionInput*> GetInputsView() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_Unknown;}
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const override;
	virtual void GetIncludeFilePaths(TSet<FString>& OutIncludeFilePaths) const override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
};



