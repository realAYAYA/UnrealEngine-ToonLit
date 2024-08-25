// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraLensDistortionAlgoAruco.h"

#include "AssetEditor/CameraCalibrationStepsController.h"
#include "AssetEditor/LensDistortionTool.h"
#include "CalibrationPointComponent.h"
#include "Camera/CameraActor.h"
#include "CameraCalibrationEditorLog.h"
#include "CameraCalibrationSolver.h"
#include "CameraCalibrationUtilsPrivate.h"
#include "Dialog/SCustomDialog.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ImageUtils.h"
#include "JsonObjectConverter.h"
#include "LensComponent.h"
#include "LensFile.h"
#include "Misc/MessageDialog.h"
#include "Models/AnamorphicLensModel.h"
#include "Models/SphericalLensModel.h"
#include "OpenCVHelper.h"
#include "UI/CameraCalibrationWidgetHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "UI/SFilterableActorPicker.h"
#include "UI/SImageTexture.h"


#define LOCTEXT_NAMESPACE "CameraLensDistortionAlgoAruco"

#if WITH_EDITOR
static TAutoConsoleVariable<bool> CVarFixExtrinsicsAruco(TEXT("LensDistortionAruco.FixExtrinsics"), false, TEXT("If true, the solver will fix the camera extrinsics to the user-provided camera poses"));
static TAutoConsoleVariable<bool> CVarFixZeroDistortionAruco(TEXT("LensDistortionAruco.FixZeroDistortion"), false, TEXT("If true, the solver will fix all distortion values to always be 0"));
static TAutoConsoleVariable<bool> CVarUseExtrinsicsGuessAruco(TEXT("LensDistortionAruco.UseExtrinsicsGuess"), false, TEXT("If true, the actual calibrator and camera poses will be used when running the solver"));
#endif

const int UCameraLensDistortionAlgoAruco::DATASET_VERSION = 1;

// String constants for import/export
namespace UE::CameraCalibration::Private::LensDistortionArucoExportFields
{
	static const FString Version(TEXT("Version"));
}

namespace UE::CameraCalibration::Private
{
	// Test the input array of weak pointers for validity, and return a list of valid pointers to calibration point components
	void GetValidActiveComponents(const TArray<TWeakObjectPtr<UCalibrationPointComponent>>& WeakComponents, TArray<UCalibrationPointComponent*>& ValidComponents)
	{
		ValidComponents.Reserve(WeakComponents.Num());
		for (const TWeakObjectPtr<UCalibrationPointComponent>& WeakComponent : WeakComponents)
		{
			if (UCalibrationPointComponent* Component = WeakComponent.Get())
			{
				ValidComponents.Add(Component);
			}
		}
	}

	// Generate a debug texture with the aruco points from the input row drawn onto it
	UTexture2D* CreateDebugTexture(TSharedPtr<FLensDistortionArucoRowData> Row)
	{
		if (!Row)
		{
			return nullptr;
		}

		TArray<FArucoMarker> DebugMarkers;
		DebugMarkers.Reserve(Row->ArucoPoints.Num());

		// Extract the data from the aruco points that OpenCV expects to draw the debug markers
		for (int32 MarkerIndex = 0; MarkerIndex < Row->ArucoPoints.Num(); ++MarkerIndex)
		{
			const FArucoCalibrationPoint& Marker = Row->ArucoPoints[MarkerIndex];

			FArucoMarker NewMarker;
			NewMarker.MarkerID = Marker.MarkerID;
			for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
			{
				NewMarker.Corners[CornerIndex] = Marker.Corners2D[CornerIndex];
			}

			DebugMarkers.Add(NewMarker);
		}

		UTexture2D* DebugTexture = FImageUtils::CreateTexture2DFromImage(Row->MediaImage);
		FOpenCVHelper::DrawArucoMarkers(DebugMarkers, DebugTexture);

		return DebugTexture;
	}

	// Open a dialog to display the input image 
	void DisplayDebugImageDialog(UTexture2D* Image)
	{
		if (Image)
		{
			TSharedRef<SCustomDialog> DebugImageDialog =
				SNew(SCustomDialog)
				.UseScrollBox(false)
				.Content()
				[
					SNew(SImageTexture, Image)
				]
				.Buttons
				({
					SCustomDialog::FButton(LOCTEXT("OkButton", "Ok")),
				});

			DebugImageDialog->Show();
		}
	}
}

namespace CameraLensDistortionAlgoAruco
{
	class SCalibrationRowGenerator : public SMultiColumnTableRow<TSharedPtr<FLensDistortionArucoRowData>>
	{

	public:
		SLATE_BEGIN_ARGS(SCalibrationRowGenerator) {}

		/** The list item for this row */
		SLATE_ARGUMENT(TSharedPtr<FLensDistortionArucoRowData>, CalibrationRowData)

		SLATE_END_ARGS()

		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
		{
			CalibrationRowData = Args._CalibrationRowData;

			SMultiColumnTableRow<TSharedPtr<FLensDistortionArucoRowData>>::Construct(
				FSuperRowType::FArguments()
				.Padding(1.0f),
				OwnerTableView
			);
		}

		//~Begin SMultiColumnTableRow
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (!CalibrationRowData)
			{
				return SNullWidget::NullWidget;
			}

			if (ColumnName == TEXT("Index"))
			{
				return SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.VAlign(EVerticalAlignment::VAlign_Center)
					[SNew(STextBlock).Text(FText::AsNumber(CalibrationRowData->Index))];
			}

			if (ColumnName == TEXT("NumMarkers"))
			{
				return SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.VAlign(EVerticalAlignment::VAlign_Center)
					[SNew(STextBlock).Text(FText::AsNumber(CalibrationRowData->ArucoPoints.Num()))];
			}

			if (ColumnName == TEXT("Image"))
			{
				// Generate a transient thumbnail texture to display in the tool
				const FImage& MediaImage = CalibrationRowData->MediaImage;

				constexpr int32 ResolutionDivider = 4;
				const FIntPoint ThumbnailSize = FIntPoint(MediaImage.SizeX / ResolutionDivider, MediaImage.SizeY / ResolutionDivider);

				FImage ThumbnailImage;
				FImageCore::ResizeTo(MediaImage, ThumbnailImage, ThumbnailSize.X, ThumbnailSize.Y, MediaImage.Format, MediaImage.GetGammaSpace());

				if (UTexture2D* Thumbnail = FImageUtils::CreateTexture2DFromImage(ThumbnailImage))
				{
					return SNew(SImageTexture, Thumbnail)
						.MinDesiredHeight(4 * FCameraCalibrationWidgetHelpers::DefaultRowHeight)
						.MaxDesiredHeight(4 * FCameraCalibrationWidgetHelpers::DefaultRowHeight);
				}
				else
				{
					const FString Text = FString::Printf(TEXT("Image Unavailable"));
					return SNew(STextBlock).Text(FText::FromString(Text));
				}
			}

			return SNullWidget::NullWidget;
		}
		//~End SMultiColumnTableRow


	private:
		TSharedPtr<FLensDistortionArucoRowData> CalibrationRowData;
	};

};

void UCameraLensDistortionAlgoAruco::Initialize(ULensDistortionTool* InTool)
{
	WeakTool = InTool;

	// Find all actors in the current level that have calibration point components
	TArray<AActor*> CalibratorActors;
	UE::CameraCalibration::Private::FindActorsWithCalibrationComponents(CalibratorActors);

	// If at least one actor was found, set the initial calibrator to the first one in the list
	if (CalibratorActors.Num() > 0)
	{
		SetCalibrator(CalibratorActors[0]);
	}
}

void UCameraLensDistortionAlgoAruco::Shutdown()
{
	WeakTool.Reset();
	WeakCalibrator.Reset();
}

bool UCameraLensDistortionAlgoAruco::SupportsModel(const TSubclassOf<ULensModel>& LensModel) const
{
	return ((LensModel == USphericalLensModel::StaticClass()) || (LensModel == UAnamorphicLensModel::StaticClass()));
}

void UCameraLensDistortionAlgoAruco::Tick(float DeltaTime)
{
}

bool UCameraLensDistortionAlgoAruco::OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// We only respond to left clicks
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return false;
	}

	// Attempt to add a new calibration row with data from the current media image, and display an error dialog to the user if any errors occur
	FText ErrorMessage;
	if (!AddCalibrationRow(ErrorMessage))
	{
		if (ErrorMessage.IsEmpty())
		{
			ErrorMessage = LOCTEXT("UnknownCalibrationError", "An error occurred. Please check the output log for more details.");
		}

		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
	}

	// If the media was currently paused, then resume it after gathering data from the current image
	if (ULensDistortionTool* LensDistortionTool = WeakTool.Get())
	{
		if (FCameraCalibrationStepsController* StepsController = LensDistortionTool->GetCameraCalibrationStepsController())
		{
			StepsController->Play();
		}
	}

	return true;
}

FDistortionCalibrationTask UCameraLensDistortionAlgoAruco::BeginCalibration(FText& OutErrorMessage)
{
	FDistortionCalibrationTask CalibrationTask = {};

	if (CalibrationRows.Num() < 1)
	{
		OutErrorMessage = LOCTEXT("NotEnoughCalibrationRowsError", "Could not initiate distortion calibration. At least 1 calibration row is required.");
		return CalibrationTask;
	}

	ULensDistortionTool* LensDistortionTool = WeakTool.Get();
	if (!ensureMsgf((LensDistortionTool != nullptr), TEXT("The Lens Distortion Tool was invalid.")))
	{
		return CalibrationTask;
	}

	FCameraCalibrationStepsController* StepsController = LensDistortionTool->GetCameraCalibrationStepsController();
	if (!ensureMsgf((StepsController != nullptr), TEXT("The Calibration Steps Controller was invalid.")))
	{
		return CalibrationTask;
	}

	ULensFile* LensFile = StepsController->GetLensFile();
	if (!ensureMsgf((LensFile != nullptr), TEXT("The Lens File was invalid.")))
	{
		return CalibrationTask;
	}

	// The evaluation focus and zoom values should not change during data collection, so all rows should have the same values.
	const FLensFileEvaluationInputs LensFileEvalInputs = StepsController->GetLensFileEvaluationInputs();
	const float Focus = LensFileEvalInputs.Focus;
	const float Zoom = LensFileEvalInputs.Zoom;

	// Initialize the focal length estimate in pixels
	const FIntPoint ImageSize = LensFile->CameraFeedInfo.GetDimensions();

	const float PixelAspect = LensFile->LensInfo.SqueezeFactor;

	if (FMath::IsNearlyZero(PixelAspect))
	{
		OutErrorMessage = LOCTEXT("PixelAspectZeroError", "The pixel aspect ratio of the CineCamera is zero, which is invalid.");
		return CalibrationTask;
	}

	const float PhysicalSensorWidth = StepsController->GetLensFileEvaluationInputs().Filmback.SensorWidth;
	const float DesqueezeSensorWidth = PhysicalSensorWidth * PixelAspect;

	if (FMath::IsNearlyZero(DesqueezeSensorWidth))
	{
		OutErrorMessage = LOCTEXT("SensorWidthZeroError", "One of the filmback dimensions of the CineCamera is zero, which is invalid.");
		return CalibrationTask;
	}

	if (!FocalLengthEstimate.IsSet() || FMath::IsNearlyZero(FocalLengthEstimate.GetValue()))
	{
		OutErrorMessage = LOCTEXT("FocalLengthEstimateError", "Enter a non-zero value (in mm) for the focal length estimate.");
		return CalibrationTask;
	}

	const float FocalLengthEstimateValue = FocalLengthEstimate.GetValue();
	const double Fx = (FocalLengthEstimateValue / DesqueezeSensorWidth) * ImageSize.X;

	// When operating on a desqueezed image, we expect our pixel aspect to be square, so horizontal and vertical field of view are assumed to be equal (i.e. Fx == Fy)
	FVector2D FocalLength = FVector2D(Fx, Fx);
	FVector2D ImageCenter = FVector2D((ImageSize.X - 1) * 0.5, (ImageSize.Y - 1) * 0.5);

	TArray<FObjectPoints> Samples3d;
	Samples3d.Reserve(CalibrationRows.Num());

	TArray<FImagePoints> Samples2d;
	Samples2d.Reserve(CalibrationRows.Num());

	TArray<FTransform> CameraPoses;
	CameraPoses.Reserve(CalibrationRows.Num());

	// Extract the 3D points, 2D points, and camera poses from each row to pass to the solver
	for (const TSharedPtr<FLensDistortionArucoRowData>& Row : CalibrationRows)
	{
		check(Row.IsValid());

		FObjectPoints Points3d;
		Points3d.Points.Reserve(Row->ArucoPoints.Num() * 4);

		FImagePoints Points2d;
		Points2d.Points.Reserve(Row->ArucoPoints.Num() * 4);

		for (const FArucoCalibrationPoint& Marker : Row->ArucoPoints)
		{
			for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
			{
				Points3d.Points.Add(Marker.Corners3D[CornerIndex]);

				const FVector2f& MarkerCorner2D = Marker.Corners2D[CornerIndex];
				Points2d.Points.Add(FVector2D(MarkerCorner2D.X, MarkerCorner2D.Y));
			}
		}

		Samples3d.Add(Points3d);
		Samples2d.Add(Points2d);
		CameraPoses.Add(Row->CameraPose);
	}

	ECalibrationFlags SolverFlags = ECalibrationFlags::None;

	// Aruco markers may be detected anywhere in the image, and there is no guarantee that they will be coplanar. 
	// The solver's initialization for focal length assumes all points in an image are coplanar. 
	// Therefore, we must provide an intrinsics guess to skip this initialization step in the solver. 
	EnumAddFlags(SolverFlags, ECalibrationFlags::UseIntrinsicGuess);

	if (CVarUseExtrinsicsGuessAruco.GetValueOnGameThread())
	{
		EnumAddFlags(SolverFlags, ECalibrationFlags::UseExtrinsicGuess);
	}

	if (CVarFixExtrinsicsAruco.GetValueOnAnyThread())
	{
		EnumAddFlags(SolverFlags, ECalibrationFlags::FixExtrinsics);
	}

	if (CVarFixZeroDistortionAruco.GetValueOnAnyThread())
	{
		EnumAddFlags(SolverFlags, ECalibrationFlags::FixZeroDistortion);
	}

	if (bFixFocalLength)
	{
		EnumAddFlags(SolverFlags, ECalibrationFlags::FixFocalLength);
	}

	if (bFixImageCenter)
	{
		EnumAddFlags(SolverFlags, ECalibrationFlags::FixPrincipalPoint);
	}

	const TSubclassOf<ULensModel> Model = LensFile->LensInfo.LensModel;

	UClass* SolverClass = LensDistortionTool->GetSolverClass();

	CalibrationTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [SolverClass, Model, Samples3d, Samples2d, ImageSize, FocalLength, ImageCenter, CameraPoses, PixelAspect, SolverFlags, Focus, Zoom]() mutable
		{
			ULensDistortionSolver* Solver = NewObject<ULensDistortionSolver>(GetTransientPackage(), SolverClass);

			FDistortionCalibrationResult Result = Solver->Solve(
				Samples3d,
				Samples2d,
				ImageSize,
				FocalLength,
				ImageCenter,
				CameraPoses,
				Model,
				PixelAspect,
				SolverFlags
			);

			// CalibrateCamera() returns focal length and image center in pixels, but the result is expected to be normalized by the image size
			Result.FocalLength.FxFy = Result.FocalLength.FxFy / ImageSize;
			Result.ImageCenter.PrincipalPoint = Result.ImageCenter.PrincipalPoint / ImageSize;

			// FZ inputs to LUT
			Result.EvaluatedFocus = Focus;
			Result.EvaluatedZoom = Zoom;

			return Result;
		});

	return CalibrationTask;
}

void UCameraLensDistortionAlgoAruco::OnDistortionSavedToLens()
{
	// Since the calibration result was saved, there is no further use for the current samples.
	ClearCalibrationRows();
}

bool UCameraLensDistortionAlgoAruco::AddCalibrationRow(FText& OutErrorMessage)
{
	// Detect the aruco dictionary to use by looking at the names of the calibration points on the selected calibrator actor
	AActor* CalibratorActor = WeakCalibrator.Get();
	if (!CalibratorActor)
	{
		OutErrorMessage = LOCTEXT("InvalidCalibrator", "Please pick a calibrator actor from the list that contains aruco calibration points.");
		return false;
	}

	EArucoDictionary Dictionary = UE::CameraCalibration::Private::GetArucoDictionaryForCalibrator(CalibratorActor);
	if (Dictionary == EArucoDictionary::None)
	{
		OutErrorMessage = LOCTEXT("InvalidArucoDictionary", "The names of the calibration point component(s) of the selected calibrator do not match any known aruco dictionaries.");
		return false;
	}

	ULensDistortionTool* LensDistortionTool = WeakTool.Get();
	if (!LensDistortionTool)
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to gather calibration data from the viewport. The Lens Distortion Tool was invalid."));
		return false;
	}

	FCameraCalibrationStepsController* StepsController = LensDistortionTool->GetCameraCalibrationStepsController();
	if (!StepsController)
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Failed to gather calibration data from the viewport. The Steps Controller was invalid."));
		return false;
	}

	// Read the image data from the current media source into a CPU-accessible array
	TArray<FColor> Pixels;
	FIntPoint Size = FIntPoint(0, 0);
	if (!StepsController->ReadMediaPixels(Pixels, Size, OutErrorMessage, ESimulcamViewportPortion::CameraFeed))
	{
		return false;
	}

	// Identify any aruco markers matching the current dictionary in the media image
	TArray<FArucoMarker> IdentifiedMarkers;
	bool bResult = FOpenCVHelper::IdentifyArucoMarkers(Pixels, Size, Dictionary, IdentifiedMarkers);

	if (!bResult || IdentifiedMarkers.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("NoArucoMarkersFound", "No aruco markers could be identified in the media image, or they did not match the aruco dictionary of the selected calibrator actor.");
		return false;
	}

	// For each identified marker, search the calibration components to find the subpoints matching that marker and the 3D location of each of its corners
	TArray<FArucoCalibrationPoint> ArucoCalibrationPoints;
	ArucoCalibrationPoints.Reserve(IdentifiedMarkers.Num());

	TArray<UCalibrationPointComponent*> ValidActiveCalibrationComponents;
	UE::CameraCalibration::Private::GetValidActiveComponents(ActiveCalibratorComponents, ValidActiveCalibrationComponents);

	for (const FArucoMarker& Marker : IdentifiedMarkers)
	{
		FArucoCalibrationPoint ArucoCalibrationPoint;
		if (UE::CameraCalibration::Private::FindArucoCalibrationPoint(ValidActiveCalibrationComponents, Dictionary, Marker, ArucoCalibrationPoint))
		{
			ArucoCalibrationPoints.Add(ArucoCalibrationPoint);
		}
	}

	ExportSessionData();

	// For each set of coplanar markers, add a new calibration row
	if (ArucoCalibrationPoints.Num() > 0)
	{
		TSharedPtr<FLensDistortionArucoRowData> NewRow = MakeShared<FLensDistortionArucoRowData>();
		NewRow->Index = LensDistortionTool->AdvanceSessionRowIndex();

		// Save the aruco points associated with this row 
		NewRow->ArucoPoints = ArucoCalibrationPoints;

		// Get the current pose of the selected CineCamera actor
		NewRow->CameraPose = FTransform::Identity;
		if (const ACameraActor* Camera = StepsController->GetCamera())
		{
			if (const UCameraComponent* CameraComponent = Camera->GetCameraComponent())
			{
				NewRow->CameraPose = CameraComponent->GetComponentToWorld();
			}
		}

		// Save an image view of the captured frame
		FImageView ImageView = FImageView(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8);
		ImageView.CopyTo(NewRow->MediaImage);

		CalibrationRows.Add(NewRow);

		// Export the data for this row to a .json file on disk
		ExportRow(NewRow);
	}

	// Notify the ListView of the new data
	if (CalibrationRowListWidget)
	{
		CalibrationRowListWidget->RequestListRefresh();
	}

	return true;
}

void UCameraLensDistortionAlgoAruco::ClearCalibrationRows()
{
	CalibrationRows.Empty();

	if (CalibrationRowListWidget)
	{
		CalibrationRowListWidget->RequestListRefresh();
	}
	
	// End the current calibration session (a new one will begin the next time a new row is added)
	if (ULensDistortionTool* LenDistortionTool = WeakTool.Get())
	{
		LenDistortionTool->EndCalibrationSession();
	}
}

void UCameraLensDistortionAlgoAruco::SetCalibrator(AActor* InCalibrator)
{
	WeakCalibrator = InCalibrator;

	if (!InCalibrator)
	{
		return;
	}

	// Find all of the calibration components of the input calibrator 
	constexpr uint32 NumInlineAllocations = 32;
	TArray<UCalibrationPointComponent*, TInlineAllocator<NumInlineAllocations>> CalibrationPoints;
	InCalibrator->GetComponents(CalibrationPoints);

	ActiveCalibratorComponents.Empty();

	// When a new calibrator is selected, all of its components will default to active
	for (UCalibrationPointComponent* CalibrationPoint : CalibrationPoints)
	{
		// Only add calibration point components that have an attached scene component
		if (CalibrationPoint && CalibrationPoint->GetAttachParent())
		{
			ActiveCalibratorComponents.Add(CalibrationPoint);
		}
	}
}

void UCameraLensDistortionAlgoAruco::ExportSessionData()
{
	using namespace UE::CameraCalibration::Private;

	if (ULensDistortionTool* LensDistortionTool = WeakTool.Get())
	{
		// Add all data to a json object that is needed to run this algorithm that is NOT part of a specific row
		TSharedPtr<FJsonObject> JsonSessionData = MakeShared<FJsonObject>();

		JsonSessionData->SetNumberField(LensDistortionArucoExportFields::Version, DATASET_VERSION);

		// Export the session data to a .json file
		LensDistortionTool->ExportSessionData(JsonSessionData.ToSharedRef());
	}
}

void UCameraLensDistortionAlgoAruco::ExportRow(TSharedPtr<FLensDistortionArucoRowData> Row)
{
	if (ULensDistortionTool* LensDistortionTool = WeakTool.Get())
	{
		if (const TSharedPtr<FJsonObject>& RowObject = FJsonObjectConverter::UStructToJsonObject<FLensDistortionArucoRowData>(Row.ToSharedRef().Get()))
		{
			// Export the row data to a .json file
			LensDistortionTool->ExportCalibrationRow(Row->Index, RowObject.ToSharedRef(), Row->MediaImage);
		}
	}
}

bool UCameraLensDistortionAlgoAruco::HasCalibrationData() const
{
	return (CalibrationRows.Num() > 0);
}

void UCameraLensDistortionAlgoAruco::PreImportCalibrationData()
{
	// Clear the current set of rows before importing new ones
	CalibrationRows.Empty();
}

int32 UCameraLensDistortionAlgoAruco::ImportCalibrationRow(const TSharedRef<FJsonObject>& CalibrationRowObject, const FImage& RowImage)
{
	// Create a new row to populate with data from the Json object
	TSharedPtr<FLensDistortionArucoRowData> NewRow = MakeShared<FLensDistortionArucoRowData>();

	// We enforce strict mode to ensure that every field in the UStruct of row data is present in the imported json.
	// If any fields are missing, it is likely the row will be invalid, which will lead to errors in the calibration.
	constexpr int64 CheckFlags = 0;
	constexpr int64 SkipFlags = 0;
	constexpr bool bStrictMode = true;
	if (FJsonObjectConverter::JsonObjectToUStruct<FLensDistortionArucoRowData>(CalibrationRowObject, NewRow.Get(), CheckFlags, SkipFlags, bStrictMode))
	{
		if (!RowImage.RawData.IsEmpty())
		{
			NewRow->MediaImage = RowImage;
		}

		CalibrationRows.Add(NewRow);
	}
	else
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("LensDistortionAruco algo failed to import calibration row because at least one field could not be deserialized from the json file."));
	}

	return NewRow->Index;
}

void UCameraLensDistortionAlgoAruco::PostImportCalibrationData()
{
	// Sort imported calibration rows by row index
	CalibrationRows.Sort([](const TSharedPtr<FLensDistortionArucoRowData>& LHS, const TSharedPtr<FLensDistortionArucoRowData>& RHS) { return LHS->Index < RHS->Index; });

	// Notify the ListView of the new data
	if (CalibrationRowListWidget)
	{
		CalibrationRowListWidget->RequestListRefresh();
	}
}

TSharedRef<SWidget> UCameraLensDistortionAlgoAruco::BuildUI()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot() // Calibrator picker
		.VAlign(EVerticalAlignment::VAlign_Top)
		.AutoHeight()
		.MaxHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[FCameraCalibrationWidgetHelpers::BuildLabelWidgetPair(LOCTEXT("Aruco", "Aruco"), BuildCalibratorPickerWidget())]

		+ SVerticalBox::Slot() // Calibrator component picker
		.VAlign(EVerticalAlignment::VAlign_Top)
		.AutoHeight()
		.MaxHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[FCameraCalibrationWidgetHelpers::BuildLabelWidgetPair(LOCTEXT("CalibratorComponents", "Calibrator Component(s)"), BuildCalibrationComponentPickerWidget())]

		+ SVerticalBox::Slot() // Solver Settings Title
		.Padding(0, 5)
		.AutoHeight()
		.VAlign(EVerticalAlignment::VAlign_Center)
		[
			SNew(SBox) // Constrain the height
			.MinDesiredHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
			.MaxDesiredHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
			[
				SNew(SBorder) // Background color of title
				.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
				.BorderBackgroundColor(FLinearColor::White)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(SOverlay)
					+ SOverlay::Slot() // Used to add left padding to the title
					.Padding(5, 0, 0, 0)
					[
						SNew(STextBlock) // Title text
						.Text(LOCTEXT("SolverSettings", "Solver Settings"))
						.TransformPolicy(ETextTransformPolicy::ToUpper)
						.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.BoldFont")))
						.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					]
				]
			]
		]

		+ SVerticalBox::Slot() // Focal Length Estimate
		.VAlign(EVerticalAlignment::VAlign_Top)
		.AutoHeight()
		.MaxHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[FCameraCalibrationWidgetHelpers::BuildLabelWidgetPair(LOCTEXT("FocalLengthEstimate", "Focal Length Estimate"), BuildFocalLengthEstimateWidget())]

		+ SVerticalBox::Slot() // Fix Image Center
		.VAlign(EVerticalAlignment::VAlign_Top)
		.AutoHeight()
		.MaxHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[FCameraCalibrationWidgetHelpers::BuildLabelWidgetPair(LOCTEXT("FixImageCenter", "Fix Image Center"), BuildFixImageCenterWidget())]

		+ SVerticalBox::Slot() // Calibration Rows
		.AutoHeight()
		.MaxHeight(12 * FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[BuildCalibrationRowListWidget()]

		+ SVerticalBox::Slot() // Action buttons (e.g. Clear)
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.AutoHeight()
		.Padding(0, 20)
		[BuildCalibrationActionButtons()];
}

TSharedRef<SWidget> UCameraLensDistortionAlgoAruco::BuildCalibratorPickerWidget()
{
	return SNew(SFilterableActorPicker)
		.OnSetObject_UObject(this, &UCameraLensDistortionAlgoAruco::OnCalibratorSelected)
		.OnShouldFilterAsset_UObject(this, &UCameraLensDistortionAlgoAruco::DoesAssetHaveCalibrationComponent)
		.ActorAssetData_UObject(this, &UCameraLensDistortionAlgoAruco::GetCalibratorAssetData);
}

TSharedRef<SWidget> UCameraLensDistortionAlgoAruco::BuildCalibrationComponentPickerWidget()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(SComboButton)
			.OnGetMenuContent_UObject(this, &UCameraLensDistortionAlgoAruco::BuildCalibrationComponentMenu)
			.ContentPadding(FMargin(4.0, 2.0))
			.ButtonContent()
			[
				SNew(STextBlock).Text_UObject(this, &UCameraLensDistortionAlgoAruco::GetCalibrationComponentMenuText)
			]
		];
}

TSharedRef<SWidget> UCameraLensDistortionAlgoAruco::BuildCalibrationComponentMenu()
{
	// Generate menu
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection("CalibrationComponents", LOCTEXT("CalibrationComponents", "Calibration Point Components"));

	if (AActor* Calibrator = WeakCalibrator.Get())
	{
		constexpr uint32 NumInlineAllocations = 32;
		TArray<UCalibrationPointComponent*, TInlineAllocator<NumInlineAllocations>> CalibrationPointComponents;
		Calibrator->GetComponents(CalibrationPointComponents);

		for (UCalibrationPointComponent* CalibratorComponent : CalibrationPointComponents)
		{
			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("ComponentLabel", "{0}"), FText::FromString(CalibratorComponent->GetName())),
				FText::Format(LOCTEXT("ComponentTooltip", "{0}"), FText::FromString(CalibratorComponent->GetName())),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateUObject(this, &UCameraLensDistortionAlgoAruco::OnCalibrationComponentSelected, CalibratorComponent),
					FCanExecuteAction(),
					FIsActionChecked::CreateUObject(this, &UCameraLensDistortionAlgoAruco::IsCalibrationComponentSelected, CalibratorComponent)
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}
	else
	{
		MenuBuilder.AddMenuEntry(
			FText::FromName(NAME_None),
			FText::FromName(NAME_None),
			FSlateIcon(),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked()
			),
			NAME_None,
			EUserInterfaceActionType::None
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> UCameraLensDistortionAlgoAruco::BuildFocalLengthEstimateWidget()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(0.8)
		.Padding(0, 0, 10, 0)
		[
			SNew(SNumericEntryBox<float>)
			.Value_UObject(this, &UCameraLensDistortionAlgoAruco::GetFocalLengthEstimate)
			.OnValueChanged_UObject(this, &UCameraLensDistortionAlgoAruco::SetFocalLengthEstimate)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(0.2)
		[
			SNew(SCheckBox)
			.Padding(FMargin(5, 0, 15, 0))
			.IsChecked_UObject(this, &UCameraLensDistortionAlgoAruco::IsFixFocalLengthChecked)
			.OnCheckStateChanged_UObject(this, &UCameraLensDistortionAlgoAruco::OnFixFocalLengthCheckStateChanged)
			[
				SNew(STextBlock).Text(LOCTEXT("FixText", "Fix?"))
			]
		];
}

TSharedRef<SWidget> UCameraLensDistortionAlgoAruco::BuildFixImageCenterWidget()
{
	return SNew(SCheckBox)
		.IsChecked_UObject(this, &UCameraLensDistortionAlgoAruco::IsFixImageCenterChecked)
		.OnCheckStateChanged_UObject(this, &UCameraLensDistortionAlgoAruco::OnFixImageCenterCheckStateChanged);
}

TSharedRef<SWidget> UCameraLensDistortionAlgoAruco::BuildCalibrationActionButtons()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot() // Button to clear all rows
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("ClearAll", "Clear All"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked_UObject(this, &UCameraLensDistortionAlgoAruco::OnClearCalibrationRowsClicked)
		];
}

TSharedRef<SWidget> UCameraLensDistortionAlgoAruco::BuildCalibrationRowListWidget()
{
	CalibrationRowListWidget = SNew(SListView<TSharedPtr<FLensDistortionArucoRowData>>)
		.ItemHeight(24)
		.ListItemsSource(&CalibrationRows)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow_Lambda([](TSharedPtr<FLensDistortionArucoRowData> InItem, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			return SNew(CameraLensDistortionAlgoAruco::SCalibrationRowGenerator, OwnerTable).CalibrationRowData(InItem);
		})
		.OnMouseButtonDoubleClick_Lambda([](TSharedPtr<FLensDistortionArucoRowData> Row)
		{
			// Open a dialog showing the media image of the selected row with debug aruco markers drawn on it
			UTexture2D* DebugImage = UE::CameraCalibration::Private::CreateDebugTexture(Row);
			UE::CameraCalibration::Private::DisplayDebugImageDialog(DebugImage);
		})
		.OnKeyDownHandler_UObject(this, &UCameraLensDistortionAlgoAruco::OnCalibrationRowListKeyPressed)
		.HeaderRow
		(
			SNew(SHeaderRow)

			+ SHeaderRow::Column("Index")
			.DefaultLabel(LOCTEXT("Index", "Index"))
			.FillWidth(0.1f)

			+ SHeaderRow::Column("NumMarkers")
			.DefaultLabel(LOCTEXT("NumMarkers", "Num Markers"))
			.FillWidth(0.2f)

			+ SHeaderRow::Column("Image")
			.DefaultLabel(LOCTEXT("Image", "Image"))
			.FillWidth(0.7f)
		);

	return CalibrationRowListWidget.ToSharedRef();
}

void UCameraLensDistortionAlgoAruco::OnCalibratorSelected(const FAssetData& AssetData)
{
	if (AssetData.IsValid())
	{
		SetCalibrator(Cast<AActor>(AssetData.GetAsset()));
	}
}

bool UCameraLensDistortionAlgoAruco::DoesAssetHaveCalibrationComponent(const FAssetData& AssetData) const
{
	if (const AActor* Actor = Cast<AActor>(AssetData.GetAsset()))
	{
		constexpr uint32 NumInlineAllocations = 32;
		TArray<UCalibrationPointComponent*, TInlineAllocator<NumInlineAllocations>> CalibrationPoints;
		Actor->GetComponents(CalibrationPoints);

		return (CalibrationPoints.Num() > 0);
	}

	return false;
}

FAssetData UCameraLensDistortionAlgoAruco::GetCalibratorAssetData() const
{
	return FAssetData(WeakCalibrator.Get(), true);
}

FText UCameraLensDistortionAlgoAruco::GetCalibrationComponentMenuText() const 
{
	if (ActiveCalibratorComponents.Num() > 1)
	{
		return LOCTEXT("MultipleCalibrationComponents", "Multiple Values");
	}
	else if (ActiveCalibratorComponents.Num() == 1)
	{
		if (UCalibrationPointComponent* CalibratorComponent = ActiveCalibratorComponents[0].Get())
		{
			return FText::FromString(CalibratorComponent->GetName());
		}
	}

	return LOCTEXT("NoCalibrationComponents", "None");
}

void UCameraLensDistortionAlgoAruco::OnCalibrationComponentSelected(UCalibrationPointComponent* SelectedComponent)
{
	if (IsCalibrationComponentSelected(SelectedComponent))
	{
		ActiveCalibratorComponents.Remove(SelectedComponent);
	}
	else
	{
		ActiveCalibratorComponents.Add(SelectedComponent);
	}
}

bool UCameraLensDistortionAlgoAruco::IsCalibrationComponentSelected(UCalibrationPointComponent* SelectedComponent) const
{
	return ActiveCalibratorComponents.Contains(SelectedComponent);
}

TOptional<float> UCameraLensDistortionAlgoAruco::GetFocalLengthEstimate() const
{
	return FocalLengthEstimate;
}

void UCameraLensDistortionAlgoAruco::SetFocalLengthEstimate(float NewValue)
{
	FocalLengthEstimate = NewValue;
}

ECheckBoxState UCameraLensDistortionAlgoAruco::IsFixFocalLengthChecked() const
{
	return bFixFocalLength ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void UCameraLensDistortionAlgoAruco::OnFixFocalLengthCheckStateChanged(ECheckBoxState NewState)
{
	bFixFocalLength = (NewState == ECheckBoxState::Checked);
}

ECheckBoxState UCameraLensDistortionAlgoAruco::IsFixImageCenterChecked() const
{
	return bFixImageCenter ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void UCameraLensDistortionAlgoAruco::OnFixImageCenterCheckStateChanged(ECheckBoxState NewState)
{
	bFixImageCenter = (NewState == ECheckBoxState::Checked);
}

FReply UCameraLensDistortionAlgoAruco::OnClearCalibrationRowsClicked()
{
	ClearCalibrationRows();
	return FReply::Handled();
}

FReply UCameraLensDistortionAlgoAruco::OnCalibrationRowListKeyPressed(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (!CalibrationRowListWidget)
	{
		return FReply::Unhandled();
	}

	if (KeyEvent.GetKey() == EKeys::Delete)
	{
		// Delete selected items
		const TArray<TSharedPtr<FLensDistortionArucoRowData>> SelectedItems = CalibrationRowListWidget->GetSelectedItems();

		for (const TSharedPtr<FLensDistortionArucoRowData>& SelectedItem : SelectedItems)
		{
			CalibrationRows.Remove(SelectedItem);

			// Delete the previously exported .json file for the row that is being deleted
			if (ULensDistortionTool* LensDistortionTool = WeakTool.Get())
			{
				LensDistortionTool->DeleteExportedRow(SelectedItem->Index);
			}
		}

		CalibrationRowListWidget->RequestListRefresh();
		return FReply::Handled();
	}
	else if (KeyEvent.GetModifierKeys().IsControlDown() && (KeyEvent.GetKey() == EKeys::A))
	{
		// Select all items
		CalibrationRowListWidget->SetItemSelection(CalibrationRows, true);
		return FReply::Handled();
	}
	else if (KeyEvent.GetKey() == EKeys::Escape)
	{
		// De-select all items
		CalibrationRowListWidget->ClearSelection();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
