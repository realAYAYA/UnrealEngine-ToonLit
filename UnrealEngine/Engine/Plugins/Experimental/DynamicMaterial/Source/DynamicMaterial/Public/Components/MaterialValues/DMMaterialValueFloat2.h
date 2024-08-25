// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once
 
#include "DMMaterialValueFloat.h"
#include "DMMaterialValueFloat2.generated.h"
 
UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIAL_API UDMMaterialValueFloat2 : public UDMMaterialValueFloat
{
	GENERATED_BODY()
 
public: 
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FVector2D& GetValue() const { return Value; }
 
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetValue(const FVector2D& InValue);
 
#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	FVector2D GetDefaultValue() const { return DefaultValue; }
 
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetDefaultValue(FVector2D InDefaultValue);
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
		meta = (AllowPrivateAccess = "true", DisplayName = "Vector"))
	FVector2D Value;
 
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = SetDefaultValue, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	FVector2D DefaultValue;
#endif
 
	UDMMaterialValueFloat2();
};
