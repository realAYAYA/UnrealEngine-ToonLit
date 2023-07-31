// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborTrainingModel.h"
#include "NearestNeighborModel.h"
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

void UNearestNeighborTrainingModel::SamplePart(int32 PartId, int32 Index)
{
	FNearestNeighborGeomCacheSampler* Sampler = static_cast<FNearestNeighborGeomCacheSampler*>(EditorModel->GetSampler());
	Sampler->SamplePart(Index, NearestNeighborModel->PartVertexMap(PartId));
	PartSampleDeltas = Sampler->GetPartVertexDeltas();
	SampleBoneRotations = Sampler->GetBoneRotations();
}

void UNearestNeighborTrainingModel::SetSamplerPartData(const int32 PartId)
{
	GetNearestNeighborEditorModel()->SetSamplerPartData(PartId);
}

int32 UNearestNeighborTrainingModel::GetPartNumNeighbors(const int32 PartId) const
{
	return NearestNeighborModel->GetNumNeighborsFromGeometryCache(PartId);
}

void UNearestNeighborTrainingModel::SampleKmeansAnim(const int32 SkeletonId)
{
	FNearestNeighborGeomCacheSampler* Sampler = static_cast<FNearestNeighborGeomCacheSampler*>(EditorModel->GetSampler());
	Sampler->SampleKMeansAnim(SkeletonId);
}

void UNearestNeighborTrainingModel::SampleKmeansFrame(const int32 Frame)
{
	FNearestNeighborGeomCacheSampler* Sampler = static_cast<FNearestNeighborGeomCacheSampler*>(EditorModel->GetSampler());
	Sampler->SampleKMeansFrame(Frame);
	SampleBoneRotations = Sampler->GetBoneRotations();
}


int32 UNearestNeighborTrainingModel::GetKmeansNumAnims() const
{
	return NearestNeighborModel->SourceSkeletons.Num();
}

int32 UNearestNeighborTrainingModel::GetKmeansAnimNumFrames(const int32 SkeletonId) const
{
	if (SkeletonId < NearestNeighborModel->SourceSkeletons.Num())
	{
		return NearestNeighborModel->SourceSkeletons[SkeletonId]->GetDataModel()->GetNumberOfFrames();
	}
	return 0;
}

int32 UNearestNeighborTrainingModel::GetKmeansNumClusters() const
{
	return NearestNeighborModel->NumClusters;
}


#undef LOCTEXT_NAMESPACE
