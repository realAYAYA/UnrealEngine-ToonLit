// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once
 
#include "Components/DMMaterialValue.h"
#include "DMMaterialValueTexture.generated.h"
 
class UTexture;

#if WITH_EDITOR
namespace UE::DynamicMaterial::Private
{
	bool DYNAMICMATERIAL_API HasAlpha(UTexture* InTexture);
}

DECLARE_DELEGATE_RetVal(UTexture*, FDMGetDefaultRGBTexture);
#endif

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIAL_API UDMMaterialValueTexture : public UDMMaterialValue
{
	GENERATED_BODY()
 
public:
#if WITH_EDITOR
	static FDMGetDefaultRGBTexture GetDefaultRGBTexture;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	static UDMMaterialValueTexture* CreateMaterialValueTexture(UObject* Outer, UTexture* InTexture);
#endif

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UTexture* GetValue() const { return Value; }
 
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetValue(UTexture* InValue);

#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool HasAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UTexture* GetDefaultValue() const { return DefaultValue; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetDefaultValue(UTexture* InDefaultValue);
#endif // WITH_EDITOR

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	//~ End UObject

	//~ Begin UDMMaterialValue
	virtual void SetMIDParameter(UMaterialInstanceDynamic* InMID) const override;
#if WITH_EDITOR
	virtual FName GetMainPropertyName() const override { return ValueName; }
	virtual void GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const override;
	virtual bool IsDefaultValue() const override;
	virtual void ApplyDefaultValue() override;
	virtual void ResetDefaultValue() override;
#endif
	//~ End UDMMaterialValue

protected:
	UDMMaterialValueTexture();
 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = GetValue, Setter = SetValue, BlueprintSetter = SetValue, Category = "Material Designer", 
		meta = (DisplayThumbnail = true, DisplayName = "Texture", AllowPrivateAccess = "true", HighPriority, NotKeyframeable))
	TObjectPtr<UTexture> Value;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetDefaultValue, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture> DefaultValue;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UTexture> OldValue;
#endif
};
