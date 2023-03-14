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
	bool CalcFrustum(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FDisplayClusterWarpContext& OutFrustum)
	{
		if (!GeometryContext.GeometryProxy.bIsGeometryValid)
		{
			// Bad source geometry
			return false;
		}

		// Get view base:
		ImplCalcView();

		if (bFindBestProjectionType == false)
		{
			ImplCalcViewProjection();
			ImplCalcFrustum();
		}
		else
		{
			// Frustum projection with auto-fix (back-side view planes)
			// Projection type changed runtime. Validate frustum points, all must be over view plane:
			EDisplayClusterWarpBlendProjectionType BaseProjectionType = ProjectionType;
			EDisplayClusterWarpBlendFrustumType    BaseFrustumType = FrustumType;

			// Optimize for PerfectCPU:
			if (FrustumType == EDisplayClusterWarpBlendFrustumType::FULL && GeometryContext.GeometryProxy.GeometryType == EDisplayClusterWarpGeometryType::WarpMap)
			{
				while (true)
				{
					SetParameter(EDisplayClusterWarpBlendFrustumType::LOD);

					// Fast check for bad view
					ImplCalcViewProjection();
					if (ImplCalcFrustum())
					{
						break;
					}

					if (!ImplUpdateProjectionType())
					{
						break;
					}
				}
			}

			// Restore base parameter
			SetParameter(BaseFrustumType);

			// Search valid projection mode:
			if (BaseProjectionType == ProjectionType)
			{

				while (true)
				{
					//Full check for bad projection:
					ImplCalcViewProjection();
					if (ImplCalcFrustum())
					{
						break;
					}

					if (!ImplUpdateProjectionType())
					{
						break;
					}
				}
			}
		}

		ImplBuildFrustum(InViewport, InContextNum);

		// Return result back:
		OutFrustum = Frustum;

		return true;
	}

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

	void ImplCalcViewProjection()
	{
		// CUstomize view direction
		FVector ViewDir = ViewDirection;

		switch (ProjectionType)
		{
		case EDisplayClusterWarpBlendProjectionType::StaticSurfacePlane:
			ViewDir = GeometryContext.SurfaceViewPlane;
			break;

		case EDisplayClusterWarpBlendProjectionType::RuntimeStaticSurfacePlaneInverted:
			// Frustum projection fix (back-side view planes)
			ViewDir = -GeometryContext.SurfaceViewPlane;
			break;

		case EDisplayClusterWarpBlendProjectionType::StaticSurfaceNormal:
			// Use fixed surface view normal
			ViewDir = GeometryContext.SurfaceViewNormal;
			break;

		case EDisplayClusterWarpBlendProjectionType::RuntimeStaticSurfaceNormalInverted:
			// Frustum projection fix (back-side view planes)
			ViewDir = -GeometryContext.SurfaceViewNormal;
			break;

		default:
			break;
		}

		FVector const NewX = ViewDir.GetSafeNormal();
		// make sure we don't ever pick the same as NewX
		if(FMath::Abs(NewX.Z) < (1.f - KINDA_SMALL_NUMBER))
		{
			Local2World = FRotationMatrix::MakeFromXZ(NewX, FVector(0.f, 0.f, 1.f));
		}
		else
		{
			Local2World = FRotationMatrix::MakeFromXY(NewX, FVector(0.f, 1.f, 0.f));
		}

		Local2World.SetOrigin(EyeOrigin); // Finally set view origin to eye location

		World2Local = Local2World.Inverse();
	}

	FMatrix GeometryToFrustum;

	bool ImplCalcFrustum()
	{
		// Reset extend of the frustum
		Frustum.ProjectionAngles.Top = -FLT_MAX;
		Frustum.ProjectionAngles.Bottom = FLT_MAX;
		Frustum.ProjectionAngles.Left = FLT_MAX;
		Frustum.ProjectionAngles.Right = -FLT_MAX;

		GeometryToFrustum = GeometryContext.GeometryToOrigin * World2Local;

		bool bResult = false;

		//Compute rendering frustum with current method
		switch (FrustumType)
		{
		case EDisplayClusterWarpBlendFrustumType::AABB:
			bResult = ImplCalcFrustum_AABB();
			break;

		case EDisplayClusterWarpBlendFrustumType::FULL:
			bResult = ImplCalcFrustum_FULL();
			break;

		case EDisplayClusterWarpBlendFrustumType::LOD:
			bResult = ImplCalcFrustum_LOD();
			break;

		default:
			break;
		}

		return bResult;
	}

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

private:
	FDisplayClusterWarpBlend_GeometryContext& GeometryContext;

	//const EDisplayClusterWarpBlendFrustumType FrustumType;
	const FDisplayClusterWarpEye& Eye;

	// legacy WarpMap, now always use x16 downscale ratio
	const int32 WarpMapLODRatio = 16;

	// Build
	FDisplayClusterWarpContext Frustum;
};

