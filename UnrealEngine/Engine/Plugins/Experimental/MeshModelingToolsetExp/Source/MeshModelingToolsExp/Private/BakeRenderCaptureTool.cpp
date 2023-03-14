// Copyright Epic Games, Inc. All Rights Reserved.

#include "BakeRenderCaptureTool.h"

#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "TargetInterfaces/StaticMeshBackedTarget.h"
#include "ToolTargetManager.h"

#include "DynamicMesh/MeshTransforms.h"

#include "ModelingToolTargetUtil.h"

#include "ModelingObjectsCreationAPI.h"

#include "EngineAnalytics.h"

#include "Sampling/MeshImageBakingCache.h"
#include "Sampling/MeshMapBaker.h"
#include "Sampling/RenderCaptureMapEvaluator.h"
#include "Image/ImageInfilling.h"
#include "Algo/NoneOf.h"
#include "Misc/ScopedSlowTask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BakeRenderCaptureTool)


using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UBakeRenderCaptureTool"

//
// Implementation details
//



class FSceneCapturePhotoSetSampler : public FMeshBakerDynamicMeshSampler
{
public:
	FSceneCapturePhotoSetSampler(
		FSceneCapturePhotoSet* SceneCapture,
		float ValidSampleDepthThreshold,
		TFunctionRef<bool(const FVector3d&, const FVector3d&)> VisibilityFunction,
		const FDynamicMesh3* Mesh,
		const FDynamicMeshAABBTree3* Spatial,
		const FMeshTangentsd* Tangents)
	:
		FMeshBakerDynamicMeshSampler(Mesh, Spatial, Tangents),
		SceneCapture(SceneCapture),
		ValidSampleDepthThreshold(ValidSampleDepthThreshold),
		VisibilityFunction(VisibilityFunction)
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
	}

	virtual bool SupportsCustomCorrespondence() const override
	{
		return true;
	}

	// Warning: Expects that Sample.BaseSample.SurfacePoint and Sample.BaseNormal are set when the function is called
	virtual void* ComputeCustomCorrespondence(const FMeshUVSampleInfo& SampleInfo, FMeshMapEvaluator::FCorrespondenceSample& Sample) const override
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

	virtual bool IsValidCorrespondence(const FMeshMapEvaluator::FCorrespondenceSample& Sample) const override
	{
		return Sample.DetailTriID != IndexConstants::InvalidID;
	}

public:
	FSceneCapturePhotoSet* SceneCapture = nullptr;
	float ValidSampleDepthThreshold = 0;
	TFunctionRef<bool(const FVector3d&, const FVector3d&)> VisibilityFunction;
};



static FString BaseColorTexParamName = TEXT("BaseColor");
static FString RoughnessTexParamName = TEXT("Roughness");
static FString MetallicTexParamName = TEXT("Metallic");
static FString SpecularTexParamName = TEXT("Specular");
static FString EmissiveTexParamName = TEXT("Emissive");
static FString NormalTexParamName = TEXT("NormalMap");
static FString PackedMRSTexParamName = TEXT("PackedMRS");

class FRenderCaptureSettings
{
public:
	enum class ETextureSizePolicy : uint8
	{
		TextureSize = 0,
		TexelDensity = 1
	};

	/**
	 * Input options to Actor Approximation process
	 */
	struct FOptions
	{
		//
		// Material approximation settings
		//
		int32 RenderCaptureImageSize = 1024;
		bool bAntiAliasing = false;

		// render capture parameters
		double FieldOfViewDegrees = 45.0;
		double NearPlaneDist = 1.0;
		double ValidSampleDepthThreshold = 0;

		//
		// Material output settings
		//

		// A new MIC derived from this material will be created and assigned to the generated mesh
		UMaterialInterface* BakeMaterial = nullptr;		// if null, will use /MeshModelingToolsetExp/Materials/FullMaterialBakePreviewMaterial_PackedMRS instead
		bool bBakeBaseColor = true;
		bool bBakeRoughness = true;
		bool bBakeMetallic = true;
		bool bBakeSpecular = true;
		bool bBakeEmissive = true;
		bool bBakeNormalMap = true;
		bool bBakeDeviceDepth = true;
		
		bool bUsePackedMRS = true;

		//
		// Mesh settings
		//

		//  Which UV layer of the Target mesh (the one we're baking to) should be used
		int32 TargetUVLayer = 0;
	};

	/**
	 * Construct an FOptions from the provided FMeshApproximationSettings.
	 */
	static FOptions ConstructOptions(
		const URenderCaptureProperties& RenderCaptureProperties,
		const UBakeRenderCaptureToolProperties& ToolProperties,
		const UBakeRenderCaptureInputToolProperties& InputMeshSettings)
	{
		//
		// Construct options for ApproximateActors operation
		//
		FOptions Options;

		Options.TargetUVLayer = InputMeshSettings.GetTargetUVLayerIndex();

		Options.RenderCaptureImageSize = static_cast<int32>(RenderCaptureProperties.Resolution);
		Options.ValidSampleDepthThreshold = ToolProperties.ValidSampleDepthThreshold;

		Options.bBakeBaseColor = RenderCaptureProperties.bBaseColorMap;
		Options.bBakeNormalMap = RenderCaptureProperties.bNormalMap;
		Options.bBakeEmissive =  RenderCaptureProperties.bEmissiveMap;
		Options.bBakeDeviceDepth = RenderCaptureProperties.bDeviceDepthMap;
		
		// Enforce the PackedMRS precondition here so we don't have to check it at each usage site.  Note: We don't
		// apply this precondition on the RenderCaptureProperties because we don't want the user to have to re-enable
		// options which enabling PackedMRS disabled.
		Options.bUsePackedMRS =  RenderCaptureProperties.bPackedMRSMap;
		Options.bBakeMetallic =  RenderCaptureProperties.bPackedMRSMap ? false : RenderCaptureProperties.bMetallicMap;
		Options.bBakeRoughness = RenderCaptureProperties.bPackedMRSMap ? false : RenderCaptureProperties.bRoughnessMap;
		Options.bBakeSpecular =  RenderCaptureProperties.bPackedMRSMap ? false : RenderCaptureProperties.bSpecularMap;

		Options.bAntiAliasing = RenderCaptureProperties.bAntiAliasing;
		Options.FieldOfViewDegrees = RenderCaptureProperties.CaptureFieldOfView;
		Options.NearPlaneDist = RenderCaptureProperties.NearPlaneDist;

		return Options;
	}
};

static TUniquePtr<FSceneCapturePhotoSet> CapturePhotoSet(
	const TArray<TObjectPtr<AActor>>& Actors,
	const FRenderCaptureSettings::FOptions& Options,
	bool bAllowCancel
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CapturePhotoSet);

	FScopedSlowTask Progress(1.f, LOCTEXT("CapturingScene", "Capturing Scene..."));
	Progress.EnterProgressFrame(1.f);
	Progress.MakeDialog(bAllowCancel);

	double FieldOfView = Options.FieldOfViewDegrees;
	double NearPlaneDist = Options.NearPlaneDist;

	FImageDimensions CaptureDimensions(Options.RenderCaptureImageSize, Options.RenderCaptureImageSize);

	TUniquePtr<FSceneCapturePhotoSet> SceneCapture = MakeUnique<FSceneCapturePhotoSet>();
	SceneCapture->SetAllowCancel(bAllowCancel);

	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::DeviceDepth, Options.bBakeDeviceDepth);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::BaseColor,   Options.bBakeBaseColor);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::WorldNormal, Options.bBakeNormalMap);
	SceneCapture->SetCaptureTypeEnabled(ERenderCaptureType::Emissive,    Options.bBakeEmissive);
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

	SceneCapture->SetCaptureSceneActors(Actors[0]->GetWorld(), Actors);

	// SceneCapture->SetEnableWriteDebugImages(true);

	SceneCapture->AddStandardExteriorCapturesFromBoundingBox(
		CaptureDimensions, FieldOfView, NearPlaneDist,
		true, true, true, true, true);

	return SceneCapture;
}

template <ERenderCaptureType CaptureType>
TSharedPtr<FRenderCaptureMapEvaluator<FVector4f>>
MakeColorEvaluator(
	const FSceneCapturePhotoSet::FSceneSample& DefaultSample,
	const FSceneCapturePhotoSet* SceneCapture,
	const FImageDimensions& CaptureDimensions)
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
	case ERenderCaptureType::DeviceDepth:
		Evaluator->Channel = ERenderCaptureChannel::DeviceDepth;
		break;
	}

	Evaluator->DefaultResult = DefaultSample.GetValue4f(CaptureType);

	Evaluator->EvaluateSampleCallback = [&DefaultSample, SceneCapture, &CaptureDimensions](const FMeshMapEvaluator::FCorrespondenceSample& Sample)
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

//
// Tool Operator
//

class FRenderCaptureMapBakerOp : public TGenericDataOperator<FMeshMapBaker>
{
public:
	UE::Geometry::FDynamicMesh3* BaseMesh = nullptr;
	TSharedPtr<UE::Geometry::FMeshTangentsd, ESPMode::ThreadSafe> BaseMeshTangents;
	FRenderCaptureSettings::FOptions Options;
	int32 TextureImageSize;
	EBakeTextureSamplesPerPixel SamplesPerPixel;
	FSceneCapturePhotoSet* SceneCapture = nullptr;

	// Begin TGenericDataOperator interface
	virtual void CalculateResult(FProgressCancel* Progress) override;
	// End TGenericDataOperator interface
};

// Bake textures onto the base/target mesh by projecting/sampling the set of captured photos
void FRenderCaptureMapBakerOp::CalculateResult(FProgressCancel*)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRenderCaptureMapBakerOp_CalculateResult);

	check(BaseMesh != nullptr);
	check(BaseMeshTangents.IsValid());
	check(SceneCapture != nullptr);

	FDynamicMeshAABBTree3 BaseMeshSpatial(BaseMesh, true);

	// Compute an offset shift surface positions toward the render camera position so we don't immediately self intersect
	// The Max expression ensures the code works when the base mesh is 2D along one coordinate direction
	const double RayOffsetHackDist = (double)(100.0 * FMathf::ZeroTolerance * FMath::Max(BaseMesh->GetBounds().MinDim(), 0.01));

	auto VisibilityFunction = [&BaseMeshSpatial, RayOffsetHackDist](const FVector3d& SurfPos, const FVector3d& ViewPos)
	{
		FVector3d RayDir = ViewPos - SurfPos;
		double Dist = Normalize(RayDir);
		FVector3d RayOrigin = SurfPos + RayOffsetHackDist * RayDir;
		int32 HitTID = BaseMeshSpatial.FindNearestHitTriangle(FRay3d(RayOrigin, RayDir), IMeshSpatial::FQueryOptions(Dist));
		return (HitTID == IndexConstants::InvalidID);
	};

	struct FInfillData
	{
		struct FSampleStats {
			uint16 NumValid = 0;
			uint16 NumInvalid = 0;

			// The ==, !=, += operators and the Zero() function are required by the TMarchingPixelInfill implementation

			bool operator==(const FSampleStats& Other) const
			{
				return (NumValid == Other.NumValid) && (NumInvalid == Other.NumInvalid);
			}

			bool operator!=(const FSampleStats& Other) const
			{
				return !(*this == Other);
			}

			FSampleStats& operator+=(const FSampleStats& Other)
			{
				NumValid += Other.NumValid;
				NumInvalid += Other.NumInvalid;
				return *this;
			}

			static FSampleStats Zero()
			{
				return FSampleStats{0, 0};
			}
		};

		// Collect some sample stats per pixel, used to determine if a pixel requires infill or not
		TImageBuilder<FSampleStats> SampleStats;

		// The i-th element of this array indicates if the i-th evaluator needs infill
		TArray<bool> EvaluatorNeedsInfill;
	} InfillData;

	InfillData.SampleStats.SetDimensions(FImageDimensions(TextureImageSize, TextureImageSize));
	InfillData.SampleStats.Clear(FInfillData::FSampleStats{});

	auto RegisterSampleStats = [&InfillData](bool bSampleValid, const FMeshMapEvaluator::FCorrespondenceSample& Sample, const FVector2d& UVPosition, const FVector2i& ImageCoords)
	{
		checkSlow(InfillData.SampleStats.GetDimensions().IsValidCoords(ImageCoords));
		if (bSampleValid)
		{
			InfillData.SampleStats.GetPixel(ImageCoords).NumValid += 1;
		}
		else
		{
			InfillData.SampleStats.GetPixel(ImageCoords).NumInvalid += 1;
		}
	};

	auto ComputeAndApplyInfill = [&InfillData](TArray<TUniquePtr<TImageBuilder<FVector4f>>>& BakeResults)
	{
		check(BakeResults.Num() == InfillData.EvaluatorNeedsInfill.Num());

		if (BakeResults.IsEmpty() || Algo::NoneOf(InfillData.EvaluatorNeedsInfill))
		{
			return;
		}

		// Find pixels that need infill
		TArray<FVector2i> MissingPixels;
		FCriticalSection MissingPixelsLock;
		ParallelFor(InfillData.SampleStats.GetDimensions().GetHeight(), [&MissingPixels, &MissingPixelsLock, &InfillData](int32 Y)
		{
			for (int32 X = 0; X < InfillData.SampleStats.GetDimensions().GetWidth(); X++)
			{
				const FInfillData::FSampleStats& Stats = InfillData.SampleStats.GetPixel(X, Y);
				// TODO experiment with other classifications
				if (Stats.NumInvalid > 0 && Stats.NumValid == 0)
				{
					MissingPixelsLock.Lock();
					MissingPixels.Add(FVector2i(X, Y));
					MissingPixelsLock.Unlock();
				}
			}
		});

		auto NormalizeFunc = [](FVector4f SumValue, int32 Count)
		{
			float InvSum = (Count == 0) ? 1.0f : (1.0f / Count);
			return FVector4f(SumValue.X * InvSum, SumValue.Y * InvSum, SumValue.Z * InvSum, 1.0f);
		};
		
		auto DummyNormalizeStatsFunc = [](FInfillData::FSampleStats SumValue, int32 Count)
		{
			// The return value must be different from MissingValue below so that ComputeInfill works correctly
			return FInfillData::FSampleStats{TNumericLimits<uint16>::Max(), TNumericLimits<uint16>::Max()};
		};

		TMarchingPixelInfill<FInfillData::FSampleStats> Infill;
		// This must be the same as the value of exterior pixels, otherwise infill will spread the exterior values into the texture
		FInfillData::FSampleStats MissingValue{0, 0};
		Infill.ComputeInfill(InfillData.SampleStats, MissingPixels, MissingValue, DummyNormalizeStatsFunc);
		for (int EvaluatorIndex = 0; EvaluatorIndex < BakeResults.Num(); EvaluatorIndex++)
		{
			if (InfillData.EvaluatorNeedsInfill[EvaluatorIndex])
			{
				Infill.ApplyInfill<FVector4f>(*BakeResults[EvaluatorIndex], NormalizeFunc);
			}
		}
	};

	Result->SetTargetMesh(BaseMesh);
	Result->SetTargetMeshTangents(BaseMeshTangents);
	Result->SetDimensions(FImageDimensions(TextureImageSize, TextureImageSize));
	Result->SetSamplesPerPixel(static_cast<int32>(SamplesPerPixel));
	Result->SetFilter(FMeshMapBaker::EBakeFilterType::BSpline);
	Result->SetTargetMeshUVLayer(Options.TargetUVLayer);
	Result->InteriorSampleCallback = RegisterSampleStats;
	Result->PostWriteToImageCallback = ComputeAndApplyInfill;

	FSceneCapturePhotoSetSampler Sampler(SceneCapture, Options.ValidSampleDepthThreshold, VisibilityFunction, BaseMesh, &BaseMeshSpatial, BaseMeshTangents.Get());
	Result->SetDetailSampler(&Sampler);
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
	DefaultColorSample.WorldNormal = FVector4f((DefaultNormal + FVector3f::One()) * .5f, InvalidColor.W);
	
	// We use 0 here since this corresponds to the infinite far plane value. Also, we don't preview the depth texture
	DefaultColorSample.DeviceDepth = 0;

	auto AddColorEvaluator = [this, &InfillData] (const TSharedPtr<FRenderCaptureMapEvaluator<FVector4f>>& Evaluator)
	{
		Result->AddEvaluator(Evaluator);
		InfillData.EvaluatorNeedsInfill.Add(true);
	};
	
	const FImageDimensions CaptureDimensions(Options.RenderCaptureImageSize, Options.RenderCaptureImageSize);

	if (Options.bBakeDeviceDepth)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::DeviceDepth>(DefaultColorSample, SceneCapture, CaptureDimensions));
	}
	if (Options.bBakeBaseColor)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::BaseColor>(DefaultColorSample, SceneCapture, CaptureDimensions));
	}
	if (Options.bUsePackedMRS)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::CombinedMRS>(DefaultColorSample, SceneCapture, CaptureDimensions));
	}
	if (Options.bBakeRoughness)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::Roughness>(DefaultColorSample, SceneCapture, CaptureDimensions));
	}
	if (Options.bBakeMetallic)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::Metallic>(DefaultColorSample, SceneCapture, CaptureDimensions));
	}
	if (Options.bBakeSpecular)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::Specular>(DefaultColorSample, SceneCapture, CaptureDimensions));
	}
	if (Options.bBakeEmissive)
	{
		AddColorEvaluator(MakeColorEvaluator<ERenderCaptureType::Emissive>(DefaultColorSample, SceneCapture, CaptureDimensions));
	}
	if (Options.bBakeNormalMap)
	{
		TSharedPtr<FRenderCaptureMapEvaluator<FVector3f>> Evaluator = MakeShared<FRenderCaptureMapEvaluator<FVector3f>>();

		Evaluator->Channel = ERenderCaptureChannel::WorldNormal;

		Evaluator->DefaultResult = DefaultNormal;

		Evaluator->EvaluateSampleCallback = [this, &DefaultColorSample, &CaptureDimensions](const FMeshMapEvaluator::FCorrespondenceSample& Sample)
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
		InfillData.EvaluatorNeedsInfill.Add(false);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRenderCaptureMapBakerOp_CalculateResult_Bake);
		Result->Bake();
	}
}


//
// Tool Builder
//



const FToolTargetTypeRequirements& UBakeRenderCaptureToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({
		UMeshDescriptionProvider::StaticClass(),
		UPrimitiveComponentBackedTarget::StaticClass(),
		UStaticMeshBackedTarget::StaticClass(),			// FMeshSceneAdapter currently only supports StaticMesh targets
		UMaterialProvider::StaticClass()
		});
	return TypeRequirements;
}

bool UBakeRenderCaptureToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	const int32 NumTargets = SceneState.TargetManager->CountSelectedAndTargetable(SceneState, GetTargetRequirements());
	return (NumTargets > 1);
}

UMultiSelectionMeshEditingTool* UBakeRenderCaptureToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UBakeRenderCaptureTool>(SceneState.ToolManager);
}



//
// Tool
//



void UBakeRenderCaptureTool::Setup()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UBakeRenderCaptureTool::Setup);

	Super::Setup();

	InitializePreviewMaterials();

	// Initialize base mesh
	const FTransformSRT3d BaseToWorld = UE::ToolTarget::GetLocalToWorldTransform(Targets[0]);
	PreviewMesh->ProcessMesh([this, BaseToWorld](const FDynamicMesh3& Mesh)
	{
		TargetMesh.Copy(Mesh);
		TargetMeshTangents = MakeShared<FMeshTangentsd, ESPMode::ThreadSafe>(&TargetMesh);
		TargetMeshTangents->CopyTriVertexTangents(Mesh);
		
		// FMeshSceneAdapter operates in world space, so ensure our mesh transformed to world.
		MeshTransforms::ApplyTransform(TargetMesh, BaseToWorld, true);
		TargetSpatial.SetMesh(&TargetMesh, true);
	});

	// Initialize actors
	const int NumTargets = Targets.Num();
	Actors.Empty(NumTargets - 1);
	for (int Idx = 1; Idx < NumTargets; ++Idx)
	{
		if (AActor* Actor = UE::ToolTarget::GetTargetActor(Targets[Idx]))
		{
			Actors.Add(Actor);
		}
	}

	UToolTarget* Target = Targets[0];

	// Setup tool property sets

	Settings = NewObject<UBakeRenderCaptureToolProperties>(this);
	Settings->RestoreProperties(this);
	AddToolPropertySource(Settings);

	Settings->MapPreview = BaseColorTexParamName;
	Settings->WatchProperty(Settings->MapPreview, [this](FString) { UpdateVisualization(); GetToolManager()->PostInvalidation(); });
	Settings->WatchProperty(Settings->SamplesPerPixel, [this](EBakeTextureSamplesPerPixel) { OpState |= EBakeOpState::Evaluate; });
	Settings->WatchProperty(Settings->TextureSize, [this](EBakeTextureResolution) { OpState |= EBakeOpState::Evaluate; });
	Settings->WatchProperty(Settings->ValidSampleDepthThreshold, [this](float ValidSampleDepthThreshold)
	{
		// Only compute the device depth if we compute at least one other channel, the DeviceDepth is used to eliminate
		// occlusion artefacts from the other channels
		RenderCaptureProperties->bDeviceDepthMap = (ValidSampleDepthThreshold > 0) &&
			(
			RenderCaptureProperties->bBaseColorMap ||
			RenderCaptureProperties->bNormalMap    ||
			RenderCaptureProperties->bEmissiveMap  ||
			RenderCaptureProperties->bPackedMRSMap ||
			RenderCaptureProperties->bMetallicMap  ||
			RenderCaptureProperties->bRoughnessMap ||
			RenderCaptureProperties->bSpecularMap
			);

		OpState |= EBakeOpState::Evaluate;
	});

	RenderCaptureProperties = NewObject<URenderCaptureProperties>(this);
	RenderCaptureProperties->RestoreProperties(this);
	AddToolPropertySource(RenderCaptureProperties);

	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->Resolution, [this](EBakeTextureResolution) { OpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bBaseColorMap, [this](bool) { OpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bNormalMap, [this](bool) { OpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bMetallicMap, [this](bool) { OpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bRoughnessMap, [this](bool) { OpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bSpecularMap, [this](bool) { OpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bPackedMRSMap, [this](bool) { OpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bEmissiveMap, [this](bool) { OpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bAntiAliasing, [this](bool) { OpState |= EBakeOpState::Evaluate; });
	// These are not exposed to the UI, but we watch them anyway because we might change that later
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->CaptureFieldOfView, [this](float) { OpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->NearPlaneDist, [this](float) { OpState |= EBakeOpState::Evaluate; });
	
	InputMeshSettings = NewObject<UBakeRenderCaptureInputToolProperties>(this);
	InputMeshSettings->RestoreProperties(this);
	AddToolPropertySource(InputMeshSettings);
	InputMeshSettings->TargetStaticMesh = GetStaticMeshTarget(Target);
	UpdateUVLayerNames(InputMeshSettings->TargetUVLayer, InputMeshSettings->TargetUVLayerNamesList, TargetMesh);
	InputMeshSettings->WatchProperty(InputMeshSettings->TargetUVLayer, [this](FString) { OpState |= EBakeOpState::Evaluate; });
	
	{
		Settings->MapPreviewNamesList.Add(BaseColorTexParamName);
		Settings->MapPreviewNamesList.Add(NormalTexParamName);
		Settings->MapPreviewNamesList.Add(PackedMRSTexParamName);
		Settings->MapPreviewNamesList.Add(MetallicTexParamName);
		Settings->MapPreviewNamesList.Add(RoughnessTexParamName);
		Settings->MapPreviewNamesList.Add(SpecularTexParamName);
		Settings->MapPreviewNamesList.Add(EmissiveTexParamName);
	}

	ResultSettings = NewObject<UBakeRenderCaptureResults>(this);
	ResultSettings->RestoreProperties(this);
	AddToolPropertySource(ResultSettings);
	SetToolPropertySourceEnabled(ResultSettings, true);

	TargetUVLayerToError.Reset();

	// Used to implement SceneCapture cancellation
	ComputedRenderCaptureProperties = NewObject<URenderCaptureProperties>(this);

	// Hide the render capture meshes since this baker operates solely in world space which will occlude the preview of
	// the target mesh.
	for (int Idx = 1; Idx < NumTargets; ++Idx)
	{
		UE::ToolTarget::HideSourceObject(Targets[Idx]);
	}

	// Make sure we trigger SceneCapture computation in UpdateResult
	OpState |= EBakeOpState::Evaluate;
	ComputedRenderCaptureProperties->NearPlaneDist = 0.f; // Arbitrary invalid value

	SetToolDisplayName(LOCTEXT("ToolName", "Bake Render Capture"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Bake Render Capture. Select Bake Mesh (LowPoly) first, then select Detail Meshes (HiPoly) to bake. Assets will be created on Accept."),
		EToolMessageLevel::UserNotification);

	PostSetup();
}





void UBakeRenderCaptureTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);

	const float Brightness = VisualizationProps->Brightness;
	const FVector BrightnessColor(Brightness, Brightness, Brightness);
	PreviewMaterialRC->SetVectorParameterValue(TEXT("Brightness"), BrightnessColor);
	PreviewMaterialPackedRC->SetVectorParameterValue(TEXT("Brightness"), BrightnessColor);
}




void UBakeRenderCaptureTool::OnShutdown(EToolShutdownType ShutdownType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UBakeRenderCaptureTool::Shutdown);
	
	Super::OnShutdown(ShutdownType);
	
	Settings->SaveProperties(this);
	RenderCaptureProperties->SaveProperties(this);
	InputMeshSettings->SaveProperties(this);

	if (Compute)
	{
		Compute->Shutdown();
	}

	// Restore visibility of source meshes
	const int NumTargets = Targets.Num();
	for (int Idx = 1; Idx < NumTargets; ++Idx)
	{
		UE::ToolTarget::ShowSourceObject(Targets[Idx]);
	}
	
	if (ShutdownType == EToolShutdownType::Accept)
	{
		IStaticMeshBackedTarget* StaticMeshTarget = Cast<IStaticMeshBackedTarget>(Targets[0]);
		UObject* SourceAsset = StaticMeshTarget ? StaticMeshTarget->GetStaticMesh() : nullptr;
		const UPrimitiveComponent* SourceComponent = UE::ToolTarget::GetTargetComponent(Targets[0]);
		CreateTextureAssetsRC(SourceComponent->GetWorld(), SourceAsset);
	}

	// Clear actors on shutdown so that their lifetime is not tied to the lifetime of the tool
	Actors.Empty();
}

void UBakeRenderCaptureTool::CreateTextureAssetsRC(UWorld* SourceWorld, UObject* SourceAsset)
{
	bool bCreatedAssetOK = true;
	const FString BaseName = UE::ToolTarget::GetTargetActor(Targets[0])->GetActorNameOrLabel();

	auto CreateTextureAsset = [this, &bCreatedAssetOK, &SourceWorld, &SourceAsset](const FString& TexName, FTexture2DBuilder::ETextureType Type, TObjectPtr<UTexture2D> Tex)
	{
		// See :DeferredPopulateSourceData
		FTexture2DBuilder::CopyPlatformDataToSourceData(Tex, Type);

		// TODO The original implementation in ApproximateActors also did the following, see WriteTextureLambda in ApproximateActorsImpl.cpp
		//if (Type == FTexture2DBuilder::ETextureType::Roughness
		//	|| Type == FTexture2DBuilder::ETextureType::Metallic
		//	|| Type == FTexture2DBuilder::ETextureType::Specular)
		//{
		//	UE::AssetUtils::ConvertToSingleChannel(Texture);
		//}

		bCreatedAssetOK = bCreatedAssetOK &&
			UE::Modeling::CreateTextureObject(
				GetToolManager(),
				FCreateTextureObjectParams{ 0, SourceWorld, SourceAsset, TexName, Tex }).IsOK();
	};

	if (RenderCaptureProperties->bBaseColorMap && ResultSettings->BaseColorMap != nullptr)
	{
		const FString TexName = FString::Printf(TEXT("%s_%s"), *BaseName, *BaseColorTexParamName);
		CreateTextureAsset(TexName, FTexture2DBuilder::ETextureType::Color, ResultSettings->BaseColorMap);
	}

	if (RenderCaptureProperties->bNormalMap && ResultSettings->NormalMap != nullptr)
	{
		const FString TexName = FString::Printf(TEXT("%s_%s"), *BaseName, *NormalTexParamName);
		CreateTextureAsset(TexName, FTexture2DBuilder::ETextureType::NormalMap, ResultSettings->NormalMap);
	}

	if (RenderCaptureProperties->bEmissiveMap && ResultSettings->EmissiveMap != nullptr)
	{
		const FString TexName = FString::Printf(TEXT("%s_%s"), *BaseName, *EmissiveTexParamName);
		CreateTextureAsset(TexName, FTexture2DBuilder::ETextureType::EmissiveHDR, ResultSettings->EmissiveMap);
	}

	// We need different code paths based on PackedMRS here because we don't want to uncheck the separate channels
	// when PackedMRS is enabled to give the user a better UX (they don't have to re-check them after disabling
	// PackedMRS). In other place we can test the PackedMRS and separate channel booleans in series and avoid the
	// complexity of nested if statements.
	if (RenderCaptureProperties->bPackedMRSMap && ResultSettings->PackedMRSMap != nullptr)
	{
		const FString TexName = FString::Printf(TEXT("%s_%s"), *BaseName, *PackedMRSTexParamName);
		CreateTextureAsset(TexName, FTexture2DBuilder::ETextureType::ColorLinear, ResultSettings->PackedMRSMap);
	}
	else
	{
		if (RenderCaptureProperties->bMetallicMap && ResultSettings->MetallicMap != nullptr)
		{
			const FString TexName = FString::Printf(TEXT("%s_%s"), *BaseName, *MetallicTexParamName);
			CreateTextureAsset(TexName, FTexture2DBuilder::ETextureType::Metallic, ResultSettings->MetallicMap);
		}

		if (RenderCaptureProperties->bRoughnessMap && ResultSettings->RoughnessMap != nullptr)
		{
			const FString TexName = FString::Printf(TEXT("%s_%s"), *BaseName, *RoughnessTexParamName);
			CreateTextureAsset(TexName, FTexture2DBuilder::ETextureType::Roughness, ResultSettings->RoughnessMap);
		}

		if (RenderCaptureProperties->bSpecularMap && ResultSettings->SpecularMap != nullptr)
		{
			const FString TexName = FString::Printf(TEXT("%s_%s"), *BaseName, *SpecularTexParamName);
			CreateTextureAsset(TexName, FTexture2DBuilder::ETextureType::Specular, ResultSettings->SpecularMap);
		}
	}

	ensure(bCreatedAssetOK);

	RecordAnalytics();
}



bool UBakeRenderCaptureTool::CanAccept() const
{
	if ((OpState & EBakeOpState::Invalid) == EBakeOpState::Invalid)
	{
		return false;
	}

	if (RenderCaptureProperties->bBaseColorMap && ResultSettings->BaseColorMap == nullptr)
	{
		return false;
	}
	if (RenderCaptureProperties->bNormalMap && ResultSettings->NormalMap == nullptr)
	{
		return false;
	}
	if (RenderCaptureProperties->bEmissiveMap && ResultSettings->EmissiveMap == nullptr)
	{
		return false;
	}
	
	// We need different code paths based on PackedMRS here because we don't want to uncheck the separate channels
	// when PackedMRS is enabled to give the user a better UX (they don't have to re-check them after disabling
	// PackedMRS). In other place we can test the PackedMRS and separate channel booleans in series and avoid the
	// complexity of nested if statements.
	if (RenderCaptureProperties->bPackedMRSMap)
	{
		if (ResultSettings->PackedMRSMap == nullptr)
		{
			return false;
		}
	}
	else
	{
		if (RenderCaptureProperties->bMetallicMap && ResultSettings->MetallicMap == nullptr)
		{
			return false;
		}
		if (RenderCaptureProperties->bRoughnessMap && ResultSettings->RoughnessMap == nullptr)
		{
			return false;
		}
		if (RenderCaptureProperties->bSpecularMap && ResultSettings->SpecularMap == nullptr)
		{
			return false;
		}
	}

	return true;
}



TUniquePtr<TGenericDataOperator<FMeshMapBaker>> UBakeRenderCaptureTool::MakeNewOperator()
{
	TUniquePtr<FRenderCaptureMapBakerOp> Op = MakeUnique<FRenderCaptureMapBakerOp>();
	Op->BaseMesh = &TargetMesh;
	Op->BaseMeshTangents = TargetMeshTangents;
	Op->Options = FRenderCaptureSettings::ConstructOptions(*RenderCaptureProperties, *Settings, *InputMeshSettings);
	Op->TextureImageSize = static_cast<int32>(Settings->TextureSize);
	Op->SamplesPerPixel = Settings->SamplesPerPixel;
	Op->SceneCapture = SceneCapture.Get();
	return Op;
}



void UBakeRenderCaptureTool::OnMapsUpdatedRC(const TUniquePtr<FMeshMapBaker>& NewResult)
{
	// We do this to defer work I guess, it was like this in the original ApproximateActors implementation :DeferredPopulateSourceData 
	constexpr bool bPopulateSourceData = false;

	TRACE_CPUPROFILER_EVENT_SCOPE(BakeRenderCaptureTool_Textures_BuildTextures);

	const int32 NumEval = NewResult->NumEvaluators();
	for (int32 EvalIdx = 0; EvalIdx < NumEval; ++EvalIdx)
	{
		FMeshMapEvaluator* BaseEval = NewResult->GetEvaluator(EvalIdx);

		check(BaseEval->DataLayout().Num() == 1);

		switch (BaseEval->DataLayout()[0])
		{
		case FMeshMapEvaluator::EComponents::Float4:

			{
				FRenderCaptureMapEvaluator<FVector4f>* Eval = static_cast<FRenderCaptureMapEvaluator<FVector4f>*>(BaseEval);
				TUniquePtr<TImageBuilder<FVector4f>> ImageBuilder = MoveTemp(NewResult->GetBakeResults(EvalIdx)[0]);
				
				if (ensure(ImageBuilder.IsValid()) == false) return;

				switch (Eval->Channel)
				{
				case ERenderCaptureChannel::BaseColor:

					ResultSettings->BaseColorMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::Color,
						true,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::Roughness:

					ResultSettings->RoughnessMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::Roughness,
						false,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::Metallic:

					ResultSettings->MetallicMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::Metallic,
						false,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::Specular:

					ResultSettings->SpecularMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::Specular,
						false,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::Emissive:

					ResultSettings->EmissiveMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::EmissiveHDR,
						false,
						bPopulateSourceData);
					ResultSettings->EmissiveMap->CompressionSettings = TC_HDR_Compressed;
					break;

				case ERenderCaptureChannel::CombinedMRS:

					ResultSettings->PackedMRSMap = FTexture2DBuilder::BuildTextureFromImage(
						*ImageBuilder,
						FTexture2DBuilder::ETextureType::ColorLinear,
						false,
						bPopulateSourceData);
					break;

				case ERenderCaptureChannel::DeviceDepth:
					// Do nothing, the depth capture is used internally and not presented to the user
					break;

				default:
					ensure(false);
					return;
				}

			} break; // Float4

		case FMeshMapEvaluator::EComponents::Float3:

			{
				FRenderCaptureMapEvaluator<FVector3f>* Eval = static_cast<FRenderCaptureMapEvaluator<FVector3f>*>(BaseEval);
				TUniquePtr<TImageBuilder<FVector4f>> ImageBuilder = MoveTemp(NewResult->GetBakeResults(EvalIdx)[0]);
				
				if (ensure(ImageBuilder.IsValid()) == false) return;
				if (ensure(Eval->Channel == ERenderCaptureChannel::WorldNormal) == false) return;

				ResultSettings->NormalMap = FTexture2DBuilder::BuildTextureFromImage(
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

	GatherAnalytics(*NewResult);
	UpdateVisualization();
	GetToolManager()->PostInvalidation();
}


void UBakeRenderCaptureTool::InitializePreviewMaterials()
{
	// EmptyColorMapWhite, EmptyColorMapBlack and EmptyNormalMap are defined in the base tool

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::EmissiveHDR, FImageDimensions(16, 16));
		Builder.Commit(false);
		EmptyEmissiveMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::ColorLinear, FImageDimensions(16, 16));
		Builder.Clear(FColor(0,0,0));
		Builder.Commit(false);
		EmptyPackedMRSMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::Roughness, FImageDimensions(16, 16));
		Builder.Commit(false);
		EmptyRoughnessMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::Metallic, FImageDimensions(16, 16));
		Builder.Commit(false);
		EmptyMetallicMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::Specular, FImageDimensions(16, 16));
		Builder.Commit(false);
		EmptySpecularMap = Builder.GetTexture2D();
	}
	
	{
		UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/MeshModelingToolsetExp/Materials/BakeRenderCapturePreviewMaterial"));
		check(Material);
		if (Material != nullptr)
		{
			PreviewMaterialRC = UMaterialInstanceDynamic::Create(Material, GetToolManager());
			PreviewMaterialRC->SetTextureParameterValue(TEXT("BaseColor"), EmptyColorMapWhite);
			PreviewMaterialRC->SetTextureParameterValue(TEXT("Roughness"), EmptyRoughnessMap);
			PreviewMaterialRC->SetTextureParameterValue(TEXT("Metallic"), EmptyMetallicMap);
			PreviewMaterialRC->SetTextureParameterValue(TEXT("Specular"), EmptySpecularMap);
			PreviewMaterialRC->SetTextureParameterValue(TEXT("Emissive"), EmptyEmissiveMap);
			PreviewMaterialRC->SetTextureParameterValue(TEXT("NormalMap"), EmptyNormalMap);
		}
	}
	
	{
		UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/MeshModelingToolsetExp/Materials/FullMaterialBakePreviewMaterial_PackedMRS"));
		check(Material);
		if (Material != nullptr)
		{
			PreviewMaterialPackedRC = UMaterialInstanceDynamic::Create(Material, GetToolManager());
			PreviewMaterialPackedRC->SetTextureParameterValue(TEXT("BaseColor"), EmptyColorMapWhite);
			PreviewMaterialPackedRC->SetTextureParameterValue(TEXT("PackedMRS"), EmptyPackedMRSMap);
			PreviewMaterialPackedRC->SetTextureParameterValue(TEXT("Emissive"), EmptyEmissiveMap);
			PreviewMaterialPackedRC->SetTextureParameterValue(TEXT("NormalMap"), EmptyNormalMap);
		}
	}
}


void UBakeRenderCaptureTool::InvalidateComputeRC()
{
	// Note: This implementation is identical to UBakeMeshAttributeMapsToolBase::InvalidateCompute but calls
	// OnMapsUpdatedRC rather than OnMapsUpdated
	if (!Compute)
	{
		// Initialize background compute
		Compute = MakeUnique<TGenericDataBackgroundCompute<FMeshMapBaker>>();
		Compute->Setup(this);
		Compute->OnResultUpdated.AddLambda([this](const TUniquePtr<FMeshMapBaker>& NewResult) { OnMapsUpdatedRC(NewResult); });
	}
	Compute->InvalidateResult();
}


void UBakeRenderCaptureTool::UpdateResult()
{
	if (OpState == EBakeOpState::Clean)
	{
		// Evaluation already launched/complete. Note that the Compute background compute updates ResultSettings when
		// they are available by calling OnMapsUpdatedRC in its OnResultUpdated delegate.
		return;
	}

	//
	// create a set of spatially located render captures of the scene ("photo set"). We need to recompute this if the
	// render capture properties changed. Note we only compare the URenderCaptureProperties only, and not the
	// ValidSampleDepthThreshold, this is intentional so that we only trigger a scene capture recompute when we go from
	// a zero to a positive threshold (we need to compute the depth capture), or a positive to a zero threshold (we can
	// save memory and not compute the depth capture), we don't need to recompute the scene capture when the user is
	// changing between positive threshold values.
	//
	if (*RenderCaptureProperties != *ComputedRenderCaptureProperties)
	{
		for (int Idx = 1; Idx < Targets.Num(); ++Idx)
		{
			UE::ToolTarget::ShowSourceObject(Targets[Idx]);
		}

		// Do not allow user-cancellation on the call that occurs when the Render Capture Tool starts up
		const bool bAllowCancel = (bFirstEverSceneCapture == false);

		SceneCapture.Reset();
		FRenderCaptureSettings::FOptions Options = FRenderCaptureSettings::ConstructOptions(*RenderCaptureProperties, *Settings, *InputMeshSettings);
		SceneCapture = CapturePhotoSet(Actors, Options, bAllowCancel);

		for (int Idx = 1; Idx < Targets.Num(); ++Idx)
		{
			UE::ToolTarget::HideSourceObject(Targets[Idx]);
		}

		if (SceneCapture->Cancelled())
		{
			// Restore the settings present before the change that invoked the scene capture recompute
			RenderCaptureProperties->Resolution         = ComputedRenderCaptureProperties->Resolution;
			RenderCaptureProperties->bBaseColorMap      = ComputedRenderCaptureProperties->bBaseColorMap;
			RenderCaptureProperties->bNormalMap         = ComputedRenderCaptureProperties->bNormalMap;
			RenderCaptureProperties->bMetallicMap       = ComputedRenderCaptureProperties->bMetallicMap;
			RenderCaptureProperties->bRoughnessMap      = ComputedRenderCaptureProperties->bRoughnessMap;
			RenderCaptureProperties->bSpecularMap       = ComputedRenderCaptureProperties->bSpecularMap;
			RenderCaptureProperties->bPackedMRSMap      = ComputedRenderCaptureProperties->bPackedMRSMap;
			RenderCaptureProperties->bEmissiveMap       = ComputedRenderCaptureProperties->bEmissiveMap;
			RenderCaptureProperties->bAntiAliasing      = ComputedRenderCaptureProperties->bAntiAliasing;
			RenderCaptureProperties->bDeviceDepthMap    = ComputedRenderCaptureProperties->bDeviceDepthMap;
			RenderCaptureProperties->CaptureFieldOfView = ComputedRenderCaptureProperties->CaptureFieldOfView;
			RenderCaptureProperties->NearPlaneDist      = ComputedRenderCaptureProperties->NearPlaneDist;
			Settings->ValidSampleDepthThreshold = ComputedValidDepthThreshold;

			// Silently make the above updates so we don't overwrite the change to OpState below and call this function again
			RenderCaptureProperties->SilentUpdateWatched();
			Settings->SilentUpdateWatched();

			OpState = EBakeOpState::Clean;

			return;
		}

		// Cache Settings used to compute this SceneCapture so we can restore them if the user cancels a SceneCapture recompute
		ComputedRenderCaptureProperties->Resolution         = RenderCaptureProperties->Resolution;
		ComputedRenderCaptureProperties->bBaseColorMap      = RenderCaptureProperties->bBaseColorMap;
		ComputedRenderCaptureProperties->bNormalMap         = RenderCaptureProperties->bNormalMap;
		ComputedRenderCaptureProperties->bMetallicMap       = RenderCaptureProperties->bMetallicMap;
		ComputedRenderCaptureProperties->bRoughnessMap      = RenderCaptureProperties->bRoughnessMap;
		ComputedRenderCaptureProperties->bSpecularMap       = RenderCaptureProperties->bSpecularMap;
		ComputedRenderCaptureProperties->bPackedMRSMap      = RenderCaptureProperties->bPackedMRSMap;
		ComputedRenderCaptureProperties->bEmissiveMap       = RenderCaptureProperties->bEmissiveMap;
		ComputedRenderCaptureProperties->bAntiAliasing      = RenderCaptureProperties->bAntiAliasing;
		ComputedRenderCaptureProperties->bDeviceDepthMap    = RenderCaptureProperties->bDeviceDepthMap;
		ComputedRenderCaptureProperties->CaptureFieldOfView = RenderCaptureProperties->CaptureFieldOfView;
		ComputedRenderCaptureProperties->NearPlaneDist      = RenderCaptureProperties->NearPlaneDist;
		ComputedValidDepthThreshold = Settings->ValidSampleDepthThreshold;

		bFirstEverSceneCapture = false;
	}

	FText ErrorMessage; // Empty message indicates no error

	{
		const int32 TargetUVLayer = InputMeshSettings->GetTargetUVLayerIndex();
		if (FText* Message = TargetUVLayerToError.Find(TargetUVLayer); Message)
		{
			ErrorMessage = *Message;
		}
		else
		{
			const auto HasDegenerateUVs = [this]
			{
				FDynamicMeshUVOverlay* UVOverlay = TargetMesh.Attributes()->GetUVLayer(InputMeshSettings->GetTargetUVLayerIndex());
				FAxisAlignedBox2f Bounds = FAxisAlignedBox2f::Empty();
				for (const int Index : UVOverlay->ElementIndicesItr())
				{
					FVector2f UV;
					UVOverlay->GetElement(Index, UV);
					Bounds.Contain(UV);
				}
				return Bounds.Min == Bounds.Max;
			};

			if (TargetMesh.Attributes()->GetUVLayer(InputMeshSettings->GetTargetUVLayerIndex()) == nullptr)
			{
				ErrorMessage = LOCTEXT("TargetMeshMissingUVs", "The Target Mesh UV layer is missing");
			}
			else if (HasDegenerateUVs())
			{
				ErrorMessage = LOCTEXT("TargetMeshDegenerateUVs", "The Target Mesh UV layer is degenerate");
			}
			else
			{
				ErrorMessage = FText(); // No error
			}
			TargetUVLayerToError.Add(TargetUVLayer, ErrorMessage);
		}

		// If there are no UV layer errors check for missing tangent space error
		if (ErrorMessage.IsEmpty() && RenderCaptureProperties->bNormalMap && ValidTargetMeshTangents() == false)
		{
			ErrorMessage = LOCTEXT("TargetMeshMissingTangentSpace", "The Target Mesh is missing a tangent space. Disable Normal Map capture to continue.");
		}
	}

	// Calling DisplayMessage with an empty string will clear existing messages
	GetToolManager()->DisplayMessage(ErrorMessage, EToolMessageLevel::UserWarning);

	InvalidateResults();

	const bool bIsInvalid = (ErrorMessage.IsEmpty() == false);
	if (bIsInvalid)
	{
		const bool bWasValid = static_cast<bool>(OpState & EBakeOpState::Invalid) == false;
		if (bWasValid)
		{
			UpdateVisualization(); // Clear the preview mesh material inputs
		}
		OpState = EBakeOpState::Invalid;
		return;
	}

	InvalidateComputeRC();

	OpState = EBakeOpState::Clean;
}


void UBakeRenderCaptureTool::UpdateVisualization()
{
	if (Settings->MapPreview.IsEmpty())
	{
		return;
	}

	if (ResultSettings->PackedMRSMap)
	{
		TObjectPtr<UMaterialInstanceDynamic> Material = PreviewMaterialPackedRC;
		PreviewMesh->SetOverrideRenderMaterial(Material);

		if (VisualizationProps->bPreviewAsMaterial)
		{
			// We set all textures which were computed in the corresponding texture channels
			Material->SetTextureParameterValue(FName(BaseColorTexParamName), ResultSettings->BaseColorMap ? ResultSettings->BaseColorMap : EmptyColorMapWhite);
			Material->SetTextureParameterValue(FName(EmissiveTexParamName),  ResultSettings->EmissiveMap  ? ResultSettings->EmissiveMap  : EmptyEmissiveMap);
			Material->SetTextureParameterValue(FName(NormalTexParamName),    ResultSettings->NormalMap    ? ResultSettings->NormalMap    : EmptyNormalMap);
			Material->SetTextureParameterValue(FName(PackedMRSTexParamName), ResultSettings->PackedMRSMap);
		}
		else
		{
			// The BaseColor texture channel will be set according to the selected MapPreview
			TObjectPtr<UTexture2D> BaseColorMap = EmptyColorMapWhite;
			if (ResultSettings->BaseColorMap && Settings->MapPreview == BaseColorTexParamName)
			{
				BaseColorMap = ResultSettings->BaseColorMap;
			}
			else if (ResultSettings->EmissiveMap && Settings->MapPreview == EmissiveTexParamName)
			{
				BaseColorMap = ResultSettings->EmissiveMap;
			}
			else if (ResultSettings->NormalMap && Settings->MapPreview == NormalTexParamName)
			{
				BaseColorMap = ResultSettings->NormalMap;
			}
			else if (ResultSettings->PackedMRSMap && Settings->MapPreview == PackedMRSTexParamName)
			{
				BaseColorMap = ResultSettings->PackedMRSMap;
			}
			Material->SetTextureParameterValue(FName(BaseColorTexParamName), BaseColorMap);
			Material->SetTextureParameterValue(FName(EmissiveTexParamName),  EmptyEmissiveMap);
			Material->SetTextureParameterValue(FName(NormalTexParamName), EmptyNormalMap);
			Material->SetTextureParameterValue(FName(PackedMRSTexParamName), EmptyPackedMRSMap);
		}

		Material->SetScalarParameterValue(TEXT("UVChannel"), InputMeshSettings->GetTargetUVLayerIndex());
	}
	else
	{
		TObjectPtr<UMaterialInstanceDynamic> Material = PreviewMaterialRC;
		PreviewMesh->SetOverrideRenderMaterial(Material);

		if (VisualizationProps->bPreviewAsMaterial)
		{
			// We set all textures which were computed in the corresponding texture channels
			Material->SetTextureParameterValue(FName(BaseColorTexParamName), ResultSettings->BaseColorMap ? ResultSettings->BaseColorMap : EmptyColorMapWhite);
			Material->SetTextureParameterValue(FName(RoughnessTexParamName), ResultSettings->RoughnessMap ? ResultSettings->RoughnessMap : EmptyRoughnessMap);
			Material->SetTextureParameterValue(FName(MetallicTexParamName),  ResultSettings->MetallicMap  ? ResultSettings->MetallicMap  : EmptyMetallicMap);
			Material->SetTextureParameterValue(FName(SpecularTexParamName),  ResultSettings->SpecularMap  ? ResultSettings->SpecularMap  : EmptySpecularMap);
			Material->SetTextureParameterValue(FName(EmissiveTexParamName),  ResultSettings->EmissiveMap  ? ResultSettings->EmissiveMap  : EmptyEmissiveMap);
			Material->SetTextureParameterValue(FName(NormalTexParamName),    ResultSettings->NormalMap    ? ResultSettings->NormalMap    : EmptyNormalMap);
		}
		else
		{
			// The BaseColor texture channel will be set according to the selected MapPreview
			TObjectPtr<UTexture2D> BaseColorMap = EmptyColorMapWhite;
			if (ResultSettings->BaseColorMap && Settings->MapPreview == BaseColorTexParamName)
			{
				BaseColorMap = ResultSettings->BaseColorMap;
			}
			else if (ResultSettings->RoughnessMap && Settings->MapPreview == RoughnessTexParamName)
			{
				BaseColorMap = ResultSettings->RoughnessMap;
			}
			else if (ResultSettings->MetallicMap && Settings->MapPreview == MetallicTexParamName)
			{
				BaseColorMap = ResultSettings->MetallicMap;
			}
			else if (ResultSettings->SpecularMap && Settings->MapPreview == SpecularTexParamName)
			{
				BaseColorMap = ResultSettings->SpecularMap;
			}
			else if (ResultSettings->EmissiveMap && Settings->MapPreview == EmissiveTexParamName)
			{
				BaseColorMap = ResultSettings->EmissiveMap;
			}
			else if (ResultSettings->NormalMap && Settings->MapPreview == NormalTexParamName)
			{
				BaseColorMap = ResultSettings->NormalMap;
			}
			Material->SetTextureParameterValue(TEXT("BaseColor"), BaseColorMap);
			
			Material->SetTextureParameterValue(TEXT("Roughness"), EmptyRoughnessMap);
			Material->SetTextureParameterValue(TEXT("Metallic"),  EmptyMetallicMap);
			Material->SetTextureParameterValue(TEXT("Specular"),  EmptySpecularMap);
			Material->SetTextureParameterValue(TEXT("Emissive"),  EmptyEmissiveMap);
			Material->SetTextureParameterValue(TEXT("NormalMap"), EmptyNormalMap);
		}
		
		Material->SetScalarParameterValue(TEXT("UVChannel"), InputMeshSettings->GetTargetUVLayerIndex());
	}
}



void UBakeRenderCaptureTool::InvalidateResults()
{
	ResultSettings->BaseColorMap = nullptr;
	ResultSettings->RoughnessMap = nullptr;
	ResultSettings->MetallicMap = nullptr;
	ResultSettings->SpecularMap = nullptr;
	ResultSettings->PackedMRSMap = nullptr;
	ResultSettings->EmissiveMap = nullptr;
	ResultSettings->NormalMap = nullptr;
}



void UBakeRenderCaptureTool::RecordAnalytics() const
{
	if (FEngineAnalytics::IsAvailable() == false)
	{
		return;
	}

	TArray<FAnalyticsEventAttribute> Attributes;

	// General
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Bake.Duration.Total.Seconds"), BakeAnalytics.TotalBakeDuration));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Bake.Duration.WriteToImage.Seconds"), BakeAnalytics.WriteToImageDuration));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Bake.Duration.WriteToGutter.Seconds"), BakeAnalytics.WriteToGutterDuration));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Bake.Stats.NumSamplePixels"), BakeAnalytics.NumSamplePixels));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Bake.Stats.NumGutterPixels"), BakeAnalytics.NumGutterPixels));

	// Input mesh data
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Input.TargetMesh.NumTriangles"), BakeAnalytics.MeshSettings.NumTargetMeshTris));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Input.RenderCapture.NumMeshes"), BakeAnalytics.MeshSettings.NumDetailMesh));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Input.RenderCapture.NumTriangles"), BakeAnalytics.MeshSettings.NumDetailMeshTris));

	// Bake settings
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.Image.Width"), static_cast<int32>(Settings->TextureSize)));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.Image.Height"), static_cast<int32>(Settings->TextureSize)));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.SamplesPerPixel"), static_cast<int32>(Settings->SamplesPerPixel)));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.TargetUVLayer"), InputMeshSettings->GetTargetUVLayerIndex()));

	// Render Capture settings
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.Image.Width"), static_cast<int32>(RenderCaptureProperties->Resolution)));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.Image.Height"), static_cast<int32>(RenderCaptureProperties->Resolution)));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.BaseColorMap.Enabled"), RenderCaptureProperties->bBaseColorMap));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.NormalMap.Enabled"), RenderCaptureProperties->bNormalMap));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.MetallicMap.Enabled"), RenderCaptureProperties->bMetallicMap));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.RoughnessMap.Enabled"), RenderCaptureProperties->bRoughnessMap));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.SpecularMap.Enabled"), RenderCaptureProperties->bSpecularMap));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.PackedMRSMap.Enabled"), RenderCaptureProperties->bPackedMRSMap));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.EmissiveMap.Enabled"), RenderCaptureProperties->bEmissiveMap));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.CaptureFieldOfView"), RenderCaptureProperties->CaptureFieldOfView));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.NearPlaneDistance"), RenderCaptureProperties->NearPlaneDist));

	FEngineAnalytics::GetProvider().RecordEvent(FString(TEXT("Editor.Usage.MeshModelingMode.")) + GetAnalyticsEventName(), Attributes);

	constexpr bool bDebugLogAnalytics = false;
	if constexpr (bDebugLogAnalytics)
	{
		for (const FAnalyticsEventAttribute& Attr : Attributes)
		{
			UE_LOG(LogGeometry, Log, TEXT("[%s] %s = %s"), *GetAnalyticsEventName(), *Attr.GetName(), *Attr.GetValue());
		}
	}
}


void UBakeRenderCaptureTool::GatherAnalytics(const FMeshMapBaker& Result)
{
	if (!FEngineAnalytics::IsAvailable())
	{
		return;
	}

	BakeAnalytics.TotalBakeDuration = Result.BakeAnalytics.TotalBakeDuration;
	BakeAnalytics.WriteToImageDuration = Result.BakeAnalytics.WriteToImageDuration;
	BakeAnalytics.WriteToGutterDuration = Result.BakeAnalytics.WriteToGutterDuration;
	BakeAnalytics.NumSamplePixels = Result.BakeAnalytics.NumSamplePixels;
	BakeAnalytics.NumGutterPixels = Result.BakeAnalytics.NumGutterPixels;
}


void UBakeRenderCaptureTool::GatherAnalytics(FBakeAnalytics::FMeshSettings& Data)
{
	if (FEngineAnalytics::IsAvailable() == false)
	{
		return;
	}

	Data.NumTargetMeshTris = TargetMesh.TriangleCount();
	Data.NumDetailMesh = Actors.Num();
	Data.NumDetailMeshTris = 0;
	for (AActor* Actor : Actors)
	{
		check(Actor != nullptr);
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimitiveComponent))
			{
				if (StaticMeshComponent->GetStaticMesh() != nullptr)
				{
					// TODO We could also check GetNumNaniteTriangles here and use the maximum
					Data.NumDetailMeshTris += StaticMeshComponent->GetStaticMesh()->GetNumTriangles(0);
				}
			} 
		}
	}
}

#undef LOCTEXT_NAMESPACE

