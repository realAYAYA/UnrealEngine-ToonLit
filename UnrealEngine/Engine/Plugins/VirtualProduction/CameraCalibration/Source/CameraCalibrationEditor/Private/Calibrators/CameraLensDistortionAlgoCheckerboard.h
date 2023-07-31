// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CameraLensDistortionAlgo.h"

#include "LensFile.h"

#if WITH_OPENCV

#include <vector>

#include "OpenCVHelper.h"
#include "PreOpenCVHeaders.h"
#include "opencv2/calib3d.hpp"
#include "opencv2/imgproc.hpp"
#include "PostOpenCVHeaders.h"

#endif //WITH_OPENCV


#include "CameraLensDistortionAlgoCheckerboard.generated.h"

struct FGeometry;
struct FPointerEvent;

class ACameraCalibrationCheckerboard;
class FCameraCalibrationStepsController;
class ULensDistortionTool;
class SSimulcamViewport;

template <typename ItemType>
class SListView;

class SWidget;
class UMediaTexture;
class UWorld;
class FJsonObject;

template<typename OptionType>
class SComboBox;

namespace CameraLensDistortionAlgoCheckerboard
{
	class SCalibrationRowGenerator;
}

/** Holds camera information that can be used to add the samples */
USTRUCT()
struct FLensDistortionCheckerboardCameraData
{
	GENERATED_BODY()

	// True if the rest of the contents are valid.
	UPROPERTY()
	bool bIsValid = false;

	// Input focus from lens file evaluation data
	UPROPERTY()
	float InputFocus = 0.0f;

	// Input zoom from lens file evaluation data
	UPROPERTY()
	float InputZoom = 0.0f;
};

/** Holds information of the calibration row */
USTRUCT()
struct FLensDistortionCheckerboardRowData
{
	GENERATED_BODY()

	// Thumbnail to display in list
	TSharedPtr<SSimulcamViewport> Thumbnail;

	// Captured image containing checkerboard
	FImageView ImageView;

	// Index to display in list
	UPROPERTY()
	int32 Index = -1;

	// Checkerboard corners in 2d image pixel coordinates.
	UPROPERTY()
	TArray<FVector2D> Points2d;

	// Checkerboard corners in 3d local space.
	UPROPERTY()
	TArray<FVector> Points3d;

	// Which calibrator was used
	UPROPERTY()
	FString CalibratorName;

	// Number of corner rows in pattern
	UPROPERTY()
	int32 NumCornerRows = 0;

	// Number of corner columns in pattern
	UPROPERTY()
	int32 NumCornerCols = 0;

	// Side dimension in cm
	UPROPERTY()
	float SquareSideInCm = 0;

	// Width of image
	UPROPERTY()
	int32 ImageWidth = 0;

	// Height of image
	UPROPERTY()
	int32 ImageHeight = 0;

	// Holds information of the camera data for this sample
	UPROPERTY()
	FLensDistortionCheckerboardCameraData CameraData;
};

/** 
 * Implements a lens distortion calibration algorithm. It requires a checkerboard pattern
 */
UCLASS()
class UCameraLensDistortionAlgoCheckerboard : public UCameraLensDistortionAlgo
{
	GENERATED_BODY()
	
public:

	//~ Begin CameraLensDistortionAlgo
	virtual void Initialize(ULensDistortionTool* InTool) override;
	virtual void Shutdown() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual TSharedRef<SWidget> BuildUI() override;
	virtual FName FriendlyName() const override { return TEXT("Lens Distortion Checkerboard"); };
	virtual FName ShortName() const override { return TEXT("Checkerboard"); };
	virtual UMaterialInterface* GetOverlayMaterial() const override;
	virtual bool IsOverlayEnabled() const override { return bShouldShowOverlay; };
	virtual void OnDistortionSavedToLens() override;
	virtual bool GetLensDistortion(
		float& OutFocus,
		float& OutZoom,
		FDistortionInfo& OutDistortionInfo,
		FFocalLengthInfo& OutFocalLengthInfo,
		FImageCenterInfo& OutImageCenterInfo,
		TSubclassOf<ULensModel>& OutLensModel,
		double& OutError,
		FText& OutErrorMessage
	) override;
	virtual bool HasCalibrationData() const override;
	virtual void PreImportCalibrationData() override;
	virtual int32 ImportCalibrationRow(const TSharedRef<FJsonObject>& CalibrationRowObject, const FImage& RowImage) override;
	virtual void PostImportCalibrationData() override;
	virtual TSharedRef<SWidget> BuildHelpWidget() override;
	//~ End CameraLensDistortionAlgo

private:

	// SCalibrationRowGenerator will need access to the row structures below.
	friend class CameraLensDistortionAlgoCheckerboard::SCalibrationRowGenerator;

private:
	/** Version of the current calibration dataset */
	static const int DATASET_VERSION;

	/** The lens distortion tool controller */
	TWeakObjectPtr<ULensDistortionTool> Tool;

	/** The currently selected checkerboard object. */
	TWeakObjectPtr<ACameraCalibrationCheckerboard> Calibrator;

	/** Rows source for the CalibrationListView */
	TArray<TSharedPtr<FLensDistortionCheckerboardRowData>> CalibrationRows;

	/** Displays the list of calibration points that will be used to calculate the lens distortion */
	TSharedPtr<SListView<TSharedPtr<FLensDistortionCheckerboardRowData>>> CalibrationListView;

	/** Caches the last camera data.  Will hold last value before the media is paused */
	FLensDistortionCheckerboardCameraData LastCameraData;

	/** True if the coverage overlay should be shown */
	bool bShouldShowOverlay = false;

	/** True if a detection window should be shown after every capture */
	bool bShouldShowDetectionWindow = false;

	/** Texture into which detected chessboard corners from each calibration row are drawn */
	TObjectPtr<UTexture2D> CoverageTexture;

#if WITH_OPENCV
	/** OpenCV matrix used to draw chessboard corners */
	cv::Mat CvCoverage;
#endif

private:

	/** Builds the UI of the calibration device picker */
	TSharedRef<SWidget> BuildCalibrationDevicePickerWidget();

	/** Builds the UI for the user to select if they want to display the coverage overlay */
	TSharedRef<SWidget> BuildShowOverlayWidget();

	/** Builds the UI for the user to select if they want a corner detection window to be shown after every capture */
	TSharedRef<SWidget> BuildShowDetectionWidget();

	/** Builds the UI of the calibration points table */
	TSharedRef<SWidget> BuildCalibrationPointsTable();

	/** Builds the UI for the action buttons (RemoveLast, ClearAll) */
	TSharedRef<SWidget> BuildCalibrationActionButtons();

private:

	/** Returns the first checkerboard object in the scene that it can find */
	ACameraCalibrationCheckerboard* FindFirstCalibrator() const;

	/** Sets the calibrator object to be used. Updates the selection picker. */
	void SetCalibrator(ACameraCalibrationCheckerboard* InCalibrator);

	/** Returns the currently selected calibrator object. */
	ACameraCalibrationCheckerboard* GetCalibrator() const;

	/** Clears the list of calibration sample rows */
	void ClearCalibrationRows();

	/** Validates a new calibration point to determine if it should be added as a new sample row */
	bool ValidateNewRow(TSharedPtr<FLensDistortionCheckerboardRowData>& Row, FText& OutErrorMessage) const;

	/** Add a new calibration row from media texture and camera data */
	bool AddCalibrationRow(FText& OutErrorMessage);

	/** Returns the steps controller */
	FCameraCalibrationStepsController* GetStepsController() const;

	/** Update the coverage texture to reflect the current set of calibration rows */
	void RefreshCoverage();

	/** Export global session data to a .json file */
	void ExportSessionData();

	/** Export the row data to a json file */
	void ExportRow(TSharedPtr<FLensDistortionCheckerboardRowData> Row);
};
