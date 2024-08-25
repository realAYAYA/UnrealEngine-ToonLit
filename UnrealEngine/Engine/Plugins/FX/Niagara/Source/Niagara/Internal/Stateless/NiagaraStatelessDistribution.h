// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/RichCurve.h"
#include "NiagaraStatelessCommon.h"
#include "NiagaraParameterBinding.h"

#include "NiagaraStatelessDistribution.generated.h"

UENUM()
enum class ENiagaraDistributionMode
{
	Binding,
	UniformConstant,
	NonUniformConstant,
	UniformRange,
	NonUniformRange,
	UniformCurve,
	NonUniformCurve
};

USTRUCT()
struct FNiagaraDistributionBase
{
	GENERATED_BODY()

	virtual ~FNiagaraDistributionBase() = default;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	ENiagaraDistributionMode Mode = ENiagaraDistributionMode::UniformConstant;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FNiagaraVariableBase ParameterBinding;

	bool IsBinding() const { return Mode == ENiagaraDistributionMode::Binding; }
	bool IsConstant() const { return Mode == ENiagaraDistributionMode::UniformConstant || Mode == ENiagaraDistributionMode::NonUniformConstant; }
	bool IsUniform() const { return Mode == ENiagaraDistributionMode::UniformConstant || Mode == ENiagaraDistributionMode::UniformRange; }
	bool IsCurve() const { return Mode == ENiagaraDistributionMode::UniformCurve || Mode == ENiagaraDistributionMode::NonUniformCurve; }
	bool IsRange() const { return Mode == ENiagaraDistributionMode::UniformRange || Mode == ENiagaraDistributionMode::NonUniformRange; }

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<float> ChannelConstantsAndRanges;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<FRichCurve> ChannelCurves;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	int32 MaxLutSampleCount = 64;

	virtual bool AllowBinding() const { return true; }
	virtual bool AllowCurves() const { return true; }
	virtual bool DisplayAsColor() const { return false; }
	virtual void UpdateValuesFromDistribution() { }

	virtual FNiagaraTypeDefinition GetBindingTypeDef() const { return FNiagaraTypeDefinition(); }

	static void PostEditChangeProperty(UObject* OwnerObject, FPropertyChangedEvent& PropertyChangedEvent);
#endif
};

USTRUCT()
struct FNiagaraDistributionRangeInt
{
	GENERATED_BODY()

	FNiagaraDistributionRangeInt() = default;
	explicit FNiagaraDistributionRangeInt(int32 ConstantValue) { InitConstant(ConstantValue); }

	UPROPERTY(EditAnywhere, Category = "Parameters")
	ENiagaraDistributionMode Mode = ENiagaraDistributionMode::UniformConstant;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FNiagaraVariableBase ParameterBinding;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	int32 Min = 0;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	int32 Max = 0;

	NIAGARA_API void InitConstant(int32 Value);
	NIAGARA_API FNiagaraStatelessRangeInt CalculateRange(const int32 Default = 0) const;

	bool IsBinding() const { return Mode == ENiagaraDistributionMode::Binding; }
	bool IsConstant() const { return Mode == ENiagaraDistributionMode::UniformConstant || Mode == ENiagaraDistributionMode::NonUniformConstant; }
	bool IsUniform() const { return Mode == ENiagaraDistributionMode::UniformConstant || Mode == ENiagaraDistributionMode::UniformRange; }
	bool IsCurve() const { return Mode == ENiagaraDistributionMode::UniformCurve || Mode == ENiagaraDistributionMode::NonUniformCurve; }
	bool IsRange() const { return Mode == ENiagaraDistributionMode::UniformRange || Mode == ENiagaraDistributionMode::NonUniformRange; }

#if WITH_EDITORONLY_DATA
	FNiagaraTypeDefinition GetBindingTypeDef() const { return FNiagaraTypeDefinition::GetIntDef(); }
#endif
};

USTRUCT()
struct FNiagaraDistributionRangeFloat : public FNiagaraDistributionBase
{
	GENERATED_BODY()

	FNiagaraDistributionRangeFloat() = default;
	explicit FNiagaraDistributionRangeFloat(float ConstantValue) { InitConstant(ConstantValue); }

	UPROPERTY(EditAnywhere, Category = "Parameters")
	float Min = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	float Max = 0.0f;

	NIAGARA_API void InitConstant(float Value);
	NIAGARA_API FNiagaraStatelessRangeFloat CalculateRange(const float Default = 0.0f) const;

#if WITH_EDITORONLY_DATA
	virtual bool AllowCurves() const override { return false; }
	NIAGARA_API virtual void UpdateValuesFromDistribution() override;
	virtual FNiagaraTypeDefinition GetBindingTypeDef() const { return FNiagaraTypeDefinition::GetFloatDef(); }
#endif
};

USTRUCT()
struct FNiagaraDistributionRangeVector2 : public FNiagaraDistributionBase
{
	GENERATED_BODY()

	FNiagaraDistributionRangeVector2() = default;
	explicit FNiagaraDistributionRangeVector2(const FVector2f& ConstantValue) { InitConstant(ConstantValue); }

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector2f Min = FVector2f::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector2f Max = FVector2f::ZeroVector;

	NIAGARA_API void InitConstant(const FVector2f& Value);
	NIAGARA_API FNiagaraStatelessRangeVector2 CalculateRange(const FVector2f& Default = FVector2f::ZeroVector) const;

#if WITH_EDITORONLY_DATA
	virtual bool AllowCurves() const override { return false; }
	virtual void UpdateValuesFromDistribution() override;
	virtual FNiagaraTypeDefinition GetBindingTypeDef() const { return FNiagaraTypeDefinition::GetVec2Def(); }
#endif
};

USTRUCT()
struct FNiagaraDistributionRangeVector3 : public FNiagaraDistributionBase
{
	GENERATED_BODY()

	FNiagaraDistributionRangeVector3() = default;
	explicit FNiagaraDistributionRangeVector3(const FVector3f& ConstantValue) { InitConstant(ConstantValue); }

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f Min = FVector3f::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f Max = FVector3f::ZeroVector;

	NIAGARA_API void InitConstant(const FVector3f& Value);
	NIAGARA_API FNiagaraStatelessRangeVector3 CalculateRange(const FVector3f& Default = FVector3f::ZeroVector) const;

#if WITH_EDITORONLY_DATA
	virtual bool AllowCurves() const override { return false; }
	virtual void UpdateValuesFromDistribution() override;
	virtual FNiagaraTypeDefinition GetBindingTypeDef() const { return FNiagaraTypeDefinition::GetVec3Def(); }
#endif
};

USTRUCT()
struct FNiagaraDistributionRangeColor : public FNiagaraDistributionBase
{
	GENERATED_BODY()

	FNiagaraDistributionRangeColor() = default;
	explicit FNiagaraDistributionRangeColor(const FLinearColor& ConstantValue) { InitConstant(ConstantValue); }

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FLinearColor Min = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FLinearColor Max = FLinearColor::White;

	NIAGARA_API void InitConstant(const FLinearColor& Value);
	NIAGARA_API FNiagaraStatelessRangeColor CalculateRange(const FLinearColor& Default = FLinearColor::White) const;

#if WITH_EDITORONLY_DATA
	virtual bool AllowCurves() const override { return false; }
	virtual bool DisplayAsColor() const { return true; }
	virtual void UpdateValuesFromDistribution() override;
	virtual FNiagaraTypeDefinition GetBindingTypeDef() const { return FNiagaraTypeDefinition::GetColorDef(); }
#endif
};

USTRUCT()
struct FNiagaraDistributionFloat : public FNiagaraDistributionBase
{
	GENERATED_BODY()

	FNiagaraDistributionFloat() = default;
	explicit FNiagaraDistributionFloat(float ConstantValue) { InitConstant(ConstantValue); }

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<float> Values;

	NIAGARA_API void InitConstant(float Value);
	NIAGARA_API FNiagaraStatelessRangeFloat CalculateRange(const float Default = 0.0f) const;

#if WITH_EDITORONLY_DATA
	virtual void UpdateValuesFromDistribution() override;
	virtual FNiagaraTypeDefinition GetBindingTypeDef() const { return FNiagaraTypeDefinition::GetFloatDef(); }
#endif
};

USTRUCT()
struct FNiagaraDistributionVector2 : public FNiagaraDistributionBase
{
	GENERATED_BODY()

	FNiagaraDistributionVector2() = default;
	explicit FNiagaraDistributionVector2(const float ConstantValue) { InitConstant(ConstantValue); }
	explicit FNiagaraDistributionVector2(const FVector2f& ConstantValue) { InitConstant(ConstantValue); }

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<FVector2f> Values;

	NIAGARA_API void InitConstant(const float Value);
	NIAGARA_API void InitConstant(const FVector2f& Value);
	NIAGARA_API FNiagaraStatelessRangeVector2 CalculateRange(const FVector2f& Default = FVector2f::ZeroVector) const;

#if WITH_EDITORONLY_DATA
	virtual void UpdateValuesFromDistribution() override;
	virtual FNiagaraTypeDefinition GetBindingTypeDef() const { return FNiagaraTypeDefinition::GetVec2Def(); }
#endif
};

USTRUCT()
struct FNiagaraDistributionVector3 : public FNiagaraDistributionBase
{
	GENERATED_BODY()

	FNiagaraDistributionVector3() = default;
	explicit FNiagaraDistributionVector3(const float ConstantValue) { InitConstant(ConstantValue); }
	explicit FNiagaraDistributionVector3(const FVector3f& ConstantValue) { InitConstant(ConstantValue); }

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<FVector3f> Values;

	NIAGARA_API void InitConstant(const float Value);
	NIAGARA_API void InitConstant(const FVector3f& Value);
	NIAGARA_API FNiagaraStatelessRangeVector3 CalculateRange(const FVector3f& Default = FVector3f::ZeroVector) const;

#if WITH_EDITORONLY_DATA
	virtual void UpdateValuesFromDistribution() override;
	virtual FNiagaraTypeDefinition GetBindingTypeDef() const { return FNiagaraTypeDefinition::GetVec3Def(); }
#endif
};

USTRUCT()
struct FNiagaraDistributionColor : public FNiagaraDistributionBase
{
	GENERATED_BODY()

	FNiagaraDistributionColor() = default;
	explicit FNiagaraDistributionColor(const FLinearColor& ConstantValue) { InitConstant(ConstantValue); }

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<FLinearColor> Values;

	NIAGARA_API void InitConstant(const FLinearColor& Value);
	NIAGARA_API FNiagaraStatelessRangeColor CalculateRange(const FLinearColor& Default = FLinearColor::White) const;

#if WITH_EDITORONLY_DATA
	virtual bool DisplayAsColor() const override { return true; }
	virtual void UpdateValuesFromDistribution() override;
	virtual FNiagaraTypeDefinition GetBindingTypeDef() const { return FNiagaraTypeDefinition::GetColorDef(); }
#endif
};
