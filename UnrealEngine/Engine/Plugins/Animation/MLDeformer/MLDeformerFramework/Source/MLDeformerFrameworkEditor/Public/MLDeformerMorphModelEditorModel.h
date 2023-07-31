// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerGeomCacheEditorModel.h"
#include "MLDeformerGeomCacheSampler.h"
#include "MLDeformerEditorActor.h"

class UMLDeformerMorphModel;
class UMLDeformerMorphModelVizSettings;

namespace UE::MLDeformer
{
	/**
	 * The editor model related to the runtime UMLDeformerMorphModel class, or models inherited from that.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerMorphModelEditorModel
		: public FMLDeformerGeomCacheEditorModel
	{
	public:
		// We need to implement this static MakeInstance method.
		static FMLDeformerEditorModel* MakeInstance();

		// FGCObject overrides.
		virtual FString GetReferencerName() const override { return TEXT("FMLDeformerMorphModelEditorModel"); }
		// ~END FGCObject overrides.

		// FMLDeformerEditorModel overrides.
		virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
		virtual void OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent) override;
		virtual FString GetHeatMapDeformerGraphPath() const override;
		virtual void OnPreTraining() override;
		virtual void OnPostTraining(ETrainingResult TrainingResult, bool bUsePartiallyTrainedWhenAborted) override;
		// ~END FMLDeformerEditorModel overrides.

		// Helpers.
		UMLDeformerMorphModel* GetMorphModel() const;
		UMLDeformerMorphModelVizSettings* GetMorphModelVizSettings() const;

	protected:
		/**
		 * Initialize a set of engine morph targets and compress them to GPU friendly buffers.
		 * These morph targets are initialized from a set of deltas. Each morph target needs to have Model->GetNumBaseVerts() number of deltas.
		 * All deltas are concatenated in one big array. So all deltas of all vertices for the second morph target are appended to the deltas for
		 * the first morph target, etc. In other words, the layout is: [morph0_deltas][morph1_deltas][morph2_deltas][...].
		 * @param Deltas The deltas for all morph targets concatenated. So the number of items in this array is a multiple of Model->GetNumBaseVerts().
		 */
		void InitEngineMorphTargets(const TArray<FVector3f>& Deltas);

		/**
		 * Clamp the current morph target number inside the visualization settings to a valid range.
		 * This won't let the number of the morph target that we are visualizing go above the total number of morph targets.
		 */
		void ClampMorphTargetNumber();

	protected:
		/**
		 * The entire set of morph target deltas, 3 per vertex, for each morph target, as one flattened buffer.
		 * So the size of this buffer is: (NumVertsPerMorphTarget * NumMorphTargets).
		 */
		TArray<FVector3f> MorphTargetDeltasBackup;
	};
}	// namespace UE::MLDeformer
