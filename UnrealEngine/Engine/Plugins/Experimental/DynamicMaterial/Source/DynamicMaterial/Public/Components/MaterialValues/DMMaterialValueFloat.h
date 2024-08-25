// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once
 
#include "Components/DMMaterialValue.h"
#include "DMMaterialValueFloat.generated.h"
 
UCLASS(Abstract, BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIAL_API UDMMaterialValueFloat : public UDMMaterialValue
{
	GENERATED_BODY()
 
public:
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FFloatInterval& GetValueRange() const { return ValueRange; }
 
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool HasValueRange() const { return ValueRange.Min != ValueRange.Max; }
 
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetValueRange(const FFloatInterval& InValueRange) { ValueRange = InValueRange; }
 
protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Getter = GetValueRange, Setter = SetValueRange, BlueprintSetter = SetValueRange, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", DisplayName = "Value Range"))
	FFloatInterval ValueRange;
 
	UDMMaterialValueFloat(); 
	UDMMaterialValueFloat(EDMValueType InValueType);
};
