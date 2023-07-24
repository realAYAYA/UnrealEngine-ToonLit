// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModelEditorModel.h"
#include "MLDeformerMorphModel.h"
#include "MLDeformerMorphModelVizSettings.h"
#include "MLDeformerMorphModelInstance.h"
#include "MLDeformerMorphModelInstance.h"
#include "MLDeformerGeomCacheActor.h"
#include "MLDeformerEditorToolkit.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerComponent.h"
#include "MLDeformerGeomCacheHelpers.h"
#include "Components/ExternalMorphSet.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/MorphTarget.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
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

		if (Property->GetFName() == UMLDeformerMorphModel::GetMorphDeltaZeroThresholdPropertyName() || 
			Property->GetFName() == UMLDeformerMorphModel::GetMorphCompressionLevelPropertyName() ||
			Property->GetFName() == UMLDeformerMorphModel::GetIncludeMorphTargetNormalsPropertyName() ||
			Property->GetFName() == UMLDeformerMorphModel::GetMaskChannelPropertyName() ||
			Property->GetFName() == UMLDeformerMorphModel::GetInvertMaskChannelPropertyName())
		{
			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
			{
				if (GetMorphModel()->CanDynamicallyUpdateMorphTargets())
				{
					InitEngineMorphTargets(GetMorphModel()->GetMorphTargetDeltas());
				}
			}
		}
		else if (Property->GetFName() == UMLDeformerMorphModelVizSettings::GetMorphTargetNumberPropertyName())
		{
			ClampMorphTargetNumber();
		}
		else if (Property->GetFName() == UMLDeformerMorphModel::GetQualityLevelsPropertyName())
		{
			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
			{
				TArray<FMLDeformerMorphModelQualityLevel>& QualityLevels = GetMorphModel()->GetQualityLevelsArray();				
				if (QualityLevels.Num() == 1)
				{
					const int32 NewQualityValue = FMath::Max(GetMorphModel()->GetNumMorphTargets() - 1, 1);
					QualityLevels[QualityLevels.Num() - 1].SetMaxActiveMorphs(NewQualityValue);
				}
				else
				{
					int32 NewQuality = static_cast<int32>(QualityLevels[QualityLevels.Num() - 2].GetMaxActiveMorphs() * 0.75f);
					NewQuality = FMath::Clamp<int32>(NewQuality, 1, GetMorphModel()->GetNumMorphTargets() - 1);
					QualityLevels[QualityLevels.Num() - 1].SetMaxActiveMorphs(NewQuality);
				}
			}
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
			// Restore the morph target vertex deltas backup.
			GetMorphModel()->SetMorphTargetDeltas(MorphTargetDeltasBackup);
		}
		else if (TrainingResult == ETrainingResult::Success || (TrainingResult == ETrainingResult::Aborted && bUsePartiallyTrainedWhenAborted))
		{
			// Build morph targets inside the engine, using the engine's compression scheme.
			// Add one as we included the means now as extra morph target.
			InitEngineMorphTargets(GetMorphModel()->GetMorphTargetDeltas());
		}

		// This internally calls InitGPUData() which updates the GPU buffer with the deltas.
		FMLDeformerGeomCacheEditorModel::OnPostTraining(TrainingResult, bUsePartiallyTrainedWhenAborted);
	}

	float CalcStandardDeviation(TArrayView<const float> Values)
	{
		if (Values.IsEmpty())
		{
			return 0.0f;
		}

		// First calculate the mean.
		float Mean = 0.0f;
		for (float Value : Values)
		{
			Mean += Value;
		}
		Mean /= static_cast<float>(Values.Num());

		// Now calculate the standard deviation.
		float Sum = 0.0f;
		for (float Value : Values)
		{
			Sum += FMath::Square(Value - Mean);
		}
		Sum /= static_cast<float>(Values.Num());

		return FMath::Sqrt(Sum);
	}

	void FMLDeformerMorphModelEditorModel::UpdateMorphErrorValues(TArrayView<UMorphTarget*> MorphTargets)
	{
		// If we don't have morph targets yet, there is nothing to do.
		UMLDeformerMorphModel* MorphModel = GetMorphModel();
		if (MorphModel->GetNumMorphTargets() == 0)
		{
			return;
		}

		// Check if we have max morph weight information.
		// If we do not have this yet, we have to initialize the weights to 1.
		TArrayView<const float> MaxMorphWeights = MorphModel->GetMorphTargetMaxWeights();

		// Preallocate space for the standard deviation of each morph target.
		TArray<float> ErrorValues;
		ErrorValues.SetNumZeroed(MorphTargets.Num() - 1);

		const int32 LOD = 0;
		TArray<float> DeltaLengths;
		for (int32 MorphIndex = 0; MorphIndex < MorphTargets.Num() - 1; ++MorphIndex)	// We have one extra morph for the means, skip that one.
		{
			const UMorphTarget* MorphTarget = MorphTargets[MorphIndex + 1];
			const float MaxWeight = !MaxMorphWeights.IsEmpty() ? MaxMorphWeights[MorphIndex] : 1.0f;

			// Get the array of deltas.
			int32 NumDeltas = 0;
			const FMorphTargetDelta* Deltas = MorphTarget->GetMorphTargetDelta(LOD, NumDeltas);

			// Build the array of position delta lengths.
			DeltaLengths.Reset();
			DeltaLengths.SetNumUninitialized(NumDeltas);
			for (int32 DeltaIndex = 0; DeltaIndex < NumDeltas; ++DeltaIndex)
			{
				DeltaLengths[DeltaIndex] = Deltas[DeltaIndex].PositionDelta.Length() * MaxWeight;
			}

			// Now calculate the standard deviation of those lengths.
			const float StandardDeviation = CalcStandardDeviation(DeltaLengths);
			ErrorValues[MorphIndex] = StandardDeviation;
		}

		// Build a list of array indices, so we know the order in which things got sorted.
		TArray<int32> SortedIndices;
		SortedIndices.SetNumUninitialized(MorphTargets.Num() - 1);
		for (int32 Index = 0; Index < SortedIndices.Num(); ++Index)
		{
			SortedIndices[Index] = Index;
		}

		// Now that we have a list of standard deviations, sort them.
		SortedIndices.Sort
		(
			[&ErrorValues](const int32& IndexA, const int32& IndexB)
			{
				return ErrorValues[IndexA] > ErrorValues[IndexB];
			}
		);

		// Update the morph model with the newly calculated error values.
		MorphModel->SetMorphTargetsErrorOrder(SortedIndices, ErrorValues);
	}

	void FMLDeformerMorphModelEditorModel::InitEngineMorphTargets(const TArray<FVector3f>& Deltas)
	{
		UMLDeformerMorphModel* MorphModel = GetMorphModel();
		if (MorphModel->GetMorphTargetDeltas().IsEmpty())
		{
			return;
		}

		// Zero out small deltas.
		TArray<FVector3f> MorphTargetDeltas = Deltas;
		ZeroDeltasByThreshold(MorphTargetDeltas, MorphModel->GetMorphDeltaZeroThreshold());

		// Turn the delta buffer in a set of engine morph targets.
		const int32 LOD = 0;
		const bool bIncludeNormals = MorphModel->GetIncludeMorphTargetNormals();
		const EMLDeformerMaskChannel MaskChannel = MorphModel->GetMaskChannel();
		const bool bInvertMaskChannel = MorphModel->GetInvertMaskChannel();

		// Create the engine morph targets.
		TArray<UMorphTarget*> MorphTargets;
		CreateEngineMorphTargets(
			MorphTargets,
			Deltas, 
			TEXT("MLDeformerMorph_"), 
			LOD,
			MorphModel->GetMorphDeltaZeroThreshold(),
			bIncludeNormals,
			MaskChannel,
			bInvertMaskChannel);

		// Analyze the error values of the morph targets.
		UpdateMorphErrorValues(MorphTargets);

		// Now compress the morph targets to GPU friendly buffers.
		check(MorphModel->GetMorphTargetSet().IsValid());
		FMorphTargetVertexInfoBuffers& MorphBuffers = MorphModel->GetMorphTargetSet()->MorphBuffers;
		CompressEngineMorphTargets(MorphBuffers, MorphTargets, LOD, MorphModel->GetMorphCompressionLevel());

		if (MorphBuffers.GetNumBatches() == 0 || MorphBuffers.GetNumMorphs() == 0)
		{
			MorphBuffers = FMorphTargetVertexInfoBuffers();
		}

		MorphModel->UpdateStatistics();

		// Remove the morph targets again, as we don't need them anymore.
		for (UMorphTarget* MorphTarget : MorphTargets)
		{
			MorphTarget->ConditionalBeginDestroy();
		}
	}

	void FMLDeformerMorphModelEditorModel::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
	{
		FMLDeformerGeomCacheEditorModel::Render(View, Viewport, PDI);

		// Debug draw the selected morph target.
		UMLDeformerMorphModel* MorphModel = GetMorphModel();
		const UMLDeformerMorphModelVizSettings* VizSettings = GetMorphModelVizSettings();
		if (VizSettings->GetDrawMorphTargets() && VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
		{
			const FVector DrawOffset = -VizSettings->GetMeshSpacingOffsetVector();
			DrawMorphTarget(PDI, GetMorphModel()->GetMorphTargetDeltas(), MorphModel->GetMorphDeltaZeroThreshold(), VizSettings->GetMorphTargetNumber(), DrawOffset);
		}
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE
