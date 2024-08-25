// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once
 
#include "DMMaterialValueFloat.h"
#include "DMMaterialValueFloat1.generated.h"
 
UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIAL_API UDMMaterialValueFloat1 : public UDMMaterialValueFloat
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
#endif
 
	//~ Begin UDMMaterialValue
	virtual void SetMIDParameter(UMaterialInstanceDynamic* InMID) const override;
#if WITH_EDITOR
	virtual void GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const override;
	virtual bool IsDefaultValue() const override;
	virtual void ApplyDefaultValue() override;
	virtual void ResetDefaultValue() override;
#endif
	// ~End UDMMaterialValue
 
protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = GetValue, Setter = SetValue, BlueprintSetter = SetValue, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"), DisplayName = "Float")
	float Value;
 
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = SetDefaultValue, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	float DefaultValue;
#endif
 
	UDMMaterialValueFloat1();
};
