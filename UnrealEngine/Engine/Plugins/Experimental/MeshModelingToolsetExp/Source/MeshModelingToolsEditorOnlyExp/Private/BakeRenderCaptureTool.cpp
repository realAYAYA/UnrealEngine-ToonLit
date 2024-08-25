// Copyright Epic Games, Inc. All Rights Reserved.

#include "BakeRenderCaptureTool.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/MeshDescriptionProvider.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "TargetInterfaces/StaticMeshBackedTarget.h"
#include "ToolTargetManager.h"
#include "InteractiveToolManager.h"

#include "DynamicMesh/MeshTransforms.h"

#include "BakeToolUtils.h"
#include "ToolSetupUtil.h"
#include "ModelingToolTargetUtil.h"

#include "ModelingObjectsCreationAPI.h"

#include "EngineAnalytics.h"

#include "Baking/BakingTypes.h"
#include "Sampling/MeshImageBakingCache.h"
#include "Sampling/MeshMapBaker.h"
#include "AssetUtils/Texture2DBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BakeRenderCaptureTool)


using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UBakeRenderCaptureTool"


static FString BaseColorTexParamName = TEXT("BaseColor");
static FString RoughnessTexParamName = TEXT("Roughness");
static FString MetallicTexParamName = TEXT("Metallic");
static FString SpecularTexParamName = TEXT("Specular");
static FString EmissiveTexParamName = TEXT("Emissive");
static FString OpacityTexParamName  = TEXT("Opacity");
static FString SubsurfaceColorTexParamName = TEXT("SubsurfaceColor");
static FString NormalTexParamName = TEXT("NormalMap");
static FString PackedMRSTexParamName = TEXT("PackedMRS");


FSceneCaptureConfig GetSceneCaptureConfig(const URenderCaptureProperties& Properties)
{
	FSceneCaptureConfig Config;

	Config.Flags.bBaseColor       = Properties.bBaseColorMap;
	Config.Flags.bWorldNormal     = Properties.bNormalMap;
	Config.Flags.bEmissive        = Properties.bEmissiveMap;
	Config.Flags.bOpacity         = Properties.bOpacityMap;
	Config.Flags.bSubsurfaceColor = Properties.bSubsurfaceColorMap;
	Config.Flags.bDeviceDepth     = Properties.bDeviceDepthMap;

	// When we convert Properties (controls the tool UI) to Config (controls what is computed in SceneCapture) we make
	// sure that the PackedMRS and the separate Metallic/Roughness/Specular captures are mutually exclusive. These
	// captures are intended to be used in different materials so we assume the user will want one or the other. Also,
	// for high resolution captures with lots of viewpoints each enabled capture takes up a lot of memory. We could
	// enforce this mutually exclusive behaviour in Properties but doing it Config improves the UX because enabling the
	// PackedMRS checkbox can just clear the Metallic/Roughness/Specular textures and photosets and disable the
	// corresponding checkboxes while leaving the checkbox state unchanged. See :HandlingPackedMRS
	Config.Flags.bCombinedMRS = Properties.bPackedMRSMap;
	Config.Flags.bMetallic    = Properties.bPackedMRSMap ? false : Properties.bMetallicMap;
	Config.Flags.bRoughness   = Properties.bPackedMRSMap ? false : Properties.bRoughnessMap;
	Config.Flags.bSpecular    = Properties.bPackedMRSMap ? false : Properties.bSpecularMap;

	Config.RenderCaptureImageSize = static_cast<int32>(Properties.Resolution);
	Config.bAntiAliasing          = Properties.bAntiAliasing;
	Config.FieldOfViewDegrees     = Properties.CaptureFieldOfView;
	Config.NearPlaneDist          = Properties.NearPlaneDist;

	return Config;
}

void SetSceneCaptureConfig(URenderCaptureProperties& Properties, const FSceneCaptureConfig& Config)
{
	Properties.bBaseColorMap       = Config.Flags.bBaseColor;
	Properties.bNormalMap          = Config.Flags.bWorldNormal;
	Properties.bEmissiveMap        = Config.Flags.bEmissive;
	Properties.bOpacityMap         = Config.Flags.bOpacity;
	Properties.bSubsurfaceColorMap = Config.Flags.bSubsurfaceColor;
	Properties.bDeviceDepthMap     = Config.Flags.bDeviceDepth;

	// When we convert Config (controls what is computed in SceneCapture) to Properties (controls the tool UI) we
	// can directly copy the MRS booleans since Properties must always reflect the state of the computed SceneCapture
	// channels. The reverse conversion in GetSceneCaptureConfig (see comment tagged :HandlingPackedMRS) ensures that
	// the PackedMRS and separate Metallic/Roughness/Specular captures are mutually exclusive.
	Properties.bPackedMRSMap = Config.Flags.bCombinedMRS;
	Properties.bMetallicMap  = Config.Flags.bMetallic;
	Properties.bRoughnessMap = Config.Flags.bRoughness;
	Properties.bSpecularMap  = Config.Flags.bSpecular;

	Properties.Resolution         = static_cast<EBakeTextureResolution>(Config.RenderCaptureImageSize);
	Properties.bAntiAliasing      = Config.bAntiAliasing;
	Properties.CaptureFieldOfView = Config.FieldOfViewDegrees;
	Properties.NearPlaneDist      = Config.NearPlaneDist;
}


//
// Tool Properties
//

bool UBakeRenderCaptureResults::IsEmpty() const
{
	bool bEmpty = true;
	ForEachCaptureType([this, &bEmpty](ERenderCaptureType CaptureType)
	{
		bEmpty = bEmpty && (this->operator[](CaptureType) == nullptr);
	});
	return bEmpty;
}

const TObjectPtr<UTexture2D>& UBakeRenderCaptureResults::operator[](ERenderCaptureType CaptureType) const
{
	switch (CaptureType)
	{
	case ERenderCaptureType::BaseColor:
		return BaseColorMap;
	case ERenderCaptureType::WorldNormal:
		return NormalMap;
	case ERenderCaptureType::Roughness:
		return RoughnessMap;
	case ERenderCaptureType::Metallic:
		return MetallicMap;
	case ERenderCaptureType::Specular:
		return SpecularMap;
	case ERenderCaptureType::Emissive:
		return EmissiveMap;
	case ERenderCaptureType::CombinedMRS:
		return PackedMRSMap;
	case ERenderCaptureType::Opacity:
		return OpacityMap;
	case ERenderCaptureType::SubsurfaceColor:
		return SubsurfaceColorMap;
	case ERenderCaptureType::DeviceDepth:
		ensure(DeviceDepthMap == nullptr); // DeviceDepth is unused and shouldn't change from the default
		return DeviceDepthMap;
	default:
		ensure(false);
	}
	return DeviceDepthMap;
}

bool URenderCaptureProperties::operator==(const URenderCaptureProperties& Other) const
{
	const FSceneCaptureConfig Config = GetSceneCaptureConfig(*this);
	const FSceneCaptureConfig OtherConfig = GetSceneCaptureConfig(Other);
	return Config == OtherConfig;
}

bool URenderCaptureProperties::operator!=(const URenderCaptureProperties& Other) const
{
	return !(*this == Other);
}






//
// Tool Operator
//

class FRenderCaptureMapBakerOp : public TGenericDataOperator<FMeshMapBaker>
{
public:
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> BaseMesh;
	TSharedPtr<UE::Geometry::FDynamicMeshAABBTree3, ESPMode::ThreadSafe> BaseMeshSpatial;
	TSharedPtr<UE::Geometry::FMeshTangentsd, ESPMode::ThreadSafe> BaseMeshTangents;
	TSharedPtr<TArray<int32>, ESPMode::ThreadSafe> BaseMeshUVCharts;
	int32 TargetUVLayer;
	double ValidSampleDepthThreshold;
	EBakeTextureResolution TextureImageSize;
	EBakeTextureSamplesPerPixel SamplesPerPixel;
	TSharedPtr<FSceneCapturePhotoSet, ESPMode::ThreadSafe> SceneCapture;

	// Used to pass the channels which need baking via the bBakeXXX and bUsePackedMRS members
	// PendingBake allows us to skip baking for computed capture types previously baked
	FRenderCaptureTypeFlags PendingBake;

	// Begin TGenericDataOperator interface
	virtual void CalculateResult(FProgressCancel* Progress) override;
	// End TGenericDataOperator interface
};

// Bake textures onto the base/target mesh by projecting/sampling the set of captured photos
void FRenderCaptureMapBakerOp::CalculateResult(FProgressCancel*)
{
	FSceneCapturePhotoSetSampler Sampler(
		SceneCapture.Get(),
		ValidSampleDepthThreshold,
		BaseMesh.Get(),
		BaseMeshSpatial.Get(),
		BaseMeshTangents.Get());

	const FImageDimensions TextureDimensions(
		static_cast<int32>(TextureImageSize),
		static_cast<int32>(TextureImageSize));

	FRenderCaptureOcclusionHandler OcclusionHandler(TextureDimensions);

	Result = MakeRenderCaptureBaker(
		BaseMesh.Get(),
		BaseMeshTangents,
		BaseMeshUVCharts,
		SceneCapture.Get(),
		&Sampler,
		PendingBake,
		TargetUVLayer,
		TextureImageSize,
		SamplesPerPixel,
		&OcclusionHandler);

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

	UE::ToolTarget::HideSourceObject(Targets[0]);

	// Initialize materials and textures for background compute, error feedback and previewing the results
	InitializePreviewMaterials();

	// Initialize the PreviewMesh, which displays intermediate results
	PreviewMesh = CreateBakePreviewMesh(this, Targets[0], GetTargetWorld());

	// Initialize the datastructures used by the bake background compute/tool operator
	PreviewMesh->ProcessMesh([this](const FDynamicMesh3& Mesh)
	{
		TargetMesh = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>();
		TargetMesh->Copy(Mesh);
		const FTransformSRT3d BaseToWorld = UE::ToolTarget::GetLocalToWorldTransform(Targets[0]);
		MeshTransforms::ApplyTransform(*TargetMesh, BaseToWorld, true);

		// Initialize UV charts
		TargetMeshUVCharts = MakeShared<TArray<int32>, ESPMode::ThreadSafe>();
		FMeshMapBaker::ComputeUVCharts(*TargetMesh, *TargetMeshUVCharts);

		// Initialize tangents
		TargetMeshTangents = MakeShared<FMeshTangentsd, ESPMode::ThreadSafe>(TargetMesh.Get());
		TargetMeshTangents->CopyTriVertexTangents(*TargetMesh);

		// Initialize spatial index
		TargetMeshSpatial = MakeShared<FDynamicMeshAABBTree3, ESPMode::ThreadSafe>();
		TargetMeshSpatial->SetMesh(TargetMesh.Get(), true);
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
	MapPreviewWatcherIndex = Settings->WatchProperty(Settings->MapPreview, [this](FString) { UpdateVisualization(); });
	Settings->WatchProperty(Settings->SamplesPerPixel, [this](EBakeTextureSamplesPerPixel) { BakeOpState |= EBakeOpState::Evaluate; });
	Settings->WatchProperty(Settings->TextureSize, [this](EBakeTextureResolution) { BakeOpState |= EBakeOpState::Evaluate; });
	Settings->WatchProperty(Settings->ValidSampleDepthThreshold, [this](float ValidSampleDepthThreshold)
	{
		// The depth capture channel is enabled implicitly when the following parameter is greater than 0.
		// To disable the depth capture we need to set the value to 0. See :EnableDisableDeviceDepthMap 
		RenderCaptureProperties->bDeviceDepthMap = (ValidSampleDepthThreshold > 0);
		BakeOpState |= EBakeOpState::Evaluate;
	});

	// Put these properties before the list of preview textures so its easier to find
	VisualizationProps = NewObject<UBakeRenderCaptureVisualizationProperties>(this);
	VisualizationProps->RestoreProperties(this);
	AddToolPropertySource(VisualizationProps);
	VisualizationProps->WatchProperty(VisualizationProps->bPreviewAsMaterial, [this](bool) { UpdateVisualization(); });

	RenderCaptureProperties = NewObject<URenderCaptureProperties>(this);
	RenderCaptureProperties->RestoreProperties(this);
	AddToolPropertySource(RenderCaptureProperties);

	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->Resolution, [this](EBakeTextureResolution) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bBaseColorMap, [this](bool) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bNormalMap, [this](bool) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bMetallicMap, [this](bool) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bRoughnessMap, [this](bool) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bSpecularMap, [this](bool) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bPackedMRSMap, [this](bool) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bEmissiveMap, [this](bool) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bOpacityMap, [this](bool) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bSubsurfaceColorMap, [this](bool) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->bAntiAliasing, [this](bool) { BakeOpState |= EBakeOpState::Evaluate; });
	// These are not exposed to the UI, but we watch them anyway because we might change that later
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->CaptureFieldOfView, [this](float) { BakeOpState |= EBakeOpState::Evaluate; });
	RenderCaptureProperties->WatchProperty(RenderCaptureProperties->NearPlaneDist, [this](float) { BakeOpState |= EBakeOpState::Evaluate; });
	
	InputMeshSettings = NewObject<UBakeRenderCaptureInputToolProperties>(this);
	InputMeshSettings->RestoreProperties(this);
	AddToolPropertySource(InputMeshSettings);
	InputMeshSettings->TargetStaticMesh = UE::ToolTarget::GetStaticMeshFromTargetIfAvailable(Target);
	UpdateUVLayerNames(InputMeshSettings->TargetUVLayer, InputMeshSettings->TargetUVLayerNamesList, *TargetMesh);
	InputMeshSettings->WatchProperty(InputMeshSettings->TargetUVLayer, [this](FString) { BakeOpState |= EBakeOpState::Evaluate; });
	
	ResultSettings = NewObject<UBakeRenderCaptureResults>(this);
	ResultSettings->RestoreProperties(this);
	AddToolPropertySource(ResultSettings);
	SetToolPropertySourceEnabled(ResultSettings, true);

	TargetUVLayerToError.Reset();

	// Hide the render capture meshes since this baker operates solely in world space which will occlude the preview of
	// the target mesh.
	for (int Idx = 1; Idx < NumTargets; ++Idx)
	{
		UE::ToolTarget::HideSourceObject(Targets[Idx]);
	}

	SceneCapture = MakeShared<FSceneCapturePhotoSet>();

	// Initialize baker background compute
	BakeOp = MakeUnique<TGenericDataBackgroundCompute<FMeshMapBaker>>();
	BakeOp->Setup(this);
	BakeOp->OnResultUpdated.AddLambda([this](const TUniquePtr<FMeshMapBaker>& NewResult) { OnMapsUpdated(NewResult); });

	// Make sure we trigger SceneCapture computation in UpdateResult
	BakeOpState |= EBakeOpState::Evaluate;

	SetToolDisplayName(LOCTEXT("ToolName", "Bake Render Capture"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartTool", "Bake Render Capture. Select Bake Mesh (LowPoly) first, then select Detail Meshes (HiPoly) to bake. Assets will be created on Accept."),
		EToolMessageLevel::UserNotification);

	GatherAnalytics(BakeAnalytics.MeshSettings);
}





void UBakeRenderCaptureTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	// This block contains things we want to update every frame to respond to sliders in the UI
	{
		const float Brightness = VisualizationProps->Brightness;
		const FVector BaseColorBrightness(Brightness, Brightness, Brightness);

		PreviewMaterialRC->SetVectorParameterValue(TEXT("Brightness"), BaseColorBrightness);
		PreviewMaterialPackedRC->SetVectorParameterValue(TEXT("Brightness"), BaseColorBrightness);
		PreviewMaterialRC_Subsurface->SetVectorParameterValue(TEXT("Brightness"), BaseColorBrightness);
		PreviewMaterialPackedRC_Subsurface->SetVectorParameterValue(TEXT("Brightness"), BaseColorBrightness);

		const float EmissiveScale = VisualizationProps->EmissiveScale;

		PreviewMaterialRC->SetScalarParameterValue(TEXT("EmissiveScale"), EmissiveScale);
		PreviewMaterialPackedRC->SetScalarParameterValue(TEXT("EmissiveScale"), EmissiveScale);
		PreviewMaterialRC_Subsurface->SetScalarParameterValue(TEXT("EmissiveScale"), EmissiveScale);
		PreviewMaterialPackedRC_Subsurface->SetScalarParameterValue(TEXT("EmissiveScale"), EmissiveScale);

		const float SSBrightness = VisualizationProps->SSBrightness;
		const FVector SubsurfaceColorBrightness(SSBrightness, SSBrightness, SSBrightness);

		PreviewMaterialRC_Subsurface->SetVectorParameterValue(TEXT("BrightnessSubsurface"), SubsurfaceColorBrightness);
		PreviewMaterialPackedRC_Subsurface->SetVectorParameterValue(TEXT("BrightnessSubsurface"), SubsurfaceColorBrightness);
	}

	// This is a no-op if OpState == EBakeOpState::Clean
	UpdateResult();
}


void UBakeRenderCaptureTool::OnTick(float DeltaTime)
{
	BakeOp->Tick(DeltaTime);

	if (static_cast<bool>(BakeOpState & EBakeOpState::Invalid))
	{
		PreviewMesh->SetOverrideRenderMaterial(ErrorPreviewMaterial);
	}
	else if (!CanAccept() && BakeOp->GetElapsedComputeTime() > SecondsBeforeWorkingMaterial)
	{
		PreviewMesh->SetOverrideRenderMaterial(WorkingPreviewMaterial);
	}
}


void UBakeRenderCaptureTool::OnShutdown(EToolShutdownType ShutdownType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UBakeRenderCaptureTool::Shutdown);

	Settings->SaveProperties(this);
	RenderCaptureProperties->SaveProperties(this);
	InputMeshSettings->SaveProperties(this);
	VisualizationProps->SaveProperties(this);

	PreviewMesh->SetVisible(false);
	PreviewMesh->Disconnect();
	PreviewMesh = nullptr;

	BakeOp->Shutdown();

	if (ShutdownType == EToolShutdownType::Accept && ResultSettings->IsEmpty() == false)
	{
		// TODO Support skeletal meshes here---see BakeMeshAttributeMapsTool::OnShutdown
		const UPrimitiveComponent* SourceComponent = UE::ToolTarget::GetTargetComponent(Targets[0]);
		CreateAssets(SourceComponent->GetWorld());
	}

	// Clear actors on shutdown so that their lifetime is not tied to the lifetime of the tool
	Actors.Empty();

	// Restore visibility of the target mesh
	UE::ToolTarget::ShowSourceObject(Targets[0]);

	// Restore visibility of source meshes
	const int NumTargets = Targets.Num();
	for (int Idx = 1; Idx < NumTargets; ++Idx)
	{
		UE::ToolTarget::ShowSourceObject(Targets[Idx]);
	}
}

void UBakeRenderCaptureTool::CreateAssets(UWorld* SourceWorld)
{
	const FString BaseName = UE::ToolTarget::GetTargetActor(Targets[0])->GetActorNameOrLabel();
	const bool bPackedMRS = ResultSettings->PackedMRSMap != nullptr;
	const bool bSubsurfaceMaterial = ResultSettings->SubsurfaceColorMap || ResultSettings->OpacityMap;

	struct FCreateTextureAssetResult
	{
		TObjectPtr<UTexture2D> Texture;
		bool bIsFallbackTexture;
	};

	auto CreateTextureAsset = [this, &BaseName, &SourceWorld] (
		const FString& TexParamName,
		FTexture2DBuilder::ETextureType Type,
		TObjectPtr<UTexture2D> Texture,
		TObjectPtr<UTexture2D> Fallback,
		bool bForceFallback,
		bool bMaterialHasTexture) -> FCreateTextureAssetResult
	{
		if (!bMaterialHasTexture)
		{
			return {nullptr, false};
		}

		const bool bUseTexture = Texture && !bForceFallback;

		// See :DeferredPopulateSourceData
		if (bUseTexture)
		{
			FTexture2DBuilder::CopyPlatformDataToSourceData(Texture, Type);
		}
		else
		{
			FTexture2DBuilder::CopyPlatformDataToSourceData(Fallback, Type);
		}

		// TODO The original implementation in ApproximateActors also did the following, see WriteTextureLambda in ApproximateActorsImpl.cpp
		//if (Type == FTexture2DBuilder::ETextureType::Roughness
		//	|| Type == FTexture2DBuilder::ETextureType::Metallic
		//	|| Type == FTexture2DBuilder::ETextureType::Specular)
		//{
		//	UE::AssetUtils::ConvertToSingleChannel(Texture);
		//}

		// We need to save the Fallback textures as well so that the generated Materials can reference them
		FCreateTextureObjectParams TexParams;
		TexParams.TargetWorld = SourceWorld;
		TexParams.BaseName = FString::Printf(TEXT("%s_%s"), *BaseName, *TexParamName);
		TexParams.GeneratedTransientTexture = bUseTexture ? Texture : Fallback; 

		FCreateTextureObjectResult TexResult = UE::Modeling::CreateTextureObject(GetToolManager(), MoveTemp(TexParams));
		ensureMsgf(TexResult.IsOK(), TEXT("Failed to create %s texture"), *TexParamName);

		TObjectPtr<UTexture2D> TextureAsset = Cast<UTexture2D>(TexResult.NewAsset);
		ensureMsgf(TextureAsset, TEXT("Unexpected null %s texture"), *TexParamName);

		return {TextureAsset, bUseTexture == false};
	};

	FCreateTextureAssetResult BaseColorTexture = CreateTextureAsset(BaseColorTexParamName,
		FTexture2DBuilder::ETextureType::Color,
		ResultSettings->BaseColorMap,
		EmptyColorMapWhite,
		RenderCaptureProperties->bBaseColorMap == false,
		true);

	FCreateTextureAssetResult NormalTexture = CreateTextureAsset(NormalTexParamName,
		FTexture2DBuilder::ETextureType::NormalMap,
		ResultSettings->NormalMap,
		EmptyNormalMap,
		RenderCaptureProperties->bNormalMap == false,
		true);

	FCreateTextureAssetResult EmissiveTexture = CreateTextureAsset(EmissiveTexParamName,
		FTexture2DBuilder::ETextureType::EmissiveHDR,
		ResultSettings->EmissiveMap,
		EmptyEmissiveMap,
		RenderCaptureProperties->bEmissiveMap == false,
		true);

	FCreateTextureAssetResult PackedMRSTexture = CreateTextureAsset(PackedMRSTexParamName,
		FTexture2DBuilder::ETextureType::ColorLinear,
		ResultSettings->PackedMRSMap,
		EmptyPackedMRSMap,
		RenderCaptureProperties->bPackedMRSMap == false,
		bPackedMRS);

	FCreateTextureAssetResult RoughnessTexture = CreateTextureAsset(RoughnessTexParamName,
		FTexture2DBuilder::ETextureType::Roughness,
		ResultSettings->RoughnessMap,
		EmptyRoughnessMap,
		RenderCaptureProperties->bRoughnessMap == false,
		!bPackedMRS);

	FCreateTextureAssetResult MetallicTexture = CreateTextureAsset(MetallicTexParamName,
		FTexture2DBuilder::ETextureType::Metallic,
		ResultSettings->MetallicMap,
		EmptyMetallicMap,
		RenderCaptureProperties->bMetallicMap == false,
		!bPackedMRS);

	FCreateTextureAssetResult SpecularTexture = CreateTextureAsset(SpecularTexParamName,
		FTexture2DBuilder::ETextureType::Specular,
		ResultSettings->SpecularMap,
		EmptySpecularMap,
		RenderCaptureProperties->bSpecularMap == false,
		!bPackedMRS);

	FCreateTextureAssetResult OpacityTexture = CreateTextureAsset(OpacityTexParamName,
		FTexture2DBuilder::ETextureType::ColorLinear,
		ResultSettings->OpacityMap,
		EmptyOpacityMap,
		RenderCaptureProperties->bOpacityMap == false,
		bSubsurfaceMaterial);

	FCreateTextureAssetResult SubsurfaceColorTexture = CreateTextureAsset(SubsurfaceColorTexParamName,
		FTexture2DBuilder::ETextureType::Color,
		ResultSettings->SubsurfaceColorMap,
		EmptySubsurfaceColorMap,
		RenderCaptureProperties->bSubsurfaceColorMap == false,
		bSubsurfaceMaterial);

	// TODO It would be cleaner/simpler to programmatically create the BakeRC materials. This would fix a few problems:
	// 1) We wouldn't need corresponding *Output* and *Preview* materials to avoid the unwanted reference problem
	//    described below. We wouldn't have unwanted references because we'd only generate material expressions we need.
	// 2) We wouldn't need to write out the empty textures above which reduces clutter.
	// 3) We wouldn't need to guess what constants should be input to unbaked material expression channels to get the
	//    same results as having nothing connected to those channels.
	// 4) If we also generate the *Preview* materials in the tool Setup function we wouldn't tempt users to reference
	//    the MeshModelingTools internal tool materials in their project assets.
	{
		// Choose the Material which we will duplicate to create a Material asset. In order to create materials which
		// can be uploaded to the UEFN client we needed to create *Output* materials which are identical to the
		// *Preview* materials but add static switches in order to eliminate soft references to textures outside the
		// users project folder textures which prevented the material being uploaded to the UEFN client. Also note that
		// we need to duplicate a UMaterial rather than a UMaterialInstanceDynamic since the latter is a temporary
		// runtime material instance.
		TObjectPtr<UMaterial> Material;
		if (bSubsurfaceMaterial)
		{
			Material = LoadObject<UMaterial>(nullptr, ResultSettings->PackedMRSMap
				? TEXT("/MeshModelingToolsetExp/Materials/FullMaterialBakeOutputSubsurfaceMaterial_PackedMRS")
				: TEXT("/MeshModelingToolsetExp/Materials/BakeRenderCaptureOutputSubsurfaceMaterial"));

			ensure(Material->GetShadingModels().HasShadingModel(EMaterialShadingModel::MSM_Subsurface));
			ensure(Material->GetBlendMode() == EBlendMode::BLEND_Masked);
		}
		else
		{
			Material = LoadObject<UMaterial>(nullptr, ResultSettings->PackedMRSMap
				? TEXT("/MeshModelingToolsetExp/Materials/FullMaterialBakeOutputMaterial_PackedMRS")
				: TEXT("/MeshModelingToolsetExp/Materials/BakeRenderCaptureOutputMaterial"));
		}

		if (!ensure(Material))
		{
			return;
		}

		FCreateMaterialObjectParams MaterialParams;
		MaterialParams.TargetWorld = SourceWorld;
		MaterialParams.BaseName = FString::Printf(TEXT("%s_Material"), *BaseName);
		MaterialParams.MaterialToDuplicate = Material;
		const FCreateMaterialObjectResult MaterialResult = UE::Modeling::CreateMaterialObject(GetToolManager(), MoveTemp(MaterialParams));

		UMaterial* NewMaterial = nullptr;
		if (ensure(MaterialResult.ResultCode == ECreateModelingObjectResult::Ok))
		{
			NewMaterial = CastChecked<UMaterial>(MaterialResult.NewAsset);
		}
		
		if (!ensure(NewMaterial))
		{
			return;
		}

		const auto TrySetTextureEditorOnly = [this, NewMaterial](
			const FString& TextureName,
			FCreateTextureAssetResult Texture,
			bool bMaterialHasTexture)
		{
			if (bMaterialHasTexture)
			{
				FGuid SwitchGuid;
				bool bUnusedValue = false;
				const FName SwitchName = FName(TEXT("Enable") + TextureName);
				if (ensure(NewMaterial->GetStaticSwitchParameterValue(SwitchName, bUnusedValue, SwitchGuid)))
				{
					const bool bUseTexture = Texture.bIsFallbackTexture == false;
					ensure(NewMaterial->SetStaticSwitchParameterValueEditorOnly(SwitchName, bUseTexture, SwitchGuid));

					// To eliminate soft references to the textures used by the UMaterialExpressionTextureSampleParameter2D
					// expressions in the material we duplicated (eg /Engine/EngineResources/DefaultTexture.DefaultTexture)
					// we need to call this set texture function even if the switch above means it wont be used :( I tried
					// various (calling ForceRecompileForRendering and using FMaterialUpdateContext) but this was the only
					// way I could get rid of the references. Removing those references is needed to make the generated
					// material usable in UEFN: if the material has soft references to those textures we hit data
					// validation errors when we try to use the material on a mesh uploaded to the client.
					ensure(NewMaterial->SetTextureParameterValueEditorOnly(FName(TextureName), Texture.Texture));
				}
			}
		};

		// Set all computed textures or fallback to the empty texture map
		TrySetTextureEditorOnly(BaseColorTexParamName, BaseColorTexture,  true);
		TrySetTextureEditorOnly(EmissiveTexParamName,  EmissiveTexture,   true);
		TrySetTextureEditorOnly(NormalTexParamName,    NormalTexture,     true);
		TrySetTextureEditorOnly(PackedMRSTexParamName, PackedMRSTexture,  bPackedMRS);
		TrySetTextureEditorOnly(MetallicTexParamName,  MetallicTexture,  !bPackedMRS);
		TrySetTextureEditorOnly(RoughnessTexParamName, RoughnessTexture, !bPackedMRS);
		TrySetTextureEditorOnly(SpecularTexParamName,  SpecularTexture,  !bPackedMRS);
		TrySetTextureEditorOnly(OpacityTexParamName,   OpacityTexture,    bSubsurfaceMaterial);
		TrySetTextureEditorOnly(SubsurfaceColorTexParamName, SubsurfaceColorTexture, bSubsurfaceMaterial);

		// Force material update now that we have updated texture parameters
		NewMaterial->PostEditChange();
	}

	RecordAnalytics();
}



// Return false if the user requested a texture but it is not yet baked, or if the tool is in an invalid state
bool UBakeRenderCaptureTool::CanAccept() const
{
	if ((BakeOpState & EBakeOpState::Invalid) == EBakeOpState::Invalid)
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
	if (RenderCaptureProperties->bOpacityMap && ResultSettings->OpacityMap == nullptr)
	{
		return false;
	}
	if (RenderCaptureProperties->bSubsurfaceColorMap && ResultSettings->SubsurfaceColorMap == nullptr)
	{
		return false;
	}

	// For reasons explained in the comment tagged :HandlingPackedMRS the Metallic/Roughness/Specular checkboxes can
	// be enabled when the correponding photo sets in SceneCapture and textures in ResultSettings are empty. This means
	// we should only check the checkboxes/textures (RenderCaptureProperties/ResultSettings resp.) if we are not using
	// the PackedMRS option In other places we can test the PackedMRS and separate channel booleans in series and avoid
	// the complexity of nested if statements.
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
	// We should not have requested a bake if we don't have a SceneCapture
	check(SceneCapture.IsValid());

	TUniquePtr<FRenderCaptureMapBakerOp> Op = MakeUnique<FRenderCaptureMapBakerOp>();
	Op->BaseMesh = TargetMesh;
	Op->BaseMeshSpatial = TargetMeshSpatial;
	Op->BaseMeshTangents = TargetMeshTangents;
	Op->BaseMeshUVCharts = TargetMeshUVCharts;
	Op->TargetUVLayer = InputMeshSettings->GetTargetUVLayerIndex();
	Op->ValidSampleDepthThreshold = Settings->ValidSampleDepthThreshold;
	Op->TextureImageSize = Settings->TextureSize;
	Op->SamplesPerPixel = Settings->SamplesPerPixel;
	Op->SceneCapture = SceneCapture;

	ForEachCaptureType([this, &Op](ERenderCaptureType CaptureType)
	{
		using EStatus = FSceneCapturePhotoSet::ECaptureTypeStatus;

		const bool bComputed = SceneCapture->GetCaptureTypeStatus(CaptureType) == EStatus::Computed;
		const bool bBaked = (*ResultSettings)[CaptureType] != nullptr;
		const bool bPendingBake = bComputed && !bBaked;

		// The BakeRC tool does not bake the DeviceDepth, this channel is queried via the SceneCapture when baking the other channels
		Op->PendingBake[CaptureType] = (CaptureType == ERenderCaptureType::DeviceDepth ? false : bPendingBake);
	});

	return Op;
}



void UBakeRenderCaptureTool::OnMapsUpdated(const TUniquePtr<FMeshMapBaker>& NewResult)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(BakeRenderCaptureTool_Textures_BuildTextures);

	FRenderCaptureTextures TexturesOut;
	GetTexturesFromRenderCaptureBaker(*NewResult, TexturesOut);

	// The NewResult will contain the newly baked textures so we only update those and not overwrite any already baked
	// valid TexturesOut. If a texture is invalidated by some tool property change it will null on entry to this function
	if (TexturesOut.BaseColorMap)
	{
		ResultSettings->BaseColorMap = TexturesOut.BaseColorMap;
	}
	if (TexturesOut.NormalMap)
	{
		ResultSettings->NormalMap = TexturesOut.NormalMap;
	}
	if (TexturesOut.PackedMRSMap)
	{
		ResultSettings->PackedMRSMap = TexturesOut.PackedMRSMap;
	}
	if (TexturesOut.MetallicMap)
	{
		ResultSettings->MetallicMap = TexturesOut.MetallicMap;
	}
	if (TexturesOut.RoughnessMap)
	{
		ResultSettings->RoughnessMap = TexturesOut.RoughnessMap;
	}
	if (TexturesOut.SpecularMap)
	{
		ResultSettings->SpecularMap = TexturesOut.SpecularMap;
	}
	if (TexturesOut.EmissiveMap)
	{
		ResultSettings->EmissiveMap = TexturesOut.EmissiveMap;
	}
	if (TexturesOut.OpacityMap)
	{
		ResultSettings->OpacityMap = TexturesOut.OpacityMap;
	}
	if (TexturesOut.SubsurfaceColorMap)
	{
		ResultSettings->SubsurfaceColorMap = TexturesOut.SubsurfaceColorMap;
	}

	GatherAnalytics(*NewResult);
	UpdateVisualization();
}

bool UBakeRenderCaptureTool::ValidTargetMeshTangents()
{
	if (bCheckTargetMeshTangents)
	{
		bValidTargetMeshTangents = TargetMeshTangents ? FDynamicMeshTangents(TargetMesh.Get()).HasValidTangents(true) : false;
		bCheckTargetMeshTangents = false;
	}
	return bValidTargetMeshTangents;
}

void UBakeRenderCaptureTool::InitializePreviewMaterials()
{
	// We will only need source data if we're saving these textures so that the material generated when the tool is
	// Accepted can reference them :DeferredPopulateSourceData
	constexpr bool bPopulateSourceData = false;

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::NormalMap, FImageDimensions(16, 16));
		Builder.Commit(bPopulateSourceData);
		EmptyNormalMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::Color, FImageDimensions(16, 16));
		Builder.Clear(FColor(0,0,0));
		Builder.Commit(bPopulateSourceData);
		EmptyColorMapBlack = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::Color, FImageDimensions(16, 16));
		Builder.Clear(FColor::White);
		Builder.Commit(bPopulateSourceData);
		EmptyColorMapWhite = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::EmissiveHDR, FImageDimensions(16, 16));
		Builder.Commit(bPopulateSourceData);
		EmptyEmissiveMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::ColorLinear, FImageDimensions(16, 16));
		// The Opacity texture is passed to the Material's Opacity pin as well as the Opacity Mask pin, so set white
		// here so we see something when previewing the subsurface material when opacity is not baked
		Builder.Clear(FColor::White);
		Builder.Commit(bPopulateSourceData);
		EmptyOpacityMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::Color, FImageDimensions(16, 16));
		Builder.Clear(FColor::Black);
		Builder.Commit(bPopulateSourceData);
		EmptySubsurfaceColorMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::ColorLinear, FImageDimensions(16, 16));
		Builder.Clear(FColor(0,0,0));
		Builder.Commit(bPopulateSourceData);
		EmptyPackedMRSMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::Roughness, FImageDimensions(16, 16));
		Builder.Commit(bPopulateSourceData);
		EmptyRoughnessMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::Metallic, FImageDimensions(16, 16));
		Builder.Commit(bPopulateSourceData);
		EmptyMetallicMap = Builder.GetTexture2D();
	}

	{
		FTexture2DBuilder Builder;
		Builder.Initialize(FTexture2DBuilder::ETextureType::Specular, FImageDimensions(16, 16));
		Builder.Commit(bPopulateSourceData);
		EmptySpecularMap = Builder.GetTexture2D();
	}

	WorkingPreviewMaterial = ToolSetupUtil::GetDefaultWorkingMaterialInstance(GetToolManager());

	ErrorPreviewMaterial = ToolSetupUtil::GetDefaultErrorMaterial(GetToolManager());

	{
		UMaterial* Material = LoadObject<UMaterial>(nullptr,
			TEXT("/MeshModelingToolsetExp/Materials/BakeRenderCapturePreviewMaterial"));
		check(Material);
		if (Material != nullptr)
		{
			PreviewMaterialRC = UMaterialInstanceDynamic::Create(Material, GetToolManager());
			PreviewMaterialRC->SetTextureParameterValue(FName(*BaseColorTexParamName), EmptyColorMapWhite);
			PreviewMaterialRC->SetTextureParameterValue(FName(*RoughnessTexParamName), EmptyRoughnessMap);
			PreviewMaterialRC->SetTextureParameterValue(FName(*MetallicTexParamName), EmptyMetallicMap);
			PreviewMaterialRC->SetTextureParameterValue(FName(*SpecularTexParamName), EmptySpecularMap);
			PreviewMaterialRC->SetTextureParameterValue(FName(*EmissiveTexParamName), EmptyEmissiveMap);
			PreviewMaterialRC->SetTextureParameterValue(FName(*NormalTexParamName), EmptyNormalMap);
		}
	}

	{
		UMaterial* Material = LoadObject<UMaterial>(nullptr,
			TEXT("/MeshModelingToolsetExp/Materials/BakeRenderCapturePreviewSubsurfaceMaterial"));
		check(Material);
		if (Material != nullptr)
		{
			ensure(Material->GetShadingModels().HasShadingModel(EMaterialShadingModel::MSM_Subsurface));
			PreviewMaterialRC_Subsurface = UMaterialInstanceDynamic::Create(Material, GetToolManager());
			PreviewMaterialRC_Subsurface->SetTextureParameterValue(FName(*BaseColorTexParamName), EmptyColorMapWhite);
			PreviewMaterialRC_Subsurface->SetTextureParameterValue(FName(*RoughnessTexParamName), EmptyRoughnessMap);
			PreviewMaterialRC_Subsurface->SetTextureParameterValue(FName(*MetallicTexParamName), EmptyMetallicMap);
			PreviewMaterialRC_Subsurface->SetTextureParameterValue(FName(*SpecularTexParamName), EmptySpecularMap);
			PreviewMaterialRC_Subsurface->SetTextureParameterValue(FName(*EmissiveTexParamName), EmptyEmissiveMap);
			PreviewMaterialRC_Subsurface->SetTextureParameterValue(FName(*NormalTexParamName), EmptyNormalMap);
			PreviewMaterialRC_Subsurface->SetTextureParameterValue(FName(*OpacityTexParamName), EmptyOpacityMap);
			PreviewMaterialRC_Subsurface->SetTextureParameterValue(FName(*SubsurfaceColorTexParamName), EmptySubsurfaceColorMap);
		}
	}

	{
		UMaterial* Material = LoadObject<UMaterial>(nullptr,
			TEXT("/MeshModelingToolsetExp/Materials/FullMaterialBakePreviewMaterial_PackedMRS"));
		check(Material);
		if (Material != nullptr)
		{
			PreviewMaterialPackedRC = UMaterialInstanceDynamic::Create(Material, GetToolManager());
			PreviewMaterialPackedRC->SetTextureParameterValue(FName(*BaseColorTexParamName), EmptyColorMapWhite);
			PreviewMaterialPackedRC->SetTextureParameterValue(FName(*PackedMRSTexParamName), EmptyPackedMRSMap);
			PreviewMaterialPackedRC->SetTextureParameterValue(FName(*EmissiveTexParamName), EmptyEmissiveMap);
			PreviewMaterialPackedRC->SetTextureParameterValue(FName(*NormalTexParamName), EmptyNormalMap);
		}
	}

	{
		UMaterial* Material = LoadObject<UMaterial>(nullptr,
			TEXT("/MeshModelingToolsetExp/Materials/FullMaterialBakePreviewSubsurfaceMaterial_PackedMRS"));
		check(Material);
		if (Material != nullptr)
		{
			ensure(Material->GetShadingModels().HasShadingModel(EMaterialShadingModel::MSM_Subsurface));
			PreviewMaterialPackedRC_Subsurface = UMaterialInstanceDynamic::Create(Material, GetToolManager());
			PreviewMaterialPackedRC_Subsurface->SetTextureParameterValue(FName(*BaseColorTexParamName), EmptyColorMapWhite);
			PreviewMaterialPackedRC_Subsurface->SetTextureParameterValue(FName(*PackedMRSTexParamName), EmptyPackedMRSMap);
			PreviewMaterialPackedRC_Subsurface->SetTextureParameterValue(FName(*EmissiveTexParamName), EmptyEmissiveMap);
			PreviewMaterialPackedRC_Subsurface->SetTextureParameterValue(FName(*NormalTexParamName), EmptyNormalMap);
			PreviewMaterialPackedRC_Subsurface->SetTextureParameterValue(FName(*OpacityTexParamName), EmptyOpacityMap);
			PreviewMaterialPackedRC_Subsurface->SetTextureParameterValue(FName(*SubsurfaceColorTexParamName), EmptySubsurfaceColorMap);
		}
	}
}


// Process dirty props and update background compute. Called by UBakeRenderCaptureTool::Render
void UBakeRenderCaptureTool::UpdateResult()
{
	// Return if the bake is already launched/complete.
	if (BakeOpState == EBakeOpState::Clean)
	{
		return;
	}

	{
		Settings->MapPreviewNamesList.Reset();

		if (RenderCaptureProperties->bBaseColorMap)
		{
			Settings->MapPreviewNamesList.Add(BaseColorTexParamName);
		}
		if (RenderCaptureProperties->bNormalMap)
		{
			Settings->MapPreviewNamesList.Add(NormalTexParamName);
		}
		if (RenderCaptureProperties->bPackedMRSMap)
		{
			Settings->MapPreviewNamesList.Add(PackedMRSTexParamName);
		}
		if (RenderCaptureProperties->bMetallicMap)
		{
			Settings->MapPreviewNamesList.Add(MetallicTexParamName);
		}
		if (RenderCaptureProperties->bRoughnessMap)
		{
			Settings->MapPreviewNamesList.Add(RoughnessTexParamName);
		}
		if (RenderCaptureProperties->bSpecularMap)
		{
			Settings->MapPreviewNamesList.Add(SpecularTexParamName);
		}
		if (RenderCaptureProperties->bEmissiveMap)
		{
			Settings->MapPreviewNamesList.Add(EmissiveTexParamName);
		}
		if (RenderCaptureProperties->bOpacityMap)
		{
			Settings->MapPreviewNamesList.Add(OpacityTexParamName);
		}
		if (RenderCaptureProperties->bSubsurfaceColorMap)
		{
			Settings->MapPreviewNamesList.Add(SubsurfaceColorTexParamName);
		}

		if (Settings->MapPreviewNamesList.IsEmpty())
		{
			// Display an empty string when MapPreview is disabled
			Settings->MapPreview = TEXT("");
			Settings->SilentUpdateWatcherAtIndex(MapPreviewWatcherIndex);

			Settings->bEnableMapPreview = false;
		}
		else
		{
			// If the current MapPreview channel is disabled, switch to the first enabled channel in the list
			if (Settings->MapPreviewNamesList.Find(Settings->MapPreview) == INDEX_NONE)
			{
				Settings->MapPreview = Settings->MapPreviewNamesList[0];
				Settings->SilentUpdateWatcherAtIndex(MapPreviewWatcherIndex);
			}

			Settings->bEnableMapPreview = true;
		}
		NotifyOfPropertyChangeByTool(Settings);
	}

	// The bake operation stores a pointer to the SceneCapture so that must not be modified while baking
	const bool bBakeOpInProgress = BakeOp->GetElapsedComputeTime() > 0.f;
	if (bBakeOpInProgress)
	{
		return;
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
				FDynamicMeshUVOverlay* UVOverlay = TargetMesh->Attributes()->GetUVLayer(InputMeshSettings->GetTargetUVLayerIndex());
				FAxisAlignedBox2f Bounds = FAxisAlignedBox2f::Empty();
				for (const int Index : UVOverlay->ElementIndicesItr())
				{
					FVector2f UV;
					UVOverlay->GetElement(Index, UV);
					Bounds.Contain(UV);
				}
				return Bounds.Min == Bounds.Max;
			};

			if (TargetMesh->Attributes()->GetUVLayer(InputMeshSettings->GetTargetUVLayerIndex()) == nullptr)
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

	const bool bIsInvalid = (ErrorMessage.IsEmpty() == false);
	if (bIsInvalid)
	{
		// Clear all results and wait for user to fix the invalid tool inputs
		InvalidateResults(FRenderCaptureTypeFlags::All(true));

		// Only call UpdateVisualization when we first detect the invalid inputs
		const bool bWasValid = static_cast<bool>(BakeOpState & EBakeOpState::Invalid) == false;
		if (bWasValid)
		{
			UpdateVisualization();
		}

		// Set an invalid op state so we re-enter this function until the inputs are valid
		BakeOpState = EBakeOpState::Invalid;
	}
	else
	{
		const FSceneCaptureConfig DesiredConfig = GetSceneCaptureConfig(*RenderCaptureProperties);

		// Update the scene capture and get the capture types that were updated so we can bake them
		FRenderCaptureTypeFlags UpdatedChannels;
		{
			// Show the source meshes. If we don't do this the renderer doesn't see anything and we get a blank capture
			// TODO FSceneCapturePhotoSet should probably ensure the visibility its Actors so that we don't need to do
			// this visibility thing here in the tool
			for (int Idx = 1; Idx < Targets.Num(); ++Idx)
			{
				UE::ToolTarget::ShowSourceObject(Targets[Idx]);
			}

			UpdatedChannels = UpdateSceneCapture(*SceneCapture, Actors, DesiredConfig, false);

			// Hide the source meshes after the render capture so they don't occlude the preview
			for (int Idx = 1; Idx < Targets.Num(); ++Idx)
			{
				UE::ToolTarget::HideSourceObject(Targets[Idx]);
			}

			const FSceneCaptureConfig AchievedConfig = GetSceneCaptureConfig(*SceneCapture);
			ensure(SceneCapture->Cancelled() == (AchievedConfig != DesiredConfig));

			// If the scene capture was cancelled make sure the tool properties are consistent with the computed captures
			if (SceneCapture->Cancelled())
			{
				SetSceneCaptureConfig(*RenderCaptureProperties, AchievedConfig);
				RenderCaptureProperties->SilentUpdateWatched();

				if (AchievedConfig.Flags.bDeviceDepth == false)
				{
					// See :EnableDisableDeviceDepthMap
					Settings->ValidSampleDepthThreshold = 0.f;
					Settings->SilentUpdateWatched();
				}
			}
		}

		// Update/Bake the result textures
		{
			const bool bInvalidateAll =
				ComputedTargetUVLayer != InputMeshSettings->TargetUVLayer ||
				ComputedTextureSize != Settings->TextureSize ||
				ComputedSamplesPerPixel != Settings->SamplesPerPixel ||
				ComputedValidDepthThreshold != Settings->ValidSampleDepthThreshold;

			// Invalidate any results corresponding to SceneCapture Channels that got updated, or
			// Invalidate all results if the any baking parameters were changed
			InvalidateResults(bInvalidateAll ? FRenderCaptureTypeFlags::All(true) : UpdatedChannels);

			// Update the preview mesh material with the (possibly invalidated) bake results
			UpdateVisualization();

			// Start another bake operation, this will bake the computed captures with invalidated/null texture results
			BakeOp->InvalidateResult();
			BakeOpState = EBakeOpState::Clean;

			// Cache computed parameters which are used to determine if results need re-baking
			ComputedTargetUVLayer = InputMeshSettings->TargetUVLayer;
			ComputedTextureSize = Settings->TextureSize;
			ComputedSamplesPerPixel = Settings->SamplesPerPixel;
			ComputedValidDepthThreshold = Settings->ValidSampleDepthThreshold;
		}
	}
}



void UBakeRenderCaptureTool::UpdateVisualization()
{
	const bool bSubsurfaceMaterial = ResultSettings->SubsurfaceColorMap || ResultSettings->OpacityMap;
	const bool bPackedMRS = ResultSettings->PackedMRSMap != nullptr;

	// Choose the material
	TObjectPtr<UMaterialInstanceDynamic> Material = bPackedMRS ? PreviewMaterialPackedRC : PreviewMaterialRC;
	if (bSubsurfaceMaterial)
	{
		Material = bPackedMRS ? PreviewMaterialPackedRC_Subsurface : PreviewMaterialRC_Subsurface;
		ensure(Material->GetShadingModels().HasShadingModel(EMaterialShadingModel::MSM_Subsurface));
		ensure(Material->GetBlendMode() == EBlendMode::BLEND_Masked);
	}
	
	const auto TrySetTexture =
		[Material, this](const FString& TextureName, TObjectPtr<UTexture2D> Texture, TObjectPtr<UTexture2D> Fallback, bool bMaterialHasTexture)
	{
		if (bMaterialHasTexture)
		{
			if (VisualizationProps->bPreviewAsMaterial)
			{
				Material->SetTextureParameterValue(FName(TextureName), Texture ? Texture : Fallback);
			}
			else
			{
				if (TextureName == Settings->MapPreview)
				{
					Material->SetTextureParameterValue(FName(BaseColorTexParamName), Texture ? Texture : Fallback);
				}
				else
				{
					Material->SetTextureParameterValue(FName(TextureName), Fallback);
				}
			}
		}
	};

	TrySetTexture(BaseColorTexParamName, ResultSettings->BaseColorMap, EmptyColorMapWhite, true);
	TrySetTexture(EmissiveTexParamName,  ResultSettings->EmissiveMap,  EmptyEmissiveMap,   true);
	TrySetTexture(NormalTexParamName,    ResultSettings->NormalMap,    EmptyNormalMap,     true);
	TrySetTexture(PackedMRSTexParamName, ResultSettings->PackedMRSMap, EmptyPackedMRSMap,  bPackedMRS);
	TrySetTexture(RoughnessTexParamName, ResultSettings->RoughnessMap, EmptyRoughnessMap, !bPackedMRS);
	TrySetTexture(MetallicTexParamName,  ResultSettings->MetallicMap,  EmptyMetallicMap,  !bPackedMRS);
	TrySetTexture(SpecularTexParamName,  ResultSettings->SpecularMap,  EmptySpecularMap,  !bPackedMRS);
	TrySetTexture(OpacityTexParamName,   ResultSettings->OpacityMap,   EmptyOpacityMap,    bSubsurfaceMaterial);
	TrySetTexture(SubsurfaceColorTexParamName, ResultSettings->SubsurfaceColorMap, EmptySubsurfaceColorMap, bSubsurfaceMaterial);

	Material->SetScalarParameterValue(TEXT("UVChannel"), InputMeshSettings->GetTargetUVLayerIndex());
	PreviewMesh->SetOverrideRenderMaterial(Material);

	GetToolManager()->PostInvalidation();
}


void UBakeRenderCaptureTool::InvalidateResults(FRenderCaptureTypeFlags Invalidate)
{
	// Note that the bake operation updates ResultSettings when results are available via the OnResultUpdated delegate

	if (Invalidate.bBaseColor)
	{
		ResultSettings->BaseColorMap = nullptr;
	}
	if (Invalidate.bRoughness)
	{
		ResultSettings->RoughnessMap = nullptr;
	}
	if (Invalidate.bMetallic)
	{
		ResultSettings->MetallicMap = nullptr;
	}
	if (Invalidate.bSpecular)
	{
		ResultSettings->SpecularMap = nullptr;
	}
	if (Invalidate.bEmissive)
	{
		ResultSettings->EmissiveMap = nullptr;
	}
	if (Invalidate.bWorldNormal)
	{
		ResultSettings->NormalMap = nullptr;
	}
	if (Invalidate.bOpacity)
	{
		ResultSettings->OpacityMap = nullptr;
	}
	if (Invalidate.bSubsurfaceColor)
	{
		ResultSettings->SubsurfaceColorMap = nullptr;
	}
	if (Invalidate.bCombinedMRS)
	{
		ResultSettings->PackedMRSMap = nullptr;
	}
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
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.OpacityMap.Enabled"), RenderCaptureProperties->bOpacityMap));
	Attributes.Add(FAnalyticsEventAttribute(TEXT("Settings.RenderCapture.SubsurfaceColorMap.Enabled"), RenderCaptureProperties->bSubsurfaceColorMap));
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

	Data.NumTargetMeshTris = TargetMesh->TriangleCount();
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

