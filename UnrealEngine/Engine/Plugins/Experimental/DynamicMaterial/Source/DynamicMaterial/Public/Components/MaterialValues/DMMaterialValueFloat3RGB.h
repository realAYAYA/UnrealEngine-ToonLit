// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once
 
#include "DMMaterialValueFloat.h"
#include "DMMaterialValueFloat3RGB.generated.h"
 
UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIAL_API UDMMaterialValueFloat3RGB : public UDMMaterialValueFloat
{
	GENERATED_BODY()
 
public: 
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FLinearColor& GetValue() const { return Value; }
 
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetValue(const FLinearColor& InValue);
 
#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	FLinearColor GetDefaultValue() const { return DefaultValue; }
 
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetDefaultValue(FLinearColor InDefaultValue);
#endif
 
	//~ Begin UDMMaterialValue
	virtual void SetMIDParameter(UMaterialInstanceDynamic* InMID) const override;
#if WITH_EDITOR
	virtual void GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const override;
	virtual int32 GetInnateMaskOutput(int32 OutputChannels) const override;
	virtual bool IsDefaultValue() const override;
	virtual void ApplyDefaultValue() override;
	virtual void ResetDefaultValue() override;
	virtual FName GetMainPropertyName() const override { return ValueName; }
	//~ End UDMMaterialValue
#endif

protected: 
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = GetValue, Setter = SetValue, BlueprintSetter = SetValue, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", DisplayName = "Color", HideAlphaChannel))
	FLinearColor Value;
 
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = SetDefaultValue, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor DefaultValue;
#endif
 
	UDMMaterialValueFloat3RGB();
};
