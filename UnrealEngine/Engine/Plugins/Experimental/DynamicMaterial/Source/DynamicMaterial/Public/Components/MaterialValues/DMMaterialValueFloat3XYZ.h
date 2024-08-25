// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once
 
#include "DMMaterialValueFloat.h"
#include "DMMaterialValueFloat3XYZ.generated.h"
 
UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIAL_API UDMMaterialValueFloat3XYZ : public UDMMaterialValueFloat
{
	GENERATED_BODY()
 
public: 
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FVector& GetValue() const { return Value; }
 
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetValue(const FVector& InValue);
 
#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	FVector GetDefaultValue() const { return DefaultValue; }
 
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetDefaultValue(FVector InDefaultValue);
#endif
 
	//~ Begin UDMMaterialValue
	virtual void SetMIDParameter(UMaterialInstanceDynamic* InMID) const override;
#if WITH_EDITOR
	virtual void GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const override;
	virtual int32 GetInnateMaskOutput(int32 OutputChannels) const override;
	virtual bool IsDefaultValue() const override;
	virtual void ApplyDefaultValue() override;
	virtual void ResetDefaultValue() override;
#endif
	// ~End UDMMaterialValue
 
protected: 
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = GetValue, Setter = SetValue, BlueprintSetter = SetValue, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"), DisplayName = "Vector")
	FVector Value;
 
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetDefaultValue, 
		meta = (AllowPrivateAccess = "true"), Category = "Material Designer")
	FVector DefaultValue;
#endif
 
	UDMMaterialValueFloat3XYZ();
};
