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
	OpacityConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::Opacity);
	SubsurfaceColorConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::SubsurfaceColor);
	DeviceDepthConfig = GetDefaultRenderCaptureConfig(ERenderCaptureType::DeviceDepth);
}

void FSceneCapturePhotoSet::SetCaptureSceneActors(UWorld* World, const TArray<AActor*>& Actors)
{
	if (this->TargetWorld != World || this->VisibleActors != Actors)
	{
		// Empty the photo sets because they rendered different actors
		EmptyAllPhotoSets();

		// Empty the spatial photo parameters because these are computed from the bounding box of the actors
		PhotoSetParams.Empty();
	}
	this->TargetWorld = World;
	this->VisibleActors = Actors;
}

TArray<AActor*> FSceneCapturePhotoSet::GetCaptureSceneActors()
{
	return VisibleActors;
}

UWorld* FSceneCapturePhotoSet::GetCaptureTargetWorld()
{
	return TargetWorld;
}

void FSceneCapturePhotoSet::SetSpatialPhotoParams(const TArray<FSpatialPhotoParams>& SpatialParams)
{
	// TODO Discard/reset on a per array element level rather than discarding everything when any viewpoint changed
	if (PhotoSetParams != SpatialParams)
	{
		// Empty the photo sets because they were rendered with different viewpoints
		EmptyAllPhotoSets();

		PhotoSetParams = SpatialParams;
	}
}

const TArray<FSpatialPhotoParams>& FSceneCapturePhotoSet::GetSpatialPhotoParams() const
{
	return PhotoSetParams;
}


void FSceneCapturePhotoSet::SetCaptureConfig(ERenderCaptureType CaptureType, const FRenderCaptureConfig& Config)
{
	auto UpdateCaptureConfig1f = [Config](FRenderCaptureConfig& CaptureConfig, FSpatialPhotoSet1f& PhotoSet)
	{
		if (CaptureConfig.bAntiAliasing != Config.bAntiAliasing)
		{
			// If we're changing the AntiAliasing state we need to remove any existing photos
			PhotoSet.Empty();
		}
		CaptureConfig = Config;
	};

	auto UpdateCaptureConfig3f = [Config](FRenderCaptureConfig& CaptureConfig, FSpatialPhotoSet3f& PhotoSet)
	{
		if (CaptureConfig.bAntiAliasing != Config.bAntiAliasing)
		{
			// If we're changing the AntiAliasing state we need to remove any existing photos
			PhotoSet.Empty();
		}
		CaptureConfig = Config;
	};

	switch (CaptureType)
	{
	case ERenderCaptureType::BaseColor:
		UpdateCaptureConfig3f(BaseColorConfig, BaseColorPhotoSet);
		break;
	case ERenderCaptureType::WorldNormal:
		UpdateCaptureConfig3f(WorldNormalConfig, WorldNormalPhotoSet);
		break;
	case ERenderCaptureType::Roughness:
		UpdateCaptureConfig1f(RoughnessConfig, RoughnessPhotoSet);
		break;
	case ERenderCaptureType::Metallic:
		UpdateCaptureConfig1f(MetallicConfig, MetallicPhotoSet);
		break;
	case ERenderCaptureType::Specular:
		UpdateCaptureConfig1f(SpecularConfig, SpecularPhotoSet);
		break;
	case ERenderCaptureType::Emissive:
		UpdateCaptureConfig3f(EmissiveConfig, EmissivePhotoSet);
		break;
	case ERenderCaptureType::CombinedMRS:
		UpdateCaptureConfig3f(PackedMRSConfig, PackedMRSPhotoSet);
		break;
	case ERenderCaptureType::Opacity:
		UpdateCaptureConfig1f(OpacityConfig, OpacityPhotoSet);
		break;
	case ERenderCaptureType::SubsurfaceColor:
		UpdateCaptureConfig3f(SubsurfaceColorConfig, SubsurfaceColorPhotoSet);
		break;
	case ERenderCaptureType::DeviceDepth:
		if (DeviceDepthConfig.bAntiAliasing != Config.bAntiAliasing)
		{
			// If we're disabling DeviceDepth we need to remove any existing photos as well as the cached view matrices
			DeviceDepthPhotoSet.Empty();
			PhotoViewMatricies.Empty();
		}
		DeviceDepthConfig = Config;
		break;
	default:
		ensure(false);
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
		case ERenderCaptureType::Opacity:
			return OpacityConfig;
		case ERenderCaptureType::SubsurfaceColor:
			return SubsurfaceColorConfig;
		case ERenderCaptureType::DeviceDepth:
			return DeviceDepthConfig;
		default:
			ensure(false);
	}
	return BaseColorConfig;
}

void FSceneCapturePhotoSet::DisableAllCaptureTypes()
{
	SetCaptureTypeEnabled(ERenderCaptureType::BaseColor, false);
	SetCaptureTypeEnabled(ERenderCaptureType::WorldNormal, false);
	SetCaptureTypeEnabled(ERenderCaptureType::Roughness, false);
	SetCaptureTypeEnabled(ERenderCaptureType::Metallic, false);
	SetCaptureTypeEnabled(ERenderCaptureType::Specular, false);
	SetCaptureTypeEnabled(ERenderCaptureType::Emissive, false);
	SetCaptureTypeEnabled(ERenderCaptureType::Opacity, false);
	SetCaptureTypeEnabled(ERenderCaptureType::SubsurfaceColor, false);
	SetCaptureTypeEnabled(ERenderCaptureType::CombinedMRS, false);
	SetCaptureTypeEnabled(ERenderCaptureType::DeviceDepth, false);
}


void FSceneCapturePhotoSet::SetCaptureTypeEnabled(ERenderCaptureType CaptureType, bool bEnabled)
{
	auto UpdateCaptureType1f = [bEnabled](bool& bCaptureEnabled, FSpatialPhotoSet1f& PhotoSet)
	{
		if (!bEnabled)
		{
			// If we're disabling a CaptureType we need to remove any existing photos
			PhotoSet.Empty();
		}
		bCaptureEnabled = bEnabled;
	};

	auto UpdateCaptureType3f = [bEnabled](bool& bCaptureEnabled, FSpatialPhotoSet3f& PhotoSet)
	{
		if (!bEnabled)
		{
			// If we're disabling a CaptureType we need to remove any existing photos
			PhotoSet.Empty();
		}
		bCaptureEnabled = bEnabled;
	};

	switch (CaptureType)
	{
	case ERenderCaptureType::BaseColor:
		UpdateCaptureType3f(bEnableBaseColor, BaseColorPhotoSet);
		break;
	case ERenderCaptureType::WorldNormal:
		UpdateCaptureType3f(bEnableWorldNormal, WorldNormalPhotoSet);
		break;
	case ERenderCaptureType::Roughness:
		UpdateCaptureType1f(bEnableRoughness, RoughnessPhotoSet);
		break;
	case ERenderCaptureType::Metallic:
		UpdateCaptureType1f(bEnableMetallic, MetallicPhotoSet);
		break;
	case ERenderCaptureType::Specular:
		UpdateCaptureType1f(bEnableSpecular, SpecularPhotoSet);
		break;
	case ERenderCaptureType::Emissive:
		UpdateCaptureType3f(bEnableEmissive, EmissivePhotoSet);
		break;
	case ERenderCaptureType::CombinedMRS:
		UpdateCaptureType3f(bEnablePackedMRS, PackedMRSPhotoSet);
		break;
	case ERenderCaptureType::Opacity:
		UpdateCaptureType1f(bEnableOpacity, OpacityPhotoSet);
		break;
	case ERenderCaptureType::SubsurfaceColor:
		UpdateCaptureType3f(bEnableSubsurfaceColor, SubsurfaceColorPhotoSet);
		break;
	case ERenderCaptureType::DeviceDepth:
		if (bEnableDeviceDepth != bEnabled)
		{
			// If we're disabling DeviceDepth we need to remove any existing photos as well as the cached view matrices
			DeviceDepthPhotoSet.Empty();
			PhotoViewMatricies.Empty();
		}
		bEnableDeviceDepth = bEnabled;
		break;
	default:
		ensure(false);
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
		case ERenderCaptureType::Opacity:
			return bEnableOpacity;
		case ERenderCaptureType::SubsurfaceColor:
			return bEnableSubsurfaceColor;
		case ERenderCaptureType::CombinedMRS:
			return bEnablePackedMRS;
		case ERenderCaptureType::DeviceDepth:
			return bEnableDeviceDepth;
		default:
			ensure(false);
	}
	return false;
}

void FSceneCapturePhotoSet::Compute()
{
	check(this->TargetWorld != nullptr);

	bWasCancelled = false;

	FScopedSlowTask Progress(PhotoSetParams.Num(), LOCTEXT("ComputingViewpoints", "Computing Viewpoints..."));
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
	if (bWriteDebugImages)
	{
		RenderCapture.SetEnableWriteDebugImage(true, 0, DebugImagesFolderName);
	}

	for (const FSpatialPhotoParams& Params : PhotoSetParams)
	{
		Progress.EnterProgressFrame(1.f);
		if (Progress.ShouldCancel())
		{
			bWasCancelled = true;
			return;
		}

		RenderCapture.SetDimensions(Params.Dimensions);

		auto CaptureImageTypeFunc_3f = [this, &Progress, &RenderCapture, &Params]
			(ERenderCaptureType CaptureType, FSpatialPhotoSet3f& PhotoSet)
		{
			if (PhotoSet.Num() < PhotoSetParams.Num())
			{
				FSpatialPhoto3f NewPhoto;
				NewPhoto.Frame = Params.Frame;
				NewPhoto.NearPlaneDist = Params.NearPlaneDist;
				NewPhoto.HorzFOVDegrees = Params.HorzFOVDegrees;
				NewPhoto.Dimensions = Params.Dimensions;

				// TODO Do something with the success boolean returned by RenderCapture.CaptureFromPosition
				FImageAdapter Image(&NewPhoto.Image);
				FRenderCaptureConfig Config = GetCaptureConfig(CaptureType);
				RenderCapture.CaptureFromPosition(CaptureType, NewPhoto.Frame, NewPhoto.HorzFOVDegrees, NewPhoto.NearPlaneDist, Image, Config);
				PhotoSet.Add(MoveTemp(NewPhoto));
			}

			Progress.TickProgress();
		};

		auto CaptureImageTypeFunc_1f = [this, &Progress, &RenderCapture, &Params]
			(ERenderCaptureType CaptureType, FSpatialPhotoSet1f& PhotoSet)
		{
			// Testing NumDirections is how we currently determine if we need to compute this photo
			if (PhotoSet.Num() < PhotoSetParams.Num())
			{
				FSpatialPhoto1f NewPhoto;
				NewPhoto.Frame = Params.Frame;
				NewPhoto.NearPlaneDist = Params.NearPlaneDist;
				NewPhoto.HorzFOVDegrees = Params.HorzFOVDegrees;
				NewPhoto.Dimensions = Params.Dimensions;

				// TODO Do something with the success boolean returned by RenderCapture.CaptureFromPosition
				FImageAdapter Image(&NewPhoto.Image);
				FRenderCaptureConfig Config = GetCaptureConfig(CaptureType);
				RenderCapture.CaptureFromPosition(CaptureType, NewPhoto.Frame, NewPhoto.HorzFOVDegrees, NewPhoto.NearPlaneDist, Image, Config);
				PhotoSet.Add(MoveTemp(NewPhoto));
			}

			Progress.TickProgress();
		};

		if (bEnableDeviceDepth)
		{
			CaptureImageTypeFunc_1f(ERenderCaptureType::DeviceDepth, DeviceDepthPhotoSet);
			if (PhotoViewMatricies.Num() < PhotoSetParams.Num())
			{
				PhotoViewMatricies.Add(RenderCapture.GetLastCaptureViewMatrices());
			}
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
		if (bEnableOpacity)
		{
			CaptureImageTypeFunc_1f(ERenderCaptureType::Opacity, OpacityPhotoSet);
		}
		if (bEnableSubsurfaceColor)
		{
			CaptureImageTypeFunc_3f(ERenderCaptureType::SubsurfaceColor, SubsurfaceColorPhotoSet);
		}
	} // end directions loop
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
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AddExteriorCaptures(PhotoDimensions, HorizontalFOVDegrees, NearPlaneDist, Directions);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FSceneCapturePhotoSet::AddExteriorCaptures(
	FImageDimensions PhotoDimensions,
	double HorizontalFOVDegrees,
	double NearPlaneDist,
	const TArray<FVector3d>& Directions)
{
	check(this->TargetWorld != nullptr);

	bWasCancelled = false;

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

		FSpatialPhotoParams Params;
		Params.NearPlaneDist = NearPlaneDist;
		Params.HorzFOVDegrees = HorizontalFOVDegrees;
		Params.Dimensions = PhotoDimensions;
		// TODO Align the frame with the renderer coordinate system then remove the axis swapping in WorldRenderCapture.cpp
		Params.Frame.AlignAxis(0, ViewDirection);
		Params.Frame.ConstrainedAlignAxis(2, FVector3d::UnitZ(), Params.Frame.X());
		Params.Frame.Origin = RenderSphere.Center;
		Params.Frame.Origin -= RenderSphere.W * Params.Frame.X();

		auto CaptureImageTypeFunc_3f = [this, &Progress, &RenderCapture, &Params, &NumDirections]
			(ERenderCaptureType CaptureType, FSpatialPhotoSet3f& PhotoSet)
		{
			// Test NumDirections to avoid recomputing photo sets. Search :SceneCaptureWithExistingCaptures
			if (PhotoSet.Num() < NumDirections)
			{
				FSpatialPhoto3f NewPhoto;
				NewPhoto.Frame = Params.Frame;
				NewPhoto.NearPlaneDist = Params.NearPlaneDist;
				NewPhoto.HorzFOVDegrees = Params.HorzFOVDegrees;
				NewPhoto.Dimensions = Params.Dimensions;

				// TODO Do something with the success boolean returned by RenderCapture.CaptureFromPosition
				FImageAdapter Image(&NewPhoto.Image);
				FRenderCaptureConfig Config = GetCaptureConfig(CaptureType);
				RenderCapture.CaptureFromPosition(CaptureType, NewPhoto.Frame, NewPhoto.HorzFOVDegrees, NewPhoto.NearPlaneDist, Image, Config);
				PhotoSet.Add(MoveTemp(NewPhoto));
			}

			Progress.TickProgress();
		};

		auto CaptureImageTypeFunc_1f = [this, &Progress, &RenderCapture, &Params, &NumDirections]
			(ERenderCaptureType CaptureType, FSpatialPhotoSet1f& PhotoSet)
		{
			// Test NumDirections to avoid recomputing photo sets. Search :SceneCaptureWithExistingCaptures
			if (PhotoSet.Num() < NumDirections)
			{
				FSpatialPhoto1f NewPhoto;
				NewPhoto.Frame = Params.Frame;
				NewPhoto.NearPlaneDist = Params.NearPlaneDist;
				NewPhoto.HorzFOVDegrees = Params.HorzFOVDegrees;
				NewPhoto.Dimensions = Params.Dimensions;

				// TODO Do something with the success boolean returned by RenderCapture.CaptureFromPosition
				FImageAdapter Image(&NewPhoto.Image);
				FRenderCaptureConfig Config = GetCaptureConfig(CaptureType);
				RenderCapture.CaptureFromPosition(CaptureType, NewPhoto.Frame, NewPhoto.HorzFOVDegrees, NewPhoto.NearPlaneDist, Image, Config);
				PhotoSet.Add(MoveTemp(NewPhoto));
			}

			Progress.TickProgress();
		};

		if (bEnableDeviceDepth)
		{
			CaptureImageTypeFunc_1f(ERenderCaptureType::DeviceDepth, DeviceDepthPhotoSet);
			if (PhotoViewMatricies.Num() < NumDirections)
			{
				PhotoViewMatricies.Add(RenderCapture.GetLastCaptureViewMatrices());
			}
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
		if (bEnableOpacity)
		{
			CaptureImageTypeFunc_1f(ERenderCaptureType::Opacity, OpacityPhotoSet);
		}
		if (bEnableSubsurfaceColor)
		{
			CaptureImageTypeFunc_3f(ERenderCaptureType::SubsurfaceColor, SubsurfaceColorPhotoSet);
		}

		// AddExteriorCaptures can be called on an FSceneCapturePhotoSet which already has some capture types computed
		// and in this case we should not modify the existing photo sets/cached photo set parameters.
		// See :SceneCaptureWithExistingCaptures 
		if (PhotoSetParams.Num() < NumDirections)
		{
			PhotoSetParams.Add(Params);
		}
	} // end directions loop
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
	Opacity = 0.0f;
	SubsurfaceColor = FVector3f(0, 0, 0);
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
	case ERenderCaptureType::Opacity:
		return Opacity * FVector3f::One();
	case ERenderCaptureType::SubsurfaceColor:
		return SubsurfaceColor;
	case ERenderCaptureType::DeviceDepth:
		return DeviceDepth * FVector3f::One();
	default:
		ensure(false);
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
	case ERenderCaptureType::Opacity:
		return FVector4f(Opacity, Opacity, Opacity, 1.0f);
	case ERenderCaptureType::SubsurfaceColor:
		return FVector4f(SubsurfaceColor.X, SubsurfaceColor.Y, SubsurfaceColor.Z, 1.0f);
	case ERenderCaptureType::DeviceDepth:
		return FVector4f(DeviceDepth, DeviceDepth, DeviceDepth, 1.0f);
	default:
		ensure(false);
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

	if (ValidSampleDepthThreshold > 0)
	{
		check(DeviceDepthPhotoSet.Num() == PhotoSetParams.Num());
		check(PhotoViewMatricies.Num() == PhotoSetParams.Num());
	}

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
							FVector4d PixelPositionWorld = PhotoViewMatricies[Index].GetInvViewProjectionMatrix().TransformPosition(PixelPositionDevice);
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
	if (SampleChannels.bOpacity)
	{
		DefaultsInResultsOut.Opacity =
			OpacityPhotoSet.ComputeSample(Position, Normal, VisibilityFunction, DefaultsInResultsOut.Opacity);
		DefaultsInResultsOut.HaveValues.bOpacity = true;
	}
	if (SampleChannels.bSubsurfaceColor)
	{
		DefaultsInResultsOut.SubsurfaceColor =
			SubsurfaceColorPhotoSet.ComputeSample(Position, Normal, VisibilityFunction, DefaultsInResultsOut.SubsurfaceColor);
		DefaultsInResultsOut.HaveValues.bSubsurfaceColor = true;
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

void FSceneCapturePhotoSet::EmptyAllPhotoSets()
{
	BaseColorPhotoSet.Empty();
	RoughnessPhotoSet.Empty();
	SpecularPhotoSet.Empty();
	MetallicPhotoSet.Empty();
	PackedMRSPhotoSet.Empty();
	WorldNormalPhotoSet.Empty();
	EmissivePhotoSet.Empty();
	OpacityPhotoSet.Empty();
	SubsurfaceColorPhotoSet.Empty();

	// For the device depth photo set we have two containers to empty
	DeviceDepthPhotoSet.Empty();
	PhotoViewMatricies.Empty();
}

void FSceneCapturePhotoSet::SetEnableWriteDebugImages(bool bEnable, FString FolderName)
{
	bWriteDebugImages = bEnable;
	if (FolderName.Len() > 0)
	{
		DebugImagesFolderName = FolderName;
	}
}

TArray<FSpatialPhotoParams> UE::Geometry::ComputeStandardExteriorSpatialPhotoParameters(
	UWorld* World,
	const TArray<AActor*>& Actors,
	FImageDimensions PhotoDimensions,
	double HorizontalFOVDegrees,
	double NearPlaneDist,
	bool bFaces,
	bool bUpperCorners,
	bool bLowerCorners,
	bool bUpperEdges,
	bool bSideEdges)
{
	if (!World || Actors.IsEmpty())
	{
		return {};
	}

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
	// TODO We are missing bLowerEdges!
	if (bSideEdges)
	{
		Directions.Add(Normalized(FVector3d(1, 1, 0)));
		Directions.Add(Normalized(FVector3d(-1, 1, 0)));
		Directions.Add(Normalized(FVector3d(1, -1, 0)));
		Directions.Add(Normalized(FVector3d(-1, -1, 0)));
	}

	// Compute a sphere bounding the give actors so we can use it to position the render capture viewpoints
	// Note: We use FWorldRenderCapture to do this but we are not going to render anything in this function
	FSphere RenderSphere;
	{
		FWorldRenderCapture RenderCapture;
		RenderCapture.SetVisibleActors(Actors);
		RenderSphere = RenderCapture.ComputeContainingRenderSphere(HorizontalFOVDegrees);
	}

	TArray<FSpatialPhotoParams> Result;

	int32 NumDirections = Directions.Num();
	for (int32 di = 0; di < NumDirections; ++di)
	{
		FVector3d ViewDirection = Directions[di];
		ViewDirection.Normalize();

		FSpatialPhotoParams Params;
		Params.NearPlaneDist = NearPlaneDist;
		Params.HorzFOVDegrees = HorizontalFOVDegrees;
		Params.Dimensions = PhotoDimensions;
		// TODO Align the frame with the renderer coordinate system then remove the axis swapping in WorldRenderCapture.cpp
		Params.Frame.AlignAxis(0, ViewDirection);
		Params.Frame.ConstrainedAlignAxis(2, FVector3d::UnitZ(), Params.Frame.X());
		Params.Frame.Origin = RenderSphere.Center;
		Params.Frame.Origin -= RenderSphere.W * Params.Frame.X();

		Result.Add(Params);
	}

	return Result;
}


#undef LOCTEXT_NAMESPACE