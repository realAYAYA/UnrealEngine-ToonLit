// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerEditorModel.h"

class UMLDeformerGeomCacheModel;
class UMLDeformerGeomCacheVizSettings;

namespace UE::MLDeformer
{
	class FMLDeformerGeomCacheActor;
	class FMLDeformerGeomCacheSampler;

	/**
	 * An editor model for the a model derived from the UMLDeformerGeomCacheModel runtime model.
	 * This editor model automatically handles sampling of the geometry caches to calculate deltas.
	 * It also handles the creation of ground truth models with a geometry cache on them.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerGeomCacheEditorModel
		: public FMLDeformerEditorModel
	{
	public:
		// FGCObject overrides.
		virtual FString GetReferencerName() const override { return TEXT("FMLDeformerGeomCacheEditorModel"); }
		// ~END FGCObject overrides.

		// FMLDeformerEditorModel overrides.
		virtual FMLDeformerEditorActor* CreateEditorActor(const FMLDeformerEditorActor::FConstructSettings& Settings) const override;
		virtual FMLDeformerSampler* CreateSampler() const override;
		virtual void CreateTrainingGroundTruthActor(UWorld* World) override;
		virtual void CreateTestGroundTruthActor(UWorld* World) override;
		virtual int32 GetNumTrainingFrames() const override;
		virtual double GetTrainingTimeAtFrame(int32 FrameNumber) const override;
		virtual int32 GetTrainingFrameAtTime(double TimeInSeconds) const override;
		virtual double GetTestTimeAtFrame(int32 FrameNumber) const override;
		virtual int32 GetTestFrameAtTime(double TimeInSeconds) const override;
		virtual void UpdateIsReadyForTrainingState() override;
		virtual void OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent) override;
		virtual void OnInputAssetsChanged() override;
		virtual ETrainingResult Train() override;
		// ~END FMLDeformerEditorModel overrides.
		
		// Helpers.
		FMLDeformerGeomCacheSampler* GetGeomCacheSampler() const;
		UMLDeformerGeomCacheModel* GetGeomCacheModel() const;
		UMLDeformerGeomCacheVizSettings* GetGeomCacheVizSettings() const;
		FMLDeformerGeomCacheActor* FindGeomCacheEditorActor(int32 ID) const;

	protected:
		/**
		 * Creates a geometry cache editor actor (FMLDeformerGeomCacheActor) of a specific ID with a specific geometry cache and label.
		 * This will add a new editor actor to the list of editor actors.
		 * @param World The world to create the actor in.
		 * @param ActorID The ID that the actor will have after creation, for example UE::MLDeformer::ActorID_Train_GroundTruth.
		 * @param Name The name of the actor in the scene.
		 * @param GeomCache The geometry cache to use on this actor.
		 * @param LabelColor The color of the label that is rendered with the editor actor inside the editor viewport.
		 * @param WireframeColor The color of this actor when wireframe rendering is enabled.
		 * @param LabelText The text of the label.
		 * @param bIsTrainingActor Set this to true when the actor is an actor to be used in training mode, or set to false when it is to be used in testing mode.
		 */
		void CreateGeomCacheActor(UWorld* World, int32 ActorID, const FName& Name, UGeometryCache* GeomCache, FLinearColor LabelColor, FLinearColor WireframeColor, const FText& LabelText, bool bIsTrainingActor);
	};
}	// namespace UE::MLDeformer
