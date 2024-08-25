// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialValue.h"
#include "DMMaterialValueColorAtlas.generated.h"

#if WITH_EDITOR
class UCurveLinearColorAtlas;
class UCurveLinearColor;
#endif

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIAL_API UDMMaterialValueColorAtlas : public UDMMaterialValue
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	float GetValue() const { return Value; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetValue(float InValue);

#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	float GetDefaultValue() const { return DefaultValue; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetDefaultValue(float InDefaultValue);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UCurveLinearColorAtlas* GetAtlas() const { return Atlas; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetAtlas(UCurveLinearColorAtlas* InAtlas);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UCurveLinearColor* GetCurve() const { return Curve; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetCurve(UCurveLinearColor* InCurve);

	virtual bool IsWholeLayerValue() const override { return true; }
#endif

	//~ Begin UDMMaterialValue
	virtual void SetMIDParameter(UMaterialInstanceDynamic* InMID) const override;
#if WITH_EDITOR
	virtual void GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const override;
	virtual bool IsDefaultValue() const override;
	virtual void ApplyDefaultValue() override;
	virtual void ResetDefaultValue() override;
#endif
	//~ End UDMMaterialValue

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = GetValue, Setter = SetValue, BlueprintSetter = SetValue, Category = "Material Designer",
		meta = (DisplayName = "Alpha", AllowPrivateAccess = "true", ClampMin = 0, UIMin = 0, ClampMax = 1, UIMax = 1))
	float Value;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = SetDefaultValue, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", ClampMin = 0, UIMin = 0, ClampMax = 1, UIMax = 1))
	float DefaultValue;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = SetAtlas, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCurveLinearColorAtlas> Atlas;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = SetCurve, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCurveLinearColor> Curve;
#endif

	UDMMaterialValueColorAtlas();
};
