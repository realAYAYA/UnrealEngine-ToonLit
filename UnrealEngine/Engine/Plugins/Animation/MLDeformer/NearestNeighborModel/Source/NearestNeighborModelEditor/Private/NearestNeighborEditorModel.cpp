// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborEditorModel.h"
#include "NearestNeighborModel.h"
#include "NearestNeighborModelInstance.h"
#include "NearestNeighborTrainingModel.h"
#include "NearestNeighborModelInputInfo.h"
#include "NearestNeighborEditorModelActor.h"
#include "NearestNeighborModelStyle.h"
#include "NearestNeighborGeomCacheSampler.h"
#include "MLDeformerComponent.h"
#include "MLDeformerEditorToolkit.h"
#include "NeuralNetwork.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/MorphTarget.h"
#include "Animation/AnimSequence.h"

#define LOCTEXT_NAMESPACE "NearestNeighborEditorModel"

namespace UE::NearestNeighborModel
{
	using namespace UE::MLDeformer;

	FMLDeformerEditorModel* FNearestNeighborEditorModel::MakeInstance()
	{
		return new FNearestNeighborEditorModel();
	}

	void FNearestNeighborEditorModel::Init(const InitSettings& InitSettings)
	{
		FMLDeformerEditorModel::Init(InitSettings);
		InitInputInfo(Model->GetInputInfo());
		InitMorphTargets();
	}

	FMLDeformerSampler* FNearestNeighborEditorModel::CreateSampler() const
	{
		FNearestNeighborGeomCacheSampler* NewSampler = new FNearestNeighborGeomCacheSampler();
		NewSampler->OnGetGeometryCache().BindLambda([this] { return GetGeomCacheModel()->GetGeometryCache(); });
		return NewSampler;
	}

	void FNearestNeighborEditorModel::OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent)
	{
		const FProperty* Property = PropertyChangedEvent.Property;
		if (Property == nullptr)
		{
			return;
		}

		FMLDeformerMorphModelEditorModel::OnPropertyChanged(PropertyChangedEvent);

		if (Property->GetFName() == UMLDeformerModel::GetSkeletalMeshPropertyName() ||
			Property->GetFName() == UMLDeformerModel::GetAnimSequencePropertyName() ||
			Property->GetFName() == UMLDeformerGeomCacheModel::GetGeometryCachePropertyName() ||
			Property->GetFName() == UNearestNeighborModel::GetClothPartEditorDataPropertyName()
			|| (PropertyChangedEvent.MemberProperty != nullptr && PropertyChangedEvent.MemberProperty->GetFName() == UNearestNeighborModel::GetClothPartEditorDataPropertyName()))
		{
			GetNearestNeighborModel()->InvalidateClothPartData();
			GetEditor()->GetModelDetailsView()->ForceRefresh();
		}

		if (Property->GetFName() == UNearestNeighborModel::GetNearestNeighborDataPropertyName())
		{
			GetNearestNeighborModel()->InvalidateNearestNeighborData();
			GetEditor()->GetModelDetailsView()->ForceRefresh();
		}
	}

	void FNearestNeighborEditorModel::OnPostTraining(ETrainingResult TrainingResult, bool bUsePartiallyTrainedWhenAborted)
	{
		FMLDeformerMorphModelEditorModel::OnPostTraining(TrainingResult, bUsePartiallyTrainedWhenAborted);
		GetNearestNeighborModel()->InitPreviousWeights();
	}

	FString FNearestNeighborEditorModel::GetTrainedNetworkOnnxFile() const
	{
		const UNearestNeighborModel* NearestNeighborModel = GetNearestNeighborModel();
		if (NearestNeighborModel)
		{
			return NearestNeighborModel->GetModelDir() + TEXT("/NearestNeighborModel.onnx");
		}
		else
		{
			return FMLDeformerEditorModel::GetTrainedNetworkOnnxFile();
		}
	}

	void FNearestNeighborEditorModel::CreateActors(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
	{
		FMLDeformerMorphModelEditorModel::CreateActors(InPersonaPreviewScene);

		UWorld* World = InPersonaPreviewScene->GetWorld();
		CreateNearestNeighborActors(World);
		EditorWorld = World;
	}

	void FNearestNeighborEditorModel::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
	{
		FMLDeformerEditorModel::Tick(ViewportClient, DeltaTime);
		for (FNearestNeighborEditorModelActor* NearestNeighborActor : NearestNeighborActors)
		{
			if (NearestNeighborActor)
			{
				NearestNeighborActor->TickNearestNeighborActor();
			}
		}
	}

	void FNearestNeighborEditorModel::CreateNearestNeighborActors(UWorld* World, int32 StartIndex)
	{
		UNearestNeighborModel* NearestNeighborModel = GetNearestNeighborModel();
		const int32 NumParts = NearestNeighborModel->GetNumParts();

		if (StartIndex == 0)
		{
			NearestNeighborActors.Reset();
		}
		NearestNeighborActors.SetNumZeroed(NumParts);

		UMLDeformerComponent* MLDeformerComponent = nullptr;
		FMLDeformerEditorActor* TestActor = FindEditorActor(ActorID_Test_MLDeformed);
		if (TestActor)
		{
			AActor* Actor = TestActor->GetActor();
			if (Actor)
			{
				MLDeformerComponent = Actor->FindComponentByClass<UMLDeformerComponent>();
			}
		}
		
		for(int32 PartId = StartIndex; PartId < NumParts; PartId++)
		{
			UGeometryCache* GeomCache = NearestNeighborModel->GetNearestNeighborCache(PartId);
			const FLinearColor LabelColor = FNearestNeighborModelEditorStyle::Get().GetColor("NearestNeighborModel.NearestNeighborActors.LabelColor");
			const FLinearColor WireframeColor = FNearestNeighborModelEditorStyle::Get().GetColor("NearestNeighborModel.NearestNeighborActors.WireframeColor");
			CreateGeomCacheActor(
				World, 
				ActorID_NearestNeighborActors,
				*FString::Printf(TEXT("NearestNeighbors%d"), PartId), 
				GeomCache, 
				LabelColor,					
				WireframeColor,
				LOCTEXT("TestNearestNeighborLabelText", "Nearest Neigbors"),
				false);
			FNearestNeighborEditorModelActor* NearestNeighborActor = static_cast<FNearestNeighborEditorModelActor*>(EditorActors.Last());

			NearestNeighborActor->InitNearestNeighborActor(PartId, MLDeformerComponent);
			NearestNeighborActor->SetMeshOffsetFactor(2.0f);
			NearestNeighborActors[PartId] = NearestNeighborActor;
		}
	}

	FMLDeformerEditorActor* FNearestNeighborEditorModel::CreateEditorActor(const FMLDeformerEditorActor::FConstructSettings& Settings) const
	{
		return new FNearestNeighborEditorModelActor(Settings);
	}

	void FNearestNeighborEditorModel::InitInputInfo(UMLDeformerInputInfo* InputInfo)
	{
		FMLDeformerEditorModel::InitInputInfo(InputInfo);
		UNearestNeighborModel *NearestNeighborModel = static_cast<UNearestNeighborModel*>(Model);
		NearestNeighborModel->InitInputInfo();
	}

	ETrainingResult FNearestNeighborEditorModel::Train()
	{
		return TrainModel<UNearestNeighborTrainingModel>(this);
	}

	bool FNearestNeighborEditorModel::SetSamplerPartData(const int32 PartId)
	{
		FNearestNeighborGeomCacheSampler* GeomCacheSampler = static_cast<FNearestNeighborGeomCacheSampler*>(GetGeomCacheSampler());
		UNearestNeighborModel* NearestNeighborModel = GetNearestNeighborModel();
		if (GeomCacheSampler && NearestNeighborModel && PartId < NearestNeighborModel->GetNumParts())
		{
			TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = GeomCacheSampler->GetSkeletalMeshComponent();
			TObjectPtr<UGeometryCacheComponent> GeometryCacheComponent = GeomCacheSampler->GetGeometryCacheComponent();

			const TObjectPtr<UAnimSequence> AnimSequence = NearestNeighborModel->GetNearestNeighborSkeletons(PartId);
			const TObjectPtr<UGeometryCache> GeometryCache = NearestNeighborModel->GetNearestNeighborCache(PartId);

			if (SkeletalMeshComponent && GeometryCacheComponent && AnimSequence && GeometryCache)
			{
				const int32 NumFrames = GeometryCache->GetEndFrame() - GeometryCache->GetStartFrame() + 1;
				if (AnimSequence->GetDataModel()->GetNumberOfKeys() == NumFrames)
				{
					AnimSequence->bUseRawDataOnly = true;
					AnimSequence->Interpolation = EAnimInterpolationType::Step;

					SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
					SkeletalMeshComponent->SetAnimation(AnimSequence);
					SkeletalMeshComponent->SetPosition(0.0f);
					SkeletalMeshComponent->SetPlayRate(1.0f);
					SkeletalMeshComponent->Play(false);
					SkeletalMeshComponent->RefreshBoneTransforms();

					// assuming MeshMappings do not change
					GeometryCacheComponent->SetGeometryCache(GeometryCache);
					GeometryCacheComponent->ResetAnimationTime();
					GeometryCacheComponent->SetLooping(false);
					GeometryCacheComponent->SetManualTick(true);
					GeometryCacheComponent->SetPlaybackSpeed(1.0f);
					GeometryCacheComponent->Play();
					GeomCacheSampler->GeneratePartMeshMappings(GetNearestNeighborModel()->PartVertexMap(PartId), NearestNeighborModel->GetUsePartOnlyMesh());

					return true;
				}
				else
				{
					UE_LOG(LogNearestNeighborModel, Error, TEXT("Part %d frame mismatch: AnimSequence has %d frames and GeometryCache has %d frames"), PartId, AnimSequence->GetDataModel()->GetNumberOfKeys(), NumFrames);
				}
			}
			else
			{
				UE_LOG(LogNearestNeighborModel, Error, TEXT("Part %d: AnimSequence or GeometryCache is null"), PartId);
			}
		}
		return false;
	}

	int32 FNearestNeighborEditorModel::GetNumParts()
	{
		UNearestNeighborModel *NearestNeighborModel = GetNearestNeighborModel();
		if (NearestNeighborModel != nullptr)
		{
			return NearestNeighborModel->GetNumParts();
		}
		else
		{
			return 0;
		}
	}

	void FNearestNeighborEditorModel::ResetSamplerData()
	{
		FNearestNeighborGeomCacheSampler* GeomCacheSampler = static_cast<FNearestNeighborGeomCacheSampler*>(GetGeomCacheSampler());
		UNearestNeighborModel* NearestNeighborModel = GetNearestNeighborModel();
		if (GeomCacheSampler && NearestNeighborModel)
		{
			TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = GeomCacheSampler->GetSkeletalMeshComponent();
			const TObjectPtr<UAnimSequence> AnimSequence = NearestNeighborModel->GetAnimSequence();

			if (SkeletalMeshComponent && AnimSequence)
			{
				SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				SkeletalMeshComponent->SetAnimation(AnimSequence);
				SkeletalMeshComponent->SetPosition(0.0f);
				SkeletalMeshComponent->SetPlayRate(1.0f);
				SkeletalMeshComponent->Play(false);
				SkeletalMeshComponent->RefreshBoneTransforms();
			}

			GeomCacheSampler->RegisterTargetComponents();
		}
	}

	void FNearestNeighborEditorModel::UpdateNearestNeighborActors()
	{
		if (EditorWorld != nullptr)
		{
			const UNearestNeighborModel* NearestNeighborModel = GetNearestNeighborModel();
			const int32 TargetNumActors = NearestNeighborModel->GetNumParts();
			if (NearestNeighborActors.Num() > TargetNumActors)
			{
				for (int32 i = NearestNeighborActors.Num() - 1; i >= TargetNumActors; i--)
				{
					FNearestNeighborEditorModelActor* EditorActor = NearestNeighborActors[i];
					NearestNeighborActors.RemoveAt(i);
					EditorActors.Remove(EditorActor);
					EditorWorld->RemoveActor(EditorActor->GetActor(), true/*ShouldModifyLevel*/);
					delete EditorActor;
				}
			}
			if (NearestNeighborActors.Num() < TargetNumActors)
			{
				CreateNearestNeighborActors(EditorWorld, NearestNeighborActors.Num());
			}
		}

		for (int32 PartId = 0; PartId < NearestNeighborActors.Num(); PartId++)
		{
			const TObjectPtr<UGeometryCache> GeometryCache = GetNearestNeighborModel()->GetNearestNeighborCache(PartId);
			NearestNeighborActors[PartId]->GetGeometryCacheComponent()->SetGeometryCache(GeometryCache);
		}
	}

	void FNearestNeighborEditorModel::UpdateNearestNeighborData()
	{
		UNearestNeighborModel *NearestNeighborModel = static_cast<UNearestNeighborModel*>(Model);
		check(NearestNeighborModel != nullptr);

		UNeuralNetwork *NeuralNetwork = NearestNeighborModel->GetNeuralNetwork();
		if(NeuralNetwork && NeuralNetwork->IsLoaded())
		{
			const FString SavePath = GetTrainedNetworkOnnxFile();
			if (!FPaths::FileExists(SavePath) || !NearestNeighborModel->GetUseFileCache())
			{
				UE_LOG(LogNearestNeighborModel, Display, TEXT("Saving to %s"), *SavePath);
				NeuralNetwork->Save(SavePath);
			}
		}
		else
		{
			UE_LOG(LogNearestNeighborModel, Warning, TEXT("Network is not available. Nothing will be done."));
			return;
		}

		FNearestNeighborGeomCacheSampler* GeomCacheSampler = static_cast<FNearestNeighborGeomCacheSampler*>(GetGeomCacheSampler());
		if (GeomCacheSampler)
		{
			TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = GeomCacheSampler->GetSkeletalMeshComponent();
			TObjectPtr<UGeometryCacheComponent> GeometryCacheComponent = GeomCacheSampler->GetGeometryCacheComponent();

			if(SkeletalMeshComponent && GeometryCacheComponent)
			{
				UNearestNeighborTrainingModel *TrainingModel = InitTrainingModel<UNearestNeighborTrainingModel>(this);
				check(TrainingModel != nullptr);
				TrainingModel->UpdateNearestNeighborData();
				ResetSamplerData();
				UpdateNearestNeighborActors();
				NearestNeighborModel->ValidateNearestNeighborData();
			}
		}
	}

	void FNearestNeighborEditorModel::KMeansClusterPoses()
	{
		UNearestNeighborModel *NearestNeighborModel = static_cast<UNearestNeighborModel*>(Model);
		check(NearestNeighborModel != nullptr);

		UNeuralNetwork *NeuralNetwork = NearestNeighborModel->GetNeuralNetwork();
		if(NeuralNetwork && NeuralNetwork->IsLoaded())
		{
			const FString SavePath = GetTrainedNetworkOnnxFile();
			if (!FPaths::FileExists(SavePath) || !NearestNeighborModel->GetUseFileCache())
			{
				UE_LOG(LogNearestNeighborModel, Display, TEXT("Saving to %s"), *SavePath);
				NeuralNetwork->Save(SavePath);
			}
		}
		else
		{
			UE_LOG(LogNearestNeighborModel, Warning, TEXT("Network is not available. Nothing will be done."));
			return;
		}

		UNearestNeighborTrainingModel *TrainingModel = InitTrainingModel<UNearestNeighborTrainingModel>(this);
		check(TrainingModel != nullptr);
		TrainingModel->KmeansClusterPoses();
		ResetSamplerData();
	}

	void FNearestNeighborEditorModel::AddFloatArrayToDeltaArray(const TArray<float>& FloatArr, const TArray<uint32>& VertexMap, TArray<FVector3f>& DeltaArr, int32 DeltaArrayOffset, float ScaleFactor)
	{
		const int32 NumBaseMeshVerts = Model->GetNumBaseMeshVerts();
		const UNearestNeighborModel* NearestNeighborModel = GetNearestNeighborModel();
		const int32 PartNumVerts = VertexMap.Num();
		if (PartNumVerts == 0)
		{
			return;
		}
		const int32 NumShapes = FloatArr.Num() / (PartNumVerts * 3);
		if (DeltaArrayOffset < 0)
		{
			DeltaArrayOffset = DeltaArr.Num();
		}
		DeltaArr.SetNumZeroed(FMath::Max(DeltaArrayOffset + NumShapes * NumBaseMeshVerts, DeltaArr.Num()), false);

		for(int32 ShapeId = 0; ShapeId < NumShapes; ShapeId++)
		{
			for(int32 VertexId = 0; VertexId < PartNumVerts; VertexId++)
			{
				const int32 DeltaId = ShapeId * NumBaseMeshVerts + VertexMap[VertexId];
				const int32 FloatId = (ShapeId * PartNumVerts + VertexId) * 3;
				DeltaArr[DeltaArrayOffset + DeltaId] = FVector3f(FloatArr[FloatId], FloatArr[FloatId + 1], FloatArr[FloatId + 2]) * ScaleFactor;
			}
		}
	}

	void FNearestNeighborEditorModel::InitMorphTargets()
	{
		UNearestNeighborModel* NearestNeighborModel = GetNearestNeighborModel();
		if (NearestNeighborModel->GetNumBaseMeshVerts() == 0)
		{
			return;
		}
		TArray<FVector3f> Deltas;
		Deltas.Reset();

		int32 NumPCACoeff = 0;
		for (int32 PartId = 0; PartId < NearestNeighborModel->GetNumParts(); PartId++)
		{
			NumPCACoeff += NearestNeighborModel->GetPCACoeffNum(PartId);
		}
		Deltas.Reserve((1 + NumPCACoeff) * NearestNeighborModel->GetNumBaseMeshVerts());

		for (int32 PartId = 0; PartId < NearestNeighborModel->GetNumParts(); PartId++)
		{
			const TArray<uint32>& VertexMap = NearestNeighborModel->PartVertexMap(PartId);
			AddFloatArrayToDeltaArray(NearestNeighborModel->ClothPartData[PartId].VertexMean, VertexMap, Deltas, 0);
			AddFloatArrayToDeltaArray(NearestNeighborModel->ClothPartData[PartId].PCABasis, VertexMap, Deltas);
		}

		for (int32 PartId = 0; PartId < NearestNeighborModel->GetNumParts(); PartId++)
		{
			const TArray<uint32>& VertexMap = NearestNeighborModel->PartVertexMap(PartId);
			AddFloatArrayToDeltaArray(NearestNeighborModel->ClothPartData[PartId].NeighborOffsets, VertexMap, Deltas);
		}

		if (Deltas.Num() == 0)
		{
			return;
		}

		const int32 NumImportedModelVerts = FMath::Max(NearestNeighborModel->GetVertexMap()) + 1;
		const int32 NumBaseMeshVerts = Model->GetNumBaseMeshVerts();
		if (NumImportedModelVerts != NumBaseMeshVerts)
		{
			UE_LOG(LogNearestNeighborModel, Error, TEXT("Vertex count mismatch: imported model of SkeletalMesh has %d vertices and cached SkeletalMesh has %d vertices"), NumImportedModelVerts, NumBaseMeshVerts);
			return;
		}

		const int32 LOD = 0;
		TArray<UMorphTarget*> MorphTargets;
		CreateEngineMorphTargets(MorphTargets, Deltas, FString("NNMorphTarget_"), LOD, NearestNeighborModel->GetMorphTargetDeltaThreshold());
		check(NearestNeighborModel->GetMorphTargetSet().IsValid());
		FMorphTargetVertexInfoBuffers& MorphBuffers = NearestNeighborModel->GetMorphTargetSet()->MorphBuffers;
		CompressEngineMorphTargets(MorphBuffers, MorphTargets, LOD, NearestNeighborModel->GetMorphTargetErrorTolerance());
		NearestNeighborModel->SetMorphTargetDeltas(Deltas);
	}

	void FNearestNeighborEditorModel::RefreshMorphTargets()
	{
		USkeletalMeshComponent* SkelMeshComponent = FindEditorActor(ActorID_Test_MLDeformed)->GetSkeletalMeshComponent() ;
		if (SkelMeshComponent)
		{
			FMorphTargetVertexInfoBuffers& MorphBuffers = GetNearestNeighborModel()->GetMorphTargetSet()->MorphBuffers;
			BeginReleaseResource(&MorphBuffers);
			if (MorphBuffers.IsMorphCPUDataValid() && MorphBuffers.GetNumMorphs() > 0)
			{
				BeginInitResource(&MorphBuffers);
			}
			SkelMeshComponent->RefreshExternalMorphTargetWeights();
		}
	}
}	// namespace UE::NearestNeighborModel

#undef LOCTEXT_NAMESPACE
