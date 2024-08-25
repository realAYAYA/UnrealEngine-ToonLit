// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTextDefs.h"
#include "Materials/Material.h"
#include "UObject/SoftObjectPath.h"
#include "AvaTextMaterialHub.generated.h"

/**
 * Associates Material Settings with a Material Asset (through its path).
 */
struct FAvaTextMaterialData
{
	/** Material asset is stored as FSoftObjectPath*/
	FSoftObjectPath MaterialAssetPath;

	/** Settings describing this material */
	FAvaTextMaterialSettings MaterialSettings;

	FAvaTextMaterialData(const FSoftObjectPath& InSoftObjectPath, FAvaTextMaterialSettings InMaterialSettings)
		: MaterialAssetPath(InSoftObjectPath)
		, MaterialSettings(InMaterialSettings)
	{
	}

	/**
	 * Tries to load and return the material
	 * 
	 * @return: The material asset, nullptr if internal TryLoad() fails.
	 */
	UMaterialInterface* GetMaterial() const
	{
		return Cast<UMaterialInterface>(MaterialAssetPath.TryLoad());
	}
};

/**
 * Keeps a reference to a Material asset path.
 * Provides functionality to compare materials according to the needs of UAvaText3DComponent materials
 */
class FAvaTextMaterialId
{
public:
	/** Default constructor does not compute CachedHash, so the instance is not valid. */
	FAvaTextMaterialId() = default;
	
	FAvaTextMaterialId(const FSoftObjectPath& InSoftObjectPath)
		: MaterialAssetPath(InSoftObjectPath)
	{
		CalculateTypeHash();
	}

	/**
	 * Comparison is successful if material is the same, or if their name is the same
	 * This allows to have a successful comparison even for assets saved with different material versions.
	 */
	bool operator==(const FAvaTextMaterialId& Other) const
	{		
		return
			MaterialAssetPath == Other.MaterialAssetPath &&
			CachedHash == Other.CachedHash;	
	}

	bool operator!=(const FAvaTextMaterialId& Other) const
	{
		return !(*this == Other);
	}

	FAvaTextMaterialId(const FAvaTextMaterialId& Other)
	{
		*this = Other;
	}

	FAvaTextMaterialId(FAvaTextMaterialId&& Other) noexcept
	{
		*this = MoveTemp(Other);
	}

	FAvaTextMaterialId& operator=(const FAvaTextMaterialId& Other)
{
		MaterialAssetPath = Other.MaterialAssetPath;
		CalculateTypeHash();		
		
		return *this;
	}

	FAvaTextMaterialId& operator=(FAvaTextMaterialId&& Other) noexcept
	{
		Swap(*this, Other);
		CalculateTypeHash();
		
		return *this;
	}

	friend uint32 GetTypeHash(const FAvaTextMaterialId& InMaterialId)
	{
		return InMaterialId.CachedHash;
	}

private:
	void CalculateTypeHash()
	{
		CachedHash = GetTypeHash(MaterialAssetPath);
	}
	
	FSoftObjectPath MaterialAssetPath;
	
	uint32 CachedHash;
};

/**
 * The UAvaTextMaterialHub class can be used to:
 * 
 * - Retrieve a Text3D Material with the specified FAvaTextMaterialSettings
 * - Retrieve FAvaTextMaterialSettings for a given material
 */
UCLASS(MinimalAPI, Config=Editor)
class UAvaTextMaterialHub : public UObject
{
	GENERATED_BODY()
	
public:
	/**
	 * Gets a material with the specified settings.
	 *
	 * @param InMaterialSettings: Settings for the desired material (coloring style, features)
	 *
	 * @return A material matching the desired settings
	 */
	AVALANCHETEXT_API static UMaterialInterface* GetMaterial(FAvaTextMaterialSettings InMaterialSettings);

	/**
	 * Can be used to verify if the provided material has the same type as one of the materials handled by FAvaTextMaterials.
	 * Outputs information about its settings
	 *
	 * @param InMaterial: The material to get the settings from
	 * @return If lookup is successful, returns the FAvaTextMaterialSettings struct with the current parameters of the looked up material
	 */
	static FAvaTextMaterialSettings* GetSettingsFromMaterial(const UMaterialInterface* InMaterial);

	/** Static variables for material handling */
	struct FMaterialStatics
	{
		/** Rotation values are in the [0.0;1.0] domain */
		inline static float MaterialMaskTopBottomRotationValue   = 0.0f;
		inline static float MaterialMaskLeftToRightRotationValue = 0.75f;
		inline static float MaterialMaskRightToLeftRotationValue = 0.25f;
		
		/**
		 * Due to a typo, the value of 0.25f above got written as 2.25f
		 * This value is kept since it might be needed when retrieving rotation parameter from materials saved prior to fixing it
		 */
		inline static float MaterialMaskRightToLeftRotationValue_DEPRECATED = 2.25f;

		/** make sure the following names match the ones of their respective material parameter */
		inline static FName SolidColor_MatParam         = TEXT("Color");
		inline static FName Opacity_MatParam            = TEXT("Opacity");

		inline static FName MainTexture_MatParam        = TEXT("MainTexture");
		inline static FName TexturedUTiling_MatParam    = TEXT("U_Tiling");
		inline static FName TexturedVTiling_MatParam    = TEXT("V_Tiling");

		inline static FName BoundsOrigin_MatParam       = TEXT("TextBoundsOrigin");
		inline static FName BoundsSize_MatParam         = TEXT("TextBoundsSize");
		inline static FName TextPosition_MatParam       = TEXT("TextActorPosition");

		inline static FName GradientOffset_MatParam     = TEXT("GradientOffset");
		inline static FName GradientColorA_MatParam     = TEXT("GradientColorA");
		inline static FName GradientColorB_MatParam     = TEXT("GradientColorB");
		inline static FName GradientRotation_MatParam   = TEXT("GradientRotation");
		inline static FName GradientSmoothness_MatParam = TEXT("GradientSmoothness");
		
		inline static FName MaskRotation_MatParam       = TEXT("MaskRotationDegrees");
		inline static FName MaskOffset_MatParam         = TEXT("MaskOffset");
		inline static FName MaskSmoothness_MatParam     = TEXT("MaskSmoothness");
	};
	
private:
	/** Creates, initializes and returns an UAvaTextMaterialHub instance */
	static UAvaTextMaterialHub* Get();
	
	UAvaTextMaterialHub();

	void SetupInternalMaps();
	void AddMaterial(const FSoftObjectPath& InMaterialSoftObjectPath, FAvaTextMaterialSettings InMaterialSettings);

	UMaterialInterface* GetMaterialInternal(FAvaTextMaterialSettings InMaterialSettings) const;
	FAvaTextMaterialSettings* GetSettingsFromMaterialInternal(const UMaterialInterface* InMaterial);
	
	UPROPERTY(Config)
	FSoftObjectPath SolidMaterial;

	UPROPERTY(Config)
	FSoftObjectPath SolidTranslucentMaterial;

	UPROPERTY(Config)
	FSoftObjectPath SolidTranslucentUnlitMaterial;

	UPROPERTY(Config)
	FSoftObjectPath SolidMaskedMaterial;

	UPROPERTY(Config)
	FSoftObjectPath SolidUnlitMaterial;

	UPROPERTY(Config)
	FSoftObjectPath SolidUnlitMaskedTextMaterial;

	UPROPERTY(Config)
	FSoftObjectPath GradientMaterial;

	UPROPERTY(Config)
	FSoftObjectPath GradientTranslucentMaterial;

	UPROPERTY(Config)
	FSoftObjectPath GradientTranslucentUnlitMaterial;

	UPROPERTY(Config)
	FSoftObjectPath GradientMaskedMaterial;

	UPROPERTY(Config)
	FSoftObjectPath GradientUnlitMaterial;

	UPROPERTY(Config)
	FSoftObjectPath GradientUnlitMaskedMaterial;
	
	UPROPERTY(Config)
	FSoftObjectPath TexturedMaterial;

	UPROPERTY(Config)
	FSoftObjectPath TexturedTranslucentMaterial;

	UPROPERTY(Config)
	FSoftObjectPath TexturedTranslucentUnlitMaterial;

	UPROPERTY(Config)
	FSoftObjectPath TexturedMaskedMaterial;

	UPROPERTY(Config)
	FSoftObjectPath TexturedUnlitMaterial;

	UPROPERTY(Config)
	FSoftObjectPath TexturedUnlitMaskedMaterial;

	/** Material paths, indexed by their settings. Used to easily retrieve a UMaterial based on a set of requirements */
	TMap<FAvaTextMaterialSettings, FSoftObjectPath> MaterialsMap;

	/**
	 * MaterialData, indexed by their FAvaTextMaterialId, which is SoftObjectPath-based.
	 * This allows to retrieve material settings starting from a material asset.
	 * Useful when loading an AvaTextComponent, in order to retrieve material parameters and settings based on its original asset/type.
	 */
	TMap<FAvaTextMaterialId, FAvaTextMaterialData> MaterialDataMap;
};
