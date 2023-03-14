// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerGeomCacheEditorModel.h"
#include "MLDeformerGeomCacheActor.h"
#include "MLDeformerEditorToolkit.h"
#include "MLDeformerEditorStyle.h"
#include "MLDeformerGeomCacheModel.h"
#include "MLDeformerGeomCacheVizSettings.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerGeomCacheSampler.h"
#include "MLDeformerGeomCacheHelpers.h"
#include "MLDeformerGeomCacheModel.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/MorphTarget.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "MLDeformerGeomCacheEditorModel"

namespace UE::MLDeformer
{
	FMLDeformerSampler* FMLDeformerGeomCacheEditorModel::CreateSampler() const
	{
		FMLDeformerGeomCacheSampler* NewSampler = new FMLDeformerGeomCacheSampler();
		NewSampler->OnGetGeometryCache().BindLambda([this] { return GetGeomCacheModel()->GetGeometryCache(); });
		return NewSampler;
	}

	FMLDeformerEditorActor* FMLDeformerGeomCacheEditorModel::CreateEditorActor(const FMLDeformerEditorActor::FConstructSettings& Settings) const
	{
		return new FMLDeformerGeomCacheActor(Settings);
	}

	UMLDeformerGeomCacheVizSettings* FMLDeformerGeomCacheEditorModel::GetGeomCacheVizSettings() const 
	{ 
		return Cast<UMLDeformerGeomCacheVizSettings>(Model->GetVizSettings());
	}

	FMLDeformerGeomCacheSampler* FMLDeformerGeomCacheEditorModel::GetGeomCacheSampler() const
	{ 
		return static_cast<FMLDeformerGeomCacheSampler*>(Sampler);
	}

	UMLDeformerGeomCacheModel* FMLDeformerGeomCacheEditorModel::GetGeomCacheModel() const
	{ 
		return Cast<UMLDeformerGeomCacheModel>(Model);
	}

	FMLDeformerGeomCacheActor* FMLDeformerGeomCacheEditorModel::FindGeomCacheEditorActor(int32 ID) const
	{
		return static_cast<FMLDeformerGeomCacheActor*>(FindEditorActor(ID));
	}

	double FMLDeformerGeomCacheEditorModel::GetTrainingTimeAtFrame(int32 FrameNumber) const
	{
		// Try to get the frame from the geometry cache.
		const FMLDeformerGeomCacheActor* EditorActor = static_cast<FMLDeformerGeomCacheActor*>(FindEditorActor(ActorID_Train_GroundTruth));
		if (EditorActor && EditorActor->GetGeometryCacheComponent() && EditorActor->GetGeometryCacheComponent()->GeometryCache.Get())
		{
			return EditorActor->GetGeometryCacheComponent()->GetTimeAtFrame(FrameNumber);
		}

		return FMLDeformerEditorModel::GetTrainingTimeAtFrame(FrameNumber);
	}

	int32 FMLDeformerGeomCacheEditorModel::GetTrainingFrameAtTime(double TimeInSeconds) const
	{
		const FMLDeformerGeomCacheActor* EditorActor = static_cast<FMLDeformerGeomCacheActor*>(FindEditorActor(ActorID_Train_GroundTruth));
		if (EditorActor && EditorActor->GetGeometryCacheComponent() && EditorActor->GetGeometryCacheComponent()->GeometryCache.Get())
		{
			return EditorActor->GetGeometryCacheComponent()->GetFrameAtTime(TimeInSeconds);
		}

		return FMLDeformerEditorModel::GetTrainingFrameAtTime(TimeInSeconds);
	}

	double FMLDeformerGeomCacheEditorModel::GetTestTimeAtFrame(int32 FrameNumber) const
	{
		// Try to get the frame from the geometry cache.
		const FMLDeformerGeomCacheActor* EditorActor = static_cast<FMLDeformerGeomCacheActor*>(FindEditorActor(ActorID_Test_GroundTruth));
		if (EditorActor && EditorActor->GetGeometryCacheComponent() && EditorActor->GetGeometryCacheComponent()->GeometryCache.Get())
		{
			return EditorActor->GetGeometryCacheComponent()->GetTimeAtFrame(FrameNumber);
		}

		return FMLDeformerEditorModel::GetTestTimeAtFrame(FrameNumber);
	}

	int32 FMLDeformerGeomCacheEditorModel::GetTestFrameAtTime(double TimeInSeconds) const
	{
		const FMLDeformerGeomCacheActor* EditorActor = static_cast<FMLDeformerGeomCacheActor*>(FindEditorActor(ActorID_Test_GroundTruth));
		if (EditorActor && EditorActor->GetGeometryCacheComponent() && EditorActor->GetGeometryCacheComponent()->GeometryCache.Get())
		{
			return EditorActor->GetGeometryCacheComponent()->GetFrameAtTime(TimeInSeconds);
		}

		return FMLDeformerEditorModel::GetTestFrameAtTime(TimeInSeconds);
	}

	int32 FMLDeformerGeomCacheEditorModel::GetNumTrainingFrames() const
	{
		const UGeometryCache* GeometryCache = GetGeomCacheModel()->GetGeometryCache();
		if (GeometryCache == nullptr)
		{
			return 0;
		}
		const int32 StartFrame = GeometryCache->GetStartFrame();
		const int32 EndFrame = GeometryCache->GetEndFrame();
		return (EndFrame - StartFrame) + 1;
	}

	void FMLDeformerGeomCacheEditorModel::UpdateIsReadyForTrainingState()
	{
		bIsReadyForTraining = false;

		// Do some basic checks first, like if there is a skeletal mesh, ground truth, anim sequence, and if there are frames.
		if (!IsEditorReadyForTrainingBasicChecks())
		{
			return;
		}

		// Now make sure the assets are compatible.
		UMLDeformerGeomCacheModel* GeomCacheModel = GetGeomCacheModel();
		UGeometryCache* GeomCache = GeomCacheModel->GetGeometryCache();
		USkeletalMesh* SkeletalMesh = GeomCacheModel->GetSkeletalMesh();
		if (!GetGeomCacheErrorText(SkeletalMesh, GeomCache).IsEmpty())
		{
			return;
		}

		// Make sure every skeletal imported mesh has some geometry track.
		const int32 NumGeomCacheTracks = GeomCache ? GeomCache->Tracks.Num() : 0;
		int32 NumSkelMeshes = 0;
		check(SkeletalMesh);
		FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
		if (ImportedModel)
		{
			NumSkelMeshes = ImportedModel->LODModels[0].ImportedMeshInfos.Num();
		}

		// Check if we have any mappings at all.
		FMLDeformerGeomCacheSampler* GeomCacheSampler = GetGeomCacheSampler();
		GeomCacheSampler->Init(this);
		if (GeomCacheSampler->GetMeshMappings().IsEmpty())
		{
			return;
		}

		// Allow the special case where there is just one mesh and track.
		if (NumGeomCacheTracks != 1 || NumSkelMeshes != 1)
		{
			if (!GeomCacheSampler->GetFailedImportedMeshNames().IsEmpty())
			{
				return;
			}
		}

		bIsReadyForTraining = true;
	}

	void FMLDeformerGeomCacheEditorModel::CreateTrainingGroundTruthActor(UWorld* World)
	{
		UGeometryCache* GeomCache = GetGeomCacheModel()->GetGeometryCache();
		const FLinearColor LabelColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.TargetMesh.LabelColor");
		const FLinearColor WireframeColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.TargetMesh.WireframeColor");
		CreateGeomCacheActor(
			World,
			ActorID_Train_GroundTruth,
			"Train GroundTruth",
			GeomCache,
			LabelColor,
			WireframeColor,
			LOCTEXT("TrainGroundTruthActorLabelText", "Target Mesh"),
			true);
	}

	void FMLDeformerGeomCacheEditorModel::CreateTestGroundTruthActor(UWorld* World)
	{
		UGeometryCache* GeomCache = GetGeomCacheVizSettings()->GetTestGroundTruth();
		const FLinearColor LabelColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.GroundTruth.LabelColor");
		const FLinearColor WireframeColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.GroundTruth.WireframeColor");
		CreateGeomCacheActor(
			World,
			ActorID_Test_GroundTruth,
			"Test GroundTruth",
			GeomCache,
			LabelColor,
			WireframeColor,
			LOCTEXT("TestGroundTruthActorLabelText", "Ground Truth"),
			false);
	}

	void FMLDeformerGeomCacheEditorModel::CreateGeomCacheActor(UWorld* World, int32 ActorID, const FName& Name, UGeometryCache* GeomCache, FLinearColor LabelColor, FLinearColor WireframeColor, const FText& LabelText, bool bIsTrainingActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), Name);
		AActor* Actor = World->SpawnActor<AActor>(SpawnParams);
		Actor->SetFlags(RF_Transient);

		// Create the Geometry Cache Component.
		UGeometryCacheComponent* GeomCacheComponent = NewObject<UGeometryCacheComponent>(Actor);
		GeomCacheComponent->SetGeometryCache(GeomCache);
		GeomCacheComponent->RegisterComponent();
		GeomCacheComponent->SetOverrideWireframeColor(true);
		GeomCacheComponent->SetWireframeOverrideColor(WireframeColor);
		GeomCacheComponent->MarkRenderStateDirty();
		GeomCacheComponent->SetVisibility(false);
		Actor->SetRootComponent(GeomCacheComponent);

		// Create the editor actor.
		FMLDeformerEditorActor::FConstructSettings Settings;
		Settings.Actor = Actor;
		Settings.TypeID = ActorID;
		Settings.LabelColor = LabelColor;
		Settings.LabelText = LabelText;
		Settings.bIsTrainingActor = bIsTrainingActor;
		FMLDeformerGeomCacheActor* EditorActor = static_cast<FMLDeformerGeomCacheActor*>(CreateEditorActor(Settings));
		EditorActor->SetGeometryCacheComponent(GeomCacheComponent);
		EditorActors.Add(EditorActor);
	}

	ETrainingResult FMLDeformerGeomCacheEditorModel::Train()
	{
		UE_LOG(LogMLDeformer, Error, TEXT("Please implement your Train method."));
		return ETrainingResult::Success;
	}

	void FMLDeformerGeomCacheEditorModel::OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent)
	{
		const FProperty* Property = PropertyChangedEvent.Property;
		if (Property == nullptr)
		{
			return;
		}

		// Handle base class property changes.
		FMLDeformerEditorModel::OnPropertyChanged(PropertyChangedEvent);

		// Properties specific to the geometry cache editor model.
		if (Property->GetFName() == UMLDeformerGeomCacheModel::GetGeometryCachePropertyName())
		{
			SetResamplingInputOutputsNeeded(true);
			TriggerInputAssetChanged(true);
		}
		else if (Property->GetFName() == UMLDeformerGeomCacheVizSettings::GetTestGroundTruthPropertyName())
		{
			TriggerInputAssetChanged(true);
		}
	}

	void FMLDeformerGeomCacheEditorModel::OnInputAssetsChanged()
	{
		// Update the skeletal mesh components of the training, test base, and ml deformed actor.
		FMLDeformerEditorModel::OnInputAssetsChanged();

		UMLDeformerGeomCacheVizSettings* VizSettings = GetGeomCacheVizSettings();
		check(VizSettings);
		const float TestAnimSpeed = VizSettings->GetAnimPlaySpeed();
		UAnimSequence* TestAnimSequence = VizSettings->GetTestAnimSequence();

		// Update the training geometry cache.
		UGeometryCacheComponent* GeometryCacheComponent = FindGeomCacheEditorActor(ActorID_Train_GroundTruth)->GetGeometryCacheComponent();
		check(GeometryCacheComponent);
		GeometryCacheComponent->SetGeometryCache(GetGeomCacheModel()->GetGeometryCache());
		GeometryCacheComponent->SetLooping(false);
		GeometryCacheComponent->SetManualTick(true);
		GeometryCacheComponent->SetPlaybackSpeed(TestAnimSpeed);
		GeometryCacheComponent->Play();

		// Update the test geometry cache (ground truth) component.
		GeometryCacheComponent = FindGeomCacheEditorActor(ActorID_Test_GroundTruth)->GetGeometryCacheComponent();
		check(GeometryCacheComponent);
		GeometryCacheComponent->SetGeometryCache(VizSettings->GetTestGroundTruth());
		GeometryCacheComponent->SetLooping(true);
		GeometryCacheComponent->SetManualTick(true);
		GeometryCacheComponent->SetPlaybackSpeed(TestAnimSpeed);
		GeometryCacheComponent->Play();

		GetGeomCacheModel()->GetGeomCacheMeshMappings().Reset();
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE
