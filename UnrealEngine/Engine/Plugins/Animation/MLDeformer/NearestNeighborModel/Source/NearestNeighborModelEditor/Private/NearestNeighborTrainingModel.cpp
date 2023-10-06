// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborTrainingModel.h"
#include "NearestNeighborModel.h"
#include "NearestNeighborModelInstance.h"
#include "NearestNeighborEditorModel.h"
#include "NearestNeighborGeomCacheSampler.h"
#include "Animation/AnimSequence.h"

#define LOCTEXT_NAMESPACE "NearestNeighborTrainingModel"
using namespace UE::NearestNeighborModel;

void UNearestNeighborTrainingModel::Init(UE::MLDeformer::FMLDeformerEditorModel* InEditorModel)
{
	UMLDeformerTrainingModel::Init(InEditorModel);
	check(InEditorModel != nullptr);
	check(InEditorModel->GetModel() != nullptr);
	NearestNeighborModel = static_cast<UNearestNeighborModel*>(InEditorModel->GetModel());
}

UNearestNeighborModel* UNearestNeighborTrainingModel::GetNearestNeighborModel() const
{
	return Cast<UNearestNeighborModel>(GetModel());
}

UE::NearestNeighborModel::FNearestNeighborEditorModel* UNearestNeighborTrainingModel::GetNearestNeighborEditorModel() const
{
	return static_cast<UE::NearestNeighborModel::FNearestNeighborEditorModel*>(EditorModel);
}

const TArray<int32> UNearestNeighborTrainingModel::GetPartVertexMap(const int32 PartId) const
{
	const TArray<uint32>& VertexMap = NearestNeighborModel->PartVertexMap(PartId);
	return TArray<int32>((int32*)VertexMap.GetData(), VertexMap.Num());
}

int32 UNearestNeighborTrainingModel::SetSamplerPartData(const int32 PartId)
{
	return GetNearestNeighborEditorModel()->SetSamplerPartData(PartId);
}

int32 UNearestNeighborTrainingModel::GetPartNumNeighbors(const int32 PartId) const
{
	return FMath::Min(NearestNeighborModel->GetNumNeighborsFromAnimSequence(PartId), NearestNeighborModel->GetNumNeighborsFromGeometryCache(PartId));
}

bool UNearestNeighborTrainingModel::SampleKmeansAnim(const int32 SkeletonId)
{
	FNearestNeighborGeomCacheSampler* Sampler = static_cast<FNearestNeighborGeomCacheSampler*>(EditorModel->GetSampler());
	return Sampler->SampleKMeansAnim(SkeletonId);
}

bool UNearestNeighborTrainingModel::SampleKmeansFrame(const int32 Frame)
{
	FNearestNeighborGeomCacheSampler* Sampler = static_cast<FNearestNeighborGeomCacheSampler*>(EditorModel->GetSampler());
	const bool bSampleExist = Sampler->SampleKMeansFrame(Frame);
	if (bSampleExist)
	{
		SampleBoneRotations = Sampler->GetBoneRotations();
		return true;	
	}
	return false;
}


int32 UNearestNeighborTrainingModel::GetKmeansNumAnims() const
{
	return NearestNeighborModel->SourceAnims.Num();
}

int32 UNearestNeighborTrainingModel::GetKmeansAnimNumFrames(const int32 SkeletonId) const
{
	if (SkeletonId < NearestNeighborModel->SourceAnims.Num())
	{
		return NearestNeighborModel->SourceAnims[SkeletonId]->GetDataModel()->GetNumberOfFrames();
	}
	return 0;
}

int32 UNearestNeighborTrainingModel::GetKmeansNumClusters() const
{
	return NearestNeighborModel->NumClusters;
}

const TArray<float> UNearestNeighborTrainingModel::GetUnskinnedVertexPositions() const
{
	const TArray<FVector3f>& PositionsVec = EditorModel->GetSampler()->GetUnskinnedVertexPositions();
	TArray<float> PositionsFloat; 
	PositionsFloat.SetNumUninitialized(PositionsVec.Num() * 3);
	for (int32 i = 0; i < PositionsVec.Num(); i++)
	{
		PositionsFloat[i * 3] = PositionsVec[i].X;
		PositionsFloat[i * 3 + 1] = PositionsVec[i].Y;
		PositionsFloat[i * 3 + 2] = PositionsVec[i].Z;
	}
	return MoveTemp(PositionsFloat);
}

const TArray<int32> UNearestNeighborTrainingModel::GetMeshIndexBuffer() const
{
	FNearestNeighborGeomCacheSampler* Sampler = static_cast<FNearestNeighborGeomCacheSampler*>(EditorModel->GetSampler());
	TArray<uint32> UIndexBuffer = Sampler->GetMeshIndexBuffer();
	return TArray<int32>((int32*)UIndexBuffer.GetData(), UIndexBuffer.Num());
}

UNearestNeighborModelInstance* UNearestNeighborTrainingModel::CreateModelInstance()
{
	UNearestNeighborModelInstance* ModelInstance = NewObject<UNearestNeighborModelInstance>(this);
	ModelInstance->SetModel(GetNearestNeighborModel());
	ModelInstance->Init(nullptr);
	ModelInstance->PostMLDeformerComponentInit();
	return ModelInstance;
}

void UNearestNeighborTrainingModel::DestroyModelInstance(UNearestNeighborModelInstance* ModelInstance)
{
	ModelInstance->ConditionalBeginDestroy();
}


#undef LOCTEXT_NAMESPACE
