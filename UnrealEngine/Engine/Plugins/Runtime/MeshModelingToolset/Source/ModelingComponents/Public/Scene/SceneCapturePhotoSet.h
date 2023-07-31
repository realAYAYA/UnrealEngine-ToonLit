// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VectorTypes.h"
#include "Image/SpatialPhotoSet.h"
#include "Scene/WorldRenderCapture.h"

class UWorld;
class AActor;
struct FScopedSlowTask;

namespace UE
{
namespace Geometry
{

/**
 * FSceneCapturePhotoSet creates a set of render captures for a given World and set of Actors,
 * stored as a SpatialPhotoSet for each desired render buffer type. Currently the set of buffers are
 * defined by ERenderCaptureType:
 * 
 *		BaseColor
 *		Roughness
 *		Metallic
 *		Specular
 *		PackedMRS   (Metallic / Roughness / Specular)
 *		Emissive
 *		WorldNormal
 *		DeviceDepth
 *
 * There are various efficiences possible by doing these captures as a group, rather
 * than doing each one individually.
 * 
 * One the capture set is computed, the ComputeSample() function can be used to 
 * call SpatialPhotoSet::ComputeSample() on each photo set, ie to estimate the
 * value of the different channels at a given 3D position/normal by raycasting
 * against the photo set. Again, it can be more efficient to do this on the
 * group, rather than each individually.
 * 
 */
class MODELINGCOMPONENTS_API FSceneCapturePhotoSet
{
public:

	FSceneCapturePhotoSet();

	/**
	 * Set the target World and set of Actors
	 */
	void SetCaptureSceneActors(UWorld* World, const TArray<AActor*>& Actors);

	/**
	 * Enable/Disable a particular capture type
	 */
	void SetCaptureTypeEnabled(ERenderCaptureType CaptureType, bool bEnabled);
	bool GetCaptureTypeEnabled(ERenderCaptureType CaptureType) const;

	/**
	 * Configure the given capture type
	 */
	void SetCaptureConfig(ERenderCaptureType CaptureType, const FRenderCaptureConfig& Config);
	FRenderCaptureConfig GetCaptureConfig(ERenderCaptureType CaptureType) const;

	/**
	 * Add captures at the corners and face centers of the "view box",
	 * ie the bounding box that contains the view sphere (see AddExteriorCaptures)
	 */
	void AddStandardExteriorCapturesFromBoundingBox(
		FImageDimensions PhotoDimensions,
		double HorizontalFOVDegrees,
		double NearPlaneDist,
		bool bFaces,
		bool bUpperCorners,
		bool bLowerCorners,
		bool bUpperEdges,
		bool bSideEdges);

	/**
	 * Add captures on the "view sphere", ie a sphere centered/sized such that the target actors
	 * will be fully contained inside a square image rendered from locations on the sphere, where
	 * the view direction is towards the sphere center. The Directions array defines the directions.
	 */
	void AddExteriorCaptures(
		FImageDimensions PhotoDimensions,
		double HorizontalFOVDegrees,
		double NearPlaneDist,
		const TArray<FVector3d>& Directions);

	/**
	 * Post-process the various PhotoSets after capture, to reduce memory usage and sampling cost.
	 */
	void OptimizePhotoSets();


	/**
	 * FSceneSample stores a full sample of all possible channels, some
	 * values may be default-values though
	 */
	struct MODELINGCOMPONENTS_API FSceneSample
	{
		FRenderCaptureTypeFlags HaveValues;		// defines which channels have non-default values
		FVector3f BaseColor;
		float Roughness;
		float Specular;
		float Metallic;
		FVector3f Emissive;
		FVector3f WorldNormal;
		float DeviceDepth;

		FSceneSample();

		/** @return value for the given captured channel, or default value */
		FVector3f GetValue3f(ERenderCaptureType CaptureType) const;
		FVector4f GetValue4f(ERenderCaptureType CaptureType) const;
	};


	/**
	 * Sample the requested SampleChannels from the available PhotoSets to determine
	 * values at the given 3D Position/Normal. This calls TSpatialPhotoSet::ComputeSample()
	 * internally, see that function for more details.
	 * @param DefaultsInResultsOut this value is passed in by caller with suitable Default values, and returned with any available computed values updated
	 */
	bool ComputeSample(
		const FRenderCaptureTypeFlags& SampleChannels,
		const FVector3d& Position,
		const FVector3d& Normal,
		TFunctionRef<bool(const FVector3d&, const FVector3d&)> VisibilityFunction,
		FSceneSample& DefaultsInResultsOut) const;

	/**
	 * If ValidSampleDepthThreshold >  0 then the VisibilityFunction and the DeviceDepthPhotoSet will be used to determine
	 * sample validity. Using DeviceDepthPhotoSet will mitigate artefacts caused by samples which cannot be determined
	 * invalid by querying only the VisibilityFunction.
	 * If ValidSampleDepthThreshold <= 0 only VisibilityFunction will be used to determine sample validity
	 */
	bool ComputeSampleLocation(
		const FVector3d& Position,
		const FVector3d& Normal,
		float ValidSampleDepthThreshold,
		TFunctionRef<bool(const FVector3d&, const FVector3d&)> VisibilityFunction,
		int& PhotoIndex,
		FVector2d& PhotoCoords) const;

	/**
	 * @returns the value of the nearest pixel in the given photo for the given CaptureType
	 * Note: This function does not interpolate because this causes artefacts; two adjacent pixels on a photo can be
	 * from parts of the scene which are very far from each other so interpolating the values when makes no sense.
	 * Texture filtering is accomplished by the baking framework where it can use correctly localized information.
	 */
	template <ERenderCaptureType CaptureType>
	FVector4f ComputeSampleNearest(
		const int& PhotoIndex,
		const FVector2d& PhotoCoords,
		const FSceneSample& DefaultSample) const;

	const FSpatialPhotoSet3f& GetBaseColorPhotoSet() { return BaseColorPhotoSet; }
	const FSpatialPhotoSet1f& GetRoughnessPhotoSet() { return RoughnessPhotoSet; }
	const FSpatialPhotoSet1f& GetSpecularPhotoSet() { return SpecularPhotoSet; }
	const FSpatialPhotoSet1f& GetMetallicPhotoSet() { return MetallicPhotoSet; }
	const FSpatialPhotoSet3f& GetPackedMRSPhotoSet() { return PackedMRSPhotoSet; }
	const FSpatialPhotoSet3f& GetWorldNormalPhotoSet() { return WorldNormalPhotoSet; }
	const FSpatialPhotoSet3f& GetEmissivePhotoSet() { return EmissivePhotoSet; }
	const FSpatialPhotoSet1f& GetDeviceDepthPhotoSet() { return DeviceDepthPhotoSet; }

	/**
	 * Enable debug image writing. All captured images will be written to <Project>/Intermediate/<FolderName>.
	 * If FolderName is not specified, "SceneCapturePhotoSet" is used by default.
	 * See FWorldRenderCapture::SetEnableWriteDebugImage() for more details
	 */
	void SetEnableWriteDebugImages(bool bEnable, FString FolderName = FString());

	/**
	 * If enabled, any Component Scene Proxies in the level that are not meant to be included in
	 * the capture (ie not added via SetCaptureSceneActors), will be unregistered to hide them.
	 * This is generally not necessary, and disabled by default, but in some cases the Renderer
	 * may not be able to fully exclude the effects of an object via hidden/visible flags.
	 */
	void SetEnableVisibilityByUnregisterMode(bool bEnable);

	void SetAllowCancel(bool bAllowCancelIn)
	{
		bAllowCancel = bAllowCancelIn;
	}

	bool Cancelled() const
	{
		return bWasCancelled;
	}

protected:
	UWorld* TargetWorld = nullptr;
	TArray<AActor*> VisibleActors;

	bool bEnforceVisibilityViaUnregister = false;

	bool bEnableBaseColor = true;
	bool bEnableRoughness = false;
	bool bEnableSpecular = false;
	bool bEnableMetallic = false;
	bool bEnablePackedMRS = true;
	bool bEnableWorldNormal = true;
	bool bEnableEmissive = true;
	bool bEnableDeviceDepth = true;

	FSpatialPhotoSet3f BaseColorPhotoSet;
	FSpatialPhotoSet1f RoughnessPhotoSet;
	FSpatialPhotoSet1f SpecularPhotoSet;
	FSpatialPhotoSet1f MetallicPhotoSet;
	FSpatialPhotoSet3f PackedMRSPhotoSet;
	FSpatialPhotoSet3f WorldNormalPhotoSet;
	FSpatialPhotoSet3f EmissivePhotoSet;
	FSpatialPhotoSet1f DeviceDepthPhotoSet;

	FRenderCaptureConfig BaseColorConfig;
	FRenderCaptureConfig RoughnessConfig;
	FRenderCaptureConfig SpecularConfig;
	FRenderCaptureConfig MetallicConfig;
	FRenderCaptureConfig PackedMRSConfig;
	FRenderCaptureConfig WorldNormalConfig;
	FRenderCaptureConfig EmissiveConfig;
	FRenderCaptureConfig DeviceDepthConfig;

	TArray<FSpatialPhotoParams> PhotoSetParams;

	bool bWriteDebugImages = false;
	FString DebugImagesFolderName = TEXT("SceneCapturePhotoSet");

	bool bAllowCancel = false;
	bool bWasCancelled = false;
};


template <ERenderCaptureType CaptureType>
FVector4f FSceneCapturePhotoSet::ComputeSampleNearest(
	const int& PhotoIndex,
	const FVector2d& PhotoCoords,
	const FSceneSample& DefaultSample) const
{
	if constexpr (CaptureType == ERenderCaptureType::BaseColor)
	{
		FVector3f BaseColor = BaseColorPhotoSet.ComputeSampleNearest(PhotoIndex, PhotoCoords, DefaultSample.BaseColor);
		return FVector4f(BaseColor, 1.f);
	}

	if constexpr (CaptureType == ERenderCaptureType::Roughness)
	{
		float Roughness = RoughnessPhotoSet.ComputeSampleNearest(PhotoIndex, PhotoCoords, DefaultSample.Roughness);
		return FVector4f(Roughness, Roughness, Roughness, 1.f);
	}

	if constexpr (CaptureType == ERenderCaptureType::Specular)
	{
		float Specular = SpecularPhotoSet.ComputeSampleNearest(PhotoIndex, PhotoCoords, DefaultSample.Specular);
		return FVector4f(Specular, Specular, Specular, 1.f);
	}

	if constexpr (CaptureType == ERenderCaptureType::Metallic)
	{
		float Metallic = MetallicPhotoSet.ComputeSampleNearest(PhotoIndex, PhotoCoords, DefaultSample.Metallic);
		return FVector4f(Metallic, Metallic, Metallic, 1.f);
	}

	if constexpr (CaptureType == ERenderCaptureType::CombinedMRS)
	{
		FVector3f MRSValue(DefaultSample.Metallic, DefaultSample.Roughness, DefaultSample.Specular);
		MRSValue = PackedMRSPhotoSet.ComputeSampleNearest(PhotoIndex, PhotoCoords, MRSValue);
		return FVector4f(MRSValue, 1.f);
	}

	if constexpr (CaptureType == ERenderCaptureType::Emissive)
	{
		FVector3f Emissive = EmissivePhotoSet.ComputeSampleNearest(PhotoIndex, PhotoCoords, DefaultSample.Emissive);
		return FVector4f(Emissive, 1.f);
	}

	if constexpr (CaptureType == ERenderCaptureType::WorldNormal)
	{
		FVector3f WorldNormal = WorldNormalPhotoSet.ComputeSampleNearest(PhotoIndex, PhotoCoords, DefaultSample.WorldNormal);
		return FVector4f(WorldNormal, 1.f);
	}

	if constexpr (CaptureType == ERenderCaptureType::DeviceDepth)
	{
		float Depth = DeviceDepthPhotoSet.ComputeSampleNearest(PhotoIndex, PhotoCoords, DefaultSample.DeviceDepth);
		return FVector4f(Depth, Depth, Depth, 1.f);
	}

	ensure(false);
	return FVector4f::Zero();
}


} // end namespace UE::Geometry
} // end namespace UE
