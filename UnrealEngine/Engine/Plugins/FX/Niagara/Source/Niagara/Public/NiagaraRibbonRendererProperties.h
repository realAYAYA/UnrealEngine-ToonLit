// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraRibbonRendererProperties.generated.h"

class FNiagaraEmitterInstance;
class FAssetThumbnailPool;
class SWidget;

UENUM()
enum class ENiagaraRibbonFacingMode : uint8
{
	/** Have the ribbon face the screen. */
	Screen = 0,

	/** Use Particles.RibbonFacing as the facing vector. */
	Custom,

	/** Use Particles.RibbonFacing as the side vector, and calculate the facing vector from that.
	 *  Using ribbon twist with this mode is NOT supported.
	 */
	CustomSideVector
};

/** Defines different modes for offsetting UVs by age when ordering ribbon particles using normalized age. */
UENUM()
enum class ENiagaraRibbonAgeOffsetMode : uint8
{
	/** Offset the UVs by age for smooth texture movement, but scale the 0-1 UV range to the current normalized age range of the particles. */
	Scale,
	/** Offset the UVs by age for smooth texture movement, but use the normalized age range directly as the UV range which will clip the texture for normalized age ranges that don't go from 0-1. */
	Clip
};

/** This enum decides in which order the ribbon segments will be rendered */
UENUM()
enum class ENiagaraRibbonDrawDirection : uint8
{
	FrontToBack,
	BackToFront
};

UENUM()
enum class ENiagaraRibbonShapeMode : uint8
{
	/** Default shape, flat plane facing the camera. */
	Plane,
	/** Multiple Planes evenly rotated around the axis to 180 degrees. */
	MultiPlane,
	/** 3D Tube shape, from triangular to cylindrical depending on vertex count. */
	Tube,
	/** Custom shape, defined by cross section. */
	Custom
};

USTRUCT()
struct FNiagaraRibbonShapeCustomVertex
{
	GENERATED_BODY();

	FNiagaraRibbonShapeCustomVertex();

	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering")
	FVector2f Position;

	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering")
	FVector2f Normal;

	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering")
	float TextureV;
};

UENUM()
enum class ENiagaraRibbonTessellationMode : uint8
{
	/** Default tessellation parameters. */
	Automatic,
	/** Custom tessellation parameters. */
	Custom,
	/** Disable tessellation entirely. */
	Disabled
};

/** Specifies options for handling UVs at the leading and trailing edges of ribbons. */
UENUM()
enum class ENiagaraRibbonUVEdgeMode
{
	/** The UV value at the edge will smoothly transition across the segment using normalized age.
	This will result in	UV values which are outside of the standard 0-1 range and works best with
	clamped textures. */
	SmoothTransition,
	/** The UV value at the edge will be locked to 0 at the leading edge, or locked to 1 at the
	Trailing edge. */
	Locked,
};

/** Specifies options for distributing UV values across ribbon segments. */
UENUM()
enum class ENiagaraRibbonUVDistributionMode
{
	/** Ribbon UVs will stretch the length of the ribbon, without repeating, but distributed by segment, so can be uneven with unequal length segments. */
	ScaledUniformly UMETA(DisplayName = "Uniform Scale (By Segment)"),
	/** Ribbon UVs will stretch the length of the ribbon, without repeating, but account for segment length to make an even distribution the entire length of the ribbon. */
	ScaledUsingRibbonSegmentLength UMETA(DisplayName = "Non-Uniform Scale (By Total Length)"),
	/** Ribbon UVs will be tiled along the length of the ribbon evenly, based on TilingLength setting. */
	TiledOverRibbonLength UMETA(DisplayName = "Tiled (By Segment Length)"),
	/** Ribbon UVs will be tiled along the length of the ribbon evenly, based on RibbonUVDistance parameter and the TilingLength scale value, to create 'traintrack' style UVs. NOTE: Dependent on Particle Attribute RibbonUVDistance */
	TiledFromStartOverRibbonLength UMETA(DisplayName = "Tiled By Distance (By Particles.RibbonUVDistance)")
};



/** Defines settings for UV behavior for a UV channel on ribbons. */
USTRUCT()
struct FNiagaraRibbonUVSettings
{
	GENERATED_BODY();

	FNiagaraRibbonUVSettings();


	/** Specifies how ribbon UVs are distributed along the length of a ribbon. */
	UPROPERTY(EditAnywhere, Category = UVs, meta = (DisplayName="UV Mode", EditCondition = "!bEnablePerParticleUOverride"))
	ENiagaraRibbonUVDistributionMode DistributionMode;

	/** Specifies how UVs transition into life at the leading edge of the ribbon. */
	UPROPERTY(EditAnywhere, Category = UVs, meta = (DisplayName="Leading Edge Transition", EditCondition = "!bEnablePerParticleUOverride && DistributionMode != ENiagaraRibbonUVDistributionMode::TiledFromStartOverRibbonLength"))
	ENiagaraRibbonUVEdgeMode LeadingEdgeMode;

	/** Specifies how UVs transition out of life at the trailing edge of the ribbon. */
	UPROPERTY(EditAnywhere, Category = UVs, meta = (DisplayName="Trailing Edge Transition", EditCondition = "!bEnablePerParticleUOverride && DistributionMode != ENiagaraRibbonUVDistributionMode::TiledOverRibbonLength && DistributionMode != ENiagaraRibbonUVDistributionMode::TiledFromStartOverRibbonLength"))
	ENiagaraRibbonUVEdgeMode TrailingEdgeMode;

	/** Specifies the length in world units to use when tiling UVs across the ribbon when using one of the tiled distribution modes. */
	UPROPERTY(EditAnywhere, Category = UVs, meta = (EditCondition="!bEnablePerParticleUOverride && DistributionMode == ENiagaraRibbonUVDistributionMode::TiledOverRibbonLength || DistributionMode == ENiagaraRibbonUVDistributionMode::TiledFromStartOverRibbonLength"))
	float TilingLength;

	/** Specifies an additional offset which is applied to the UV range */
	UPROPERTY(EditAnywhere, Category = UVs)
	FVector2D Offset;

	/** Specifies an additional scaler which is applied to the UV range. */
	UPROPERTY(EditAnywhere, Category = UVs)
	FVector2D Scale;

	/** Enables overriding the U component with values read from the particles. When enabled, edge behavior and distribution are ignored. */
	UPROPERTY(EditAnywhere, Category = UVs)
	bool bEnablePerParticleUOverride;

	/** Enables overriding the range of the V component with values read from the particles. */
	UPROPERTY(EditAnywhere, Category = UVs)
	bool bEnablePerParticleVRangeOverride;
};

namespace ENiagaraRibbonVFLayout
{
	enum Type
	{
		Position,
		Velocity,
		Color,
		Width,
		Twist,
		Facing,
		NormalizedAge,
		MaterialRandom,
		MaterialParam0,
		MaterialParam1,
		MaterialParam2,
		MaterialParam3,
		DistanceFromStart,
		U0Override,
		V0RangeOverride,
		U1Override,
		V1RangeOverride,
		PrevPosition,
		PrevRibbonWidth,
		PrevRibbonFacing,
		PrevRibbonTwist,
		LinkOrder,
		Num,
	};
};

UCLASS(editinlinenew, meta = (DisplayName = "Ribbon Renderer"))
class NIAGARA_API UNiagaraRibbonRendererProperties : public UNiagaraRendererProperties
{
public:
	GENERATED_BODY()

	UNiagaraRibbonRendererProperties();

	//UObject Interface
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void RenameVariable(const FNiagaraVariableBase& OldVariable, const FNiagaraVariableBase& NewVariable, const FVersionedNiagaraEmitter& InEmitter) override;
	virtual void RemoveVariable(const FNiagaraVariableBase& OldVariable, const FVersionedNiagaraEmitter& InEmitter) override;
#endif
	//UObject Interface END

	static void InitCDOPropertiesAfterModuleStartup();

	//UNiagaraRendererProperties Interface
	virtual FNiagaraRenderer* CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, const FNiagaraEmitterInstance* Emitter, const FNiagaraSystemInstanceController& InController) override;
	virtual class FNiagaraBoundsCalculator* CreateBoundsCalculator() override;
	virtual void GetUsedMaterials(const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& OutMaterials) const override;
	virtual const FVertexFactoryType* GetVertexFactoryType() const override;
	virtual bool IsBackfaceCullingDisabled() const override;
	virtual bool IsSimTargetSupported(ENiagaraSimTarget InSimTarget) const override { return true; };
	virtual bool PopulateRequiredBindings(FNiagaraParameterStore& InParameterStore) override;

#if WITH_EDITOR
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
	virtual void GetAdditionalVariables(TArray<FNiagaraVariableBase>& OutArray) const override;
	virtual FNiagaraVariable GetBoundAttribute(const FNiagaraVariableAttributeBinding* Binding) const override;
	virtual void GetRendererWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const override;
	virtual void GetRendererTooltipWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const override;
	virtual void GetRendererFeedback(const FVersionedNiagaraEmitter& InEmitter, TArray<FNiagaraRendererFeedback>& OutErrors, TArray<FNiagaraRendererFeedback>& OutWarnings, TArray<FNiagaraRendererFeedback>& OutInfo) const override;

	virtual TArray<FNiagaraVariable> GetBoundAttributes() const override; 
#endif
	virtual void CacheFromCompiledData(const FNiagaraDataSetCompiledData* CompiledData) override;
	
#if WITH_EDITORONLY_DATA
	virtual bool IsSupportedVariableForBinding(const FNiagaraVariableBase& InSourceForBinding, const FName& InTargetBindingName) const override;
#endif
	//UNiagaraRendererProperties Interface END

	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering")
	TObjectPtr<UMaterialInterface> Material;

	/** Use the UMaterialInterface bound to this user variable if it is set to a valid value. If this is bound to a valid value and Material is also set, UserParamBinding wins.*/
	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering")
	FNiagaraUserParameterBinding MaterialUserParamBinding;

	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering")
	ENiagaraRibbonFacingMode FacingMode;

	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering", meta=(DisplayName="UV0 Settings"))
	FNiagaraRibbonUVSettings UV0Settings;

	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering", meta=(DisplayName="UV1 Settings"))
	FNiagaraRibbonUVSettings UV1Settings;

#if WITH_EDITORONLY_DATA
private:
	/** Tiles UV0 based on the distance traversed by the ribbon. Disables offsetting UVs by age. */
	UPROPERTY()
	float UV0TilingDistance_DEPRECATED;
	UPROPERTY()
	FVector2D UV0Scale_DEPRECATED;
	UPROPERTY()
	FVector2D UV0Offset_DEPRECATED;

	/** Defines the mode to use when offsetting UV channel 0 by age which enables smooth texture movement when particles are added and removed at the end of the ribbon.  Not used when the RibbonLinkOrder binding is in use or when tiling distance is in use. */
	UPROPERTY()
	ENiagaraRibbonAgeOffsetMode UV0AgeOffsetMode_DEPRECATED;

	/** Tiles UV1 based on the distance traversed by the ribbon. Disables offsetting UVs by age. */
	UPROPERTY()
	float UV1TilingDistance_DEPRECATED;
	UPROPERTY()
	FVector2D UV1Scale_DEPRECATED;
	UPROPERTY()
	FVector2D UV1Offset_DEPRECATED;

	/** Defines the mode to use when offsetting UV channel 1 by age which enables smooth texture movement when particles are added and removed at the end of the ribbon.  Not used when the RibbonLinkOrder binding is in use or when tiling distance is in use. */
	UPROPERTY()
	ENiagaraRibbonAgeOffsetMode UV1AgeOffsetMode_DEPRECATED;
#endif

public:

	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering")	
	int32 MaxNumRibbons;
	
	/**
	*	Whether we use the CPU or GPU to generate ribbon geometry for CPU systems.
	*	GPU systems will always use a fully GPU initialization pipeline,
	*	Will fall back to CPU init when GPU init isn't available.
	*/ 
	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering")	
	bool bUseGPUInit;


	

	/** If true, the particles are only sorted when using a translucent material. */
	UPROPERTY(EditAnywhere, Category = "Ribbon Rendering")
	ENiagaraRibbonDrawDirection DrawDirection;

	/** Shape of the ribbon, from flat plane, multiplane, 3d tube, and custom shapes. */
	UPROPERTY(EditAnywhere, Category = "Ribbon Shape")
	ENiagaraRibbonShapeMode Shape;

	/** Disables two-sided forced rendering (Will still respect material settings)
	  * MultiPlane will double geometry count to have triangles facing both sides. With this off MultiPlane will switch normal direction to face view.
	  * 3D Ribbons will render like normal meshes with backface culling enabled.
	  */
	UPROPERTY(EditAnywhere, Category = "Ribbon Shape", meta = (EditCondition = "Shape != ENiagaraRibbonShapeMode::Plane"))
	bool bEnableAccurateGeometry;

	/** Tessellation factor to apply to the width of the ribbon.
	* Ranges from 1 to 16. Greater values increase amount of tessellation.
	*/
	UPROPERTY(EditAnywhere, Category = "Ribbon Shape", meta = (EditCondition = "Shape == ENiagaraRibbonShapeMode::Plane || Shape == ENiagaraRibbonShapeMode::MultiPlane", ClampMin = "1", ClampMax = "16"))
	int32 WidthSegmentationCount;

	/** Number of planes in multiplane shape. Evenly distributed from 0-90 or 0-180 degrees off camera facing depending on setting */
	UPROPERTY(EditAnywhere, Category = "Ribbon Shape", meta = (EditCondition = "Shape == ENiagaraRibbonShapeMode::MultiPlane", ClampMin = "2", ClampMax = "8"))
	int32 MultiPlaneCount;

	/** Number of vertices/faces in a tube.  */
	UPROPERTY(EditAnywhere, Category = "Ribbon Shape", meta = (EditCondition = "Shape == ENiagaraRibbonShapeMode::Tube", ClampMin = "3", ClampMax = "16"))
	int32 TubeSubdivisions;

	/** Vertices for a cross section of the ribbon in custom shape mode. */
	UPROPERTY(EditAnywhere, Category = "Ribbon Shape", meta = (EditCondition = "Shape == ENiagaraRibbonShapeMode::Custom"))
	TArray<FNiagaraRibbonShapeCustomVertex> CustomVertices;


	/** Defines the curve tension, or how long the curve's tangents are.
	  * Ranges from 0 to 1. The higher the value, the sharper the curve becomes.
	  */
	UPROPERTY(EditAnywhere, Category = "Tessellation", meta = (ClampMin = "0", ClampMax = "0.99"))
	float CurveTension;

	/** Defines the tessellation mode allowing custom tessellation parameters or disabling tessellation entirely. */
	UPROPERTY(EditAnywhere, Category = "Tessellation", meta = (DisplayName = "Mode"))
	ENiagaraRibbonTessellationMode TessellationMode;

	/** Custom tessellation factor.
	  * Ranges from 1 to 16. Greater values increase amount of tessellation.
	  */
	UPROPERTY(EditAnywhere, Category = "Tessellation", meta = (DisplayName = "Max Tessellation Factor", ClampMin = "1", ClampMax = "16"))
	int32 TessellationFactor;

	/** If checked, use the above constant factor. Otherwise, adaptively select the tessellation factor based on the below parameters. */
	UPROPERTY(EditAnywhere, Category = "Tessellation")
	bool bUseConstantFactor;

	/** Defines the angle in degrees at which tessellation occurs.
	  * Ranges from 1 to 180. Smaller values increase amount of tessellation.
	  * If set to 0, use the maximum tessellation set above.
	  */
	UPROPERTY(EditAnywhere, Category = "Tessellation", meta = (EditCondition = "!bUseConstantFactor", ClampMin = "0", ClampMax = "180", UIMin = "1", UIMax = "180"))
	float TessellationAngle;

	/** If checked, use the ribbon's screen space percentage to adaptively adjust the tessellation factor. */
	UPROPERTY(EditAnywhere, Category = "Tessellation", meta = (DisplayName = "Screen Space", EditCondition = "!bUseConstantFactor"))
	bool bScreenSpaceTessellation;

	/** Which attribute should we use for position when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding PositionBinding;

	/** Which attribute should we use for color when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding ColorBinding;

	/** Which attribute should we use for velocity when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding VelocityBinding;

	/** Which attribute should we use for normalized age when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding NormalizedAgeBinding;

	/** Which attribute should we use for ribbon twist when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding RibbonTwistBinding;

	/** Which attribute should we use for ribbon width when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding RibbonWidthBinding;

	/** Which attribute should we use for ribbon facing when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding RibbonFacingBinding;

	/** Which attribute should we use for ribbon id when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding RibbonIdBinding;

	/** Which attribute should we use for RibbonLinkOrder when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding RibbonLinkOrderBinding;

	/** Which attribute should we use for MaterialRandom when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding MaterialRandomBinding;

	/** Which attribute should we use for dynamic material parameters when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding DynamicMaterialBinding;

	/** Which attribute should we use for dynamic material parameters when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding DynamicMaterial1Binding;

	/** Which attribute should we use for dynamic material parameters when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding DynamicMaterial2Binding;

	/** Which attribute should we use for dynamic material parameters when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding DynamicMaterial3Binding;

	/** Which attribute should we use for ribbon distance traveled for use in UV operations when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding RibbonUVDistance;

	/** Which attribute should we use for UV0 U when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding U0OverrideBinding;

	/** Which attribute should we use for UV0 V when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding V0RangeOverrideBinding;

	/** Which attribute should we use for UV1 U when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding U1OverrideBinding;

	/** Which attribute should we use for UV1 V when generating ribbons?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding V1RangeOverrideBinding;

	/** If this array has entries, we will create a MaterialInstanceDynamic per Emitter instance from Material and set the Material parameters using the Niagara simulation variables listed.*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraRendererMaterialParameters MaterialParameters;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FNiagaraMaterialAttributeBinding> MaterialParameterBindings_DEPRECATED;
#endif

	/** Implicit binding for previous position */
	UPROPERTY(Transient)
	FNiagaraVariableAttributeBinding PrevPositionBinding;

	/** Implicit binding for previous ribbon width */
	UPROPERTY(Transient)
	FNiagaraVariableAttributeBinding PrevRibbonWidthBinding;

	/** Implicit binding for previous ribbon facing */
	UPROPERTY(Transient)
	FNiagaraVariableAttributeBinding PrevRibbonFacingBinding;

	/** Implicit binding for previous ribbon twist */
	UPROPERTY(Transient)
	FNiagaraVariableAttributeBinding PrevRibbonTwistBinding;


	bool								bSortKeyDataSetAccessorIsAge = false;
	FNiagaraDataSetAccessor<float>		SortKeyDataSetAccessor;
	FNiagaraDataSetAccessor<FNiagaraPosition>	PositionDataSetAccessor;
	FNiagaraDataSetAccessor<float>		NormalizedAgeAccessor;
	FNiagaraDataSetAccessor<float>		SizeDataSetAccessor;
	FNiagaraDataSetAccessor<float>		TwistDataSetAccessor;
	FNiagaraDataSetAccessor<FVector3f>	FacingDataSetAccessor;
	FNiagaraDataSetAccessor<FVector4f>	MaterialParam0DataSetAccessor;
	FNiagaraDataSetAccessor<FVector4f>	MaterialParam1DataSetAccessor;
	FNiagaraDataSetAccessor<FVector4f>	MaterialParam2DataSetAccessor;
	FNiagaraDataSetAccessor<FVector4f>	MaterialParam3DataSetAccessor;
	bool								DistanceFromStartIsBound;
	bool								U0OverrideIsBound;
	bool								U1OverrideIsBound;

	FNiagaraDataSetAccessor<int32>		RibbonIdDataSetAccessor;
	FNiagaraDataSetAccessor<FNiagaraID>	RibbonFullIDDataSetAccessor;
	
	FNiagaraDataSetAccessor<float>		RibbonLinkOrderDataSetAccessor;

	uint32 MaterialParamValidMask = 0;
	FNiagaraRendererLayout RendererLayout;

protected:
	void InitBindings();
	void SetPreviousBindings(const FVersionedNiagaraEmitter& SrcEmitter);

	void UpdateSourceModeDerivates(ENiagaraRendererSourceDataMode InSourceMode, bool bFromPropertyEdit);

	virtual bool NeedsMIDsForMaterials() const { return MaterialParameters.HasAnyBindings(); }
private: 
	static TArray<TWeakObjectPtr<UNiagaraRibbonRendererProperties>> RibbonRendererPropertiesToDeferredInit;
};
