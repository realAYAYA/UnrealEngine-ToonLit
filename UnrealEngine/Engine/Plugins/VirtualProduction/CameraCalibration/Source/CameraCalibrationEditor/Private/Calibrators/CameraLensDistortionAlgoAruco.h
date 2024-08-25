// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CameraLensDistortionAlgo.h"
#include "ImageCore.h"

#include "CameraLensDistortionAlgoAruco.generated.h"

class UCalibrationPointComponent;

template <typename ItemType>
class SListView;

struct FArucoCalibrationPoint;

/** Holds information of the calibration row */
USTRUCT()
struct FLensDistortionArucoRowData
{
	GENERATED_BODY()

	/** Index to display in list */
	UPROPERTY()
	int32 Index = -1;

	/** Array of aruco points (with their 3D and 2D corner locations) */
	UPROPERTY()
	TArray<FArucoCalibrationPoint> ArucoPoints;

	/** Pose of the tracked camera when the aruco points were detected */
	UPROPERTY()
	FTransform CameraPose = FTransform::Identity;

	/** Cached image of the media texture that contained the aruco markers */
	FImage MediaImage;
};

/** 
 * Implements a lens distortion calibration algorithm. It requires a Aruco pattern
 */
UCLASS()
class UCameraLensDistortionAlgoAruco : public UCameraLensDistortionAlgo
{
	GENERATED_BODY()
	
public:

	//~ Begin CameraLensDistortionAlgo
	virtual void Initialize(ULensDistortionTool* InTool) override;
	virtual void Shutdown() override;
	virtual bool SupportsModel(const TSubclassOf<ULensModel>& LensModel) const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual TSharedRef<SWidget> BuildUI() override;
	virtual FName FriendlyName() const override { return TEXT("Lens Distortion Aruco"); };
	virtual FName ShortName() const override { return TEXT("Aruco"); };
	virtual void OnDistortionSavedToLens() override;
	virtual FDistortionCalibrationTask BeginCalibration(FText& OutErrorMessage) override;
	virtual bool SupportsAsyncCalibration() override { return true; };
	virtual bool HasCalibrationData() const override;
	virtual void PreImportCalibrationData() override;
	virtual int32 ImportCalibrationRow(const TSharedRef<FJsonObject>& CalibrationRowObject, const FImage& RowImage) override;
	virtual void PostImportCalibrationData() override;
	//~ End CameraLensDistortionAlgo

private:
	/** Add a new calibration row from media texture and camera data */
	bool AddCalibrationRow(FText& OutErrorMessage);

	/** Clears the list of calibration sample rows */
	void ClearCalibrationRows();

	/** Sets the calibrator object to be used. Updates the selection picker. */
	void SetCalibrator(AActor* InCalibrator);

	/** Export global session data to a .json file */
	void ExportSessionData();

	/** Export the row data to a json file */
	void ExportRow(TSharedPtr<FLensDistortionArucoRowData> Row);

private:
	/** Builds the UI of the calibration device picker */
	TSharedRef<SWidget> BuildCalibratorPickerWidget();

	/** Builds the UI of the fix focal length width */
	TSharedRef<SWidget> BuildFocalLengthEstimateWidget();

	/** Builds the UI of the fix image center widget */
	TSharedRef<SWidget> BuildFixImageCenterWidget();

	/** Builds the UI of the calibration points table */
	TSharedRef<SWidget> BuildCalibrationRowListWidget();

	/** Builds the UI for the action buttons (RemoveLast, ClearAll) */
	TSharedRef<SWidget> BuildCalibrationActionButtons();

	/** Builds the UI of the calibration component picker */
	TSharedRef<SWidget> BuildCalibrationComponentPickerWidget();

	/** Builds the UI of the calibration component picker */
	TSharedRef<SWidget> BuildCalibrationComponentMenu();

	/** Gets the actor from the input asset data and caches it as the current calibrator */
	void OnCalibratorSelected(const FAssetData& AssetData);

	/** Returns true if the actor represented by the input asset data has any calibration point components. Used to filter calibrator actors in the picker. */
	bool DoesAssetHaveCalibrationComponent(const FAssetData& AssetData) const;

	/** Get an FAssetData representation of the current calibrator actor */
	FAssetData GetCalibratorAssetData() const;

	/** Get the text to display in the calibration component combobox. */
	FText GetCalibrationComponentMenuText() const;

	/** Add or remove the selected component from the set of active components and refresh the calibrator point combobox */
	void OnCalibrationComponentSelected(UCalibrationPointComponent* SelectedComponent);

	/** Returns true if the input component is in the set of active calibration components */
	bool IsCalibrationComponentSelected(UCalibrationPointComponent* SelectedComponent) const;

	/** Get the current value of the focal length estimate */
	TOptional<float> GetFocalLengthEstimate() const;

	/** Set the current value of the focal length estimate */
	void SetFocalLengthEstimate(float NewValue);

	/** Returns the state of the fix focal length checkbox */
	ECheckBoxState IsFixFocalLengthChecked() const;

	/** Changes the state of the fix focal length checkbox */
	void OnFixFocalLengthCheckStateChanged(ECheckBoxState NewState);

	/** Returns the state of the fix image center checkbox */
	ECheckBoxState IsFixImageCenterChecked() const;

	/** Changes the state of the fix image center checkbox */
	void OnFixImageCenterCheckStateChanged(ECheckBoxState NewState);

	/** Event triggered when the Clear button is pressed. Clears all of the captured calibration rows */
	FReply OnClearCalibrationRowsClicked();

	/** Event triggered when a key is pressed on the calibration list view */
	FReply OnCalibrationRowListKeyPressed(const FGeometry& Geometry, const FKeyEvent& KeyEvent);

private:
	/** Version of the current calibration dataset */
	static const int DATASET_VERSION;

	/** The lens distortion tool controller */
	TWeakObjectPtr<ULensDistortionTool> WeakTool;

	/** The currently selected calibrator object. It is expected to contain one or more UCalibrationPointComponent in it */
	TWeakObjectPtr<AActor> WeakCalibrator;

	/** Rows source for the CalibrationRowListWidget */
	TArray<TSharedPtr<FLensDistortionArucoRowData>> CalibrationRows;

	/** Displays the list of calibration points that will be used to calculate the lens distortion */
	TSharedPtr<SListView<TSharedPtr<FLensDistortionArucoRowData>>> CalibrationRowListWidget;

	/** If true, the solver will not solve for focal length, and will just use the input value */
	bool bFixFocalLength = false;

	/** If true, the solver will not solve for image center, and will just use the input value */
	bool bFixImageCenter = false;

	/** Estimate for the focal length to provide to the solver. If not set, user will be warned to set before continuing calibration */
	TOptional<float> FocalLengthEstimate;

	/** Container for the set of calibrator components selected in the component combobox */
	TArray<TWeakObjectPtr<UCalibrationPointComponent>> ActiveCalibratorComponents;
};
