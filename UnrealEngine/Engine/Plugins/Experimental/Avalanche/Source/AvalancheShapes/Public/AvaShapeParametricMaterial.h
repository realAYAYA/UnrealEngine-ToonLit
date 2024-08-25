// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaShapesDefs.h"
#include "Math/Color.h"
#include "Math/MathFwd.h"
#include "UObject/ObjectPtr.h"
#include "AvaShapeParametricMaterial.generated.h"

class UMaterial;
class UMaterialInstance;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UObject;
struct FAvaShapeParametricMaterial;

USTRUCT(BlueprintType)
struct FAvaShapeParametricMaterial
{
	GENERATED_BODY()

	static inline constexpr int32 OPAQUE_LIT = 0;
	static inline constexpr int32 TRANSLUCENT_LIT = 1;
	static inline constexpr int32 OPAQUE_UNLIT = 2;
	static inline constexpr int32 TRANSLUCENT_UNLIT = 3;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMaterialChanged, FAvaShapeParametricMaterial&)
	DECLARE_MULTICAST_DELEGATE(FOnMaterialParameterChanged)

	AVALANCHESHAPES_API FAvaShapeParametricMaterial();

	AVALANCHESHAPES_API FAvaShapeParametricMaterial(const FAvaShapeParametricMaterial& Other);
	AVALANCHESHAPES_API FAvaShapeParametricMaterial& operator=(const FAvaShapeParametricMaterial& Other);

	/** Called when active material parameters are updated */
	AVALANCHESHAPES_API FOnMaterialParameterChanged& OnMaterialParameterChanged();

	/** Called when active material index changed */
	static FOnMaterialChanged& OnMaterialChanged();

	AVALANCHESHAPES_API bool CopyFromMaterialParameters(UMaterialInstance* InMaterial);

	/** Check if input material is a parametric material */
	bool IsParametricMaterial(UMaterialInterface* InMaterial, const bool bCheckIfDefault = false) const;

	/** Get default parent material currently active */
	AVALANCHESHAPES_API UMaterialInterface* GetDefaultMaterial() const;

	/** Get active up to date material instance */
	AVALANCHESHAPES_API UMaterialInstanceDynamic* GetMaterial() const;

	/** Get active up to date material instance or creates it */
	AVALANCHESHAPES_API UMaterialInstanceDynamic* GetOrCreateMaterial(UObject* InOuter);

	/** Set instanced material currently active */
	void SetMaterial(UMaterialInstanceDynamic* InMaterial);

	bool GetUseTranslucentMaterial() const
	{
		return bUseTranslucentMaterial;
	}

	AVALANCHESHAPES_API void SetUseTranslucentMaterial(bool bInUse);

	bool GetUseAutoTranslucency() const
	{
		return bUseAutoTranslucency;
	}

	AVALANCHESHAPES_API void SetUseAutoTranslucency(bool bInUse);

	EAvaShapeParametricMaterialStyle GetStyle() const
	{
		return Style;
	}

	AVALANCHESHAPES_API void SetStyle(EAvaShapeParametricMaterialStyle InStyle);

	const FLinearColor& GetPrimaryColor() const
	{
		return ColorA;
	}

	AVALANCHESHAPES_API void SetPrimaryColor(const FLinearColor& InColor);

	const FLinearColor& GetSecondaryColor() const
	{
		return ColorB;
	}

	AVALANCHESHAPES_API void SetSecondaryColor(const FLinearColor& InColor);

	float GetGradientOffset() const
	{
		return GradientOffset;
	}

	AVALANCHESHAPES_API void SetGradientOffset(float InOffset);

	bool GetUseUnlitMaterial() const
	{
		return bUseUnlitMaterial;
	}

	AVALANCHESHAPES_API void SetUseUnlitMaterial(bool bInUse);

	/** Set parameter values on a material instance */
	AVALANCHESHAPES_API void SetMaterialParameterValues(UMaterialInstanceDynamic* InMaterialInstance, bool bInNotifyUpdate = false) const;

protected:
	/** Parent material
	 * 1. Opaque Lit
	 * 2. Translucent Lit
	 * 3. Opaque Unlit
	 * 4. Translucent Unlit
	 * ...
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> DefaultMaterials;

	/** Material instances corresponding to the parent material instanced
	 * 1. Opaque Lit
	 * 2. Translucent Lit
	 * 3. Opaque Unlit
	 * 4. Translucent Unlit
	 * ...
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> InstanceMaterials;

	// whether we use opaque or translucent material, set automatically when opacity is below 1
	UPROPERTY()
	bool bUseTranslucentMaterial;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, AdvancedDisplay, Category="Material")
	bool bUseAutoTranslucency = true;

	// Default style for the material
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Material", meta=(DisplayName="Simple Style", AllowPrivateAccess = "true"))
	EAvaShapeParametricMaterialStyle Style;

	// Primary colour for the material
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Material", meta=(DisplayName="Simple Color", AllowPrivateAccess = "true"))
	FLinearColor ColorA;

	// Secondary colour for the material
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Material", meta=(DisplayName="Gradient Color", EditCondition="Style == EAvaShapeParametricMaterialStyle::LinearGradient", EditConditionHides, AllowPrivateAccess = "true"))
	FLinearColor ColorB;

	// Offset for gradient style material
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Material", meta=(ClampMin="0", ClampMax="1", DisplayName="Gradient Offset", EditCondition = "Style == EAvaShapeParametricMaterialStyle::LinearGradient", EditConditionHides, AllowPrivateAccess = "true"))
	float GradientOffset;

	// whether the material is unlit or default lit
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Material", meta=(DisplayName="Use Unlit Material", AllowPrivateAccess = "true"))
	bool bUseUnlitMaterial;

private:
	static FOnMaterialChanged OnMaterialChangedDelegate;

	FOnMaterialParameterChanged OnMaterialParameterChangedDelegate;

	int32 ActiveInstanceIndex = INDEX_NONE;

	/** Load parents materials to create instance materials */
	void LoadDefaultMaterials();

	/** Create an instance material based on the current active parent */
	UMaterialInstanceDynamic* CreateMaterialInstance(UObject* InOuter);

	/** Update parameter values on all instance materials */
	void OnMaterialParameterUpdated();

	/** Get the active material index */
	int32 GetActiveInstanceIndex() const;
};
