// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraLensDistortionAlgoCheckerboard.h"

#include "AssetEditor/CameraCalibrationStepsController.h"
#include "AssetEditor/LensDistortionTool.h"
#include "AssetEditor/SSimulcamViewport.h"
#include "CameraCalibrationCheckerboard.h"
#include "CameraCalibrationEditorLog.h"
#include "Dom/JsonObject.h"
#include "EditorFontGlyphs.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "JsonObjectConverter.h"
#include "Misc/MessageDialog.h"
#include "PropertyCustomizationHelpers.h"
#include "SphericalLensDistortionModelHandler.h"
#include "UI/CameraCalibrationWidgetHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"


#define LOCTEXT_NAMESPACE "CameraLensDistortionAlgoCheckerboard"

const int UCameraLensDistortionAlgoCheckerboard::DATASET_VERSION = 1;

// String constants for import/export
namespace UE::CameraCalibration::Private::LensDistortionCheckerboardExportFields
{
	static const FString Version(TEXT("Version"));
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

#if WITH_OPENCV
	// Initialize coverage matrix
	FCameraCalibrationStepsController* StepsController = Tool->GetCameraCalibrationStepsController();

	if (!ensure(StepsController))
	{
		return;
	}

	FIntPoint Size = StepsController->GetCompRenderTargetSize();
	CvCoverage = cv::Mat(cv::Size(Size.X, Size.Y), CV_8UC4);
	CoverageTexture = FOpenCVHelper::TextureFromCvMat(CvCoverage, CoverageTexture);
#endif

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
				LastCameraData.bIsValid = true;
			}

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
	{
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
#if WITH_OPENCV
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
	ETextureRenderTargetFormat PixelFormat;

	if (!StepsController->ReadMediaPixels(Pixels, Size, PixelFormat, OutErrorMessage))
	{
		return false;
	}

	if (PixelFormat != ETextureRenderTargetFormat::RTF_RGBA8)
	{
		OutErrorMessage = LOCTEXT("InvalidFormat", "MediaPlateRenderTarget did not have the expected RTF_RGBA8 format");
		return false;
	}

	// Create OpenCV Mat with those pixels
	cv::Mat CvFrame(cv::Size(Size.X, Size.Y), CV_8UC4, Pixels.GetData());

	cv::Mat CvGray;
	cv::cvtColor(CvFrame, CvGray, cv::COLOR_RGBA2GRAY);

	// Export the latest session data
	ExportSessionData();

	// Create the row that we're going to add
	TSharedPtr<FLensDistortionCheckerboardRowData> Row = MakeShared<FLensDistortionCheckerboardRowData>();

	// Get the next row index for the current calibration session to assign to this new row
	const uint32 RowIndex = Tool->AdvanceSessionRowIndex();

	Row->Index = RowIndex;
	Row->CameraData = LastCameraData;
	Row->ImageHeight = CvGray.rows;
	Row->ImageWidth = CvGray.cols;

	Row->NumCornerRows = Calibrator->NumCornerRows;
	Row->NumCornerCols = Calibrator->NumCornerCols;
	Row->SquareSideInCm = Calibrator->SquareSideLength;

	// Fill out checkerboard 3d points
	for (int32 RowIdx = 0; RowIdx < Row->NumCornerRows; ++RowIdx)
	{
		for (int32 ColIdx = 0; ColIdx < Row->NumCornerCols; ++ColIdx)
		{
			Row->Points3d.Add(Row->SquareSideInCm * FVector(ColIdx, RowIdx, 0));
		}
	}

	// Identify checkerboard
	{
		cv::Size CheckerboardSize(Row->NumCornerCols, Row->NumCornerRows);

		std::vector<cv::Point2f> Corners;

		const bool bCornersFound = cv::findChessboardCorners(
			CvGray,
			CheckerboardSize,
			Corners,
			cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE
		);

		if (!bCornersFound)
		{
			OutErrorMessage = FText::FromString(FString::Printf(TEXT(
				"Could not identify the expected checkerboard points of interest. "
				"The expected checkerboard has %dx%d inner corners."),
				Row->NumCornerCols, Row->NumCornerRows)
			);

			return false;
		}

		// cv::TermCriteria::Type::EPS will stop the search when the error is under the given epsilon.
		// cv::TermCriteria::Type::COUNT will stop after the specified number of iterations regardless of epsilon.
		cv::TermCriteria Criteria(cv::TermCriteria::Type::EPS | cv::TermCriteria::Type::COUNT, 30, 0.001);
		cv::cornerSubPix(CvGray, Corners, cv::Size(11, 11), cv::Size(-1, -1), Criteria);

		if (!Corners.empty())
		{
			for (cv::Point2f& Corner : Corners)
			{
				Row->Points2d.Add(FVector2D(Corner.x, Corner.y));
			}
		}

		// Save an image view of the captured frame with the corners overlaid on it (for exporting)
		FImageView ImageView = FImageView(CvFrame.data, CvFrame.cols, CvFrame.rows, ERawImageFormat::BGRA8);
		Row->ImageView = ImageView;

		// Update the coverage overlay image with information from the newly added row
		cv::drawChessboardCorners(CvCoverage, CheckerboardSize, Corners, true);

		CoverageTexture = FOpenCVHelper::TextureFromCvMat(CvCoverage, CoverageTexture);
		StepsController->RefreshOverlay();

		// Show the detection to the user
		if (bShouldShowDetectionWindow)
		{
			cv::Mat CvFrameWithOverlay;
			CvFrame.copyTo(CvFrameWithOverlay);
			cv::drawChessboardCorners(CvFrameWithOverlay, CheckerboardSize, Corners, true);

			FCameraCalibrationWidgetHelpers::DisplayTextureInWindowAlmostFullScreen(
				FOpenCVHelper::TextureFromCvMat(CvFrameWithOverlay),
				LOCTEXT("CheckerboardDetection", "Checkerboard Detection")
			);
		}
	}

	// Create thumbnail and add it to the row
	{
		// Resize the frame to thumbnail size

		cv::Mat CvThumbnail;
		const int32 ResolutionDivider = 4;
		cv::resize(CvFrame, CvThumbnail, cv::Size(CvFrame.cols / ResolutionDivider, CvFrame.rows / ResolutionDivider));

		if (UTexture2D* ThumbnailTexture = FOpenCVHelper::TextureFromCvMat(CvThumbnail))
		{
			Row->Thumbnail = SNew(SSimulcamViewport, ThumbnailTexture).WithZoom(false).WithPan(false);
		}
	}

	// Validate the new row, show a message if validation fails.
	{
		if (!ValidateNewRow(Row, OutErrorMessage))
		{
			return false;
		}
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
#endif //WITH_OPENCV

	OutErrorMessage = LOCTEXT("OpenCVRequired", "OpenCV is required");
	return false;
}

TSharedRef<SWidget> UCameraLensDistortionAlgoCheckerboard::BuildUI()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot() // Calibrator picker
		.VAlign(EVerticalAlignment::VAlign_Top)
		.AutoHeight()
		.MaxHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[ FCameraCalibrationWidgetHelpers::BuildLabelWidgetPair(LOCTEXT("Checkerboard", "Checkerboard"), BuildCalibrationDevicePickerWidget()) ]

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

		+ SVerticalBox::Slot() // Calibration Rows
		.AutoHeight()
		.MaxHeight(12 * FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[ BuildCalibrationPointsTable() ]
		
		+ SVerticalBox::Slot() // Action buttons (e.g. Remove, Clear)
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.AutoHeight()
		.Padding(0,20)
		[ BuildCalibrationActionButtons() ];
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

bool UCameraLensDistortionAlgoCheckerboard::GetLensDistortion(
	float& OutFocus,
	float& OutZoom,
	FDistortionInfo& OutDistortionInfo,
	FFocalLengthInfo& OutFocalLengthInfo,
	FImageCenterInfo& OutImageCenterInfo,
	TSubclassOf<ULensModel>& OutLensModel,
	double& OutError,
	FText& OutErrorMessage)
{
	// Sanity checks
	//

	// Enough points
	if (CalibrationRows.Num() < 4)
	{
		OutErrorMessage = LOCTEXT("NotEnoughSamples", "At least 4 calibration rows are required");
		return false;
	}

	// All points are valid
	for (const TSharedPtr<FLensDistortionCheckerboardRowData>& Row : CalibrationRows)
	{
		if (!ensure(Row.IsValid()))
		{
			return false;
		}
	}

	if (!ensure(Tool.IsValid()))
	{
		return false;
	}

	FCameraCalibrationStepsController* StepsController = Tool->GetCameraCalibrationStepsController();

	if (!ensure(StepsController))
	{
		return false;
	}

	const ULensFile* LensFile = StepsController->GetLensFile();

	if (!ensure(LensFile))
	{
		OutErrorMessage = LOCTEXT("LensFileNotFound", "LensFile not found");
		return false;
	}

	const TSharedPtr<FLensDistortionCheckerboardRowData>& LastRow = CalibrationRows.Last();

	// Only parameters data mode supported at the moment
	if (LensFile->DataMode != ELensDataMode::Parameters)
	{
		OutErrorMessage = LOCTEXT("OnlyParametersDataModeSupported", "Only Parameters Data Mode supported");
		return false;
	}

	// Only spherical lens distortion is currently supported at the moment.

	const USphericalLensDistortionModelHandler* SphericalHandler = Cast<USphericalLensDistortionModelHandler>(StepsController->GetDistortionHandler());

	if (!SphericalHandler)
	{
		OutErrorMessage = LOCTEXT("OnlySphericalDistortionSupported", "Only spherical distortion is currently supported. Please update the distortion model used by the camera.");
		return false;
	}

#if WITH_OPENCV

	cv::Mat CameraMatrix = cv::Mat::eye(3, 3, CV_64F);;
	cv::Mat DistortionCoefficients;

	std::vector<cv::Mat> Rvecs;
	std::vector<cv::Mat> Tvecs;

	std::vector<std::vector<cv::Point2f>> Samples2d;
	std::vector<std::vector<cv::Point3f>> Samples3d;

	for (const TSharedPtr<FLensDistortionCheckerboardRowData>& Row : CalibrationRows)
	{
		// add 2d (image) points
		{
			std::vector<cv::Point2f> Points2d;

			for (FVector2D& Point2d : Row->Points2d)
			{
				Points2d.push_back(cv::Point2f(Point2d.X, Point2d.Y));
			}

			Samples2d.push_back(Points2d);
		}

		// add 3d points
		{
			std::vector<cv::Point3f> Points3d;

			for (FVector& Point3d : Row->Points3d)
			{
				Points3d.push_back(cv::Point3f(Point3d.X, Point3d.Y, Point3d.Z));
			}

			Samples3d.push_back(Points3d);
		}
	}

	OutError = cv::calibrateCamera(
		Samples3d,
		Samples2d,
		cv::Size(LastRow->ImageWidth, LastRow->ImageHeight), 
		CameraMatrix, 
		DistortionCoefficients, 
		Rvecs, 
		Tvecs
	);

	check(DistortionCoefficients.total() == 5);
	check((CameraMatrix.rows == 3) && (CameraMatrix.cols == 3));

	// Valid image sizes were verified when adding the calibration rows.
	checkSlow(LastRow->ImageWidth > 0);
	checkSlow(LastRow->ImageHeight > 0);

	// FZ inputs to LUT
	OutFocus = LastRow->CameraData.InputFocus;
	OutZoom = LastRow->CameraData.InputZoom;

	// FocalLengthInfo
	OutFocalLengthInfo.FxFy = FVector2D(
		float(CameraMatrix.at<double>(0, 0) / LastRow->ImageWidth),
		float(CameraMatrix.at<double>(1, 1) / LastRow->ImageHeight)
	);

	// DistortionInfo
	{
		FSphericalDistortionParameters SphericalParameters;

		SphericalParameters.K1 = DistortionCoefficients.at<double>(0);
		SphericalParameters.K2 = DistortionCoefficients.at<double>(1);
		SphericalParameters.P1 = DistortionCoefficients.at<double>(2);
		SphericalParameters.P2 = DistortionCoefficients.at<double>(3);
		SphericalParameters.K3 = DistortionCoefficients.at<double>(4);

		USphericalLensModel::StaticClass()->GetDefaultObject<ULensModel>()->ToArray(
			SphericalParameters, 
			OutDistortionInfo.Parameters
		);
	}

	// ImageCenterInfo
	OutImageCenterInfo.PrincipalPoint = FVector2D(
		float(CameraMatrix.at<double>(0, 2) / LastRow->ImageWidth),
		float(CameraMatrix.at<double>(1, 2) / LastRow->ImageHeight)
	);

	// Lens Model
	OutLensModel = USphericalLensModel::StaticClass();

	return true;
#else
	{
		OutErrorMessage = LOCTEXT("OpenCVRequired", "OpenCV is required");
		return false;
	}
#endif //WITH_OPENCV
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
		]
		;
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
		]
		;
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
#if WITH_OPENCV
	FCameraCalibrationStepsController* StepsController = Tool->GetCameraCalibrationStepsController();

	if (!ensure(StepsController))
	{
		return;
	}

	FIntPoint Size = StepsController->GetCompRenderTargetSize();

	CvCoverage.release();
	CvCoverage = cv::Mat(cv::Size(Size.X, Size.Y), CV_8UC4);

	for (const TSharedPtr<FLensDistortionCheckerboardRowData>& Row : CalibrationRows)
	{
		cv::Size CheckerboardSize(Row->NumCornerCols, Row->NumCornerRows);

		std::vector<cv::Point2f> Corners;

		for (const FVector2D& Corner : Row->Points2d)
		{
			Corners.push_back(cv::Point2f(Corner.X, Corner.Y));
		}

		cv::drawChessboardCorners(CvCoverage, CheckerboardSize, Corners, true);
	}

	CoverageTexture = FOpenCVHelper::TextureFromCvMat(CvCoverage, CoverageTexture);

	StepsController->RefreshOverlay();
#endif
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
