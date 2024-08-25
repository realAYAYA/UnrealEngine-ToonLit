// Copyright Epic Games, Inc. All Rights Reserve

#pragma once

#include "CoreMinimal.h"

#include "Expressions/TG_Expression.h"

#include "TG_Expression_InputParam.generated.h"

UCLASS(Abstract)
class TEXTUREGRAPH_API UTG_Expression_InputParam : public UTG_Expression
{
	GENERATED_BODY()

protected:
	// these 2 methods need to be implemented to provide concrete signatures for derived InputParam expression classes
	virtual FTG_SignaturePtr BuildInputParameterSignature() const { return nullptr; }
	virtual FTG_SignaturePtr BuildInputConstantSignature() const { return nullptr; }
public:

#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Node can be converted from Param to Constant
	UPROPERTY(Setter)
	bool bIsConstant = false;
	void SetbIsConstant(bool InIsConstant);
	void ToggleIsConstant();

};


#define TG_DECLARE_INPUT_PARAM_EXPRESSION(Category) \
	protected: virtual FTG_SignaturePtr BuildInputParameterSignature() const override; \
	protected: virtual FTG_SignaturePtr BuildInputConstantSignature() const override; \
	public:	virtual FTG_SignaturePtr GetSignature() const override { if (bIsConstant) { \
			static FTG_SignaturePtr ConstantSignature = BuildInputConstantSignature(); return ConstantSignature; \
		} else { \
			static FTG_SignaturePtr ParameterSignature = BuildInputParameterSignature(); return ParameterSignature; \
		} } \
	public: virtual FName GetCategory() const override { return Category; }
