// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionStrata.generated.h"


/**
 * Compile a special blend function for strata when blending material attribute
 *
 * @param Compiler				The compiler to add code to
 * @param Foreground			Entry A, has a bigger impact when Alpha is close to 0
 * @param Background			Entry B, has a bigger impact when Alpha is close to 1
 * @param Alpha					Blend factor [0..1], when 0
 * @return						Index to a new code chunk
 */
extern int32 CompileStrataBlendFunction(FMaterialCompiler* Compiler, const int32 A, const int32 B, const int32 Alpha);


///////////////////////////////////////////////////////////////////////////////
// BSDF nodes

// UMaterialExpressionStrataBSDF can only be used for Strata nodes ouputing StrataData that would need a preview,
UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, Abstract, DisplayName = "Strata Expression")
class UMaterialExpressionStrataBSDF : public UMaterialExpression
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


UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Legacy Conversion")
class UMaterialExpressionStrataLegacyConversion : public UMaterialExpressionStrataBSDF
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
	
	/** SubsurfaceProfile, for Screen Space Subsurface Scattering. The profile needs to be set up on both the Strata diffuse node, and the material node at the moment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Material, meta = (DisplayName = "Subsurface Profile"))
	TObjectPtr<class USubsurfaceProfile> SubsurfaceProfile;

	/** Store converted material models. */
	UPROPERTY()
	FStrataMaterialInfo ConvertedStrataMaterialInfo;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual const TArray<FExpressionInput*> GetInputs() override;

	bool HasSSS() const;
	bool HasAnisotropy() const;

#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Slab")
class UMaterialExpressionStrataSlabBSDF : public UMaterialExpressionStrataBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Defines the overall color of the Material. (type = float3, unit = unitless, defaults to 0.18)
	 */
	UPROPERTY()
	FExpressionInput BaseColor;

	/**
	 * Defines the edge color of the Material. This is only applied on metallic material (type = float3, unit = unitless, defaults to 1.0)
	 */
	UPROPERTY()
	FExpressionInput EdgeColor;

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
	 * Defines the diffused albedo, the percentage of light reflected as diffuse from the surface. (type = float3, unit = unitless, defaults to 0.18)
	 */
	UPROPERTY()
	FExpressionInput DiffuseAlbedo;

	/**
	 * Defines F0, the percentage of light reflected as specular from a surface when the view is perpendicular to the surface. (type = float3, unit = unitless, defaults to plastic 0.04)
	 */
	UPROPERTY()
	FExpressionInput F0;

	/**
	 * Defines F90, the percentage of light reflected as specular from a surface when the view is tangent to the surface. (type = float3, unit = unitless, defaults to 1.0f)
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
	 * The slab thickness. (type = float, centimeters, default = 0.01 centimeter = 0.1 millimeter)
	 */
	UPROPERTY()
	FExpressionInput Thickness;

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

	/** SubsurfaceProfile, for Screen Space Subsurface Scattering. The profile needs to be set up on both the Strata diffuse node, and the material node at the moment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Material, meta = (DisplayName = "Subsurface Profile"))
	TObjectPtr<class USubsurfaceProfile> SubsurfaceProfile;

	/** Whether to use the metalness workflow relying on BaseColor, Specular, EdgeColor and Metallic inputs. Or use the DiffuseColor, F0 and F90 specification. */
	UPROPERTY(EditAnywhere, Category = Mode)
	uint32 bUseMetalness : 1;

	/** Whether to use light diffusion (i.e., SSS diffusion) or wrap-approximation for material with scattering behavior. */
	UPROPERTY(EditAnywhere, Category = Mode, meta = (DisplayName = "Use Subsurface Diffusion"))
	uint32 bUseSSSDiffusion : 1;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual const TArray<FExpressionInput*> GetInputs() override;

	bool HasEdgeColor() const;
	bool HasFuzz() const;
	bool HasSecondRoughness() const;
	bool HasSSS() const;
	bool HasSSSProfile() const;
	bool HasMFPPluggedIn() const;
	bool HasAnisotropy() const;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Simple Clear Coat")
class UMaterialExpressionStrataSimpleClearCoatBSDF : public UMaterialExpressionStrataBSDF
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
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual const TArray<FExpressionInput*> GetInputs() override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Volumetric-Fog-Cloud BSDF")
class UMaterialExpressionStrataVolumetricFogCloudBSDF : public UMaterialExpressionStrataBSDF
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
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Unlit BSDF")
class UMaterialExpressionStrataUnlitBSDF : public UMaterialExpressionStrataBSDF
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

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Hair BSDF")
class UMaterialExpressionStrataHairBSDF : public UMaterialExpressionStrataBSDF
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
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Eye BSDF")
class UMaterialExpressionStrataEyeBSDF : public UMaterialExpressionStrataBSDF
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
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Single Layer Water BSDF")
class UMaterialExpressionStrataSingleLayerWaterBSDF : public UMaterialExpressionStrataBSDF
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
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Light Function")
class UMaterialExpressionStrataLightFunction : public UMaterialExpressionStrataBSDF
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
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Post Process")
class UMaterialExpressionStrataPostProcess : public UMaterialExpressionStrataBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * The output color of the post process: it represents a color added over the back buffer, or a color multiplied if the Strata blend mode is TransmittanceOnly.
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
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Convert To Decal")
class UMaterialExpressionStrataConvertToDecal : public UMaterialExpressionStrataBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * The Strata material to convert to a decal.
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
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};



///////////////////////////////////////////////////////////////////////////////
// Operator nodes

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Horizontal Blend")
class UMaterialExpressionStrataHorizontalMixing : public UMaterialExpressionStrataBSDF
{
	GENERATED_UCLASS_BODY()
		
	/**
	 * Strata material
	 */
	UPROPERTY()
	FExpressionInput Background;

	/**
	 * Strata material
	 */
	UPROPERTY()
	FExpressionInput Foreground;

	/**
	 * Lerp factor between Background (Mix == 0) and Foreground (Mix == 1).
	 */
	UPROPERTY()
	FExpressionInput Mix;

	UPROPERTY(EditAnywhere, Category = Mode)
	uint32 bUseParameterBlending : 1;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Vertical Layer")
class UMaterialExpressionStrataVerticalLayering : public UMaterialExpressionStrataBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Strata material layer on top of the Base material layer
	 */
	UPROPERTY()
	FExpressionInput Top;
	
	/**
	 * Strata material layer below the Top material layer
	 */
	UPROPERTY()
	FExpressionInput Base;

	UPROPERTY(EditAnywhere, Category = Mode)
	uint32 bUseParameterBlending : 1;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Add")
class UMaterialExpressionStrataAdd : public UMaterialExpressionStrataBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Strata material
	 */
	UPROPERTY()
	FExpressionInput A;
	
	/**
	 * Strata material
	 */
	UPROPERTY()
	FExpressionInput B;

	UPROPERTY(EditAnywhere, Category = Mode)
	uint32 bUseParameterBlending : 1;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Coverage Weight")
class UMaterialExpressionStrataWeight : public UMaterialExpressionStrataBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Strata material
	 */
	UPROPERTY()
	FExpressionInput A;
	
	/**
	 * Weight to apply to the strata material BSDFs
	 */
	UPROPERTY()
	FExpressionInput Weight;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Thin-Film")
class UMaterialExpressionStrataThinFilm : public UMaterialExpressionStrataBSDF
{
	GENERATED_UCLASS_BODY()

	/**
	 * Strata material
	 */
	UPROPERTY()
	FExpressionInput A;
	
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
	virtual bool IsResultStrataMaterial(int32 OutputIndex) override;
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) override;
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};


///////////////////////////////////////////////////////////////////////////////
// Utilities

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, Abstract, DisplayName = "Strata Utility Base Class")
class UMaterialExpressionStrataUtilityBase : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()
};

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Transmittance-To-MeanFreePath")
class UMaterialExpressionStrataTransmittanceToMFP : public UMaterialExpressionStrataUtilityBase
{
	GENERATED_UCLASS_BODY()

	/**
	* The colored transmittance for a view perpendicular to the surface. The transmittance for other view orientations will automatically be deduced according to surface thickness.
	*/
	UPROPERTY()
	FExpressionInput TransmittanceColor;

	/**
	* The desired thickness in centimeter. This can be set lower than 0.1mm (= 0.01cm) to enable the Thin lighting model on the slab node for instance.
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

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Metalness-To-DiffuseColorF0")
class UMaterialExpressionStrataMetalnessToDiffuseAlbedoF0 : public UMaterialExpressionStrataUtilityBase
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

UCLASS(MinimalAPI, collapsecategories, hidecategories = Object, DisplayName = "Strata Haziness-To-Secondary-Roughness")
class UMaterialExpressionStrataHazinessToSecondaryRoughness : public UMaterialExpressionStrataUtilityBase
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

