// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModelEditorModel.h"
#include "MLDeformerMorphModel.h"
#include "MLDeformerMorphModelVizSettings.h"
#include "MLDeformerGeomCacheActor.h"
#include "MLDeformerEditorToolkit.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerGeomCacheHelpers.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/MorphTarget.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "MLDeformerMorphModelEditorModel"

namespace UE::MLDeformer
{
	FMLDeformerEditorModel* FMLDeformerMorphModelEditorModel::MakeInstance()
	{
		return new FMLDeformerMorphModelEditorModel();
	}

	void FMLDeformerMorphModelEditorModel::OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent)
	{
		const FProperty* Property = PropertyChangedEvent.Property;
		if (Property == nullptr)
		{
			return;
		}

		FMLDeformerGeomCacheEditorModel::OnPropertyChanged(PropertyChangedEvent);

		// Clamp the morph target number, if it changes.
		if (Property->GetFName() == UMLDeformerMorphModelVizSettings::GetMorphTargetNumberPropertyName())
		{
			ClampMorphTargetNumber();
		}
	}

	void FMLDeformerMorphModelEditorModel::ClampMorphTargetNumber()
	{
		const UMLDeformerMorphModel* MorphModel = GetMorphModel();			
		UMLDeformerMorphModelVizSettings* MorphViz = GetMorphModelVizSettings();
		const int32 NumMorphTargets = MorphModel->GetMorphTargetSet().IsValid() ? MorphModel->GetMorphTargetSet()->MorphBuffers.GetNumMorphs() : 0;
		const int32 ClampedMorphTargetNumber = (NumMorphTargets > 0) ? FMath::Min<int32>(MorphViz->GetMorphTargetNumber(), NumMorphTargets - 1) : 0;
		MorphViz->SetMorphTargetNumber(ClampedMorphTargetNumber);
	}

	UMLDeformerMorphModel* FMLDeformerMorphModelEditorModel::GetMorphModel() const
	{ 
		return Cast<UMLDeformerMorphModel>(Model);
	}

	UMLDeformerMorphModelVizSettings* FMLDeformerMorphModelEditorModel::GetMorphModelVizSettings() const
	{
		return Cast<UMLDeformerMorphModelVizSettings>(GetMorphModel()->GetVizSettings());
	}

	FString FMLDeformerMorphModelEditorModel::GetHeatMapDeformerGraphPath() const
	{
		return FString(TEXT("/MLDeformerFramework/Deformers/DG_MLDeformerModel_GPUMorph_HeatMap.DG_MLDeformerModel_GPUMorph_HeatMap"));
	}

	void FMLDeformerMorphModelEditorModel::OnPreTraining()
	{
		// Backup the morph target deltas in case we abort training.
		MorphTargetDeltasBackup = GetMorphModel()->GetMorphTargetDeltas();
	}

	void FMLDeformerMorphModelEditorModel::OnPostTraining(ETrainingResult TrainingResult, bool bUsePartiallyTrainedWhenAborted)
	{
		// We aborted and don't want to use partially trained results, we should restore the deltas that we just overwrote after training.
		if (TrainingResult == ETrainingResult::Aborted && !bUsePartiallyTrainedWhenAborted)
		{
			// Restore the blendshape backup.
			GetMorphModel()->SetMorphTargetDeltas(MorphTargetDeltasBackup);
		}
		else if (TrainingResult == ETrainingResult::Success || (TrainingResult == ETrainingResult::Aborted && bUsePartiallyTrainedWhenAborted))
		{
			const UMLDeformerMorphModel* MorphModel = GetMorphModel();
			if (!MorphModel->GetMorphTargetDeltas().IsEmpty())
			{
				// Set deltas with a length equal or below a given threshold to zero, for better compression.
				TArray<FVector3f> MorphTargetDeltas = MorphModel->GetMorphTargetDeltas();
				ZeroDeltasByThreshold(MorphTargetDeltas, MorphModel->GetMorphTargetDeltaThreshold());

				// Build morph targets inside the engine, using the engine's compression scheme.
				// Add one as we included the means now as extra morph target.
				InitEngineMorphTargets(MorphTargetDeltas);
			}
		}

		// This internally calls InitGPUData() which updates the GPU buffer with the deltas.
		FMLDeformerGeomCacheEditorModel::OnPostTraining(TrainingResult, bUsePartiallyTrainedWhenAborted);
	}

	void FMLDeformerMorphModelEditorModel::InitEngineMorphTargets(const TArray<FVector3f>& Deltas)
	{
		const int32 LOD = 0;

		// Turn the delta buffer in a set of engine morph targets.
		UMLDeformerMorphModel* MorphModel = GetMorphModel();
		TArray<UMorphTarget*> MorphTargets;	// These will be garbage collected.
		CreateEngineMorphTargets(MorphTargets, Deltas, TEXT("MLDeformerMorph_"), LOD, MorphModel->GetMorphTargetDeltaThreshold());

		// Now compress the morph targets to GPU friendly buffers.
		check(MorphModel->GetMorphTargetSet().IsValid());
		FMorphTargetVertexInfoBuffers& MorphBuffers = MorphModel->GetMorphTargetSet()->MorphBuffers;
		CompressEngineMorphTargets(MorphBuffers, MorphTargets, LOD, MorphModel->GetMorphTargetErrorTolerance());

		if (MorphBuffers.GetNumBatches() == 0 || MorphBuffers.GetNumMorphs() == 0)
		{
			MorphBuffers = FMorphTargetVertexInfoBuffers();
		}
	}

	void FMLDeformerMorphModelEditorModel::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
	{
		FMLDeformerGeomCacheEditorModel::Render(View, Viewport, PDI);

		// Debug draw the selected morph target.
		const UMLDeformerMorphModelVizSettings* VizSettings = GetMorphModelVizSettings();
		if (VizSettings->GetDrawMorphTargets() && VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
		{
			const FVector DrawOffset = -VizSettings->GetMeshSpacingOffsetVector();
			DrawMorphTarget(PDI, GetMorphModel()->GetMorphTargetDeltas(), VizSettings->GetMorphTargetDeltaThreshold(), VizSettings->GetMorphTargetNumber(), DrawOffset);
		}
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE
