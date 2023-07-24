// Copyright Epic Games, Inc. All Rights Reserved.

#include "Baking/RenderCaptureFunctions.h"
#include "Scene/SceneCapturePhotoSet.h"
#include "Sampling/MeshMapBaker.h"
#include "Sampling/RenderCaptureMapEvaluator.h"
#include "AssetUtils/Texture2DBuilder.h"

#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Algo/NoneOf.h"
#include "Algo/AnyOf.h"
#include "Misc/ScopedSlowTask.h"


using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "RenderCaptureFunctions"



FRenderCaptureOcclusionHandler::FRenderCaptureOcclusionHandler(FImageDimensions Dimensions)
{
	SampleStats.SetDimensions(Dimensions);
	SampleStats.Clear({});
}

void FRenderCaptureOcclusionHandler::RegisterSample(const FVector2i& ImageCoords, bool bSampleValid)
{
	checkSlow(SampleStats.GetDimensions().IsValidCoords(ImageCoords));
	if (bSampleValid)
	{
		SampleStats.GetPixel(ImageCoords).NumValid += 1;
	}
	else
	{
		SampleStats.GetPixel(ImageCoords).NumInvalid += 1;
	}
}

void FRenderCaptureOcclusionHandler::PushInfillRequired(bool bInfillRequired)
{
	InfillRequired.Add(bInfillRequired);
}

void FRenderCaptureOcclusionHandler::ComputeAndApplyInfill(TArray<TUniquePtr<TImageBuilder<FVector4f>>>& Images)
{
	check(Images.Num() == InfillRequired.Num());

	if (Images.IsEmpty() || Algo::NoneOf(InfillRequired))
	{
		return;
	}

	ComputeInfill();

	for (int ImageIndex = 0; ImageIndex < Images.Num(); ImageIndex++)
	{
		if (InfillRequired[ImageIndex])
		{
			ApplyInfill(*Images[ImageIndex]);
		}
	}
}

void FRenderCaptureOcclusionHandler::ComputeInfill()
{
	// Find pixels that need infill
	TArray<FVector2i> MissingPixels;
	FCriticalSection MissingPixelsLock;
	ParallelFor(SampleStats.GetDimensions().GetHeight(), [this, &MissingPixels, &MissingPixelsLock](int32 Y)
	{
		for (int32 X = 0; X < SampleStats.GetDimensions().GetWidth(); X++)
		{
			const FSampleStats& Stats = SampleStats.GetPixel(X, Y);
			// TODO experiment with other classifications
			if (Stats.NumInvalid > 0 && Stats.NumValid == 0)
			{
				MissingPixelsLock.Lock();
				MissingPixels.Add(FVector2i(X, Y));
				MissingPixelsLock.Unlock();
			}
		}
	});
	
	auto DummyNormalizeStatsFunc = [](FSampleStats SumValue, int32 Count)
	{
		// The return value must be different from MissingValue below so that ComputeInfill works correctly
		return FSampleStats{TNumericLimits<uint16>::Max(), TNumericLimits<uint16>::Max()};
	};

	// This must be the same as the value of exterior pixels, otherwise infill will spread the exterior values into the texture
	FSampleStats MissingValue{0, 0};
	Infill.ComputeInfill(SampleStats, MissingPixels, MissingValue, DummyNormalizeStatsFunc);
}

void FRenderCaptureOcclusionHandler::ApplyInfill(TImageBuilder<FVector4f>& Image) const
{
	auto NormalizeFunc = [](FVector4f SumValue, int32 Count)
	{
		float InvSum = (Count == 0) ? 1.0f : (1.0f / Count);
		return FVector4f(SumValue.X * InvSum, SumValue.Y * InvSum, SumValue.Z * InvSum, 1.0f);
	};

	Infill.ApplyInfill<FVector4f>(Image, NormalizeFunc);
}

bool FRenderCaptureOcclusionHandler::FSampleStats::operator==(const FSampleStats& Other) const
{
	return (NumValid == Other.NumValid) && (NumInvalid == Other.NumInvalid);
}

bool FRenderCaptureOcclusionHandler::FSampleStats::operator!=(const FSampleStats& Other) const
{
	return !(*this == Other);
}

FRenderCaptureOcclusionHandler::FSampleStats& FRenderCaptureOcclusionHandler::FSampleStats::operator+=(const FSampleStats& Other)
{
	NumValid += Other.NumValid;
	NumInvalid += Other.NumInvalid;
	return *this;
}

FRenderCaptureOcclusionHandler::FSampleStats FRenderCaptureOcclusionHandler::FSampleStats::Zero()
{
	return FSampleStats{0, 0};
}








FSceneCapturePhotoSetSampler::FSceneCapturePhotoSetSampler(
		FSceneCapturePhotoSet* SceneCapture,
		float ValidSampleDepthThreshold,
		const FDynamicMesh3* Mesh,
		const FDynamicMeshAABBTree3* Spatial,
		const FMeshTangentsd* Tangents) :
	FMeshBakerDynamicMeshSampler(Mesh, Spatial, Tangents),
	SceneCapture(SceneCapture),
	ValidSampleDepthThreshold(ValidSampleDepthThreshold)
{
	check(SceneCapture != nullptr);
	check(Mesh != nullptr);
	check(Spatial != nullptr);
	check(Tangents != nullptr);

	bool bHasDepth = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::DeviceDepth);
	if (bHasDepth)
	{
		ensure(ValidSampleDepthThreshold > 0); // We only need the depth capture if this threshold is positive
	}

	// Compute an offset shift surface positions toward the render camera position so we don't 	immediately self intersect
	// The Max expression ensures the code works when the base mesh is 2D along one coordinate direction
	const double RayOffsetHackDist((double)(100.0 * FMathf::ZeroTolerance * FMath::Max(Mesh->GetBounds().MinDim(), 0.01)));

	VisibilityFunction = [this, RayOffsetHackDist](const FVector3d& SurfPos, const FVector3d& ViewPos)
	{
		FVector3d RayDir = ViewPos - SurfPos;
		double Dist = Normalize(RayDir);
		FVector3d RayOrigin = SurfPos + RayOffsetHackDist * RayDir;
		int32 HitTID = DetailSpatial->FindNearestHitTriangle(FRay3d(RayOrigin, RayDir), IMeshSpatial::FQueryOptions(Dist));
		return (HitTID == IndexConstants::InvalidID);
	};
}

bool FSceneCapturePhotoSetSampler::SupportsCustomCorrespondence() const
{
	return true;
}

void* FSceneCapturePhotoSetSampler::ComputeCustomCorrespondence(const FMeshUVSampleInfo& SampleInfo, FMeshMapEvaluator::FCorrespondenceSample& Sample) const
{
	
	// Perform a ray-cast to determine which photo/coordinate, if any, should be sampled
	int PhotoIndex;
	FVector2d PhotoCoords;
	SceneCapture->ComputeSampleLocation(
		Sample.BaseSample.SurfacePoint,
		Sample.BaseNormal,
		ValidSampleDepthThreshold,
		VisibilityFunction,
		PhotoIndex,
		PhotoCoords);

	// Store the photo coordinates and index in the correspondence sample
	Sample.DetailMesh = SceneCapture;
	Sample.DetailTriID = PhotoIndex;
	Sample.DetailBaryCoords.X = PhotoCoords.X;
	Sample.DetailBaryCoords.Y = PhotoCoords.Y;

	// This will be set to Sample.DetailMesh but we can already do that internally so it's kindof redundant
	return SceneCapture;
}

bool FSceneCapturePhotoSetSampler::IsValidCorrespondence(const FMeshMapEvaluator::FCorrespondenceSample& Sample) const
{
	return Sample.DetailTriID != IndexConstants::InvalidID;
}



namespace UE::Geometry
{
namespace
{

void UpdateSceneCaptureSettings(
	const TUniquePtr<FSceneCapturePhotoSet>& SceneCapture,
	const TArray<AActor*>& Actors,
	const FRenderCaptureOptions& Options)
{
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::DeviceDepth, Options.bBakeDeviceDepth);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::BaseColor,   Options.bBakeBaseColor);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::WorldNormal, Options.bBakeNormalMap);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::Emissive,    Options.bBakeEmissive);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::Opacity,     Options.bBakeOpacity);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::SubsurfaceColor, Options.bBakeSubsurfaceColor);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::CombinedMRS, Options.bUsePackedMRS);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::Metallic,    Options.bBakeMetallic);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::Roughness,   Options.bBakeRoughness);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::Specular,    Options.bBakeSpecular);

	FRenderCaptureConfig Config;
	Config.bAntiAliasing = Options.bAntiAliasing;
	SceneCapture->SetCaptureConfig(ERenderCaptureType::BaseColor,   Config);
	SceneCapture->SetCaptureConfig(ERenderCaptureType::WorldNormal, Config);
	SceneCapture->SetCaptureConfig(ERenderCaptureType::CombinedMRS, Config);
	SceneCapture->SetCaptureConfig(ERenderCaptureType::Metallic,    Config);
	SceneCapture->SetCaptureConfig(ERenderCaptureType::Roughness,   Config);
	SceneCapture->SetCaptureConfig(ERenderCaptureType::Specular,    Config);
	SceneCapture->SetCaptureConfig(ERenderCaptureType::Emissive,    Config);
	SceneCapture->SetCaptureConfig(ERenderCaptureType::Opacity,     Config);
	SceneCapture->SetCaptureConfig(ERenderCaptureType::SubsurfaceColor, Config);

	UWorld* World = Actors.IsEmpty() ? nullptr : Actors[0]->GetWorld();

	SceneCapture->SetCaptureSceneActors(World, Actors);

	const TArray<FSpatialPhotoParams> SpatialParams = ComputeStandardExteriorSpatialPhotoParameters(
		World,
		Actors,
		FImageDimensions(Options.RenderCaptureImageSize, Options.RenderCaptureImageSize),
		Options.FieldOfViewDegrees,
		Options.NearPlaneDist,
		true, true, true, true, true);

	SceneCapture->SetSpatialPhotoParams(SpatialParams);
}

} // namespace
} // namespace UE::Geometry



TUniquePtr<FSceneCapturePhotoSet> UE::Geometry::CapturePhotoSet(
	const TArray<TObjectPtr<AActor>>& Actors,
	const FRenderCaptureOptions& Options,
	FRenderCaptureUpdate& Update,
	bool bAllowCancel)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CapturePhotoSet);

	TUniquePtr<FSceneCapturePhotoSet> SceneCapture = MakeUnique<FSceneCapturePhotoSet>();

	Update = UpdatePhotoSets(SceneCapture, Actors, Options, bAllowCancel);

	return SceneCapture;
}

FRenderCaptureUpdate UE::Geometry::UpdatePhotoSets(
	const TUniquePtr<FSceneCapturePhotoSet>& SceneCapture,
	const TArray<TObjectPtr<AActor>>& Actors,
	const FRenderCaptureOptions& Options,
	bool bAllowCancel)
{
	FScopedSlowTask Progress(0.f, LOCTEXT("CapturingScene", "Capturing Scene..."));
	Progress.MakeDialog(bAllowCancel);

	// Cache previous SceneCapture settings so these can be restored if the computation is cancelled
	const FRenderCaptureOptions ComputedOptions = GetComputedPhotoSetOptions(SceneCapture);
	const TArray<AActor*> ComputedActors = SceneCapture->GetCaptureSceneActors();

	// We track the photo set counts in order to determine which photosets were updated
	struct FPhotoSetNums
	{
		FPhotoSetNums(const TUniquePtr<FSceneCapturePhotoSet>& SceneCapture)
		{
			NumBaseColor       = SceneCapture->GetBaseColorPhotoSet().Num();
			NumRoughness       = SceneCapture->GetRoughnessPhotoSet().Num();
			NumSpecular        = SceneCapture->GetSpecularPhotoSet().Num();
			NumMetallic        = SceneCapture->GetMetallicPhotoSet().Num();
			NumPackedMRS       = SceneCapture->GetPackedMRSPhotoSet().Num();
			NumNormalMap       = SceneCapture->GetWorldNormalPhotoSet().Num();
			NumEmissive        = SceneCapture->GetEmissivePhotoSet().Num();
			NumOpacity         = SceneCapture->GetOpacityPhotoSet().Num();
			NumSubsurfaceColor = SceneCapture->GetSubsurfaceColorPhotoSet().Num();
			NumDeviceDepth     = SceneCapture->GetDeviceDepthPhotoSet().Num();
		}
		
		int32 NumBaseColor;
		int32 NumRoughness;
		int32 NumSpecular;
		int32 NumMetallic;
		int32 NumPackedMRS;
		int32 NumNormalMap;
		int32 NumEmissive;
		int32 NumOpacity;
		int32 NumSubsurfaceColor;
		int32 NumDeviceDepth;
	};

	const FPhotoSetNums Step1(SceneCapture);

	// This will clear any photosets that are disabled or need recomputing
	UpdateSceneCaptureSettings(SceneCapture, Actors, Options);

	const FPhotoSetNums Step2(SceneCapture);

	SceneCapture->SetAllowCancel(bAllowCancel);

	// This will compute newly requested photosets
	SceneCapture->Compute();

	if (SceneCapture->Cancelled())
	{
		// This will clear any newly requested photosets
		UpdateSceneCaptureSettings(SceneCapture, ComputedActors, ComputedOptions);
	}

	const FPhotoSetNums Step3(SceneCapture);

	// If the photo sets were cleared/computed/recomputed we consider them updated, use the counts to figure this out
	FRenderCaptureUpdate Update;
	Update.bUpdatedBaseColor   = !(Step1.NumBaseColor   == Step2.NumBaseColor   && Step2.NumBaseColor   == Step3.NumBaseColor);
	Update.bUpdatedRoughness   = !(Step1.NumRoughness   == Step2.NumRoughness   && Step2.NumRoughness   == Step3.NumRoughness);
	Update.bUpdatedSpecular    = !(Step1.NumSpecular    == Step2.NumSpecular    && Step2.NumSpecular    == Step3.NumSpecular);
	Update.bUpdatedMetallic    = !(Step1.NumMetallic    == Step2.NumMetallic    && Step2.NumMetallic    == Step3.NumMetallic);
	Update.bUpdatedPackedMRS   = !(Step1.NumPackedMRS   == Step2.NumPackedMRS   && Step2.NumPackedMRS   == Step3.NumPackedMRS);
	Update.bUpdatedNormalMap   = !(Step1.NumNormalMap   == Step2.NumNormalMap   && Step2.NumNormalMap   == Step3.NumNormalMap);
	Update.bUpdatedEmissive    = !(Step1.NumEmissive    == Step2.NumEmissive    && Step2.NumEmissive    == Step3.NumEmissive);
	Update.bUpdatedOpacity     = !(Step1.NumOpacity     == Step2.NumOpacity     && Step2.NumOpacity     == Step3.NumOpacity);
	Update.bUpdatedDeviceDepth = !(Step1.NumDeviceDepth == Step2.NumDeviceDepth && Step2.NumDeviceDepth == Step3.NumDeviceDepth);
	Update.bUpdatedSubsurfaceColor = !(Step1.NumSubsurfaceColor == Step2.NumSubsurfaceColor && Step2.NumSubsurfaceColor == Step3.NumSubsurfaceColor);

	return Update;
}

FRenderCaptureOptions UE::Geometry::GetComputedPhotoSetOptions(const TUniquePtr<FSceneCapturePhotoSet>& SceneCapture)
{
	FRenderCaptureOptions Result;

	Result.bBakeBaseColor       = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::BaseColor);
	Result.bBakeNormalMap       = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::WorldNormal);
	Result.bBakeMetallic        = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::Metallic);
	Result.bBakeRoughness       = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::Roughness);
	Result.bBakeSpecular        = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::Specular);
	Result.bUsePackedMRS        = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::CombinedMRS);
	Result.bBakeEmissive        = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::Emissive);
	Result.bBakeOpacity         = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::Opacity);
	Result.bBakeSubsurfaceColor = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::SubsurfaceColor);
	Result.bBakeDeviceDepth     = SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::DeviceDepth);

	const TArray<FSpatialPhotoParams> SpatialParams = SceneCapture->GetSpatialPhotoParams();

	if (!SpatialParams.IsEmpty())
	{
		ensure(SpatialParams[0].Dimensions.IsSquare());
		Result.RenderCaptureImageSize = SpatialParams[0].Dimensions.GetWidth();
		Result.FieldOfViewDegrees = SpatialParams[0].HorzFOVDegrees;
		Result.NearPlaneDist = SpatialParams[0].NearPlaneDist;

		const bool Values[] = {
			SceneCapture->GetCaptureConfig(ERenderCaptureType::BaseColor).bAntiAliasing,
			SceneCapture->GetCaptureConfig(ERenderCaptureType::WorldNormal).bAntiAliasing,
			SceneCapture->GetCaptureConfig(ERenderCaptureType::Metallic).bAntiAliasing,
			SceneCapture->GetCaptureConfig(ERenderCaptureType::Roughness).bAntiAliasing,
			SceneCapture->GetCaptureConfig(ERenderCaptureType::Specular).bAntiAliasing,
			SceneCapture->GetCaptureConfig(ERenderCaptureType::CombinedMRS).bAntiAliasing,
			SceneCapture->GetCaptureConfig(ERenderCaptureType::Emissive).bAntiAliasing,
			SceneCapture->GetCaptureConfig(ERenderCaptureType::Opacity).bAntiAliasing,
			SceneCapture->GetCaptureConfig(ERenderCaptureType::SubsurfaceColor).bAntiAliasing,
			SceneCapture->GetCaptureConfig(ERenderCaptureType::DeviceDepth).bAntiAliasing,
		};
		Result.bAntiAliasing = Algo::AnyOf(Values);
	}

	return Result;
}



template <ERenderCaptureType CaptureType>
TSharedPtr<FRenderCaptureMapEvaluator<FVector4f>>
MakeColorEvaluator(
	const FSceneCapturePhotoSet::FSceneSample& DefaultSample,
	const FSceneCapturePhotoSet* SceneCapture)
{
	TSharedPtr<FRenderCaptureMapEvaluator<FVector4f>> Evaluator = MakeShared<FRenderCaptureMapEvaluator<FVector4f>>();

	switch (CaptureType) {
	case ERenderCaptureType::BaseColor:
		Evaluator->Channel = ERenderCaptureChannel::BaseColor;
		break;
	case ERenderCaptureType::Roughness:
		Evaluator->Channel = ERenderCaptureChannel::Roughness;
		break;
	case ERenderCaptureType::Metallic:
		Evaluator->Channel = ERenderCaptureChannel::Metallic;
		break;
	case ERenderCaptureType::Specular:
		Evaluator->Channel = ERenderCaptureChannel::Specular;
		break;
	case ERenderCaptureType::Emissive:
		Evaluator->Channel = ERenderCaptureChannel::Emissive;
		break;
	case ERenderCaptureType::WorldNormal:
		Evaluator->Channel = ERenderCaptureChannel::WorldNormal;
		break;
	case ERenderCaptureType::CombinedMRS:
		Evaluator->Channel = ERenderCaptureChannel::CombinedMRS;
		break;
	case ERenderCaptureType::Opacity:
		Evaluator->Channel = ERenderCaptureChannel::Opacity;
		break;
	case ERenderCaptureType::SubsurfaceColor:
		Evaluator->Channel = ERenderCaptureChannel::SubsurfaceColor;
		break;
	case ERenderCaptureType::DeviceDepth:
		Evaluator->Channel = ERenderCaptureChannel::DeviceDepth;
		break;
	}

	Evaluator->DefaultResult = DefaultSample.GetValue4f(CaptureType);

	Evaluator->EvaluateSampleCallback = [DefaultSample, SceneCapture](const FMeshMapEvaluator::FCorrespondenceSample& Sample)
	{
		const int PhotoIndex = Sample.DetailTriID;
		const FVector2d PhotoCoords(Sample.DetailBaryCoords.X, Sample.DetailBaryCoords.Y);
		const FVector4f SampleColor = SceneCapture->ComputeSampleNearest<CaptureType>(PhotoIndex, PhotoCoords, DefaultSample);
		return SampleColor;
	};

	Evaluator->EvaluateColorCallback = [](const int DataIdx, float*& In)
	{
		const FVector4f Out(In[0], In[1], In[2], In[3]);
		In += 4;
		return Out;
	};

	return Evaluator;
}


TUniquePtr<FMeshMapBaker> UE::Geometry::MakeRenderCaptureBaker(
	FDynamicMesh3* BaseMesh,
	TSharedPtr<UE::Geometry::FMeshTangentsd, ESPMode::ThreadSafe> BaseMeshTangents,
	TSharedPtr<TArray<int32>, ESPMode::ThreadSafe> BaseMeshUVCharts,
	FSceneCapturePhotoSet* SceneCapture,
	FSceneCapturePhotoSetSampler* Sampler,
	FRenderCaptureOptions PendingBake,
	int32 TargetUVLayer,
	EBakeTextureResolution TextureImageSize,
	EBakeTextureSamplesPerPixel SamplesPerPixel,
	FRenderCaptureOcclusionHandler* OcclusionHandler)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MakeRenderCaptureBaker);

	check(BaseMesh != nullptr);
	check(BaseMeshTangents.IsValid());
	check(BaseMeshUVCharts.IsValid());
	check(SceneCapture != nullptr);
	check(Sampler != nullptr);
	check(OcclusionHandler != nullptr);

	auto RegisterSampleStats = [OcclusionHandler](bool bSampleValid, const FMeshMapEvaluator::FCorrespondenceSample& Sample, const FVector2d& UVPosition, const FVector2i& ImageCoords)
	{
		OcclusionHandler->RegisterSample(ImageCoords, bSampleValid);
	};

	auto ComputeAndApplyInfill = [OcclusionHandler](TArray<TUniquePtr<TImageBuilder<FVector4f>>>& BakeResults)
	{
		OcclusionHandler->ComputeAndApplyInfill(BakeResults);
	};

	TUniquePtr<FMeshMapBaker> Result = MakeUnique<FMeshMapBaker>();
	Result->SetTargetMesh(BaseMesh);
	Result->SetTargetMeshTangents(BaseMeshTangents);
	Result->SetTargetMeshUVCharts(BaseMeshUVCharts.Get());
	Result->SetDimensions(FImageDimensions(static_cast<int32>(TextureImageSize), static_cast<int32>(TextureImageSize)));
	Result->SetSamplesPerPixel(static_cast<int32>(SamplesPerPixel));
	Result->SetFilter(FMeshMapBaker::EBakeFilterType::BSpline);
	Result->SetTargetMeshUVLayer(TargetUVLayer);
	Result->InteriorSampleCallback = RegisterSampleStats;
	Result->PostWriteToImageCallback = ComputeAndApplyInfill;
	Result->SetDetailSampler(Sampler);
	Result->SetCorrespondenceStrategy(FMeshMapBaker::ECorrespondenceStrategy::Custom);

	// Pixels in the output textures which don't map onto the mesh have a light grey color (except the normal map which
	// will show a color corresponding to a unit z tangent space normal)
	const FVector4f InvalidColor(.42, .42, .42, 1);
	const FVector3f DefaultNormal = FVector3f::UnitZ();

	FSceneCapturePhotoSet::FSceneSample DefaultColorSample;
	DefaultColorSample.BaseColor = FVector3f(InvalidColor.X, InvalidColor.Y, InvalidColor.Z);
	DefaultColorSample.Roughness = InvalidColor.X;
	DefaultColorSample.Specular = InvalidColor.X;
	DefaultColorSample.Metallic = InvalidColor.X;
	DefaultColorSample.Emissive = FVector3f(InvalidColor.X, InvalidColor.Y, InvalidColor.Z);
	DefaultColorSample.Opacity = InvalidColor.X;
	DefaultColorSample.SubsurfaceColor = FVector3f(InvalidColor.X, InvalidColor.Y, InvalidColor.Z);
	DefaultColorSample.WorldNormal = FVector4f((DefaultNormal + FVector3f::One()) * .5f, InvalidColor.W);

	// We use 0 here since this corresponds to the infinite far plane value. Also, we don't preview the depth texture
	DefaultColorSample.DeviceDepth = 0;

	auto AddColorEvaluator = [&Result, OcclusionHandler] (const TSharedPtr<FRenderCaptureMapEvaluator<FVector4f>>& Evaluator)
	{
		Result->AddEvaluator(Evaluator);
		OcclusionHandler->PushInfillRequired(true);
	};

	if (SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::DeviceDepth) && PendingBake.bBakeDeviceDepth)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::DeviceDepth>(DefaultColorSample, SceneCapture));
	}
	if (SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::BaseColor) && PendingBake.bBakeBaseColor)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::BaseColor>(DefaultColorSample, SceneCapture));
	}
	if (SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::CombinedMRS) && PendingBake.bUsePackedMRS)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::CombinedMRS>(DefaultColorSample, SceneCapture));
	}
	if (SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::Roughness) && PendingBake.bBakeRoughness)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::Roughness>(DefaultColorSample, SceneCapture));
	}
	if (SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::Metallic) && PendingBake.bBakeMetallic)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::Metallic>(DefaultColorSample, SceneCapture));
	}
	if (SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::Specular) && PendingBake.bBakeSpecular)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::Specular>(DefaultColorSample, SceneCapture));
	}
	if (SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::Emissive) && PendingBake.bBakeEmissive)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::Emissive>(DefaultColorSample, SceneCapture));
	}
	if (SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::Opacity) && PendingBake.bBakeOpacity)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::Opacity>(DefaultColorSample, SceneCapture));
	}
	if (SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::SubsurfaceColor) && PendingBake.bBakeSubsurfaceColor)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::SubsurfaceColor>(DefaultColorSample, SceneCapture));
	}
	if (SceneCapture->GetCaptureTypeEnabled(ERenderCaptureType::WorldNormal) && PendingBake.bBakeNormalMap)
	{
		TSharedPtr<FRenderCaptureMapEvaluator<FVector3f>> Evaluator = MakeShared<FRenderCaptureMapEvaluator<FVector3f>>();

		Evaluator->Channel = ERenderCaptureChannel::WorldNormal;

		Evaluator->DefaultResult = DefaultNormal;

		Evaluator->EvaluateSampleCallback = [SceneCapture, BaseMeshTangents, DefaultColorSample](const FMeshMapEvaluator::FCorrespondenceSample& Sample)
		{
			const int32 TriangleID = Sample.BaseSample.TriangleIndex;
			const FVector3d BaryCoords = Sample.BaseSample.BaryCoords;
			const int PhotoIndex = Sample.DetailTriID;
			const FVector2d PhotoCoords(Sample.DetailBaryCoords.X, Sample.DetailBaryCoords.Y);

			const FVector4f NormalColor = SceneCapture->ComputeSampleNearest<ERenderCaptureType::WorldNormal>(PhotoIndex, PhotoCoords, DefaultColorSample);

			// Map from color components [0,1] to normal components [-1,1]
			const FVector3f WorldSpaceNormal(
				(NormalColor.X - 0.5f) * 2.0f,
				(NormalColor.Y - 0.5f) * 2.0f,
				(NormalColor.Z - 0.5f) * 2.0f);

			// Get tangents on base mesh
			FVector3d BaseTangentX, BaseTangentY;
			BaseMeshTangents->GetInterpolatedTriangleTangent(TriangleID, BaryCoords, BaseTangentX, BaseTangentY);

			// Compute normal in tangent space
			const FVector3f TangentSpaceNormal(
					(float)WorldSpaceNormal.Dot(FVector3f(BaseTangentX)),
					(float)WorldSpaceNormal.Dot(FVector3f(BaseTangentY)),
					(float)WorldSpaceNormal.Dot(FVector3f(Sample.BaseNormal)));

			return TangentSpaceNormal;
		};

		Evaluator->EvaluateColorCallback = [](const int DataIdx, float*& In)
		{
			// Map normal space [-1,1] to color space [0,1]
			const FVector3f Normal(In[0], In[1], In[2]);
			const FVector3f Color = (Normal + FVector3f::One()) * 0.5f;
			const FVector4f Out(Color.X, Color.Y, Color.Z, 1.0f);
			In += 3;
			return Out;
		};

		Result->AddEvaluator(Evaluator);

		// Note: No infill on normal map for now, doesn't make sense to do after mapping to tangent space!
		//  (should we build baked normal map in world space, and then resample to tangent space??)
		OcclusionHandler->PushInfillRequired(false);
	}

	return Result;
}


void UE::Geometry::GetTexturesFromRenderCaptureBaker(const TUniquePtr<FMeshMapBaker>& Baker, FRenderCaptureTextures& TexturesOut)
{
	// We do this to defer work I guess, it was like this in the original ApproximateActors implementation :DeferredPopulateSourceData 
	constexpr bool bPopulateSourceData = false;

	const int32 NumEval = Baker->NumEvaluators();
	for (int32 EvalIdx = 0; EvalIdx < NumEval; ++EvalIdx)
	{
		FMeshMapEvaluator* BaseEval = Baker->GetEvaluator(EvalIdx);
		check(BaseEval->DataLayout().Num() == 1);
		switch (BaseEval->DataLayout()[0])
		{
		case FMeshMapEvaluator::EComponents::Float4:

			{
				FRenderCaptureMapEvaluator<FVector4f>* Eval = static_cast<FRenderCaptureMapEvaluator<FVector4f>*>(BaseEval);
				TUniquePtr<TImageBuilder<FVector4f>> ImageBuilder = MoveTemp(Baker->GetBakeResults(EvalIdx)[0]);
				
				if (ensure(ImageBuilder.IsValid()) == false) return;

				switch (Eval->Channel)
				{
				case ERenderCaptureChannel::BaseColor:

					TexturesOut.BaseColorMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::Color,
						true,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::SubsurfaceColor:

					// The SubsurfaceColor GBuffer channel is gamma encoded so:
					// 1. Dont convert the data in ImageBuilder to sRGB, it is already gamma encoded
					// 2. Pass the ETextureType::Color enum so the built UTexture2D will have SRGB checked
					TexturesOut.SubsurfaceColorMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::Color,
						false,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::Opacity:

					TexturesOut.OpacityMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::ColorLinear,
						false,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::Roughness:

					TexturesOut.RoughnessMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::Roughness,
						false,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::Metallic:

					TexturesOut.MetallicMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::Metallic,
						false,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::Specular:

					TexturesOut.SpecularMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::Specular,
						false,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::Emissive:

					TexturesOut.EmissiveMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::EmissiveHDR,
						false,
						bPopulateSourceData);
					TexturesOut.EmissiveMap->CompressionSettings = TC_HDR_Compressed;
					break;

				case ERenderCaptureChannel::CombinedMRS:

					TexturesOut.PackedMRSMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::ColorLinear,
						false,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::DeviceDepth:

					// Add a null pointer for this evaluator, the depth capture is used internally and not presented to the user
					break;

				case ERenderCaptureChannel::WorldNormal:

					// This should be handled in the Float3 branch
					ensure(false);
					return;

				default:
					ensure(false);
					return;
				}

			} break; // Float4

		case FMeshMapEvaluator::EComponents::Float3:

			{
				FRenderCaptureMapEvaluator<FVector3f>* Eval = static_cast<FRenderCaptureMapEvaluator<FVector3f>*>(BaseEval);
				TUniquePtr<TImageBuilder<FVector4f>> ImageBuilder = MoveTemp(Baker->GetBakeResults(EvalIdx)[0]);
				
				if (ensure(ImageBuilder.IsValid()) == false) return;
				if (ensure(Eval->Channel == ERenderCaptureChannel::WorldNormal) == false) return;

				TexturesOut.NormalMap = FTexture2DBuilder::BuildTextureFromImage(
					*ImageBuilder,
					FTexture2DBuilder::ETextureType::NormalMap,
					false,
					bPopulateSourceData);

			} break; // Float3
			
		default:
			ensure(false);
			return;
		}
	}
}


#undef LOCTEXT_NAMESPACE
