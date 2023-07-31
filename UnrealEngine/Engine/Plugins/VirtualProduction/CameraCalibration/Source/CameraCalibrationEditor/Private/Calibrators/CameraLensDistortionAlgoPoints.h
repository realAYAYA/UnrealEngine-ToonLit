// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CameraLensDistortionAlgo.h"

#include "LensFile.h"

#include "CameraLensDistortionAlgoPoints.generated.h"

template <typename ItemType>
class SListView;

class FJsonObject;

namespace CameraLensDistortionAlgoPoints
{
	class SCalibrationRowGenerator;
};

/** Holds information of the identified calibrator 2d point for a given sample of a 2d-3d correlation */
USTRUCT()
struct FLensDistortionPointsCalibratorPointData
{
	GENERATED_BODY()

	// True if the rest of the contents are valid.
	UPROPERTY()
	bool bIsValid = false;

	// The name of the 3d calibrator point, as defined in the mesh
	UPROPERTY()
	FString Name;

	// 3D location of the calibrator point
	UPROPERTY()
	FVector Point3d = FVector::ZeroVector;

	// 2D location of the calibrator point
	UPROPERTY()
	FVector2D Point2d = FVector2D::ZeroVector;
};

/** Holds information of the camera pose for a given sample of a 2d-3d correlation */
USTRUCT()
struct FLensDistortionPointsCameraData
{
	GENERATED_BODY()

	// True if the rest of the contents are valid.
	UPROPERTY()
	bool bIsValid = false;

	// The unique id of the camera object. Used to detect camera selection changes during a calibration session.
	UPROPERTY()
	uint32 UniqueId = INDEX_NONE;

	// The unique id of the calibrator object. Used to detect calibrator selection changes during a calibration session.
	UPROPERTY()
	uint32 CalibratorUniqueId = INDEX_NONE;

	// Input focus from lens file evaluation data
	UPROPERTY()
	float InputFocus = 0.0f;

	// Input zoom from lens file evaluation data
	UPROPERTY()
	float InputZoom = 0.0f;
};

/** Holds information of the calibrator 3d point for a given sample of a 2d-3d correlation */
USTRUCT()
struct FLensDistortionPointsRowData
{
	GENERATED_BODY()
	
	// Index to display in list
	UPROPERTY()
	int32 Index = INDEX_NONE;

	// The 3D-2D point correspondences for each of the calibrator points in the chosen calibrator for a single pose
	UPROPERTY()
	FLensDistortionPointsCalibratorPointData CalibratorPointData;

	// Holds information of the camera data for this sample
	UPROPERTY()
	FLensDistortionPointsCameraData CameraData;

	// Index of the calibration pattern this row is associated with
	UPROPERTY()
	uint32 PatternIndex = INDEX_NONE;
};

/**
 * Implements a lens distortion calibration algorithm. It uses 3d points (UCalibrationPointComponent)
 * specified in a selectable calibrator with features that the user can identify by clicking on the
 * simulcam viewport, and after enough points have been identified, it can calculate the lens distortion
 * and camera intrinsics that need to be applied to the associated camera in order to align its CG with 
 * the live action media plate.
 *
 * Limitations:
 * - Only supports Brown–Conrady model lens model (FSphericalDistortionParameters)
 * - The camera or the lens should not be moved during the calibration of each lens distortion sample.
 */
UCLASS()
class UCameraLensDistortionAlgoPoints : public UCameraLensDistortionAlgo
{
	GENERATED_BODY()

public:
	//~ Begin UCameraLensDistortionAlgo
	virtual void Initialize(ULensDistortionTool* InLensDistortionTool) override;
	virtual void Shutdown() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual TSharedRef<SWidget> BuildUI() override;
	virtual FName FriendlyName() const override { return TEXT("Lens Distortion Points Method"); };
	virtual FName ShortName() const override { return TEXT("Points"); };
	virtual void OnDistortionSavedToLens() override;

	virtual bool GetLensDistortion(
		float& OutFocus,
		float& OutZoom,
		FDistortionInfo& OutDistortionInfo,
		FFocalLengthInfo& OutFocalLengthInfo,
		FImageCenterInfo& OutImageCenterInfo,
		TSubclassOf<ULensModel>& OutLensModel,
		double& OutError,
		FText& OutErrorMessage) override;

	virtual bool HasCalibrationData() const override;
	virtual void PreImportCalibrationData() override;
	virtual void ImportSessionData(const TSharedRef<FJsonObject>& SessionDataObject) override;
	virtual int32 ImportCalibrationRow(const TSharedRef<FJsonObject>& CalibrationRowObject, const FImage& RowImage) override;
	virtual void PostImportCalibrationData() override;
	virtual TSharedRef<SWidget> BuildHelpWidget() override;
	//~ End UCameraLensDistortionAlgo

protected:

	// SCalibrationRowGenerator will need access to the row structures below.
	friend class CameraLensDistortionAlgoPoints::SCalibrationRowGenerator;

	/** Holds item information a given calibrator point in the calibrator model */
	struct FCalibratorPointData
	{
		FCalibratorPointData(FString& InName)
			: Name(InName)
		{}

		/** Name of the calibrator 3d point, as defined in the mesh */
		FString Name;
	};

protected:
	/** Version of the current calibration dataset */
	static const int DATASET_VERSION;

	/** The lens distortion tool */
	TWeakObjectPtr<ULensDistortionTool> LensDistortionTool;

	/** The currently selected calibrator object. It is expected to contain one or more UCalibrationPointComponent in it */
	TWeakObjectPtr<AActor> Calibrator;

	/** Lists the calibrator points found in the selected calibrator object */
	TArray<TSharedPtr<FCalibratorPointData>> CalibratorPoints;

	/** The current calibrator point that will be used to generate the next calibration row */
	TSharedPtr<FCalibratorPointData> CurrentCalibratorPoint;

	/** Rows source for the CalibrationListView */
	TArray<TSharedPtr<FLensDistortionPointsRowData>> CalibrationRows;

	/** Displays the list of calibration points that will be used to calculate the lens distortion */
	TSharedPtr<SListView<TSharedPtr<FLensDistortionPointsRowData>>> CalibrationListView;

	/** Caches the last calibrator point 3d location.  Will hold last value before the lens distortion tool is paused */
	TArray<FLensDistortionPointsCalibratorPointData> LastCalibratorPoints;

	/** Caches the last camera data.  Will hold last value before the lens distortion tool is paused */
	FLensDistortionPointsCameraData LastCameraData;

	/** Instructs the inline allocation policy how many elements to allocate in a single allocation */
	static constexpr uint32 NumInlineAllocations = 32;

	/** The current index of the calibration pattern being captured, used for grouping calibration rows together into meaningful patterns */
	uint32 CurrentPatternIndex = 0;

	/** The value of the current focal length of the lens (in mm), used to set a guess for the camera intrinsics fx/fy */
	float CurrentFocalLengthMM = 0.0f;

protected:

	/** Returns the first calibrator object in the scene that it can find */
	AActor* FindFirstCalibrator() const;

	/** Sets the calibrator object to be used. Updates the selection picker. */
	void SetCalibrator(AActor* InCalibrator);

	/** Selects the next available UCalibrationPointComponent of the currently selected calibrator object. Returns true when it wraps around */
	bool AdvanceCalibratorPoint();

	/**
	 * Retrieves by name the calibration point data of the currently selected calibrator.
	 *
	 * @param Name The name of the point (namespaced if it is a subpoint).
	 * @param CalibratorPointCache This data structure will be populated with the information found.
	 *
	 * @return True if successful.
	 */
	bool GetCalibratorPointCacheFromName(const FString& Name, FLensDistortionPointsCalibratorPointData& CalibratorPointCache) const;

	/** Validates a new calibration point to determine if it should be added as a new sample row */
	bool ValidateNewRow(TSharedPtr<FLensDistortionPointsRowData>& Row, FText& OutErrorMessage) const;

	/** Clears the list of calibration sample rows */
	void ClearCalibrationRows();

	/** Deletes the selected calibration rows */
	void DeleteSelectedCalibrationRows();

	/** Updates the selection in the list of calibration rows so that patterns are always grouped together */
	void OnCalibrationRowSelectionChanged(TSharedPtr<FLensDistortionPointsRowData> SelectedRow, ESelectInfo::Type SelectInfo);

	/** Export global session data to a .json file */
	void ExportSessionData();

	/** Export the row data to a .json file */
	void ExportRow(TSharedPtr<FLensDistortionPointsRowData> Row);

	/** Builds the UI of the calibration device picker */
	TSharedRef<SWidget> BuildCalibrationDevicePickerWidget();

	/** Buils the UI for selecting the current calibrator point */
	TSharedRef<SWidget> BuildCurrentCalibratorPointLabel();

	/** Buils the UI for setting the current focal length (in mm) */
	TSharedRef<SWidget> BuildCurrentFocalLengthWidget();

	/** Builds the UI of the calibration points table */
	TSharedRef<SWidget> BuildCalibrationPointsTable();

	/** Builds the UI for the action buttons (RemoveLast, ClearAll) */
	TSharedRef<SWidget> BuildCalibrationActionButtons();
};
