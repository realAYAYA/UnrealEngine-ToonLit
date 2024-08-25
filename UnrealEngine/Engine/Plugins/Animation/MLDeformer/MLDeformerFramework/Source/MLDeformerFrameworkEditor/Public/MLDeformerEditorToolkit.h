// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MLDeformerTrainingModel.h"
#include "MLDeformerEditorModel.h"
#include "MLDeformerVizSettings.h"
#include "IHasPersonaToolkit.h"
#include "IPersonaPreviewScene.h"
#include "IPersonaViewport.h"
#include "PersonaAssetEditorToolkit.h"
#include "EditorUndoClient.h"
#include "Framework/MultiBox/MultiBox.h"
#include "Math/Color.h"
#include "Widgets/Notifications/SNotificationList.h"

struct FMenuEntryParams;
class FTabManager;
class IDetailsView;
class UMLDeformerAsset;
class SSimpleTimeSlider;

namespace UE::MLDeformer
{
	class SMLDeformerTimeline;
	class FMLDeformerApplicationMode;
	class SMLDeformerDebugSelectionWidget;

	namespace MLDeformerEditorModes
	{
		extern const FName Editor;
	}

	class FMLDeformerEditorToolkit;
	class MLDEFORMERFRAMEWORKEDITOR_API FToolsMenuExtender
	{
	public:
		virtual ~FToolsMenuExtender() {}
		virtual FMenuEntryParams GetMenuEntry(FMLDeformerEditorToolkit& Toolkit) const = 0;
		virtual TSharedPtr<FWorkflowTabFactory> GetTabSummoner(const TSharedRef<FMLDeformerEditorToolkit>& Toolkit) const { return nullptr; }
	};

	/**
	 * The ML Deformer asset editor toolkit.
	 * This is the editor that opens when you double click an ML Deformer asset.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerEditorToolkit :
		public FPersonaAssetEditorToolkit,
		public IHasPersonaToolkit,
		public FGCObject,
		public FEditorUndoClient,
		public FTickableEditorObject
	{
	public:
		friend class FMLDeformerApplicationMode;
		friend struct FMLDeformerVizSettingsTabSummoner;

		~FMLDeformerEditorToolkit();

		/** Initialize the asset editor. This will register the application mode, init the preview scene, etc. */
		void InitAssetEditor(
			const EToolkitMode::Type Mode,
			const TSharedPtr<IToolkitHost>& InitToolkitHost,
			UMLDeformerAsset* InDeformerAsset);

		// FAssetEditorToolkit overrides.
		virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
		virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
		virtual FName GetToolkitFName() const override;
		virtual FText GetBaseToolkitName() const override;
		virtual FText GetToolkitName() const override;
		virtual FLinearColor GetWorldCentricTabColorScale() const override;
		virtual FString GetWorldCentricTabPrefix() const override;
		// ~END FAssetEditorToolkit overrides.

		// FGCObject overrides.
		virtual FString GetReferencerName() const override							{ return TEXT("FMLDeformerEditorToolkit"); }
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		// ~END FGCObject overrides.

		// FTickableEditorObject overrides.
		virtual void Tick(float DeltaTime) override {};
		virtual ETickableTickType GetTickableTickType() const override				{ return ETickableTickType::Always; }
		virtual TStatId GetStatId() const override;
		// ~END FTickableEditorObject overrides.

		// IHasPersonaToolkit overrides.
		virtual TSharedRef<IPersonaToolkit> GetPersonaToolkit() const override;
		IPersonaToolkit* GetPersonaToolkitPointer() const							{ return PersonaToolkit.Get(); }
		// ~END IHasPersonaToolkit overrides.

		void SetVizSettingsDetailsView(TSharedPtr<IDetailsView> InDetailsView)		{ VizSettingsDetailsView = InDetailsView; }
		IDetailsView* GetVizSettingsDetailsView() const;

		IDetailsView* GetModelDetailsView() const;

		void SetTimeSlider(TSharedPtr<SMLDeformerTimeline> InTimeSlider);
		SMLDeformerTimeline* GetTimeSlider() const;

		UMLDeformerAsset* GetDeformerAsset() const									{ return DeformerAsset.Get(); }
		FMLDeformerEditorModel* GetActiveModel()									{ return ActiveModel.Get(); }
		TWeakPtr<FMLDeformerEditorModel> GetActiveModelPointer()					{ return TWeakPtr<FMLDeformerEditorModel>(ActiveModel); }
		const FMLDeformerEditorModel* GetActiveModel() const						{ return ActiveModel.Get(); }
		FMLDeformerApplicationMode* GetApplicationMode() const						{ return ApplicationMode; }
		TSharedPtr<IPersonaViewport> GetViewport() const							{ return PersonaViewport; }

		double CalcTimelinePosition() const;
		void OnTimeSliderScrubPositionChanged(double NewScrubTime, bool bIsScrubbing);
		void UpdateTimeSliderRange();
		void SetTimeSliderRange(double StartTime, double EndTime);

		/**
		 * Switch the editor to a given model type.
		 * @param ModelType The model type you want to switch to, for example something like: UNeuralMorphModel::StaticClass().
		 * @param bForceChange Force changing to this model? This will suppress any UI popups.
		 * @return Returns true in case we successfully switched model types, or otherwise false is returned.
		 */
		bool SwitchModelType(UClass* ModelType, bool bForceChange);

		/**
		 * Switch the editor's visualization mode.
		 * This essentially allows you to switch the UI between testing and training modes.
		 * @param Mode The mode to switch to.
		 */
		void SwitchVizMode(EMLDeformerVizMode Mode);

		bool Train(bool bSuppressDialogs);
		bool IsTrainButtonEnabled() const;
		bool IsTraining() const;

		/** Get the actor we want to debug, if any. Returns a nullptr when we don't want to debug anything. */
		AActor* GetDebugActor() const;

		/** Get the component space transforms of the actor we want to debug. Returns an empty array if GetDebugActor returns a nullptr. */
		TArray<FTransform> GetDebugActorComponentSpaceTransforms() const;

		void ZoomOnActors();

		TSharedPtr<SMLDeformerDebugSelectionWidget> GetDebugWidget() const { return DebugWidget; }

		static void AddToolsMenuExtender(TUniquePtr<FToolsMenuExtender> Extender);
		static TConstArrayView<TUniquePtr<FToolsMenuExtender>> GetToolsMenuExtenders();

	private:
		UE_DEPRECATED(5.3, "Please use the OnModelChanged that takes two parameters instead.")
		void OnModelChanged(int Index);
		void OnModelChanged(int Index, bool bForceChange);

		UE_DEPRECATED(5.3, "Please use the SwitchVizMode instead.")
		void OnVizModeChanged(EMLDeformerVizMode Mode);

		/* Toolbar related. */
		void ExtendToolbar();
		void FillToolbar(FToolBarBuilder& ToolbarBuilder);

		/** Preview scene setup. */
		void HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene);
		void HandleViewportCreated(const TSharedRef<IPersonaViewport>& InPersonaViewport);
		void HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView);
		void OnFinishedChangingDetails(const FPropertyChangedEvent& PropertyChangedEvent);

		/** Helpers. */
		void ShowNotification(const FText& Message, SNotificationItem::ECompletionState State, bool PlaySound) const;
		FText GetOverlayText() const;
		void OnSwitchedVisualizationMode();
		bool HandleTrainingResult(ETrainingResult TrainingResult, double TrainingDuration, bool& bOutUsePartiallyTrained, bool bSuppressDialogs, bool& bOutSuccess);
		FText GetActiveModelName() const;
		FText GetCurrentVizModeName() const;
		FText GetVizModeName(EMLDeformerVizMode Mode) const;
		void ShowNoModelsWarningIfNeeded();
		EVisibility GetDebuggingVisibility() const;

		TSharedRef<SWidget> GenerateModelButtonContents(TSharedRef<FUICommandList> InCommandList);
		TSharedRef<SWidget> GenerateVizModeButtonContents(TSharedRef<FUICommandList> InCommandList);
		TSharedRef<SWidget> GenerateToolsMenuContents(TSharedRef<FUICommandList> InCommandList);

	private:
		/** The persona toolkit. */	
		TSharedPtr<IPersonaToolkit> PersonaToolkit;

		/** Model details view. */
		TSharedPtr<IDetailsView> ModelDetailsView;

		/** Model viz settings details view. */
		TSharedPtr<IDetailsView> VizSettingsDetailsView;

		/** The timeline slider widget. */
		TSharedPtr<SMLDeformerTimeline> TimeSlider;

		/** The currently active editor model. */
		TSharedPtr<FMLDeformerEditorModel> ActiveModel;

		// Persona viewport.
		TSharedPtr<IPersonaViewport> PersonaViewport;

		/** The ML Deformer Asset. */
		TObjectPtr<UMLDeformerAsset> DeformerAsset;

		/** The widget where you select which actor to debug. */
		TSharedPtr<SMLDeformerDebugSelectionWidget> DebugWidget;

		/** The active application mode. */
		FMLDeformerApplicationMode* ApplicationMode = nullptr;

		/** Has the asset editor been initialized? */
		bool bIsInitialized = false;

		/** Are we currently in a training process? */
		bool bIsTraining = false;

		/** Extenders for Tools menu */
		static TArray<TUniquePtr<FToolsMenuExtender>> ToolsMenuExtenders;

		/** Mutex for adding extenders */
		static FCriticalSection ExtendersMutex;
	};
}	// namespace UE::MLDeformer
