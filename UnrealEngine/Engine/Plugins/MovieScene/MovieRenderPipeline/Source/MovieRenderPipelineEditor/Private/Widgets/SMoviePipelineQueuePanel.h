// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UMoviePipelineConfigBase;
class SMoviePipelineQueueEditor;
class SWindow;
class UMoviePipelineExecutorJob;
class UMoviePipelineExecutorShot;
class IDetailsView;
struct FAssetData;

/**
 * Outermost widget that is used for adding and removing jobs from the Movie Pipeline Queue Subsystem.
 */
class SMoviePipelineQueuePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMoviePipelineQueuePanel)
		: _BasePreset(nullptr)

		{}

		/*~ All following arguments are mutually-exclusive */
		/*-------------------------------------------------*/
		/** A preset asset to base the pipeline off */
		SLATE_ARGUMENT(UMoviePipelineConfigBase*, BasePreset)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnRenderLocalRequested();
	bool IsRenderLocalEnabled() const;
	FReply OnRenderRemoteRequested();
	bool IsRenderRemoteEnabled() const;

	/** When they want to edit the current configuration for the job */
	void OnEditJobConfigRequested(TWeakObjectPtr<UMoviePipelineExecutorJob> InJob, TWeakObjectPtr<UMoviePipelineExecutorShot> InShot);
	/** When an existing preset is chosen for the specified job. */
	void OnJobPresetChosen(TWeakObjectPtr<UMoviePipelineExecutorJob> InJob, TWeakObjectPtr<UMoviePipelineExecutorShot> InShot);
	void OnConfigUpdatedForJob(TWeakObjectPtr<UMoviePipelineExecutorJob> InJob, TWeakObjectPtr<UMoviePipelineExecutorShot> InShot, UMoviePipelineConfigBase* InConfig);
	void OnConfigUpdatedForJobToPreset(TWeakObjectPtr<UMoviePipelineExecutorJob> InJob, TWeakObjectPtr<UMoviePipelineExecutorShot> InShot, UMoviePipelineConfigBase* InConfig);
	void OnConfigWindowClosed();

	void OnSelectionChanged(const TArray<UMoviePipelineExecutorJob*>& InSelectedJobs);
	int32 GetDetailsViewWidgetIndex() const;
	bool IsDetailsViewEnabled() const;

	TSharedRef<SWidget> OnGenerateSavedQueuesMenu();
	bool OpenSaveDialog(const FString& InDefaultPath, const FString& InNewNameSuggestion, FString& OutPackageName);
	bool GetSavePresetPackageName(const FString& InExistingName, FString& OutName);
	void OnSaveAsAsset();
	void OnImportSavedQueueAssest(const FAssetData& InPresetAsset);

private:
	/** Allocates a transient preset so that the user can use the pipeline without saving it to an asset first. */
	//UMoviePipelineConfigBase* AllocateTransientPreset();


private:
	/** The main movie pipeline queue editor widget */
	TSharedPtr<SMoviePipelineQueueEditor> PipelineQueueEditorWidget;

	/** The details panel for the selected job(s) */
	TSharedPtr<IDetailsView> JobDetailsPanelWidget;

	TWeakPtr<SWindow> WeakEditorWindow;
	
	int32 NumSelectedJobs;
};