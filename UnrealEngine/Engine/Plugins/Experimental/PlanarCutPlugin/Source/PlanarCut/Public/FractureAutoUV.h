// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#include "GeometryCollection/GeometryCollection.h"

#include "Image/ImageBuilder.h"

class FProgressCancel;

namespace UE { namespace PlanarCut {

enum class EUseMaterials
{
	AllMaterials,		// Include all materials
	OddMaterials,		// Include materials w/ odd IDs (+ any manually selected materials)
	NoDefaultMaterials	// No default materials; Only use manually selected materials
};

/**
 * Box project UVs
 *
 * @param TargetUVLayer		Which UV layer to update
 * @param Collection		The collection to be box projected
 * @param BoxDimensions		Scale of projection box
 * @param MaterialsPattern	Which pattern of material IDs to automatically consider for UV island layout
 * @param WhichMaterials	If non-empty, consider listed material IDs for UV island layout
 */
bool PLANARCUT_API BoxProjectUVs(
	int32 TargetUVLayer,
	FGeometryCollection& Collection,
	const FVector3d& BoxDimensions,
	EUseMaterials MaterialsPattern = EUseMaterials::OddMaterials,
	TArrayView<int32> WhichMaterials = TArrayView<int32>()
);

/**
 * Make a UV atlas of non-overlapping UV charts for a geometry collection
 *
 * @param TargetUVLayer		Which UV layer to update with new UV coordinates
 * @param Collection		The collection to be atlas'd
 * @param UVRes				Target resolution for the atlas
 * @param GutterSize		Space to leave between UV islands, in pixels at the target resolution
 * @param MaterialsPattern	Which pattern of material IDs to automatically consider for UV island layout
 * @param WhichMaterials	If non-empty, consider listed material IDs for UV island layout
 * @param bRecreateUVsForDegenerateIslands If true, detect and fix islands that don't have proper UVs (i.e. UVs all zero or otherwise collapsed to a point)
 */
bool PLANARCUT_API UVLayout(
	int32 TargetUVLayer,
	FGeometryCollection& Collection,
	int32 UVRes = 1024,
	float GutterSize = 1,
	EUseMaterials MaterialsPattern = EUseMaterials::OddMaterials,
	TArrayView<int32> WhichMaterials = TArrayView<int32>(),
	bool bRecreateUVsForDegenerateIslands = true,
	FProgressCancel* Progress = nullptr
);


// Different attributes we can bake
enum class EBakeAttributes : int32
{
	None,
	DistanceToExternal,
	AmbientOcclusion,
	Curvature,
	NormalX,
	NormalY,
	NormalZ,
	PositionX,
	PositionY,
	PositionZ
};

struct FTextureAttributeSettings
{
	double ToExternal_MaxDistance = 100.0;
	int AO_Rays = 32;
	double AO_BiasAngleDeg = 15.0;
	bool bAO_Blur = true;
	double AO_BlurRadius = 2.5;
	double AO_MaxDistance = 0.0; // 0.0 is interpreted as TNumericLimits<double>::Max()
	int Curvature_VoxelRes = 128;
	double Curvature_Winding = .5;
	int Curvature_SmoothingSteps = 10;
	double Curvature_SmoothingPerStep = .8;
	bool bCurvature_Blur = true;
	double Curvature_BlurRadius = 2.5;
	double Curvature_ThicknessFactor = 3.0; // distance to search for mesh correspondence, as a factor of voxel size
	double Curvature_MaxValue = .1; // curvatures above this value will be clamped
	int ClearGutterChannel = -1; // don't copy gutter values for this channel, if specified -- useful for visualizing the UV island borders
};


/**
 * Generate a texture for internal faces based on depth inside surface
 * TODO: add options to texture based on other quantities
 *
 * @param TargetUVLayer		Which UV layer to take UV coordinates from when creating the new texture
 * @param Collection		The collection to be create a new texture for
 * @param GutterSize		Number of texels to fill outside of UV island borders (values are copied from nearest inside pt)
 * @param BakeAttributes	Which attributes to bake into which color channel
 * @param AttributeSettings	Settings for the BakeAttributes
 * @param TextureOut		Texture to write to
 * @param MaterialsPattern	Which pattern of material IDs to apply texture to
 * @param WhichMaterials	If non-empty, apply texture to the listed material IDs
 */
bool PLANARCUT_API TextureInternalSurfaces(
	int32 TargetUVLayer,
	FGeometryCollection& Collection,
	int32 GutterSize,
	UE::Geometry::FIndex4i BakeAttributes,
	const FTextureAttributeSettings& AttributeSettings,
	UE::Geometry::TImageBuilder<FVector4f>& TextureOut,
	EUseMaterials MaterialsPattern = EUseMaterials::OddMaterials,
	TArrayView<int32> WhichMaterials = TArrayView<int32>(),
	FProgressCancel* Progress = nullptr
);

}} // namespace UE::PlanarCut