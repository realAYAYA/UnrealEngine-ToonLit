// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraLensDistortionAlgoCheckerboard.h"

#include "AssetEditor/CameraCalibrationStepsController.h"
#include "AssetEditor/LensDistortionTool.h"
#include "AssetEditor/SSimulcamViewport.h"
#include "CalibrationPointComponent.h"
#include "Camera/CameraActor.h"
#include "CameraCalibrationCheckerboard.h"
#include "CameraCalibrationEditorLog.h"
#include "CameraCalibrationSolver.h"
#include "CameraCalibrationUtilsPrivate.h"
#include "Dom/JsonObject.h"
#include "EditorFontGlyphs.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "JsonObjectConverter.h"
#include "LensComponent.h"
#include "LensFile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/MessageDialog.h"
#include "Models/AnamorphicLensModel.h"
#include "Models/SphericalLensModel.h"
#include "OpenCVHelper.h"
#include "PropertyCustomizationHelpers.h"
#include "TextureResource.h"
#include "UI/CameraCalibrationWidgetHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"


#define LOCTEXT_NAMESPACE "CameraLensDistortionAlgoCheckerboard"

#if WITH_EDITOR
static TAutoConsoleVariable<bool> CVarUseIntrinsicsGuess(TEXT("LensDistortionCheckerboard.UseIntrinsicsGuess"), true, TEXT("If true, the solver initializes the camera intrinsics to a user-provided estimate. Otherwise, the solver will compute the initial values."));
static TAutoConsoleVariable<bool> CVarFixExtrinsics(TEXT("LensDistortionCheckerboard.FixExtrinsics"), false, TEXT("If true, the solver will fix the camera extrinsics to the user-provided camera poses"));
static TAutoConsoleVariable<bool> CVarFixZeroDistortion(TEXT("LensDistortionCheckerboard.FixZeroDistortion"), false, TEXT("If true, the solver will fix all distortion values to always be 0"));
static TAutoConsoleVariable<bool> CVarUseExtrinsicsGuess(TEXT("LensDistortionCheckerboard.UseExtrinsicsGuess"), false, TEXT("If true, the actual checkerboard and camera poses will be used when running the solver"));
#endif

const int UCameraLensDistortionAlgoCheckerboard::DATASET_VERSION = 1;

// String constants for import/export
namespace UE::CameraCalibration::Private::LensDistortionCheckerboardExportFields
{
	static const FString Version(TEXT("Version"));
}

namespace UE::CameraCalibration::Private
{
	void AdjustCorners(TArray<FVector2f>& Corners, FIntPoint TextureSize, FIntPoint ImageSize)
	{
		// It is possible that the size of the debug texture where we want to draw the checkerboard corners is different than the size of the image where the checkerboard corners were found.
		// Before drawing, all of the corner positions should be shifted to account for the difference in size.
		const FVector2f TopLeftCorner = (TextureSize - ImageSize) / 2.0f;

		for (FVector2f& Corner : Corners)
		{
			Corner += TopLeftCorner;
		}
	}
}

namespace CameraLensDistortionAlgoCheckerboard
{
	class SCalibrationRowGenerator
		: public SMultiColumnTableRow<TSharedPtr<FLensDistortionCheckerboardRowData>>
	{

	public:
		SLATE_BEGIN_ARGS(SCalibrationRowGenerator) {}

		/** The list item for this row */
		SLATE_ARGUMENT(TSharedPtr<FLensDistortionCheckerboardRowData>, CalibrationRowData)

		SLATE_END_ARGS()

		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
		{
			CalibrationRowData = Args._CalibrationRowData;

			SMultiColumnTableRow<TSharedPtr<FLensDistortionCheckerboardRowData>>::Construct(
				FSuperRowType::FArguments()
				.Padding(1.0f),
				OwnerTableView
			);
		}

		//~Begin SMultiColumnTableRow
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == TEXT("Index"))
			{
				return SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.VAlign(EVerticalAlignment::VAlign_Center)
					[SNew(STextBlock).Text(FText::AsNumber(CalibrationRowData->Index))];
			}

			if (ColumnName == TEXT("Image"))
			{
				if (CalibrationRowData->Thumbnail.IsValid())
				{
					check((CalibrationRowData->ImageHeight) > 0 && (CalibrationRowData->ImageWidth > 0));

					const float AspectRatio = float(CalibrationRowData->ImageWidth) / float(CalibrationRowData->ImageHeight);

					return SNew(SBox)
						.MinAspectRatio(AspectRatio)
						.MaxAspectRatio(AspectRatio)
						.MinDesiredHeight(4 * FCameraCalibrationWidgetHelpers::DefaultRowHeight)
						.MaxDesiredHeight(4 * FCameraCalibrationWidgetHelpers::DefaultRowHeight)
						[CalibrationRowData->Thumbnail.ToSharedRef()];
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
		TSharedPtr<FLensDistortionCheckerboardRowData> CalibrationRowData;
	};

};

void UCameraLensDistortionAlgoCheckerboard::Initialize(ULensDistortionTool* InTool)
{
	Tool = InTool;

	// Guess which calibrator to use.
	SetCalibrator(FindFirstCalibrator());

	// Initialize coverage texture
	FCameraCalibrationStepsController* StepsController = Tool->GetCameraCalibrationStepsController();

	if (!ensure(StepsController))
	{
		return;
	}

	const FIntPoint Size = StepsController->GetCompRenderResolution();

	CoverageTexture = UTexture2D::CreateTransient(Size.X, Size.Y, EPixelFormat::PF_B8G8R8A8);
	UE::CameraCalibration::Private::ClearTexture(CoverageTexture);

	if (UMaterialInstanceDynamic* OverlayMID = Tool->GetOverlayMID())
	{
		OverlayMID->SetTextureParameterValue(FName(TEXT("CoverageTexture")), CoverageTexture);
	}
}

void UCameraLensDistortionAlgoCheckerboard::Shutdown()
{
	Tool.Reset();
	CalibrationRows.Reset();
}

bool UCameraLensDistortionAlgoCheckerboard::SupportsModel(const TSubclassOf<ULensModel>& LensModel) const
{
	return ((LensModel == USphericalLensModel::StaticClass()) || (LensModel == UAnamorphicLensModel::StaticClass()));
}

void UCameraLensDistortionAlgoCheckerboard::Tick(float DeltaTime)
{
	if (!ensure(Tool.IsValid()))
	{
		return;
	}

	FCameraCalibrationStepsController* StepsController = Tool->GetCameraCalibrationStepsController();

	if (!ensure(StepsController))
	{
		return;
	}

	// If the resolution of the simulcam comp has changed, update the coverage texture to be the correct size
	const FIntPoint Size = StepsController->GetCompRenderResolution();
	if (CoverageTexture && ((CoverageTexture->GetSizeX() != Size.X) || (CoverageTexture->GetSizeY() != Size.Y)))
	{
		CoverageTexture = UTexture2D::CreateTransient(Size.X, Size.Y, EPixelFormat::PF_B8G8R8A8);
		RefreshCoverage();
	}

	// If not paused, cache calibrator 3d point position
	if (!StepsController->IsPaused())
	{
		// Cache camera data
		do
		{
			LastCameraData.bIsValid = false;

			const FLensFileEvaluationInputs LensFileEvalInputs = StepsController->GetLensFileEvaluationInputs();

			// We require lens evaluation data.
			if (LensFileEvalInputs.bIsValid)
			{
				LastCameraData.InputFocus = LensFileEvalInputs.Focus;
				LastCameraData.InputZoom = LensFileEvalInputs.Zoom;
			}

			const ACameraActor* Camera = StepsController->GetCamera();

			if (!Camera)
			{
				break;
			}

			const UCameraComponent* CameraComponent = Camera->GetCameraComponent();

			if (!CameraComponent)
			{
				break;
			}

			LastCameraData.Pose = CameraComponent->GetComponentToWorld();

			const ULensComponent* LensComponent = StepsController->FindLensComponent();
			if (LensComponent)
			{
				LastCameraData.bWasNodalOffsetApplied = LensComponent->WasNodalOffsetAppliedThisTick();
			}

			LastCameraData.bIsValid = true;


		} while (0);
	}
}

bool UCameraLensDistortionAlgoCheckerboard::OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// We only respond to left clicks
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return false;
	}

	// Left Mouse button means to add a new calibration row
	FText ErrorMessage;

	if (!AddCalibrationRow(ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
	}

	// force play (whether we succeeded or not)
	if (FCameraCalibrationStepsController* StepsController = GetStepsController())
	{
		StepsController->Play();
	}

	return true;
}

FCameraCalibrationStepsController* UCameraLensDistortionAlgoCheckerboard::GetStepsController() const
{
	if (!ensure(Tool.IsValid()))
	{
		return nullptr;
	}

	return Tool->GetCameraCalibrationStepsController();
}

bool UCameraLensDistortionAlgoCheckerboard::AddCalibrationRow(FText& OutErrorMessage)
{
	using namespace CameraLensDistortionAlgoCheckerboard;

	if (!Tool.IsValid())
	{
		OutErrorMessage = LOCTEXT("InvalidTool", "Invalid Tool");
		return false;
	}

	FCameraCalibrationStepsController* StepsController = Tool->GetCameraCalibrationStepsController();

	if (!StepsController)
	{
		OutErrorMessage = LOCTEXT("InvalidStepsController", "Invalid StepsController");
		return false;
	}

	if (!LastCameraData.bIsValid)
	{
		OutErrorMessage = LOCTEXT("InvalidLastCameraData", "Could not find a cached set of camera data (e.g. FIZ). Check the Lens Component to make sure it has valid evaluation inputs.");
		return false;
	}

	if (!Calibrator.IsValid())
	{
		OutErrorMessage = LOCTEXT("InvalidCalibrator", "Please pick a calibrator actor in the given combo box that contains calibration points");
		return false;
	}

	// Read pixels

	TArray<FColor> Pixels;
	FIntPoint Size;

	if (!StepsController->ReadMediaPixels(Pixels, Size, OutErrorMessage, ESimulcamViewportPortion::CameraFeed))
	{
		return false;
	}

	// Export the latest session data
	ExportSessionData();

	// Create the row that we're going to add
	TSharedPtr<FLensDistortionCheckerboardRowData> Row = MakeShared<FLensDistortionCheckerboardRowData>();

	// Get the next row index for the current calibration session to assign to this new row
	const uint32 RowIndex = Tool->AdvanceSessionRowIndex();

	Row->Index = RowIndex;
	Row->CameraData = LastCameraData;
	Row->ImageWidth = Size.X;
	Row->ImageHeight = Size.Y;

	Row->NumCornerRows = Calibrator->NumCornerRows;
	Row->NumCornerCols = Calibrator->NumCornerCols;
	Row->SquareSideInCm = Calibrator->SquareSideLength;

	const FIntPoint CheckerboardDimensions = FIntPoint(Calibrator->NumCornerCols, Calibrator->NumCornerRows);

	const bool bCornersFound = FOpenCVHelper::IdentifyCheckerboard(Pixels, Size, CheckerboardDimensions, Row->Points2d);

 	if (!bCornersFound || Row->Points2d.IsEmpty())
 	{
 		OutErrorMessage = FText::FromString(FString::Printf(TEXT(
 			"Could not identify the expected checkerboard points of interest. "
 			"The expected checkerboard has %dx%d inner corners."),
 			Row->NumCornerCols, Row->NumCornerRows)
 		);
 
 		return false;
 	}

	// Fill out the checkerboard's 3D points
	if (CVarUseExtrinsicsGuess.GetValueOnGameThread())
	{
		const FVector LocationTL = Calibrator->TopLeft->GetComponentLocation();
		const FVector LocationTR = Calibrator->TopRight->GetComponentLocation();
		const FVector LocationBL = Calibrator->BottomLeft->GetComponentLocation();

		const FVector RightVector = LocationTR - LocationTL;
		const FVector DownVector = LocationBL - LocationTL;

		const float HorizontalStep = (Row->NumCornerCols > 1) ? (1.0f / (Row->NumCornerCols - 1)) : 0.0f;
		const float VerticalStep = (Row->NumCornerRows > 1) ? (1.0f / (Row->NumCornerRows - 1)) : 0.0f;

		for (int32 RowIdx = 0; RowIdx < Row->NumCornerRows; ++RowIdx)
		{
			for (int32 ColIdx = 0; ColIdx < Row->NumCornerCols; ++ColIdx)
			{
				const FVector PointLocation = LocationTL + (RightVector * ColIdx * HorizontalStep) + (DownVector * RowIdx * VerticalStep);
				Row->Points3d.Add(PointLocation);
			}
		}
	}
	else
	{
		for (int32 RowIdx = 0; RowIdx < Row->NumCornerRows; ++RowIdx)
		{
			for (int32 ColIdx = 0; ColIdx < Row->NumCornerCols; ++ColIdx)
			{
				Row->Points3d.Add(Row->SquareSideInCm * FVector(ColIdx, RowIdx, 0));
			}
		}
	}

	// Save an image view of the captured frame (for exporting)
	FImageView ImageView = FImageView(Pixels.GetData(), Size.X, Size.Y, ERawImageFormat::BGRA8);
	Row->ImageView = ImageView;

	// Generate a thumbnail image for display in the tool for this row
	FImage RowThumbnail;
	constexpr int32 ResolutionDivider = 4;
	FImageCore::ResizeTo(ImageView, RowThumbnail, Size.X / ResolutionDivider, Size.Y / ResolutionDivider, ERawImageFormat::BGRA8, ImageView.GetGammaSpace());

	UTexture2D* ThumbnailTexture = FImageUtils::CreateTexture2DFromImage(RowThumbnail);
	if (ThumbnailTexture)
	{
		Row->Thumbnail = SNew(SSimulcamViewport, ThumbnailTexture).WithZoom(false).WithPan(false);
	}

	// Update the coverage overlay with the latest checkerboard corners
	TArray<FVector2f> CameraFeedAdjustedCorners = Row->Points2d;
	const FIntPoint CoverageTextureSize = FIntPoint(CoverageTexture->GetSizeX(), CoverageTexture->GetSizeY());
	UE::CameraCalibration::Private::AdjustCorners(CameraFeedAdjustedCorners, CoverageTextureSize, Size);

	FOpenCVHelper::DrawCheckerboardCorners(CameraFeedAdjustedCorners, CheckerboardDimensions, CoverageTexture);
	StepsController->RefreshOverlay();

	// Show the detection to the user
	if (bShouldShowDetectionWindow)
	{
		UTexture2D* DebugTexture = UTexture2D::CreateTransient(Size.X, Size.Y, EPixelFormat::PF_B8G8R8A8);
		UE::CameraCalibration::Private::SetTextureData(DebugTexture, Pixels);
		FOpenCVHelper::DrawCheckerboardCorners(Row->Points2d, CheckerboardDimensions, DebugTexture);

		FCameraCalibrationWidgetHelpers::DisplayTextureInWindowAlmostFullScreen(DebugTexture, LOCTEXT("CheckerboardDetection", "Checkerboard Detection"));
	}

	// Validate the new row, show a message if validation fails.
	if (!ValidateNewRow(Row, OutErrorMessage))
	{
		return false;
	}

	// Add this data point
	CalibrationRows.Add(Row);

	// Export the data for this row to a .json file on disk
	ExportRow(Row);

	// Notify the ListView of the new data
	if (CalibrationListView.IsValid())
	{
		CalibrationListView->RequestListRefresh();
		CalibrationListView->RequestNavigateToItem(Row);
	}

	return true;
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildUI()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot() // Calibrator picker
		.VAlign(EVerticalAlignment::VAlign_Top)
		.AutoHeight()
		.MaxHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[FCameraCalibrationWidgetHelpers::BuildLabelWidgetPair(LOCTEXT("Checkerboard", "Checkerboard"), BuildCalibrationDevicePickerWidget())]

	+ SVerticalBox::Slot() // Show Overlay
		.VAlign(EVerticalAlignment::VAlign_Top)
		.AutoHeight()
		.MaxHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[FCameraCalibrationWidgetHelpers::BuildLabelWidgetPair(LOCTEXT("ShowOverlay", "Show Coverage Overlay"), BuildShowOverlayWidget())]

	+ SVerticalBox::Slot() // Show Detection
		.VAlign(EVerticalAlignment::VAlign_Top)
		.AutoHeight()
		.MaxHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[FCameraCalibrationWidgetHelpers::BuildLabelWidgetPair(LOCTEXT("ShowDetection", "Show Detection"), BuildShowDetectionWidget())]

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
					+ SOverlay::Slot()
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
		[BuildCalibrationPointsTable()]

	+ SVerticalBox::Slot() // Action buttons (e.g. Remove, Clear)
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.AutoHeight()
		.Padding(0, 20)
		[BuildCalibrationActionButtons()];
}

bool UCameraLensDistortionAlgoCheckerboard::ValidateNewRow(TSharedPtr<FLensDistortionCheckerboardRowData>& Row, FText& OutErrorMessage) const
{
	if (!Row.IsValid())
	{
		OutErrorMessage = LOCTEXT("InvalidRowPointer", "Invalid row pointer");
		return false;
	}

	if (!Tool.IsValid())
	{
		return false;
	}

	// Camera data is valid
	if (!Row->CameraData.bIsValid)
	{
		OutErrorMessage = LOCTEXT("InvalidCameraData", "Invalid CameraData");
		return false;
	}

	// Valid image dimensions
	if ((Row->ImageHeight < 1) || (Row->ImageWidth < 1))
	{
		OutErrorMessage = LOCTEXT("InvalidImageDimensions", "Image dimensions were less than 1 pixel");
		return false;
	}

	// Valid image pattern size
	if (Row->SquareSideInCm < 1)
	{
		OutErrorMessage = LOCTEXT("InvalidPatternSize", "Pattern size cannot be smaller than 1 cm");
		return false;
	}

	// valid number of rows and columns
	if ((Row->NumCornerCols < 3) || (Row->NumCornerRows < 3))
	{
		OutErrorMessage = LOCTEXT("InvalidPatternRowsandCols", "Number of corner rows or columns cannot be less than 3");
		return false;
	}

	// If we have no existing rows to compare this one with, we're good to go
	if (!CalibrationRows.Num())
	{
		return true;
	}

	FCameraCalibrationStepsController* StepsController = Tool->GetCameraCalibrationStepsController();

	if (!StepsController)
	{
		return false;
	}

	const ULensFile* LensFile = StepsController->GetLensFile();

	if (!LensFile)
	{
		OutErrorMessage = LOCTEXT("LensFileNotFound", "LensFile not found");
		return false;
	}

	const TSharedPtr<FLensDistortionCheckerboardRowData>& FirstRow = CalibrationRows[0];

	// NumRows didn't change
	if (Row->NumCornerRows != FirstRow->NumCornerRows)
	{
		OutErrorMessage = LOCTEXT("NumRowsChanged", "Number of corner rows changed in calibrator, which is an invalid condition.");
		return false;
	}

	// NumCols didn't change
	if (Row->NumCornerCols != FirstRow->NumCornerCols)
	{
		OutErrorMessage = LOCTEXT("NumColsChanged", "Number of corner columns changed in calibrator, which is an invalid condition.");
		return false;
	}

	// Square side length didn't change
	if (!FMath::IsNearlyEqual(Row->SquareSideInCm, FirstRow->SquareSideInCm))
	{
		OutErrorMessage = LOCTEXT("PatternSizeChanged", "Physical size of the pattern changed, which is an invalid condition");
		return false;
	}

	// Image dimensions did not change
	if ((Row->ImageWidth != FirstRow->ImageWidth) || (Row->ImageHeight != FirstRow->ImageHeight))
	{
		OutErrorMessage = LOCTEXT("ImageDimensionsChanged", "The dimensions of the media plate changed, which is an invalid condition");
		return false;
	}

	//@todo Focus and zoom did not change much (i.e. inputs to distortion and nodal offset). 
	//      Threshold for physical units should differ from normalized encoders.

	return true;
}

FDistortionCalibrationTask UCameraLensDistortionAlgoCheckerboard::BeginCalibration(FText& OutErrorMessage)
{
	FDistortionCalibrationTask CalibrationTask = {};

	// Validate that enough points were gathered to attempt a calibration
	if (CalibrationRows.Num() < 1)
	{
		OutErrorMessage = LOCTEXT("NotEnoughCalibrationRowsError", "Could not initiate distortion calibration. At least 1 calibration row is required.");
		return CalibrationTask;
	}

	// All points are valid
	for (const TSharedPtr<FLensDistortionCheckerboardRowData>& Row : CalibrationRows)
	{
		if (!ensure(Row.IsValid()))
		{
			return CalibrationTask;
		}
	}

	ULensDistortionTool* LensDistortionTool = Tool.Get();
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
	const float Focus = CalibrationRows[0]->CameraData.InputFocus;
	const float Zoom = CalibrationRows[0]->CameraData.InputZoom;

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
	TArray<FImagePoints> Samples2d;
	Samples3d.Reserve(CalibrationRows.Num());
	Samples2d.Reserve(CalibrationRows.Num());

	TArray<FTransform> CameraPoses;
	CameraPoses.Reserve(CalibrationRows.Num());

	for (const TSharedPtr<FLensDistortionCheckerboardRowData>& Row : CalibrationRows)
	{
		FObjectPoints Points3d;
		Points3d.Points = Row->Points3d;

		FImagePoints Points2d;
		Points2d.Points.Reserve(Row->Points2d.Num());
		for (const FVector2f& Point2d : Row->Points2d)
		{
			Points2d.Points.Add(FVector2D(Point2d.X, Point2d.Y));
		}

		Samples3d.Add(Points3d);
		Samples2d.Add(Points2d);
		CameraPoses.Add(Row->CameraData.Pose);
	}

	ECalibrationFlags SolverFlags = ECalibrationFlags::None;

	if (CVarUseExtrinsicsGuess.GetValueOnGameThread())
	{
		EnumAddFlags(SolverFlags, ECalibrationFlags::UseExtrinsicGuess);
	}

	if (CVarUseIntrinsicsGuess.GetValueOnGameThread())
	{
		EnumAddFlags(SolverFlags, ECalibrationFlags::UseIntrinsicGuess);
	}

	if (CVarFixExtrinsics.GetValueOnGameThread())
	{
		EnumAddFlags(SolverFlags, ECalibrationFlags::FixExtrinsics);
	}

	if (CVarFixZeroDistortion.GetValueOnGameThread())
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
	
	Solver = NewObject<ULensDistortionSolver>(this, Tool->GetSolverClass());

	CalibrationTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Solver=Solver, Model, Samples3d, Samples2d, ImageSize, FocalLength, ImageCenter, CameraPoses, PixelAspect, SolverFlags, Focus, Zoom]() mutable
		{
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

void UCameraLensDistortionAlgoCheckerboard::CancelCalibration()
{
	if (Solver)
	{
		Solver->Cancel();
	}
}

bool UCameraLensDistortionAlgoCheckerboard::GetCalibrationStatus(FText& StatusText) const
{
	if (Solver)
	{
		return Solver->GetStatusText(StatusText);
	}
	return false;
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildCalibrationDevicePickerWidget()
{
	return SNew(SHorizontalBox)

	+ SHorizontalBox::Slot() // Picker
	[
		SNew(SObjectPropertyEntryBox)
		.AllowedClass(ACameraCalibrationCheckerboard::StaticClass())
		.OnObjectChanged_Lambda([&](const FAssetData& AssetData) -> void
		{
			if (AssetData.IsValid())
			{
				SetCalibrator(Cast<ACameraCalibrationCheckerboard>(AssetData.GetAsset()));
			}
		})
		.ObjectPath_Lambda([&]() -> FString
		{
			if (AActor* TheCalibrator = GetCalibrator())
			{
				FAssetData AssetData(TheCalibrator, true);
				return AssetData.GetObjectPathString();
			}

			return TEXT("");
		})
	]

	+ SHorizontalBox::Slot() // Spawner
	.AutoWidth()
	[
		SNew(SButton)
		.Text(LOCTEXT("Spawn", "Spawn"))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.OnClicked_Lambda([&]() -> FReply
		{
			const FCameraCalibrationStepsController* StepsController = GetStepsController();

			if (!ensure(StepsController))
			{
				return FReply::Handled();
			}

			if (UWorld* const World = StepsController->GetWorld())
			{
				SetCalibrator(World->SpawnActor<ACameraCalibrationCheckerboard>());
			}

			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Font(FAppStyle::Get().GetFontStyle("FontAwesome.12"))
			.Text(FEditorFontGlyphs::Plus)
			.ColorAndOpacity(FLinearColor::White)
		]
	];
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildShowOverlayWidget()
{
	return SNew(SCheckBox)
	.IsChecked_Lambda([&]() -> ECheckBoxState
	{
		return bShouldShowOverlay ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	})
	.OnCheckStateChanged_Lambda([&](ECheckBoxState NewState) -> void
	{
		bShouldShowOverlay = (NewState == ECheckBoxState::Checked);

		FCameraCalibrationStepsController* StepsController = Tool->GetCameraCalibrationStepsController();
		
		if (!ensure(StepsController))
		{
			return;
		}

		StepsController->SetOverlayEnabled(bShouldShowOverlay);
	});
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildShowDetectionWidget()
{
	return SNew(SCheckBox)
	.IsChecked_Lambda([&]() -> ECheckBoxState
	{
		return bShouldShowDetectionWindow ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	})
	.OnCheckStateChanged_Lambda([&](ECheckBoxState NewState) -> void
	{
		bShouldShowDetectionWindow = (NewState == ECheckBoxState::Checked);
	});
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildFocalLengthEstimateWidget()
{
	return SNew(SHorizontalBox)
	
	+ SHorizontalBox::Slot()
	.FillWidth(0.8)
	.Padding(0, 0, 10, 0)
	[
		SNew(SNumericEntryBox<float>)
		.Value_UObject(this, &UCameraLensDistortionAlgoCheckerboard::GetFocalLengthEstimate)
		.OnValueChanged_UObject(this, &UCameraLensDistortionAlgoCheckerboard::SetFocalLengthEstimate)
	]

	+ SHorizontalBox::Slot()
	.FillWidth(0.2)
	[
		SNew(SCheckBox)
		.Padding(FMargin(5, 0, 15, 0))
		.IsChecked_UObject(this, &UCameraLensDistortionAlgoCheckerboard::IsFixFocalLengthChecked)
		.OnCheckStateChanged_UObject(this, &UCameraLensDistortionAlgoCheckerboard::OnFixFocalLengthCheckStateChanged)
		[
			SNew(STextBlock).Text(LOCTEXT("FixText", "Fix?"))
		]
	];
}

TOptional<float> UCameraLensDistortionAlgoCheckerboard::GetFocalLengthEstimate() const
{
	return FocalLengthEstimate;
}

void UCameraLensDistortionAlgoCheckerboard::SetFocalLengthEstimate(float NewValue)
{
	FocalLengthEstimate = NewValue;
}

ECheckBoxState UCameraLensDistortionAlgoCheckerboard::IsFixFocalLengthChecked() const
{
	return bFixFocalLength ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void UCameraLensDistortionAlgoCheckerboard::OnFixFocalLengthCheckStateChanged(ECheckBoxState NewState)
{
	bFixFocalLength = (NewState == ECheckBoxState::Checked);
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildFixImageCenterWidget()
{
	return SNew(SCheckBox)
	.IsChecked_Lambda([&]() -> ECheckBoxState
	{
		return bFixImageCenter ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	})
	.OnCheckStateChanged_Lambda([&](ECheckBoxState NewState) -> void
	{
		bFixImageCenter = (NewState == ECheckBoxState::Checked);
	});
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildExtrinsicsGuessWidget()
{
	return SNew(SCheckBox)
	.IsChecked_Lambda([&]() -> ECheckBoxState
	{
		return bUseExtrinsicsGuess ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	})
	.OnCheckStateChanged_Lambda([&](ECheckBoxState NewState) -> void
	{
		bUseExtrinsicsGuess = (NewState == ECheckBoxState::Checked);
	});
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildCalibrateNodalOffsetWidget()
{
	return SNew(SCheckBox)
	.IsChecked_Lambda([&]() -> ECheckBoxState
	{
		return bCalibrateNodalOffset ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	})
	.OnCheckStateChanged_Lambda([&](ECheckBoxState NewState) -> void
	{
		bCalibrateNodalOffset = (NewState == ECheckBoxState::Checked);
	});
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildCalibrationActionButtons()
{
	return SNew(SHorizontalBox)

	+ SHorizontalBox::Slot() // Button to clear all rows
	.AutoWidth()
	[
		SNew(SButton)
		.Text(LOCTEXT("ClearAll", "Clear All"))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.OnClicked_Lambda([&]() -> FReply
		{
			ClearCalibrationRows();
			return FReply::Handled();
		})
	];
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildCalibrationPointsTable()
{
	CalibrationListView = SNew(SListView<TSharedPtr<FLensDistortionCheckerboardRowData>>)
		.ItemHeight(24)
		.ListItemsSource(&CalibrationRows)
		.OnGenerateRow_Lambda([&](TSharedPtr<FLensDistortionCheckerboardRowData> InItem, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			return SNew(CameraLensDistortionAlgoCheckerboard::SCalibrationRowGenerator, OwnerTable)
			.CalibrationRowData(InItem);
		})
		.SelectionMode(ESelectionMode::Multi)
		.OnKeyDownHandler_Lambda([&](const FGeometry& Geometry, const FKeyEvent& KeyEvent) -> FReply
		{
			if (!CalibrationListView.IsValid())
			{
				return FReply::Unhandled();
			}

			if (KeyEvent.GetKey() == EKeys::Delete)
			{
				// Delete selected items

				const TArray<TSharedPtr<FLensDistortionCheckerboardRowData>> SelectedItems = CalibrationListView->GetSelectedItems();

				for (const TSharedPtr<FLensDistortionCheckerboardRowData>& SelectedItem : SelectedItems)
				{
					CalibrationRows.Remove(SelectedItem);

					// Delete the previously exported .json file for the row that is being deleted
					if (ULensDistortionTool* LensDistortionTool = Tool.Get())
					{
						LensDistortionTool->DeleteExportedRow(SelectedItem->Index);
					}
				}

				CalibrationListView->RequestListRefresh();
				RefreshCoverage();
				return FReply::Handled();
			}
			else if (KeyEvent.GetModifierKeys().IsControlDown() && (KeyEvent.GetKey() == EKeys::A))
			{
				// Select all items

				CalibrationListView->SetItemSelection(CalibrationRows, true);
				return FReply::Handled();
			}
			else if (KeyEvent.GetKey() == EKeys::Escape)
			{
				// Deselect all items

				CalibrationListView->ClearSelection();
				return FReply::Handled();
			}

			return FReply::Unhandled();
		})
		.HeaderRow
		(
			SNew(SHeaderRow)

			+ SHeaderRow::Column("Index")
			.DefaultLabel(LOCTEXT("Index", "Index"))

			+ SHeaderRow::Column("Image")
			.DefaultLabel(LOCTEXT("Image", "Image"))
		);

	return CalibrationListView.ToSharedRef();
}

ACameraCalibrationCheckerboard* UCameraLensDistortionAlgoCheckerboard::FindFirstCalibrator() const
{
	if (!ensure(Tool.IsValid()))
	{
		return nullptr;
	}

	const FCameraCalibrationStepsController* StepsController = Tool->GetCameraCalibrationStepsController();

	if (!ensure(StepsController))
	{
		return nullptr;
	}

	UWorld* World = StepsController->GetWorld();

	if (!ensure(World))
	{
		return nullptr;
	}

	const EActorIteratorFlags Flags = EActorIteratorFlags::SkipPendingKill;

	for (TActorIterator<AActor> It(World, ACameraCalibrationCheckerboard::StaticClass(), Flags); It; ++It)
	{
		return CastChecked<ACameraCalibrationCheckerboard>(*It);
	}

	return nullptr;
}

void UCameraLensDistortionAlgoCheckerboard::SetCalibrator(ACameraCalibrationCheckerboard* InCalibrator)
{
	Calibrator = InCalibrator;
}

ACameraCalibrationCheckerboard* UCameraLensDistortionAlgoCheckerboard::GetCalibrator() const
{
	return Calibrator.Get();
}

UMaterialInterface* UCameraLensDistortionAlgoCheckerboard::GetOverlayMaterial() const
{
	return TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/CameraCalibration/Materials/M_Coverage.M_Coverage"))).LoadSynchronous();
}

void UCameraLensDistortionAlgoCheckerboard::OnDistortionSavedToLens()
{
	// Since the calibration result was saved, there is no further use for the current samples.
	ClearCalibrationRows();
}

void UCameraLensDistortionAlgoCheckerboard::ClearCalibrationRows()
{
	CalibrationRows.Empty();

	if (CalibrationListView.IsValid())
	{
		CalibrationListView->RequestListRefresh();
	}

	RefreshCoverage();

	// End the current calibration session (a new one will begin the next time a new row is added)
	if (ULensDistortionTool* LenDistortionTool = Tool.Get())
	{
		LenDistortionTool->EndCalibrationSession();
	}
}

void UCameraLensDistortionAlgoCheckerboard::RefreshCoverage()
{
	FCameraCalibrationStepsController* StepsController = Tool->GetCameraCalibrationStepsController();

	if (!ensure(StepsController))
	{
		return;
	}

	UE::CameraCalibration::Private::ClearTexture(CoverageTexture);

	for (const TSharedPtr<FLensDistortionCheckerboardRowData>& Row : CalibrationRows)
	{
		TArray<FVector2f> CameraFeedAdjustedCorners = Row->Points2d;
		const FIntPoint CoverageTextureSize = FIntPoint(CoverageTexture->GetSizeX(), CoverageTexture->GetSizeY());
		const FIntPoint ImageSize = FIntPoint(Row->ImageWidth, Row->ImageHeight);
		UE::CameraCalibration::Private::AdjustCorners(CameraFeedAdjustedCorners, CoverageTextureSize, ImageSize);

		const FIntPoint CheckerboardDimensions = FIntPoint(Row->NumCornerCols, Row->NumCornerRows);
		FOpenCVHelper::DrawCheckerboardCorners(Row->Points2d, CheckerboardDimensions, CoverageTexture);
	}

	// The coverage texture may have changed as a result of a change in size or pixel format.
	// Therefore, the material parameter should be updated to ensure it is up to date.
	if (CoverageTexture)
	{
		if (UMaterialInstanceDynamic* OverlayMID = Tool->GetOverlayMID())
		{
			OverlayMID->SetTextureParameterValue(FName(TEXT("CoverageTexture")), CoverageTexture);
		}
	}

	StepsController->RefreshOverlay();
}


void UCameraLensDistortionAlgoCheckerboard::ExportSessionData()
{
	using namespace UE::CameraCalibration::Private;

	if (ULensDistortionTool* LensDistortionTool = Tool.Get())
	{
		// Add all data to a json object that is needed to run this algorithm that is NOT part of a specific row
		TSharedPtr<FJsonObject> JsonSessionData = MakeShared<FJsonObject>();

		JsonSessionData->SetNumberField(LensDistortionCheckerboardExportFields::Version, DATASET_VERSION);

		// Export the session data to a .json file
		Tool->ExportSessionData(JsonSessionData.ToSharedRef());
	}
}

void UCameraLensDistortionAlgoCheckerboard::ExportRow(TSharedPtr<FLensDistortionCheckerboardRowData> Row)
{
	if (ULensDistortionTool* LensDistortionTool = Tool.Get())
	{
		if (const TSharedPtr<FJsonObject>& RowObject = FJsonObjectConverter::UStructToJsonObject<FLensDistortionCheckerboardRowData>(Row.ToSharedRef().Get()))
		{
			// Export the row data to a .json file
			LensDistortionTool->ExportCalibrationRow(Row->Index, RowObject.ToSharedRef(), Row->ImageView);
		}
	}
}

bool UCameraLensDistortionAlgoCheckerboard::HasCalibrationData() const
{
	return (CalibrationRows.Num() > 0);
}

void UCameraLensDistortionAlgoCheckerboard::PreImportCalibrationData()
{
	// Clear the current set of rows before importing new ones
	CalibrationRows.Empty();
}

int32 UCameraLensDistortionAlgoCheckerboard::ImportCalibrationRow(const TSharedRef<FJsonObject>& CalibrationRowObject, const FImage& RowImage)
{
	// Create a new row to populate with data from the Json object
	TSharedPtr<FLensDistortionCheckerboardRowData> NewRow = MakeShared<FLensDistortionCheckerboardRowData>();

	// We enforce strict mode to ensure that every field in the UStruct of row data is present in the imported json.
	// If any fields are missing, it is likely the row will be invalid, which will lead to errors in the calibration.
	constexpr int64 CheckFlags = 0;
	constexpr int64 SkipFlags = 0;
	constexpr bool bStrictMode = true;
	if (FJsonObjectConverter::JsonObjectToUStruct<FLensDistortionCheckerboardRowData>(CalibrationRowObject, NewRow.Get(), CheckFlags, SkipFlags, bStrictMode))
	{
		if (!RowImage.RawData.IsEmpty())
		{
			NewRow->ImageView = RowImage;

			// Resize the image into a thumbnail, create a UTexture from it, and initialize the thumbnail viewport for this row
			FImage RowThumbnail;
			RowImage.ResizeTo(RowThumbnail, RowImage.GetWidth() / 4, RowImage.GetHeight() / 4, ERawImageFormat::BGRA8, RowImage.GetGammaSpace());

			UTexture2D* ThumbnailTexture = FImageUtils::CreateTexture2DFromImage(RowThumbnail);
			NewRow->Thumbnail = SNew(SSimulcamViewport, ThumbnailTexture).WithZoom(false).WithPan(false);
		}

		CalibrationRows.Add(NewRow);
	}
	else
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("LensDistortionCheckerboard algo failed to import calibration row because at least one field could not be deserialized from the json file."));
	}

	return NewRow->Index;
}

void UCameraLensDistortionAlgoCheckerboard::PostImportCalibrationData()
{
	// Sort imported calibration rows by row index
	CalibrationRows.Sort([](const TSharedPtr<FLensDistortionCheckerboardRowData>& LHS, const TSharedPtr<FLensDistortionCheckerboardRowData>& RHS) { return LHS->Index < RHS->Index; });

	// Notify the ListView of the new data
	if (CalibrationListView.IsValid())
	{
		CalibrationListView->RequestListRefresh();
	}
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildHelpWidget()
{
	return SNew(STextBlock)
		.Text(LOCTEXT("LensDistortionCheckerboardHelp",
			"This Lens Distortion algorithm is based on capturing at least 4 different views of a checkerboard.\n\n"
			"The camera and/or the checkerboard may be moved for each. To capture, simply click the simulcam\n"
			"viewport. You can optionally right-click the simulcam viewport to pause it and ensure it will be\n"
			"a sharp capture (if the media source supports pause).\n\n"

			"This will require the selection of a Checkerboard actor that exists in the scene, and is configured\n"
			"to match the dimensions and number of inner corner rows and columns as the physical checkerboard.\n"
			"If there isn't a checkerboard in the scene, you can spawn one from either the Place Actors panel\n"
			"or the '+' button next to the checkerboard picker. You can then edit its details panel properties.\n\n"

			"Each capture will be added to the table with the thumbnails. It is important that the collection of\n"
			"captures sweep the viewport area as much as possible, so that the calibration includes samples from\n"
			"all of its regions (otherwise the calibration may be skewed towards a particular region and not be\n"
			"very accurate in others).\n\n"

			"When you are done capturing, click the 'Add To Lens Distortion Calibration' button to run the calibration\n"
			"algorithm, which calculates the distortion parameters k1,k2,p1,p2,k3 and adds them to the lens file.\n\n"

			"A dialog window will show you the reprojection error of the calibration, and you can decide to add the\n"
			"data to the lens file or not.\n"
		));
}

#undef LOCTEXT_NAMESPACE
