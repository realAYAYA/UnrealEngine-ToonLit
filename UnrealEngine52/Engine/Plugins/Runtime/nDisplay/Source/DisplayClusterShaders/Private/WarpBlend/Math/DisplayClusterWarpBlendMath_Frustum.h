// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WarpBlend/DisplayClusterWarpContext.h"
#include "WarpBlend/DisplayClusterWarpBlend_GeometryContext.h"

class IDisplayClusterViewport;

class FDisplayClusterWarpBlendMath_Frustum
{
public:
	FDisplayClusterWarpBlendMath_Frustum(const FDisplayClusterWarpEye& InEye, FDisplayClusterWarpBlend_GeometryContext& InGeometryContext)
		: GeometryContext(InGeometryContext)
		, Eye(InEye)
	{ }

public:
	bool CalcFrustum(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FDisplayClusterWarpContext& OutFrustum);

private:
	EDisplayClusterWarpBlendFrustumType       FrustumType = EDisplayClusterWarpBlendFrustumType::AABB;
	EDisplayClusterWarpBlendStereoMode         StereoMode = EDisplayClusterWarpBlendStereoMode::AsymmetricAABB;
	EDisplayClusterWarpBlendProjectionType ProjectionType = EDisplayClusterWarpBlendProjectionType::DynamicAABBCenter;
	bool bFindBestProjectionType = false;

public:
	void SetFindBestProjectionType(bool bValue) { bFindBestProjectionType = bValue; }
	void SetParameter(EDisplayClusterWarpBlendFrustumType     InParameterValue) { FrustumType = InParameterValue; }
	void SetParameter(EDisplayClusterWarpBlendStereoMode      InParameterValue) { StereoMode = InParameterValue; }
	void SetParameter(EDisplayClusterWarpBlendProjectionType  InParameterValue)
	{ 
		ProjectionType = InParameterValue;

		// Protect runtime methods:
		if (ProjectionType >= EDisplayClusterWarpBlendProjectionType::RuntimeProjectionModes)
		{
			// Runtime method disabled for console vars
			ProjectionType = EDisplayClusterWarpBlendProjectionType::DynamicAABBCenter;
		}
	}

private:

	inline bool ImplUpdateProjectionType()
	{
		switch (ProjectionType)
		{
		// Accept any frustum for aabb center method
		case EDisplayClusterWarpBlendProjectionType::DynamicAABBCenter:
			break;

		case EDisplayClusterWarpBlendProjectionType::StaticSurfaceNormal:
			ProjectionType = EDisplayClusterWarpBlendProjectionType::RuntimeStaticSurfaceNormalInverted; // Make mirror for backside eye position
			return true;

		case EDisplayClusterWarpBlendProjectionType::StaticSurfacePlane:
			ProjectionType = EDisplayClusterWarpBlendProjectionType::RuntimeStaticSurfacePlaneInverted; // Make mirror for backside eye position
			return true;

		default:
			ProjectionType = EDisplayClusterWarpBlendProjectionType::DynamicAABBCenter;
			return true;
		}

		// Accept current projection
		return false;
	}


	// ImplCalcView() Output:
	FVector ViewDirection;
	FVector ViewOrigin;
	FVector EyeOrigin;

	void ImplCalcView()
	{
		switch (StereoMode)
		{
		case EDisplayClusterWarpBlendStereoMode::AsymmetricAABB:
		{
			// Use AABB center as view target
			FVector AABBCenter = (GeometryContext.AABBox.Max + GeometryContext.AABBox.Min) * 0.5f;
			// Use eye view location to build view vector
			FVector LookAt = Eye.OriginLocation + Eye.OriginEyeOffset;

			// Create view transform matrix from look direction vector:
			FVector LookVector = AABBCenter - LookAt;

			ViewDirection = LookVector.GetSafeNormal();
			ViewOrigin = LookAt;
			EyeOrigin = LookAt;

			break;
		}

		case EDisplayClusterWarpBlendStereoMode::SymmetricAABB:
		{
			// Use AABB center as view target
			FVector AABBCenter = (GeometryContext.AABBox.Max + GeometryContext.AABBox.Min) * 0.5f;
			// Use camera origin location to build view vector
			FVector LookAt = Eye.OriginLocation;

			// Create view transform matrix from look direction vector:
			FVector LookVector = AABBCenter - LookAt;

			ViewDirection = LookVector.GetSafeNormal();
			ViewOrigin = LookAt;
			EyeOrigin = FVector(LookAt + Eye.OriginEyeOffset);
			break;
		}
		default:
			check(false);
			break;
		}
	}

	// ImplCalcViewMatrix() Output:
	FMatrix Local2World;
	FMatrix World2Local;

	void ImplCalcViewProjection();

	FMatrix GeometryToFrustum;

	bool ImplCalcFrustum();

	void ImplBuildFrustum(IDisplayClusterViewport* InViewport, const uint32 InContextNum);

	FMatrix ImplGetTextureMatrix() const
	{
		FMatrix TextureMatrix = FMatrix::Identity;

		switch (GeometryContext.ProfileType)
		{
		case EDisplayClusterWarpProfileType::warp_A3D:
			// Fetching from a 3D source
			TextureMatrix.M[0][0] = 0.5f;
			TextureMatrix.M[1][1] = -0.5f;
			TextureMatrix.M[3][0] = 0.5f;
			TextureMatrix.M[3][1] = 0.5f;
			break;
		default:
			// Fetching from a 2D source
			TextureMatrix.M[0][0] = 1.f;
			TextureMatrix.M[1][1] = -1.f;
			TextureMatrix.M[3][0] = 0.f;
			TextureMatrix.M[3][1] = 1.f;
			break;
		}

		return TextureMatrix;
	}

	inline bool GetProjectionClip(const FVector4& InPoint)
	{
		if (InPoint.W > 0)
		{
			FVector4 ProjectedVertice = GeometryToFrustum.TransformFVector4(InPoint);

			// Use only points over view plane, ignore backside pts
			if (isnan(ProjectedVertice.X) || isnan(ProjectedVertice.Y) || isnan(ProjectedVertice.Z) || ProjectedVertice.X <= 0 || FMath::IsNearlyZero(ProjectedVertice.X, (FVector4::FReal)1.e-6f))
			{
				// This point out of view plane
				return false;
			}

			const float Scale = Eye.ZNear / ProjectedVertice.X;

			ProjectedVertice.Y *= Scale;
			ProjectedVertice.Z *= Scale;

			Frustum.ProjectionAngles.Left  = FMath::Min(Frustum.ProjectionAngles.Left, ProjectedVertice.Y);
			Frustum.ProjectionAngles.Right = FMath::Max(Frustum.ProjectionAngles.Right, ProjectedVertice.Y);

			Frustum.ProjectionAngles.Top    = FMath::Max(Frustum.ProjectionAngles.Top, ProjectedVertice.Z);
			Frustum.ProjectionAngles.Bottom = FMath::Min(Frustum.ProjectionAngles.Bottom, ProjectedVertice.Z);
		}

		return true;
	}

	bool ImplCalcFrustum_AABB()
	{
		bool bOutOfFrustumPoints = false;

		// Search a camera space frustum
		for (int32 AABBPointIndex = 0; AABBPointIndex < 8; ++AABBPointIndex)
		{
			bOutOfFrustumPoints |= !GetProjectionClip(GeometryContext.AABBoxPts[AABBPointIndex]);
		}

		return bOutOfFrustumPoints == false;
	}

	bool ImplCalcFrustum_FULL()
	{
		switch (GeometryContext.GeometryProxy.GeometryType)
		{
		case EDisplayClusterWarpGeometryType::WarpMap:
			return ImplCalcFrustum_FULL_WarpMap();

		case EDisplayClusterWarpGeometryType::WarpMesh:
			return ImplCalcFrustum_FULL_WarpMesh();

		case EDisplayClusterWarpGeometryType::WarpProceduralMesh:
			return ImplCalcFrustum_FULL_WarpProceduralMesh();

		default:
			break;
		}
		return false;
	}

	bool ImplCalcFrustum_LOD()
	{
		switch (GeometryContext.GeometryProxy.GeometryType)
		{
		case EDisplayClusterWarpGeometryType::WarpMap:
			return ImplCalcFrustum_LOD_WarpMap();

		case EDisplayClusterWarpGeometryType::WarpMesh:
			// LOD not implemented for WarpMesh, use full
			return ImplCalcFrustum_FULL_WarpMesh();

		case EDisplayClusterWarpGeometryType::WarpProceduralMesh:
			// LOD not implemented for WarpProceduralMesh, use full
			return ImplCalcFrustum_FULL_WarpProceduralMesh();
		default:
			break;
		}

		return false;
	}

	// Frustum build logics:
	bool ImplCalcFrustum_FULL_WarpMap();
	bool ImplCalcFrustum_FULL_WarpMesh();
	bool ImplCalcFrustum_FULL_WarpProceduralMesh();

	bool ImplCalcFrustum_LOD_WarpMap();

	/** 
	 * When necessary, rotates by 90 deg. the frustum to better fit the aspect ratio of the viewport.
	 * 
	 * @param ContextSize Viewport size (Width x Height).
	 */
	void ImplFitFrustumToContextSize(const FIntPoint& ContextSize);

private:
	FDisplayClusterWarpBlend_GeometryContext& GeometryContext;

	//const EDisplayClusterWarpBlendFrustumType FrustumType;
	const FDisplayClusterWarpEye& Eye;

	// legacy WarpMap, now always use x16 downscale ratio
	const int32 WarpMapLODRatio = 16;

	// Build
	FDisplayClusterWarpContext Frustum;
};

