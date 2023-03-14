// Copyright Epic Games, Inc. All Rights Reserved.

#include "VertexDeltaEditorModel.h"
#include "VertexDeltaModel.h"
#include "VertexDeltaModelVizSettings.h"
#include "VertexDeltaTrainingModel.h"
#include "MLDeformerGeomCacheActor.h"
#include "MLDeformerEditorToolkit.h"
#include "MLDeformerEditorStyle.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerGeomCacheSampler.h"
#include "MLDeformerGeomCacheHelpers.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "VertexDeltaEditorModel"

namespace UE::VertexDeltaModel
{
	using namespace UE::MLDeformer;

	FMLDeformerEditorModel* FVertexDeltaEditorModel::MakeInstance()
	{
		return new FVertexDeltaEditorModel();
	}

	ETrainingResult FVertexDeltaEditorModel::Train()
	{
		return TrainModel<UVertexDeltaTrainingModel>(this);
	}
}	// namespace UE::VertexDeltaModel

#undef LOCTEXT_NAMESPACE
