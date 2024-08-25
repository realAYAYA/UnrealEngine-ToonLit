// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionSubstrate.generated.h"

class FMaterialCompiler;
struct FSubstrateMaterialComplexity;

/**
 * Compile a special blend function for Substrate when blending material attribute
 *
 * @param Compiler				The compiler to add code to
 * @param Foreground			Entry A, has a bigger impact when Alpha is close to 0
 * @param Background			Entry B, has a bigger impact when Alpha is close to 1
 * @param Alpha					Blend factor [0..1], when 0
 * @return						Index to a new code chunk
 */
extern int32 CompileSubstrateBlendFunction(FMaterialCompiler* Compiler, const int32 A, const int32 B, const int32 Alpha);


///////////////////////////////////////////////////////////////////////////////
// BSDF nodes

// UMaterialExpressionSubstrateBSDF can only be used for Substrate nodes ouputing SubstrateData that would need a preview,
UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, Abstract, DisplayName = "Substrate Expression")
class UMaterialExpressionSubstrateBSDF : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	virtual int32 CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
#endif

	float DielectricSpecularToF0(float SpecularIn)
	{
		return 0.08f * SpecularIn;
	};
};


UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Shading Models")
class UMaterialExpressionSubstrateShadingModels : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Defines the overall color of the Material. (type = float3, unit = unitless, defaults to 0.18)
	 */
	UPROPERTY()
	FExpressionInput BaseColor;

	/**
	 * Controls how \"metal-like\" your surface looks like. 0 means dielectric, 1 means conductor (type = float, unit = unitless, defaults to 0)
	 */
	UPROPERTY()
	FExpressionInput Metallic;

	/**
	 * Used to scale the current amount of specularity on non-metallic surfaces and is a value between 0 and 1 (type = float, unit = unitless, defaults to plastic 0.5)
	 */
	UPROPERTY()
	FExpressionInput Specular;
	
	/**
	 * Controls how rough the Material is. Roughness of 0 (smooth) is a mirror reflection and 1 (rough) is completely matte or diffuse. When using anisotropy, it is the roughness used along the Tangent axis. (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput Roughness;
		
	/**
	 * Controls the anisotropy factor of the roughness. Positive value elongates the specular lobe along the Tangent vector, Negative value elongates the specular lobe along the perpendicular of the Tangent. (type = float, unit = unitless).
	 */
	UPROPERTY()
	FExpressionInput Anisotropy;

	/**
	 * Emissive color on top of the surface (type = float3, unit = luminance, default = 0)
	 */
	UPROPERTY()
	FExpressionInput EmissiveColor;

	/**
	 * Take the surface normal as input. The normal is considered tangent or world space according to the space properties on the main material node. (type = float3, unit = unitless, defaults to vertex normal)
	 */
	UPROPERTY()
	FExpressionInput Normal;

	/**
	* Take a surface tangent as input. The tangent is considered tangent or world space according to the space properties on the main material node. (type = float3, unit = unitless, defaults to vertex tangent)
	*/
	UPROPERTY()
	FExpressionInput Tangent;

	/**
	 * Scale the mean free path radius of the SSS profile according to a value between 0 and 1. Always used, when a subsurface profile is provided or not. (type = float, unitless, defaults to 1)
	 */
	UPROPERTY()
	FExpressionInput SubSurfaceColor;

	/**
	 * Coverage of the clear coat layer. (type = float, unit = unitless, defaults to 0.0)
	 */
	UPROPERTY()
	FExpressionInput ClearCoat;

	/**
	 * Roughness of the top clear coat layer. (type = float, unit = unitless, defaults to 0.0)
	 */
	UPROPERTY()
	FExpressionInput ClearCoatRoughness;

	/**
	 * Opacity of the material
	 */
	UPROPERTY()
	FExpressionInput Opacity;

	/**
	 * The amount of transmitted light from the back side of the surface to the front side of the surface (type = float3, unit = unitless, defaults to 1)
	 */
	UPROPERTY()
	FExpressionInput TransmittanceColor;

		/**
	* The single scattering Albedo defining the overall color of the Material (type = float3, unit = unitless, default = 0)
	 */
	UPROPERTY()
	FExpressionInput WaterScatteringCoefficients;

	/**
	 * The rate at which light is absorbed or out-scattered by the medium. Mean Free Path = 1 / Extinction. (type = float3, unit = 1/cm, default = 0)
	 */
	UPROPERTY()
	FExpressionInput WaterAbsorptionCoefficients;

	/**
	 * Anisotropy of the volume with values lower than 0 representing back-scattering, equal 0 representing isotropic scattering and greater than 0 representing forward scattering. (type = float, unit = unitless, defaults to 0)
	 */
	UPROPERTY()
	FExpressionInput WaterPhaseG;

	/**
	 * A scale to apply on the scene color behind the water surface. It can be used to approximate caustics for instance. (type = float3, unit = unitless, defaults to 1)
	 */
	UPROPERTY()
	FExpressionInput ColorScaleBehindWater;

	/**
	 * Take the bottom clear coat surface normal as input. The normal is considered tangent or world space according to the space properties on the main material node. (type = float3, unit = unitless, defaults to vertex normal)
	 */
	UPROPERTY()
	FExpressionInput ClearCoatNormal;

	/**
	 * Take the tangent output node as input. The tangent is considered tangent or world space according to the space properties on the main material node. (type = float3, unit = unitless, defaults to vertex tangent)
	 */
	UPROPERTY()
	FExpressionInput CustomTangent;

	/**
	 * Shading models
	 */
	UPROPERTY()
	FShadingModelMaterialInput ShadingModel;
	
	/** SubsurfaceProfile, for Screen Space Subsurface Scattering. The profile needs to be set up on both the Substrate diffuse node, and the material node at the moment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Material, meta = (DisplayName = "Subsurface Profile"))
	TObjectPtr<class USubsurfaceProfile> SubsurfaceProfile;

	UPROPERTY(EditAnywhere, Category = ShadingModel, meta = (ShowAsInputPin = "Primary", DisplayName = "Single Shading Model"))
	TEnumAsByte<enum EMaterialShadingModel> ShadingModelOverride = MSM_DefaultLit;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	bool HasSSS() const;
	bool HasAnisotropy() const;

#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Slab")
class UMaterialExpressionSubstrateSlabBSDF : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Defines the diffused albedo, the percentage of light reflected as diffuse from the surface. (type = float3, unit = unitless, defaults to 0.18)
	 */
	UPROPERTY()
	FExpressionInput DiffuseAlbedo;

	/**
	 * Defines the color and brightness of the specular highlight where the surface is facing the camera. Each specular contribution will fade to black as F0 drops below 0.02. (type = float3, unit = unitless, defaults to plastic 0.04)
	 */
	UPROPERTY()
	FExpressionInput F0;

	/**
	 * Defines the color of the specular highlight where the surface normal is 90 degrees from the view direction. Only the hue and saturation are preserved, the brightness is fixed at 1.0. Fades to black as F0 drops below 0.02. (type = float3, unit = unitless, defaults to 1.0f)
	 */
	UPROPERTY()
	FExpressionInput F90;

	/**
	 * Controls how rough the Material is. Roughness of 0 (smooth) is a mirror reflection and 1 (rough) is completely matte or diffuse. When using anisotropy, it is the roughness used along the Tangent axis. (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput Roughness;
		
	/**
	 * Controls the anisotropy factor of the roughness. Positive value elongates the specular lobe along the Tangent vector, Negative value elongates the specular lobe along the perpendicular of the Tangent. (type = float, unit = unitless).
	 */
	UPROPERTY()
	FExpressionInput Anisotropy;

	/**
	 * Take the surface normal as input. The normal is considered tangent or world space according to the space properties on the main material node. (type = float3, unit = unitless, defaults to vertex normal)
	 */
	UPROPERTY()
	FExpressionInput Normal;

	/**
	* Take a surface tangent as input. The tangent is considered tangent or world space according to the space properties on the main material node. (type = float3, unit = unitless, defaults to vertex tangent)
	*/
	UPROPERTY()
	FExpressionInput Tangent;

	/**
	 * Chromatic mean free path . Only used when there is not any sub-surface profile provided. (type = float3, unit = centimeters, default = 0)
	 */
	UPROPERTY()
	FExpressionInput SSSMFP;

	/**
	 * Scale the mean free path radius of the SSS profile according to a value between 0 and 1. Always used, when a subsurface profile is provided or not. (type = float, unitless, defaults to 1)
	 */
	UPROPERTY()
	FExpressionInput SSSMFPScale;

	/**
	 * Phase function anisotropy. Positive value elongates the phase function along the lignt direction, causing forward scattering. Negative value elongates the phase function backward to the light direction, causing backward scattering.  (type = float, unitless, defaults to 1, valid value -1..1)
	 */
	UPROPERTY()
	FExpressionInput SSSPhaseAnisotropy;

	/**
	 * Emissive color on top of the surface (type = float3, unit = luminance, default = 0)
	 */
	UPROPERTY()
	FExpressionInput EmissiveColor;

	/**
	 * Controls the roughness of a secondary specular lobe. Roughness of 0 (smooth) is a mirror reflection and 1 (rough) is completely matte or diffuse. Does not influence diffuse roughness. (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput SecondRoughness;

	/**
	 * The weight of the second specular lobe using SecondRoughness. The first specular using Roughness will have a weight of (1 - SecondRoughnessWeight). (type = float, unitless, default = 0)
	 */
	UPROPERTY()
	FExpressionInput SecondRoughnessWeight;

	/**
	 * Controls how rough the Fuzz layer is. Roughness of 0 is smooth and 1 is rough. If FuzzRoughness Is not connected, the Roughness input will be used instead. (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput FuzzRoughness;

	/**
	 * The amount of fuzz on top of the surface used to simulate cloth-like appearance.
	 */
	UPROPERTY()
	FExpressionInput FuzzAmount;

	/**
	 * The base color of the fuzz.
	 */
	UPROPERTY()
	FExpressionInput FuzzColor;

	/**
	 * This represent the logarithm of the micro facet density. Only used when `r.Substrate.Glints=1`. Defaults to 0.
	 */
	UPROPERTY()
	FExpressionInput GlintValue;

	/**
	 * The parameterization of the surface required to position glints on a surface. Only used when `r.Substrate.Glints=1`. Defaults to (0,0).
	 */
	UPROPERTY()
	FExpressionInput GlintUV;

	/** SubsurfaceProfile, for Screen Space Subsurface Scattering. The profile needs to be set up on both the Substrate diffuse node, and the material node at the moment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Material, meta = (DisplayName = "Subsurface Profile"))
	TObjectPtr<class USubsurfaceProfile> SubsurfaceProfile;

	/** SpecularProfile, for modulating specular appearance and simulating more complex visuals such as iridescence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Material, meta = (DisplayName = "Specular Profile"))
	TObjectPtr<class USpecularProfile> SpecularProfile;

	/** Whether to use light diffusion (i.e., SSS diffusion) or wrap-approximation for material with scattering behavior. This option trades quality over performance and will result into visual differences. */
	UPROPERTY(EditAnywhere, Category = Mode, meta = (DisplayName = "Use Subsurface Diffusion"))
	uint32 bUseSSSDiffusion : 1;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;

	bool HasEdgeColor() const;
	bool HasFuzz() const;
	bool HasFuzzRoughness() const;
	bool HasSecondRoughness() const;
	bool HasSSS() const;
	bool HasSSSProfile() const;
	bool HasMFPPluggedIn() const;
	bool HasAnisotropy() const;
	bool HasGlint() const;
	bool HasSpecularProfile() const;

	FSubstrateMaterialComplexity GetComplexity() const;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Simple Clear Coat")
class UMaterialExpressionSubstrateSimpleClearCoatBSDF : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Defines the diffused albedo, the percentage of light reflected as diffuse from the surface. (type = float3, unit = unitless, defaults to 0.18)
	 */
	UPROPERTY()
	FExpressionInput DiffuseAlbedo;

	/**
	 * Defines the color and brightness of the specular highlight where the surface is facing the camera. Each specular contribution will fade to black as F0 drops below 0.02. (type = float3, unit = unitless, defaults to plastic 0.04)
	 */
	UPROPERTY()
	FExpressionInput F0;

	/**
	 * Controls how rough the bottom layer of the material is. Roughness of 0 (smooth) is a mirror reflection and 1 (rough) is completely matte or diffuse. (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput Roughness;

	/**
	 * Controls the coverage of the clear coat layer: 0 means no clear coat, 1 means coat is fully visible. (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput ClearCoatCoverage;

	/**
	 * Controls how rough the top layer of the material is. Roughness of 0 (smooth) is a mirror reflection and 1 (rough) is completely matte or diffuse. (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput ClearCoatRoughness;

	/**
	 * Take the surface normal as input. The normal is considered tangent or world space according to the space properties on the main material node. (type = float3, unit = unitless, defaults to vertex normal)
	 */
	UPROPERTY()
	FExpressionInput Normal;

	/**
	 * Emissive color of the medium (type = float3, unit = luminance, default = 0)
	 */
	UPROPERTY()
	FExpressionInput EmissiveColor;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Volumetric-Fog-Cloud BSDF")
class UMaterialExpressionSubstrateVolumetricFogCloudBSDF : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	* The single scattering Albedo defining the overall color of the Material (type = float3, unit = unitless, default = 0)
	*/
	UPROPERTY()
	FExpressionInput Albedo;

	/**
	 * The rate at which light is absorbed or scattered by the medium. Mean Free Path = 1 / Extinction. (type = float3, unit = 1/m, default = 0)
	 */
	UPROPERTY()
	FExpressionInput Extinction;

	/**
	 * Emissive color of the medium (type = float3, unit = luminance, default = 0)
	 */
	UPROPERTY()
	FExpressionInput EmissiveColor;

	/**
	 * Ambient occlusion: 1 means no occlusion while 0 means fully occluded. (type = float, unit = unitless, default = 1)
	 */
	UPROPERTY()
	FExpressionInput AmbientOcclusion;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Unlit BSDF")
class UMaterialExpressionSubstrateUnlitBSDF : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	* Emissive color on top of the surface (type = float3, unit = Luminance, default = 0)
	*/
	UPROPERTY()
	FExpressionInput EmissiveColor;

	/**
	 * The amount of transmitted light from the back side of the surface to the front side of the surface (type = float3, unit = unitless, defaults to 1)
	 */
	UPROPERTY()
	FExpressionInput TransmittanceColor;

	/**
	 * The surface normal. Only used for refraction effects when `IOR` or `pixel normal offset` modes are selected.
	 */
	UPROPERTY()
	FExpressionInput Normal;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Hair BSDF")
class UMaterialExpressionSubstrateHairBSDF : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()
		
	/**
	 * Hair fiber base color resulting from single and multiple scattering combined. (type = float3, unit = unitless, defaults to black)
	 */
	UPROPERTY()
	FExpressionInput BaseColor;
	
	/**
	 * Amount of light scattering, only available for non-HairStrand rendering (type = float, unit = unitless, defaults to 0.0)
	 */
	UPROPERTY()
	FExpressionInput Scatter;
		
	/**
	 * Specular (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput Specular;
		
	/**
	 * Controls how rough the Material is. Roughness of 0 (smooth) is a mirror reflection and 1 (rough) is completely matte or diffuse (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput Roughness;

	/**
	 * How much light contributs when lighting hairs from the back side opposite from the view, only available for HairStrand rendering (type = float3, unit = unitless, defaults to 0.0)
	 */
	UPROPERTY()
	FExpressionInput Backlit;

	/**
	 * Tangent (type = float3, unit = unitless, defaults to +X vector)
	 */
	UPROPERTY()
	FExpressionInput Tangent;

	/**
	 * Emissive color on top of the surface (type = float3, unit = luminance, defaults to 0.0)
	 */
	UPROPERTY()
	FExpressionInput EmissiveColor;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Eye BSDF")
class UMaterialExpressionSubstrateEyeBSDF : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()
		
	/**
	 * Hair fiber base color resulting from single and multiple scattering combined. (type = float3, unit = unitless, defaults to black)
	 */
	UPROPERTY()
	FExpressionInput DiffuseColor;
		
	/**
	 * Controls how rough the Material is. Roughness of 0 (smooth) is a mirror reflection and 1 (rough) is completely matte or diffuse (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput Roughness;

	/**
	 * Normal of the sclera and cornea (type = float3, unit = unitless, defaults to +X vector)
	 */
	UPROPERTY()
	FExpressionInput CorneaNormal;

	/**
	 * Normal of the iris (type = float3, unit = unitless, defaults to +X vector)
	 */
	UPROPERTY()
	FExpressionInput IrisNormal;

	/**
	 * Normal of the iris plane (type = float3, unit = unitless, defaults to +X vector)
	 */
	UPROPERTY()
	FExpressionInput IrisPlaneNormal;

	/**
	 * Mask defining the iris surface (type = float, unit = unitless, defaults to 0.0)
	 */
	UPROPERTY()
	FExpressionInput IrisMask;

	/**
	 * Distance from the center of the iris (type = float, unit = unitless, defaults to 0.0)
	 */
	UPROPERTY()
	FExpressionInput IrisDistance;

	/**
	 * Emissive color on top of the surface (type = float3, unit = luminance, defaults to 0.0)
	 */
	UPROPERTY()
	FExpressionInput EmissiveColor;

	/** SubsurfaceProfile, for Subsurface Scattering diffusion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Material, meta = (DisplayName = "Subsurface Profile"))
	TObjectPtr<class USubsurfaceProfile> SubsurfaceProfile;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Single Layer Water BSDF")
class UMaterialExpressionSubstrateSingleLayerWaterBSDF : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Surface base color. (type = float3, unit = unitless, defaults to black)
	 */
	UPROPERTY()
	FExpressionInput BaseColor;

	/**
	 * Whether the surface represents a dielectric (such as plastic) or a conductor (such as metal). (type = float, unit = unitless, defaults to 0 = dielectric)
	 */
	UPROPERTY()
	FExpressionInput Metallic;

	/**
	 * Specular amount (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput Specular;

	/**
	 * Controls how rough the Material is. Roughness of 0 (smooth) is a mirror reflection and 1 (rough) is completely matte or diffuse (type = float, unit = unitless, defaults to 0.5)
	 */
	UPROPERTY()
	FExpressionInput Roughness;

	/**
	 * The normal of the surface (type = float3, unit = unitless, defaults to +Z vector)
	 */
	UPROPERTY()
	FExpressionInput Normal;

	/**
	 * Emissive color on top of the surface (type = float3, unit = luminance, defaults to 0.0)
	 */
	UPROPERTY()
	FExpressionInput EmissiveColor;

	/**
	 * Opacity of the material layered on top of the water (type = float3, unit = unitless, defaults to 0.0)
	 */
	UPROPERTY()
	FExpressionInput TopMaterialOpacity;

	/**
	* The single scattering Albedo defining the overall color of the Material (type = float3, unit = unitless, default = 0)
	 */
	UPROPERTY()
	FExpressionInput WaterAlbedo;

	/**
	 * The rate at which light is absorbed or out-scattered by the medium. Mean Free Path = 1 / Extinction. (type = float3, unit = 1/cm, default = 0)
	 */
	UPROPERTY()
	FExpressionInput WaterExtinction;

	/**
	 * Anisotropy of the volume with values lower than 0 representing back-scattering, equal 0 representing isotropic scattering and greater than 0 representing forward scattering. (type = float, unit = unitless, defaults to 0)
	 */
	UPROPERTY()
	FExpressionInput WaterPhaseG;

	/**
	 * A scale to apply on the scene color behind the water surface. It can be used to approximate caustics for instance. (type = float3, unit = unitless, defaults to 1)
	 */
	UPROPERTY()
	FExpressionInput ColorScaleBehindWater;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Light Function")
class UMaterialExpressionSubstrateLightFunction : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * The output color of the light function
	 */
	UPROPERTY()
	FExpressionInput Color;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Post Process")
class UMaterialExpressionSubstratePostProcess : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * The output color of the post process: it represents a color added over the back buffer, or a color multiplied if the Substrate blend mode is TransmittanceOnly.
	 */
	UPROPERTY()
	FExpressionInput Color;

	/**
	 * The coverage of the post process: the more the value is high, the less the back buffer will be visible. Only used if "Output Alpha" is enabled on the root node.
	 */
	UPROPERTY()
	FExpressionInput Opacity;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate UI")
class UMaterialExpressionSubstrateUI : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * The output color of the UI element.
	 */
	UPROPERTY()
	FExpressionInput Color;

	/**
	 * The coverage of the UI element: the more the value is high, the less the back buffer will be visible.
	 */
	UPROPERTY()
	FExpressionInput Opacity;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Convert To Decal")
class UMaterialExpressionSubstrateConvertToDecal : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * The Substrate material to convert to a decal.
	 */
	UPROPERTY()
	FExpressionInput DecalMaterial;

	/**
	 * The coverage of the decal (default 1)
	 */
	UPROPERTY()
	FExpressionInput Coverage;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionSubstrateConvertMaterialAttributes : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FMaterialAttributesInput MaterialAttributes;

		/**
	* The single scattering Albedo defining the overall color of the Material (type = float3, unit = unitless, default = 0)
	 */
	UPROPERTY()
	FExpressionInput WaterScatteringCoefficients;

	/**
	 * The rate at which light is absorbed or out-scattered by the medium. Mean Free Path = 1 / Extinction. (type = float3, unit = 1/cm, default = 0)
	 */
	UPROPERTY()
	FExpressionInput WaterAbsorptionCoefficients;

	/**
	 * Anisotropy of the volume with values lower than 0 representing back-scattering, equal 0 representing isotropic scattering and greater than 0 representing forward scattering. (type = float, unit = unitless, defaults to 0)
	 */
	UPROPERTY()
	FExpressionInput WaterPhaseG;

	/**
	 * A scale to apply on the scene color behind the water surface. It can be used to approximate caustics for instance. (type = float3, unit = unitless, defaults to 1)
	 */
	UPROPERTY()
	FExpressionInput ColorScaleBehindWater;

	/** SubsurfaceProfile, for Screen Space Subsurface Scattering. The profile needs to be set up on both the Substrate diffuse node, and the material node at the moment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Material, meta = (DisplayName = "Subsurface Profile"))
	TObjectPtr<class USubsurfaceProfile> SubsurfaceProfile;

	UPROPERTY(EditAnywhere, Category = ShadingModel, meta = (ShowAsInputPin = "Primary", DisplayName = "Single Shading Model"))
	TEnumAsByte<enum EMaterialShadingModel> ShadingModelOverride = MSM_DefaultLit;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) override;
	virtual bool IsInputConnectionRequired(int32 InputIndex) const override {return true;}
	
	bool HasSSS() const;

#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
};

///////////////////////////////////////////////////////////////////////////////
// Operator nodes

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Horizontal Blend")
class UMaterialExpressionSubstrateHorizontalMixing : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()
		
	/**
	 * Substrate material
	 */
	UPROPERTY()
	FExpressionInput Background;

	/**
	 * Substrate material
	 */
	UPROPERTY()
	FExpressionInput Foreground;

	/**
	 * Lerp factor between Background (Mix == 0) and Foreground (Mix == 1).
	 */
	UPROPERTY()
	FExpressionInput Mix;

	/**
	 * Merge Background and Foreground into a single material by mixing their inputs rather than their evaluation. This makes lighting evaluation cheaper (Default: off)
	 */
	UPROPERTY(EditAnywhere, Category = Mode)
	uint32 bUseParameterBlending : 1;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Vertical Layer")
class UMaterialExpressionSubstrateVerticalLayering : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Substrate material layer on top of the Base material layer
	 */
	UPROPERTY()
	FExpressionInput Top;
	
	/**
	 * Substrate material layer below the Top material layer
	 */
	UPROPERTY()
	FExpressionInput Base;

	/**
	 * Thickness of the Top material layer
	 */
	UPROPERTY()
	FExpressionInput Thickness;

	/**
	 * Merge Top and Base into a single material by mixing their inputs rather than their evaluation. This makes lighting evaluation cheaper (Default: off)
	 */
	UPROPERTY(EditAnywhere, Category = Mode)
	uint32 bUseParameterBlending : 1;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, UMaterialExpression* Parent, int32 OutputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Add")
class UMaterialExpressionSubstrateAdd : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Substrate material
	 */
	UPROPERTY()
	FExpressionInput A;
	
	/**
	 * Substrate material
	 */
	UPROPERTY()
	FExpressionInput B;

	/**
	 * Merge A and B into a single material by mixing their inputs rather than their evaluation. This makes lighting evaluation cheaper (Default: off)
	 */
	UPROPERTY(EditAnywhere, Category = Mode)
	uint32 bUseParameterBlending : 1;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Coverage Weight")
class UMaterialExpressionSubstrateWeight : public UMaterialExpressionSubstrateBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Substrate material
	 */
	UPROPERTY()
	FExpressionInput A;
	
	/**
	 * Weight to apply to the Substrate material BSDFs
	 */
	UPROPERTY()
	FExpressionInput Weight;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultSubstrateMaterial(int32 OutputIndex) override;
	virtual void GatherSubstrateMaterialInfo(FSubstrateMaterialInfo& SubstrateMaterialInfo, int32 OutputIndex) override;
	virtual FSubstrateOperator* SubstrateGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};


///////////////////////////////////////////////////////////////////////////////
// Utilities

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, Abstract, DisplayName = "Substrate Utility Base Class")
class UMaterialExpressionSubstrateUtilityBase : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Transmittance-To-MeanFreePath")
class UMaterialExpressionSubstrateTransmittanceToMFP : public UMaterialExpressionSubstrateUtilityBase
{
	GENERATED_UCLASS_BODY()

	/**
	* The colored transmittance for a view perpendicular to the surface. The transmittance for other view orientations will automatically be deduced according to surface thickness.
	*/
	UPROPERTY()
	FExpressionInput TransmittanceColor;

	/**
	* The desired thickness in centimeter.
	* Another use case example: this node output called thickness can be modulated before it is plugged in a slab node, this can be used to achieve simple scattering/transmittance variation of the same material.
	*/
	UPROPERTY()
	FExpressionInput Thickness;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Metalness-To-DiffuseColorF0")
class UMaterialExpressionSubstrateMetalnessToDiffuseAlbedoF0 : public UMaterialExpressionSubstrateUtilityBase
{
	GENERATED_UCLASS_BODY()

	/**
	 * Defines the overall color of the Material. (type = float3, unit = unitless, defaults to 0.18)
	 */
	UPROPERTY()
	FExpressionInput BaseColor;

	/**
	 * Controls how \"metal-like\" your surface looks like. 0 means dielectric, 1 means conductor (type = float, unit = unitless, defaults to 0)
	 */
	UPROPERTY()
	FExpressionInput Metallic;

	/**
	 * Used to scale the current amount of specularity on non-metallic surfaces and is a value between 0 and 1 (type = float, unit = unitless, defaults to plastic 0.5)
	 */
	UPROPERTY()
	FExpressionInput Specular;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Haziness-To-Secondary-Roughness")
class UMaterialExpressionSubstrateHazinessToSecondaryRoughness : public UMaterialExpressionSubstrateUtilityBase
{
	GENERATED_UCLASS_BODY()

	/**
	* The base roughness of the surface. It represented the smoothest part of the reflection.
	*/
	UPROPERTY()
	FExpressionInput BaseRoughness;

	/**
	* Haziness represent the amount of irregularity of the surface. A high value will lead to a second rough specular lobe causing the surface too look `milky`.
	*/
	UPROPERTY()
	FExpressionInput Haziness;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Substrate Thin-Film")
class UMaterialExpressionSubstrateThinFilm : public UMaterialExpressionSubstrateUtilityBase
{
	GENERATED_UCLASS_BODY()

	/**
	 * The normal of the surface to consider. This input respects the normal space setup on the root node (tangent or world)
	 */
	UPROPERTY()
	FExpressionInput Normal;

	/**
	 * Defines the color and brightness of the specular highlight where the surface is facing the camera. Each specular contribution will fade to black as F0 drops below 0.02. (type = float3, unit = unitless, defaults to plastic 0.04)
	 */
	UPROPERTY()
	FExpressionInput F0;

	/**
	 * Defines the color of the specular highlight where the surface normal is 90 degrees from the view direction. Only the hue and saturation are preserved, the brightness is fixed at 1.0. Fades to black as F0 drops below 0.02. (type = float3, unit = unitless, defaults to 1.0f)
	 */
	UPROPERTY()
	FExpressionInput F90;
	
	/**
	 * Thin film controls the thin film layer coating the current slab. 0 means disabled and 1 means a coating layer of 10 micrometer. (type = float, unitless, default = 0)
	 */
	UPROPERTY()
	FExpressionInput Thickness;

	/**
	 * Thin film IOR
	 */
	UPROPERTY()
	FExpressionInput IOR;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) override;
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
#endif
	//~ End UMaterialExpression Interface
};
