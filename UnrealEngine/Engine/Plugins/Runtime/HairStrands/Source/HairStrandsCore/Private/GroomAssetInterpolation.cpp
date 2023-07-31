// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomAssetInterpolation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GroomAssetInterpolation)

FHairDecimationSettings::FHairDecimationSettings()
{
	CurveDecimation = 1;
	VertexDecimation = 1;
}

FHairInterpolationSettings::FHairInterpolationSettings()
{
	bOverrideGuides = false;
	HairToGuideDensity = 0.1f;
	InterpolationQuality = EHairInterpolationQuality::High;
	InterpolationDistance = EHairInterpolationWeight::Parametric;
	bRandomizeGuide = false;
	bUseUniqueGuide = false;
}

FHairDeformationSettings::FHairDeformationSettings()
{
	bCanEditRigging = false;
	bEnableRigging = false;
	NumCurves = 10;
	NumPoints = 4;
}

FHairGroupsInterpolation::FHairGroupsInterpolation()
{
	DecimationSettings = FHairDecimationSettings();
	InterpolationSettings = FHairInterpolationSettings();
}

FHairGroupsLOD::FHairGroupsLOD()
{
	ClusterWorldSize = 1; // 1cm diameter
	ClusterScreenSizeScale = 1;
}

bool FHairLODSettings::operator==(const FHairLODSettings& A) const
{
	return
		VertexDecimation == A.VertexDecimation &&
		AngularThreshold == A.AngularThreshold &&
		CurveDecimation == A.CurveDecimation &&
		ThicknessScale == A.ThicknessScale &&
		bVisible == A.bVisible &&
		ScreenSize == A.ScreenSize;
}

bool FHairGroupsLOD::operator==(const FHairGroupsLOD& A) const
{
	if (LODs.Num() != A.LODs.Num())
	{
		return false;
	}

	for (uint32 LODIt = 0, LODCount = LODs.Num(); LODIt < LODCount; ++LODIt)
	{
		if (!(LODs[LODIt] == A.LODs[LODIt]))
		{
			return false;
		}
	}

	return
		ClusterWorldSize == A.ClusterWorldSize &&
		ClusterScreenSizeScale == A.ClusterScreenSizeScale;
}


bool FHairDecimationSettings::operator==(const FHairDecimationSettings& A) const
{
	return
		CurveDecimation == A.CurveDecimation &&
		VertexDecimation == A.VertexDecimation;
}

bool FHairInterpolationSettings::operator==(const FHairInterpolationSettings& A) const
{
	return
		bOverrideGuides == A.bOverrideGuides &&
		HairToGuideDensity == A.HairToGuideDensity &&
		InterpolationQuality == A.InterpolationQuality &&
		InterpolationDistance == A.InterpolationDistance &&
		bRandomizeGuide == A.bRandomizeGuide &&
		bUseUniqueGuide == A.bUseUniqueGuide;
}

bool FHairGroupsInterpolation::operator==(const FHairGroupsInterpolation& A) const
{
	return
		DecimationSettings == A.DecimationSettings &&
		InterpolationSettings == A.InterpolationSettings;
}

bool FHairDeformationSettings::operator==(const FHairDeformationSettings& A) const
{
	return
		bEnableRigging == A.bEnableRigging &&
		NumCurves == A.NumCurves &&
		NumPoints == A.NumPoints &&
		bCanEditRigging == A.bCanEditRigging;
}

void FHairGroupsInterpolation::BuildDDCKey(FArchive& Ar)
{
	Ar << DecimationSettings.CurveDecimation;
	Ar << DecimationSettings.VertexDecimation;
	Ar << InterpolationSettings.bOverrideGuides;
	Ar << InterpolationSettings.HairToGuideDensity;
	Ar << InterpolationSettings.InterpolationQuality;
	Ar << InterpolationSettings.InterpolationDistance;
	Ar << InterpolationSettings.bRandomizeGuide;
	Ar << InterpolationSettings.bUseUniqueGuide;
	Ar << RiggingSettings.NumCurves;
	Ar << RiggingSettings.NumPoints;
	Ar << RiggingSettings.bEnableRigging;
	Ar << RiggingSettings.bCanEditRigging;
}

void FHairGroupsLOD::BuildDDCKey(FArchive& Ar)
{
	Ar << ClusterWorldSize;
	Ar << ClusterScreenSizeScale;

	for (FHairLODSettings& LOD : LODs)
	{
		if (LOD.GeometryType == EGroomGeometryType::Strands)
		{
			Ar << LOD.CurveDecimation;
			Ar << LOD.VertexDecimation;
			Ar << LOD.AngularThreshold;
			Ar << LOD.ScreenSize;
			Ar << LOD.ThicknessScale;
		}
	}
}
