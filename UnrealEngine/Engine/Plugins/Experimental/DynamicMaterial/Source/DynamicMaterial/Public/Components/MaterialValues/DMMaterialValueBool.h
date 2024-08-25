// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once
 
#include "Components/DMMaterialValue.h"
#include "DMMaterialValueBool.generated.h"
 
UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIAL_API UDMMaterialValueBool : public UDMMaterialValue
{
	GENERATED_BODY()
 
public:
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool GetValue() const { return Value; }
 
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetValue(bool InValue);
 
#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool GetDefaultValue() const { return bDefaultValue; }
 
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetDefaultValue(bool bInDefaultValue);
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
		meta = (AllowPrivateAccess = "true", DisplayName = "Bool"))
	bool Value;
 
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = SetDefaultValue, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	bool bDefaultValue;
#endif
 
	UDMMaterialValueBool();
};
