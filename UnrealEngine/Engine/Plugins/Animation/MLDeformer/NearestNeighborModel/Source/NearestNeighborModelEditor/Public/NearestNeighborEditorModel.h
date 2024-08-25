// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MLDeformerMorphModelEditorModel.h"
#include "NearestNeighborEditorModelActor.h"
#include "NearestNeighborModelHelpers.h"
#include "Types/SlateEnums.h"

class UMLDeformerComponent;
class UNearestNeighborModelSection;
class USkeletalMesh;
class UNearestNeighborModel;
class UNearestNeighborModelInstance;
class UNearestNeighborModelVizSettings;

namespace UE::NearestNeighborModel
{
	class FVertVizSelector;
	class FNearestNeighborEditorModelActor;
	class FVertexMapSelector;

	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborEditorModel
		: public UE::MLDeformer::FMLDeformerMorphModelEditorModel
	{
	public:
		using FSection = UNearestNeighborModelSection;
		using FMLDeformerSampler = UE::MLDeformer::FMLDeformerSampler;

		// We need to implement this static MakeInstance method.
		static FMLDeformerEditorModel* MakeInstance();

		// FGCObject overrides.
		virtual FString GetReferencerName() const override { return TEXT("FNearestNeighborEditorModel"); }
		// ~END FGCObject overrides.

		// FMLDeformerEditorModel overrides.
		virtual void Init(const InitSettings& Settings) override;
		virtual TSharedPtr<FMLDeformerSampler> CreateSamplerObject() const override;
		virtual void CreateActors(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene) override;
		virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
		virtual void InitInputInfo(UMLDeformerInputInfo* InputInfo) override;
		virtual ETrainingResult Train() override;
		virtual bool LoadTrainedNetwork() const override;
		virtual FMLDeformerTrainingInputAnim* GetTrainingInputAnim(int32 Index) const override;
		virtual void UpdateTimelineTrainingAnimList() override;
		virtual void OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent) override;
		virtual void OnPostTraining(ETrainingResult TrainingResult, bool bUsePartiallyTrainedWhenAborted) override;
		virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
		// ~END FMLDeformerEditorModel overrides.
		
		// UMLDeformerMorphModelEditorModel overrides.
		virtual bool IsMorphWeightClampingSupported() const override	{ return false; }	// We already do input clamping, so output clamping really isn't needed.
		// ~END UMLDeformerMorphModelEditorModel overrides.

		void OnUpdateClicked();
		void ClearReferences();

		FVertexMapSelector* GetVertexMapSelector() const;
		FVertVizSelector* GetVertVizSelector() const;

	protected:
		virtual void CreateSamplers() override;
		virtual bool IsAnimIndexValid(int32 Index) const override; 

	private:
		static constexpr int32 DefaultState = INDEX_NONE; 

		// Some helpers that cast to this model's variants of some classes.
		UNearestNeighborModel* GetCastModel() const;
		UNearestNeighborModelVizSettings* GetCastVizSettings() const;
		UMLDeformerComponent* GetTestMLDeformerComponent() const;
		USkeletalMeshComponent* GetTestSkeletalMeshComponent() const;
		UNearestNeighborModelInstance* GetTestNearestNeighborModelInstance() const;

		EOpFlag Update();
		EOpFlag CheckNetwork();
		EOpFlag UpdateNearestNeighborData();
		EOpFlag UpdateMorphDeltas();
		void ResetMorphTargets();
		void UpdateNearestNeighborIds();

		FNearestNeighborEditorModelActor* CreateNearestNeighborActor(UWorld* World) const;
		void UpdateNearestNeighborActor(FNearestNeighborEditorModelActor& Actor) const;
	
		FNearestNeighborEditorModelActor* NearestNeighborActor = nullptr;	// This should be only set in CreateActors() and is automatically deleted by the base class.
		TUniquePtr<FVertexMapSelector> VertexMapSelector;
		TUniquePtr<FVertVizSelector> VertVizSelector;
	};

	class FVertexMapSelector
	{
	public:
		void Update(const USkeletalMesh* SkelMesh);
		TArray<TSharedPtr<FString>>* GetOptions();
		void OnSelectionChanged(UNearestNeighborModelSection& Section, TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo) const;
		TSharedPtr<FString> GetSelectedItem(const UNearestNeighborModelSection& Section) const;
		FString GetVertexMapString(const UNearestNeighborModelSection& Section) const;
		bool IsValid() const;

	private:
		void Reset();
		TArray<TSharedPtr<FString>> Options;
		TMap<TSharedPtr<FString>, FString> VertexMapStrings;
		static TSharedPtr<FString> CustomString;
	};

	class FVertVizSelector
	{
	public:
		FVertVizSelector(UNearestNeighborModelVizSettings* InSettings);
		void Update(int32 NumSections);
		TArray<TSharedPtr<FString>>* GetOptions();
		void OnSelectionChanged(TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo) const;
		TSharedPtr<FString> GetSelectedItem() const;
		int32 GetSectionIndex(TSharedPtr<FString> Item) const;
		void SelectSection(int32 SectionIndex);

	private:
		TObjectPtr<UNearestNeighborModelVizSettings> Settings;
		TArray<TSharedPtr<FString>> Options;
	};
}	// namespace UE::NearestNeighborModel
