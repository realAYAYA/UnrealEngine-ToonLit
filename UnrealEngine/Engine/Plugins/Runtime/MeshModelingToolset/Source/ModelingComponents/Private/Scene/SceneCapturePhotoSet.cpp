// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scene/SceneCapturePhotoSet.h"
#include "EngineUtils.h"
#include "Misc/ScopedSlowTask.h"

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "SceneCapture"

FSceneCapturePhotoSet::FSceneCapturePhotoSet()
{
	BaseColorConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::BaseColor);
	WorldNormalConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::WorldNormal);
	RoughnessConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::Roughness);
	MetallicConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::Metallic);
	SpecularConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::Specular);
	EmissiveConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::Emissive);
	PackedMRSConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::CombinedMRS);
	DeviceDepthConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::DeviceDepth);
}

void FSceneCapturePhotoSet::SetCaptureSceneActors(UWorld* World, const TArray<AActor*>& Actors)
{
	this->TargetWorld = World;
	this->VisibleActors = Actors;
}


void FSceneCapturePhotoSet::SetCaptureConfig(ERenderCaptureType CaptureType, const FRenderCaptureConfig& Config)
{
	switch (CaptureType)
	{
	case ERenderCaptureType::BaseColor:
			BaseColorConfig = Config;
			break;
		case ERenderCaptureType::WorldNormal:
			WorldNormalConfig = Config;
			break;
		case ERenderCaptureType::Roughness:
			RoughnessConfig = Config;
			break;
		case ERenderCaptureType::Metallic:
			MetallicConfig = Config;
			break;
		case ERenderCaptureType::Specular:
			SpecularConfig = Config;
			break;
		case ERenderCaptureType::Emissive:
			EmissiveConfig = Config;
			break;
		case ERenderCaptureType::CombinedMRS:
			PackedMRSConfig = Config;
			break;
		case ERenderCaptureType::DeviceDepth:
			DeviceDepthConfig = Config;
			break;
		default:
			check(false);
	}
}

FRenderCaptureConfig FSceneCapturePhotoSet::GetCaptureConfig(ERenderCaptureType CaptureType) const
{
	switch (CaptureType)
	{
	case ERenderCaptureType::BaseColor:
			return BaseColorConfig;
		case ERenderCaptureType::WorldNormal:
			return WorldNormalConfig;
		case ERenderCaptureType::Roughness:
			return RoughnessConfig;
		case ERenderCaptureType::Metallic:
			return MetallicConfig;
		case ERenderCaptureType::Specular:
			return SpecularConfig;
		case ERenderCaptureType::Emissive:
			return EmissiveConfig;
		case ERenderCaptureType::CombinedMRS:
			return PackedMRSConfig;
		case ERenderCaptureType::DeviceDepth:
			return DeviceDepthConfig;
		default:
			check(false);
	}
	return BaseColorConfig;
}

void FSceneCapturePhotoSet::SetCaptureTypeEnabled(ERenderCaptureType CaptureType, bool bEnabled)
{
	switch (CaptureType)
	{
	case ERenderCaptureType::BaseColor:
			bEnableBaseColor = bEnabled;
			break;
		case ERenderCaptureType::WorldNormal:
			bEnableWorldNormal = bEnabled;
			break;
		case ERenderCaptureType::Roughness:
			bEnableRoughness = bEnabled;
			break;
		case ERenderCaptureType::Metallic:
			bEnableMetallic = bEnabled;
			break;
		case ERenderCaptureType::Specular:
			bEnableSpecular = bEnabled;
			break;
		case ERenderCaptureType::Emissive:
			bEnableEmissive = bEnabled;
			break;
		case ERenderCaptureType::CombinedMRS:
			bEnablePackedMRS = bEnabled;
			break;
		case ERenderCaptureType::DeviceDepth:
			bEnableDeviceDepth = bEnabled;
			break;
		default:
			check(false);
	}
}

bool FSceneCapturePhotoSet::GetCaptureTypeEnabled(ERenderCaptureType CaptureType) const
{
	switch (CaptureType)
	{
	case ERenderCaptureType::BaseColor:
			return bEnableBaseColor;
		case ERenderCaptureType::WorldNormal:
			return bEnableWorldNormal;
		case ERenderCaptureType::Roughness:
			return bEnableRoughness;
		case ERenderCaptureType::Metallic:
			return bEnableMetallic;
		case ERenderCaptureType::Specular:
			return bEnableSpecular;
		case ERenderCaptureType::Emissive:
			return bEnableEmissive;
		case ERenderCaptureType::CombinedMRS:
			return bEnablePackedMRS;
		case ERenderCaptureType::DeviceDepth:
			return bEnableDeviceDepth;
		default:
			ensure(false);
	}
	return false;
}



void FSceneCapturePhotoSet::AddStandardExteriorCapturesFromBoundingBox(
	FImageDimensions PhotoDimensions,
	double HorizontalFOVDegrees,
	double NearPlaneDist,
	bool bFaces,
	bool bUpperCorners,
	bool bLowerCorners,
	bool bUpperEdges,
	bool bSideEdges)
{
	TArray<FVector3d> Directions;

	if (bFaces)
	{
		Directions.Add(FVector3d::UnitX());
		Directions.Add(-FVector3d::UnitX());
		Directions.Add(FVector3d::UnitY());
		Directions.Add(-FVector3d::UnitY());
		Directions.Add(FVector3d::UnitZ());
		Directions.Add(-FVector3d::UnitZ());
	}
	if (bUpperCorners)
	{
		Directions.Add(Normalized(FVector3d(1, 1, -1)));
		Directions.Add(Normalized(FVector3d(-1, 1, -1)));
		Directions.Add(Normalized(FVector3d(1, -1, -1)));
		Directions.Add(Normalized(FVector3d(-1, -1, -1)));
	}
	if (bLowerCorners)
	{
		Directions.Add(Normalized(FVector3d(1, 1, 1)));
		Directions.Add(Normalized(FVector3d(-1, 1, 1)));
		Directions.Add(Normalized(FVector3d(1, -1, 1)));
		Directions.Add(Normalized(FVector3d(-1, -1, 1)));
	}
	if (bUpperEdges)
	{
		Directions.Add(Normalized(FVector3d(-1, 0, -1)));
		Directions.Add(Normalized(FVector3d(1, 0, -1)));
		Directions.Add(Normalized(FVector3d(0, -1, -1)));
		Directions.Add(Normalized(FVector3d(0, 1, -1)));
	}
	if (bSideEdges)
	{
		Directions.Add(Normalized(FVector3d(1, 1, 0)));
		Directions.Add(Normalized(FVector3d(-1, 1, 0)));
		Directions.Add(Normalized(FVector3d(1, -1, 0)));
		Directions.Add(Normalized(FVector3d(-1, -1, 0)));
	}
	AddExteriorCaptures(PhotoDimensions, HorizontalFOVDegrees, NearPlaneDist, Directions);
}



void FSceneCapturePhotoSet::AddExteriorCaptures(
	FImageDimensions PhotoDimensions,
	double HorizontalFOVDegrees,
	double NearPlaneDist,
	const TArray<FVector3d>& Directions)
{
	check(this->TargetWorld != nullptr);
	
	FScopedSlowTask Progress(Directions.Num(), LOCTEXT("ComputingViewpoints", "Computing Viewpoints..."));
	Progress.MakeDialog(bAllowCancel);

	// Unregister all components to remove unwanted proxies from the scene. This was previously the only way to "hide" nanite meshes, now optional.
	TSet<AActor*> VisibleActorsSet(VisibleActors);
	TArray<AActor*> ActorsToRegister;
	if (bEnforceVisibilityViaUnregister)
	{
		for (TActorIterator<AActor> Actor(TargetWorld); Actor; ++Actor)
		{
			if (!VisibleActorsSet.Contains(*Actor))
			{
				Actor->UnregisterAllComponents();
				ActorsToRegister.Add(*Actor);
			}
		}
	}

	ON_SCOPE_EXIT
	{
		// Workaround for Nanite scene proxies visibility
		// Reregister all components we previously unregistered
		for (AActor* Actor : ActorsToRegister)
		{
			Actor->RegisterAllComponents();
		}
	};

	FWorldRenderCapture RenderCapture;
	RenderCapture.SetWorld(TargetWorld);
	RenderCapture.SetVisibleActors(VisibleActors);
	RenderCapture.SetDimensions(PhotoDimensions);
	if (bWriteDebugImages)
	{
		RenderCapture.SetEnableWriteDebugImage(true, 0, DebugImagesFolderName);
	}

	// this tells us origin and radius - could be view-dependent...
	FSphere RenderSphere = RenderCapture.ComputeContainingRenderSphere(HorizontalFOVDegrees);

	int32 NumDirections = Directions.Num();
	for (int32 di = 0; di < NumDirections; ++di)
	{
		Progress.EnterProgressFrame(1.f);
		if (Progress.ShouldCancel())
		{
			bWasCancelled = true;
			return;
		}

		FVector3d ViewDirection = Directions[di];
		ViewDirection.Normalize();

		// TODO Align the frame with the renderer coordinate system then remove the axis swapping in WorldRenderCapture.cpp
		FFrame3d ViewFrame;
		ViewFrame.AlignAxis(0, ViewDirection);
		ViewFrame.ConstrainedAlignAxis(2, FVector3d::UnitZ(), ViewFrame.X());
		ViewFrame.Origin = (FVector3d)RenderSphere.Center;
		ViewFrame.Origin -= (double)RenderSphere.W * ViewFrame.X();

		FSpatialPhoto3f BasePhoto3f;
		BasePhoto3f.Frame = ViewFrame;
		BasePhoto3f.NearPlaneDist = NearPlaneDist;
		BasePhoto3f.HorzFOVDegrees = HorizontalFOVDegrees;
		BasePhoto3f.Dimensions = PhotoDimensions;

		auto CaptureImageTypeFunc_3f = [this, &Progress, &BasePhoto3f, &RenderCapture](ERenderCaptureType CaptureType, FSpatialPhotoSet3f& PhotoSet)
		{
			FSpatialPhoto3f NewPhoto = BasePhoto3f;
			FImageAdapter Image(&NewPhoto.Image);
			FRenderCaptureConfig Config = GetCaptureConfig(CaptureType);
			RenderCapture.CaptureFromPosition(CaptureType, NewPhoto.Frame, NewPhoto.HorzFOVDegrees, NewPhoto.NearPlaneDist, Image, Config);
			PhotoSet.Add(MoveTemp(NewPhoto));
			Progress.TickProgress();
		};

		FSpatialPhoto1f BasePhoto1f;
		BasePhoto1f.Frame = ViewFrame;
		BasePhoto1f.NearPlaneDist = NearPlaneDist;
		BasePhoto1f.HorzFOVDegrees = HorizontalFOVDegrees;
		BasePhoto1f.Dimensions = PhotoDimensions;

		auto CaptureImageTypeFunc_1f = [this, &Progress, &BasePhoto1f, &RenderCapture](ERenderCaptureType CaptureType, FSpatialPhotoSet1f& PhotoSet)
		{
			FSpatialPhoto1f NewPhoto = BasePhoto1f;
			FImageAdapter Image(&NewPhoto.Image);
			FRenderCaptureConfig Config = GetCaptureConfig(CaptureType);
			RenderCapture.CaptureFromPosition(CaptureType, NewPhoto.Frame, NewPhoto.HorzFOVDegrees, NewPhoto.NearPlaneDist, Image, Config);
			PhotoSet.Add(MoveTemp(NewPhoto));
			Progress.TickProgress();
		};

		if (bEnableDeviceDepth)
		{
			CaptureImageTypeFunc_1f(ERenderCaptureType::DeviceDepth, DeviceDepthPhotoSet);
		}
		if (bEnableBaseColor)
		{
			CaptureImageTypeFunc_3f(ERenderCaptureType::BaseColor, BaseColorPhotoSet);
		}
		if (bEnableRoughness)
		{
			CaptureImageTypeFunc_1f(ERenderCaptureType::Roughness, RoughnessPhotoSet);
		}
		if (bEnableSpecular)
		{
			CaptureImageTypeFunc_1f(ERenderCaptureType::Specular, SpecularPhotoSet);
		}
		if (bEnableMetallic)
		{
			CaptureImageTypeFunc_1f(ERenderCaptureType::Metallic, MetallicPhotoSet);
		}
		if (bEnablePackedMRS)
		{
			CaptureImageTypeFunc_3f(ERenderCaptureType::CombinedMRS, PackedMRSPhotoSet);
		}
		if (bEnableWorldNormal)
		{
			CaptureImageTypeFunc_3f(ERenderCaptureType::WorldNormal, WorldNormalPhotoSet);
		}
		if (bEnableEmissive)
		{
			CaptureImageTypeFunc_3f(ERenderCaptureType::Emissive, EmissivePhotoSet);
		}

		FSpatialPhotoParams Params;
		Params.Frame = ViewFrame;
		Params.NearPlaneDist = NearPlaneDist;
		Params.HorzFOVDegrees = HorizontalFOVDegrees;
		Params.Dimensions = PhotoDimensions;
		Params.ViewMatrices = RenderCapture.GetLastCaptureViewMatrices();
		PhotoSetParams.Add(Params);
	}
}



void FSceneCapturePhotoSet::OptimizePhotoSets()
{
	// todo:
	//  1) crop photos to regions with actual pixels
	//  2) pack into fewer photos  (eg pack spec/rough/metallic)
	//  3) RLE encoding or other compression
}



FSceneCapturePhotoSet::FSceneSample::FSceneSample()
{
	HaveValues = FRenderCaptureTypeFlags::None();
	BaseColor = FVector3f(0, 0, 0);
	Roughness = 0.0f;
	Specular = 0.0f;
	Metallic = 0.0f;
	Emissive = FVector3f(0, 0, 0);
	WorldNormal = FVector3f(0, 0, 1);
	DeviceDepth = 0.0f;
}

FVector3f FSceneCapturePhotoSet::FSceneSample::GetValue3f(ERenderCaptureType CaptureType) const
{
	switch (CaptureType)
	{
	case ERenderCaptureType::BaseColor:
		return BaseColor;
	case ERenderCaptureType::WorldNormal:
		return WorldNormal;
	case ERenderCaptureType::Roughness:
		return Roughness * FVector3f::One();
	case ERenderCaptureType::Metallic:
		return Metallic * FVector3f::One();
	case ERenderCaptureType::Specular:
		return Specular * FVector3f::One();
	case ERenderCaptureType::Emissive:
		return Emissive;
	case ERenderCaptureType::DeviceDepth:
		return DeviceDepth * FVector3f::One();
	default:
		check(false);
	}
	return FVector3f::Zero();
}

FVector4f FSceneCapturePhotoSet::FSceneSample::GetValue4f(ERenderCaptureType CaptureType) const
{
	switch (CaptureType)
	{
	case ERenderCaptureType::BaseColor:
		return FVector4f(BaseColor.X, BaseColor.Y, BaseColor.Z, 1.0f);
	case ERenderCaptureType::WorldNormal:
		return FVector4f(WorldNormal.X, WorldNormal.Y, WorldNormal.Z, 1.0f);
	case ERenderCaptureType::Roughness:
		return FVector4f(Roughness, Roughness, Roughness, 1.0f);
	case ERenderCaptureType::Metallic:
		return FVector4f(Metallic, Metallic, Metallic, 1.0f);
	case ERenderCaptureType::Specular:
		return FVector4f(Specular, Specular, Specular, 1.0f);
	case ERenderCaptureType::CombinedMRS:
		return FVector4f(Metallic, Roughness, Specular, 1.0f);
	case ERenderCaptureType::Emissive:
		return FVector4f(Emissive.X, Emissive.Y, Emissive.Z, 1.0f);
	case ERenderCaptureType::DeviceDepth:
		return FVector4f(DeviceDepth, DeviceDepth, DeviceDepth, 1.0f);
	default:
		check(false);
	}
	return FVector4f::Zero();
}

bool FSceneCapturePhotoSet::ComputeSampleLocation(
	const FVector3d& Position,
	const FVector3d& Normal,
	const float ValidSampleDepthThreshold,
	TFunctionRef<bool(const FVector3d&, const FVector3d&)> VisibilityFunction,
	int& PhotoIndex,
	FVector2d& PhotoCoords) const
{
	double DotTolerance = -0.1;		// dot should be negative for normal pointing towards photo

	PhotoIndex = IndexConstants::InvalidID;
	PhotoCoords = FVector2d(0., 0.);

	double MinDot = 1.0;

	int32 NumPhotos = PhotoSetParams.Num();
	for (int32 Index = 0; Index < NumPhotos; ++Index)
	{
		const FSpatialPhotoParams& Params = PhotoSetParams[Index];
		check(Params.Dimensions.IsSquare());

		FFrame3d RenderFrame(Params.Frame.Origin, Params.Frame.Y(), Params.Frame.Z(), Params.Frame.X());

		FVector3d ViewDirection = RenderFrame.Z();
		double ViewDot = ViewDirection.Dot(Normal);
		if (ViewDot > DotTolerance || ViewDot > MinDot)
		{
			// The sample is facing away from the photo, or we found a photo more aligned with this sample
			continue;
		}

		FFrame3d ViewPlane = RenderFrame;
		ViewPlane.Origin += Params.NearPlaneDist * ViewDirection;

		double ViewPlaneWidthWorld = Params.NearPlaneDist * FMathd::Tan(Params.HorzFOVDegrees * 0.5 * FMathd::DegToRad);
		double ViewPlaneHeightWorld = ViewPlaneWidthWorld;

		// Shoot a ray from the camera position toward the sample position and find the hit point on photo plane
		constexpr int NormalAxisIndex = 2;
		FVector3d RayOrigin = RenderFrame.Origin;
		FVector3d RayDir = Normalized(Position - RayOrigin);
		FVector3d HitPoint;
		bool bHit = ViewPlane.RayPlaneIntersection(RayOrigin, RayDir, NormalAxisIndex, HitPoint);
		if (bHit)
		{
			FVector2d DeviceXY;
			DeviceXY.X = (HitPoint - ViewPlane.Origin).Dot(ViewPlane.X()) / ViewPlaneWidthWorld;
			DeviceXY.Y = (HitPoint - ViewPlane.Origin).Dot(ViewPlane.Y()) / ViewPlaneHeightWorld;
			if (FMathd::Abs(DeviceXY.X) < 1 && FMathd::Abs(DeviceXY.Y) < 1)
			{
				// Shoot a ray from the sample position toward the camera position checking occlusion
				bool bVisible = VisibilityFunction(Position, RayOrigin);
				if (bVisible)
				{
					FVector2d UVCoords = FRenderCaptureCoordinateConverter2D::DeviceToUV(DeviceXY);
					if (ValidSampleDepthThreshold > 0)
					{
						// Look up the device depth from the depth photo set, these values are from 0 (far plane) to
						// 1 (near plane). We skip points which would unproject to the far plane, which is positioned at
						// infinity, we also do not interpolate the depth values, doing so is not a good approximation
						// of the underlying scene
						float DeviceZ = DeviceDepthPhotoSet.Get(Index).Image.NearestSampleUV(UVCoords);
						if (DeviceZ > 0)
						{
							// Compute the pixel position in world space to use it to compute a depth according to the render
							FVector3d PixelPositionDevice{DeviceXY, DeviceZ};
							FVector4d PixelPositionWorld = Params.ViewMatrices.GetInvViewProjectionMatrix().TransformPosition(PixelPositionDevice);
							PixelPositionWorld /= PixelPositionWorld.W;

							// Compare the depth of the sample with the depth of the pixel and consider the sample invalid
							// if these do not match closely enough. This fixes artefacts which occur when sample ray just
							// misses an obstruction and hits a pixel where the color was set by a slightly different ray,
							// through the pixel center, which does hit the obstruction. This problem occurs because the
							// depth capture was obtained by renderering the the source meshes but the visiblity function
							// works on the target mesh
							float PixelDepth = (RayOrigin - FVector3d(PixelPositionWorld)).Length();
							float SampleDepth = (RayOrigin - Position).Length();
							if (FMath::Abs(PixelDepth - SampleDepth) < ValidSampleDepthThreshold)
							{
								PhotoCoords.X = UVCoords.X * (double)Params.Dimensions.GetWidth();
								PhotoCoords.Y = UVCoords.Y * (double)Params.Dimensions.GetHeight();
								PhotoIndex = Index;
								MinDot = ViewDot;
							}
						} // Test DeviceZ > 0
					} 
					else
					{
						PhotoCoords.X = UVCoords.X * (double)Params.Dimensions.GetWidth();
						PhotoCoords.Y = UVCoords.Y * (double)Params.Dimensions.GetHeight();
						PhotoIndex = Index;
						MinDot = ViewDot;
					}
				} // Test bVisible
			} // Test UVCoords in (-1,1)x(-1,1)
		} // Hit photo plane
	} // Photo loop

	return PhotoIndex != IndexConstants::InvalidID;
}

bool FSceneCapturePhotoSet::ComputeSample(
	const FRenderCaptureTypeFlags& SampleChannels,
	const FVector3d& Position,
	const FVector3d& Normal,
	TFunctionRef<bool(const FVector3d&, const FVector3d&)> VisibilityFunction,
	FSceneSample& DefaultsInResultsOut) const
{
	// This could be much more efficient if (eg) we knew that all the photo sets have
	// the same captures, then the query only has to be done once and can be used to sample each specific photo
	// This is implemented in the other ComputeSample overload

	if (SampleChannels.bBaseColor)
	{
		DefaultsInResultsOut.BaseColor =
			BaseColorPhotoSet.ComputeSample(Position, Normal, VisibilityFunction, DefaultsInResultsOut.BaseColor);
		DefaultsInResultsOut.HaveValues.bBaseColor = true;
	}
	if (SampleChannels.bRoughness)
	{
		DefaultsInResultsOut.Roughness =
			RoughnessPhotoSet.ComputeSample(Position, Normal, VisibilityFunction, DefaultsInResultsOut.Roughness);
		DefaultsInResultsOut.HaveValues.bRoughness = true;
	}
	if (SampleChannels.bSpecular)
	{
		DefaultsInResultsOut.Specular =
			SpecularPhotoSet.ComputeSample(Position, Normal, VisibilityFunction, DefaultsInResultsOut.Specular);
		DefaultsInResultsOut.HaveValues.bSpecular = true;
	}
	if (SampleChannels.bMetallic)
	{
		DefaultsInResultsOut.Metallic =
			MetallicPhotoSet.ComputeSample(Position, Normal, VisibilityFunction, DefaultsInResultsOut.Metallic);
		DefaultsInResultsOut.HaveValues.bMetallic = true;
	}
	if (SampleChannels.bCombinedMRS)
	{
		FVector3f MRSValue(DefaultsInResultsOut.Metallic, DefaultsInResultsOut.Roughness, DefaultsInResultsOut.Specular);
		MRSValue = PackedMRSPhotoSet.ComputeSample(Position, Normal, VisibilityFunction, MRSValue);
		DefaultsInResultsOut.Metallic = MRSValue.X;
		DefaultsInResultsOut.Roughness = MRSValue.Y;
		DefaultsInResultsOut.Specular = MRSValue.Z;
		DefaultsInResultsOut.HaveValues.bMetallic = true;
		DefaultsInResultsOut.HaveValues.bRoughness = true;
		DefaultsInResultsOut.HaveValues.bSpecular = true;
	}
	if (SampleChannels.bEmissive)
	{
		DefaultsInResultsOut.Emissive =
			EmissivePhotoSet.ComputeSample(Position, Normal, VisibilityFunction, DefaultsInResultsOut.Emissive);
		DefaultsInResultsOut.HaveValues.bEmissive = true;
	}
	if (SampleChannels.bWorldNormal)
	{
		DefaultsInResultsOut.WorldNormal =
			WorldNormalPhotoSet.ComputeSample(Position, Normal, VisibilityFunction, DefaultsInResultsOut.WorldNormal);
		DefaultsInResultsOut.HaveValues.bWorldNormal = true;
	}

	return true;
}


void FSceneCapturePhotoSet::SetEnableVisibilityByUnregisterMode(bool bEnable)
{
	bEnforceVisibilityViaUnregister = bEnable;
}

void FSceneCapturePhotoSet::SetEnableWriteDebugImages(bool bEnable, FString FolderName)
{
	bWriteDebugImages = bEnable;
	if (FolderName.Len() > 0)
	{
		DebugImagesFolderName = FolderName;
	}
}

#undef LOCTEXT_NAMESPACE