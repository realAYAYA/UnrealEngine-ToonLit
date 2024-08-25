// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "CameraCalibrationStep.h"
#include "CameraCalibrationTypes.h"
#include "CameraLensDistortionAlgo.h"
#include "Engine/World.h"
#include "ImageCore.h"
#include "SLensDistortionToolPanel.h"
#include "Widgets/Input/SButton.h"

#include "LensDistortionTool.generated.h"

struct FGeometry;
struct FPointerEvent;

class FJsonObject;
class UCameraLensDistortionAlgo;
class ULensModel;

/** Data associated with a lens distortion calibration session */
struct FLensDistortionSessionInfo
{
	/** The date/time when the current calibration session started */
	FDateTime StartTime;

	/** The index of the next row in the current calibration session */
	int32 RowIndex = -1;

	/** True if a calibration session is currently in progress */
	bool bIsActive = false;
};

/**
 * ULensDistortionTool is the controller for the lens distortion panel.
 */
UCLASS()
class ULensDistortionTool : public UCameraCalibrationStep
{
	GENERATED_BODY()

public:

	//~ Begin UCameraCalibrationStep interface
	virtual void Initialize(TWeakPtr<FCameraCalibrationStepsController> InCameraCalibrationStepController) override;
	virtual void Shutdown() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual TSharedRef<SWidget> BuildUI() override;
	virtual FName FriendlyName() const  override { return TEXT("Lens Distortion"); };
	virtual bool DependsOnStep(UCameraCalibrationStep* Step) const override;
	virtual void Activate() override;
	virtual void Deactivate() override;
	virtual FCameraCalibrationStepsController* GetCameraCalibrationStepsController() const override;
	virtual bool IsActive() const override;
	virtual UMaterialInstanceDynamic* GetOverlayMID() const override;
	virtual bool IsOverlayEnabled() const override;
	//~ End UCameraCalibrationStep interface

	/** Get the currently selected lens distortion solver class */
	UClass* GetSolverClass();

	/** Set the lens distortion solver class to use */
	void SetSolverClass(UClass* InSolverClass);

	/** Selects the algorithm by name */
	void SetAlgo(const FName& AlgoName);

	/** Resets the current algo to none */
	void ResetAlgo();

	/** Returns the currently selected algorithm */
	UCameraLensDistortionAlgo* GetAlgo() const;

	/** Returns available algorithm names */
	TArray<FName> GetAlgos() const;

	/** Called by the UI when the user wants to save the calibration data that the current algorithm is providing */
	void OnSaveCurrentCalibrationData();

	/** Triggered when the Lens Model used by the LensFile changes, allowing the tool to update its list of supported algos */
	void OnLensModelChanged(const TSubclassOf<ULensModel>& LensModel);

	/** Initiate a new calibration session (if one is not already active) */
	void StartCalibrationSession();

	/** End the current calibration session */
	void EndCalibrationSession();

	/** Increments the session index and returns its new value */
	uint32 AdvanceSessionRowIndex();

	/** Intended to be called by one of the algo objects. Exports the data needed by the algo for the current session to a .json file on disk. */
	void ExportSessionData(const TSharedRef<FJsonObject>& SessionDataObject);

	/** Intended to be called by one of the algo objects. Exports the data associated with a single calibration row to a .json file on disk. */
	void ExportCalibrationRow(int32 RowIndex, const TSharedRef<FJsonObject>& RowObject, const FImageView& RowImage = FImageView());

	/** Intended to be called by one of the algo objects. Deletes the .json file with the input row index that was previously exported for this session. */
	void DeleteExportedRow(const int32& RowIndex) const;

	/** Import a saved calibration data set from disk */
	void ImportCalibrationDataset();

private:
	/** Get the directory for the current session */
	FString GetSessionSaveDir() const;

	/** Get the filename of a row with the input index */
	FString GetRowFilename(int32 RowIndex) const;

	/** Update the list of algos that support the current Lens Model */
	void UpdateAlgoMap(const TSubclassOf<ULensModel>& LensModel);

	/** Build the UI for the progress window, including the window content and buttons */
	void BuildProgressWindowWidgets();

	/** Called in response to user clicking "Cancel" button in the progress window to cancel a currently running calibration task */
	FReply OnCancelPressed();

	/** Called in response to user clicking "Ok" button in the progress window to save the result from a finished calibration task */
	FReply OnOkPressed();

	/** Save the results from a finished distortion calibration to the Lens File */
	void SaveCalibrationResult();

public:
	/** Stores info about the current calibration session of this tool */
	FLensDistortionSessionInfo SessionInfo;

private:

	/** Pointer to the calibration steps controller */
	TWeakPtr<FCameraCalibrationStepsController> CameraCalibrationStepsController;

	/** The currently selected algorithm */
	UPROPERTY(Transient)
	TObjectPtr<UCameraLensDistortionAlgo> CurrentAlgo;

	/** The solver class that will be used by the algos when creating a solver to calibrate for distortion */
	TObjectPtr<UClass> SolverClass;

	/** Holds the registered camera lens distortion algos */
	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<UCameraLensDistortionAlgo>> AlgosMap;

	/** Holds a subset of the registered algos that support the current Lens Model */
	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<UCameraLensDistortionAlgo>> SupportedAlgosMap;

	/** Map of algo names to overlay MIDs used by those algos */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UMaterialInstanceDynamic>> AlgoOverlayMIDs;

	/** True if this tool is the active one in the panel */
	bool bIsActive = false;

	/** UI Widget for this Tool */
	TSharedPtr<SLensDistortionToolPanel> DistortionWidget;

	/** UI Widget for the calibration task progress window */
	TSharedPtr<SWindow> ProgressWindow;

	/** UI Widget for the text to be displayed in the progress window */
	TSharedPtr<STextBlock> ProgressTextWidget;

	/** UI Widget for the "Ok" button in the progress window */
	TSharedPtr<SButton> OkayButton;

	/** An asynchronous task handle. When valid, the tool will poll its state to determine when the task has completed, and then extract the calibration result from this task handle. */
	FDistortionCalibrationTask CalibrationTask;

	/** The result from the most recently completed distortion calibration. It is saved as a member variable because the result is not saved immediately, but only if the user confirms that it should be saved. */
	FDistortionCalibrationResult CalibrationResult;
};
