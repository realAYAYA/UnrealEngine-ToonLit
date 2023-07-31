// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerMorphModelEditorModel.h"
#include "MLDeformerEditorActor.h"

class UMLDeformerAsset;
class USkeletalMesh;
class UGeometryCache;
class UNearestNeighborModel;
class UNearestNeighborModelVizSettings;

namespace UE::NearestNeighborModel
{
	using namespace UE::MLDeformer;

	class FNearestNeighborEditorModelActor;
	class FNearestNeighborModelSampler;

	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborEditorModel 
		: public UE::MLDeformer::FMLDeformerMorphModelEditorModel
	{
	public:
		// We need to implement this static MakeInstance method.
		static FMLDeformerEditorModel* MakeInstance();

		// FGCObject overrides.
		virtual FString GetReferencerName() const override { return TEXT("FNearestNeighborEditorModel"); }
		// ~END FGCObject overrides.
	
		// FMLDeformerGeomCacheEditorModel overrides.
		virtual void Init(const InitSettings& Settings) override;
		virtual FMLDeformerSampler* CreateSampler() const override;
		virtual void CreateActors(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene) override;
		virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
		virtual FMLDeformerEditorActor* CreateEditorActor(const FMLDeformerEditorActor::FConstructSettings& Settings) const override;

		virtual void InitInputInfo(UMLDeformerInputInfo* InputInfo) override;
		virtual ETrainingResult Train() override;
		virtual void OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent) override;
		virtual void OnPostTraining(ETrainingResult TrainingResult, bool bUsePartiallyTrainedWhenAborted) override;
		virtual FString GetTrainedNetworkOnnxFile() const override;
		// ~END FMLDeformerGeomCacheEditorModel overrides.

		// Some helpers that cast to this model's variants of some classes.
		UNearestNeighborModel* GetNearestNeighborModel() const { return static_cast<UNearestNeighborModel*>(Model); }
	
		// Recomputes nearest neighbor coeffcients and nearest neighbor vertex offsets. 
		virtual void UpdateNearestNeighborData();

		// Temporarily set sampler to use part data in nearest neighbor dataset. This is useful when updating nearest neighbor data.
		bool SetSamplerPartData(const int32 PartI);

		// Reset smapler to use the original dataset.
		void ResetSamplerData();

		void UpdateNearestNeighborActors();
		void KMeansClusterPoses();

		void InitMorphTargets();
		void RefreshMorphTargets();
		void AddFloatArrayToDeltaArray(const TArray<float>& FloatArr, const TArray<uint32>& VertexMap, TArray<FVector3f>& DeltaArr, int32 DeltaArrayOffset = -1, float ScaleFactor = 1);

		int32 GetNumParts();

	protected:
		virtual void CreateNearestNeighborActors(UWorld* World, int32 StartIndex = 0);

		template<class TrainingModelClass>
		TrainingModelClass* InitTrainingModel(FMLDeformerEditorModel* EditorModel)
		{
			TArray<UClass*> TrainingModels;
			GetDerivedClasses(TrainingModelClass::StaticClass(), TrainingModels);
			if (TrainingModels.IsEmpty())
			{
				return nullptr;
			}

			TrainingModelClass* TrainingModel = Cast<TrainingModelClass>(TrainingModels.Last()->GetDefaultObject());
			TrainingModel->Init(EditorModel);
			return TrainingModel;
		}

		// Actors showing the nearest neighbors of the current frame
		TArray<FNearestNeighborEditorModelActor*> NearestNeighborActors;

	private:
		UWorld* EditorWorld = nullptr;
	};
}	// namespace UE::NearestNeighborModel
