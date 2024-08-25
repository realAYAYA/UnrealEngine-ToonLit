// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Expressions/TG_Expression_Variant.h"

#include "TG_Expression_Maths_TwoInputs.generated.h"

//////////////////////////////////////////////////////////////////////////
/// Basic math op expression
//////////////////////////////////////////////////////////////////////////
UCLASS(Abstract)
class TEXTUREGRAPH_API UTG_Expression_BasicMath_Op : public UTG_Expression_Variant
{
	GENERATED_BODY()

public:
	// First input to the math operation [Input1 <operation> Input2]
	UPROPERTY(meta = (TGType = "TG_Input", PinDisplayName = "A"))
	FTG_Variant							Input1 = 0;

	// Second input to the math operation [Input1 <operation> Input2]
	UPROPERTY(meta = (TGType = "TG_Input", PinDisplayName = "B"))
	FTG_Variant							Input2 = 0;

	virtual FName						GetCategory() const override { return TG_Category::Maths; } 

protected:
	virtual std::vector<FTG_Variant>	GetEvaluateArgs() override { return std::vector<FTG_Variant>({ Input1, Input2 }); }
};

//////////////////////////////////////////////////////////////////////////
/// Multiply
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Multiply : public UTG_Expression_BasicMath_Op
{
	GENERATED_BODY()
	
public:
	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Multiplies the two inputs, A \u2192 B.")); } 

protected:
	virtual float						EvaluateScalar_WithValue(FTG_EvaluationContext* InContext, const float* const Value, size_t Count) override;
	virtual FTG_Texture					EvaluateTexture(FTG_EvaluationContext* InContext) override;
};

//////////////////////////////////////////////////////////////////////////
/// Divide
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Divide : public UTG_Expression_BasicMath_Op
{
	GENERATED_BODY()
	
public:
	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Divides the two inputs, A \u00f7 B.")); } 

protected:
	virtual float						EvaluateScalar_WithValue(FTG_EvaluationContext* InContext, const float* const Value, size_t Count) override;
	virtual FTG_Texture					EvaluateTexture(FTG_EvaluationContext* InContext) override;
};

//////////////////////////////////////////////////////////////////////////
/// Add
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Add : public UTG_Expression_BasicMath_Op
{
	GENERATED_BODY()
	
public:
	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Adds up the two inputs, A \u002b B.")); } 

protected:
	virtual float						EvaluateScalar_WithValue(FTG_EvaluationContext* InContext, const float* const Value, size_t Count) override;
	virtual FTG_Texture					EvaluateTexture(FTG_EvaluationContext* InContext) override;
};

//////////////////////////////////////////////////////////////////////////
/// Subtract
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Subtract : public UTG_Expression_BasicMath_Op
{
	GENERATED_BODY()
	
public:
	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Subtracts the two inputs, A \u2212 B.")); } 

protected:
	virtual float						EvaluateScalar_WithValue(FTG_EvaluationContext* InContext, const float* const Value, size_t Count) override;
	virtual FTG_Texture					EvaluateTexture(FTG_EvaluationContext* InContext) override;
};

//////////////////////////////////////////////////////////////////////////
/// Dot product of two vectors
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Dot : public UTG_Expression_BasicMath_Op
{
	GENERATED_BODY()
	
public:
	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Dot product of inputs. A <dot> B.")); } 

protected:
	virtual float						EvaluateScalar_WithValue(FTG_EvaluationContext* InContext, const float* const Value, size_t Count) override;
	virtual FVector4f					EvaluateVector_WithValue(FTG_EvaluationContext* InContext, const FVector4f* const Value, size_t Count) override;
	virtual FTG_Texture					EvaluateTexture(FTG_EvaluationContext* InContext) override;
};

//////////////////////////////////////////////////////////////////////////
/// Cross product of two vectors
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Cross : public UTG_Expression_BasicMath_Op
{
	GENERATED_BODY()
	
public:
	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Cross product of inputs. A <cross> B.")); } 

protected:
	virtual float						EvaluateScalar_WithValue(FTG_EvaluationContext* InContext, const float* const Value, size_t Count) override;
	virtual FVector4f					EvaluateVector_WithValue(FTG_EvaluationContext* InContext, const FVector4f* const Value, size_t Count) override;
	virtual FTG_Texture					EvaluateTexture(FTG_EvaluationContext* InContext) override;
};

//////////////////////////////////////////////////////////////////////////
/// Power
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Pow : public UTG_Expression_Variant
{
	GENERATED_BODY()
	
	// The base of the power function
	UPROPERTY(meta = (TGType = "TG_Input"))
	FTG_Variant							Base = 0;

	// The exponent
	UPROPERTY(meta = (TGType = "TG_Input"))
	FTG_Variant							Exponent = 1; // Default exponent to 1 for a noop behavior

public:
	virtual FText						GetTooltipText() const override { return FText::FromString(TEXT("Calculates Pow(Base, Exponent).")); } 
	virtual FName						GetCategory() const override { return TG_Category::Maths; } 

protected:
	virtual std::vector<FTG_Variant>	GetEvaluateArgs() override { return std::vector<FTG_Variant>({ Base, Exponent }); }
	virtual float						EvaluateScalar_WithValue(FTG_EvaluationContext* InContext, const float* const Value, size_t Count) override;
	virtual FTG_Texture					EvaluateTexture(FTG_EvaluationContext* InContext) override;
};

