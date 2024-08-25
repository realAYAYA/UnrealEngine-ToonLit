// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensDistortionTool.h"

#include "AssetToolsModule.h"
#include "Calibrators/CameraCalibrationSolver.h"
#include "CameraCalibrationEditorLog.h"
#include "CameraCalibrationSettings.h"
#include "CameraCalibrationStepsController.h"
#include "CameraCalibrationTypes.h"
#include "CameraLensDistortionAlgo.h"
#include "DesktopPlatformModule.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "IDesktopPlatform.h"
#include "ImageUtils.h"
#include "LensFile.h"
#include "LensInfoStep.h"
#include "Misc/DateTime.h"
#include "Misc/MessageDialog.h"
#include "Models/SphericalLensModel.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "LensDistortionTool"

namespace UE::CameraCalibration::Private::LensDistortionTool
{
	static const FString SessionDateTimeField(TEXT("SessionDateTime"));
	static const FString AlgoNameField(TEXT("AlgoName"));
}

void ULensDistortionTool::Initialize(TWeakPtr<FCameraCalibrationStepsController> InCameraCalibrationStepController)
{
	CameraCalibrationStepsController = InCameraCalibrationStepController;

	// Find available solver classes

	TArray<UClass*> DerivedSolverClasses;
	GetDerivedClasses(ULensDistortionSolver::StaticClass(), DerivedSolverClasses);

	check(!DerivedSolverClasses.IsEmpty());

	SetSolverClass(DerivedSolverClasses[0]);

	// Find available algos

	TArray<TSubclassOf<UCameraLensDistortionAlgo>> Algos;

	for (TObjectIterator<UClass> AlgoIt; AlgoIt; ++AlgoIt)
	{
		if (AlgoIt->IsChildOf(UCameraLensDistortionAlgo::StaticClass()) && !AlgoIt->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			const UCameraLensDistortionAlgo* Algo = CastChecked<UCameraLensDistortionAlgo>(AlgoIt->GetDefaultObject());
			AlgosMap.Add(Algo->FriendlyName(), TSubclassOf<UCameraLensDistortionAlgo>(*AlgoIt));

			// If the algo uses an overlay material, create a new MID to use with that algo
			if (UMaterialInterface* OverlayMaterial = Algo->GetOverlayMaterial())
			{
				AlgoOverlayMIDs.Add(Algo->FriendlyName(), UMaterialInstanceDynamic::Create(OverlayMaterial, GetTransientPackage()));
			}
		}
	}

	if (TSharedPtr<FCameraCalibrationStepsController> SharedStepsController = CameraCalibrationStepsController.Pin())
	{
		if (ULensFile* const LensFile = SharedStepsController->GetLensFile())
		{
			LensFile->OnLensFileModelChanged().AddUObject(this, &ULensDistortionTool::OnLensModelChanged);

			UpdateAlgoMap(LensFile->LensInfo.LensModel);
		}
	}

	BuildProgressWindowWidgets();
}

void ULensDistortionTool::OnLensModelChanged(const TSubclassOf<ULensModel>& LensModel)
{
	UpdateAlgoMap(LensModel);
}

void ULensDistortionTool::UpdateAlgoMap(const TSubclassOf<ULensModel>& LensModel)
{
	SupportedAlgosMap.Empty();
	for (const TPair<FName, TSubclassOf<UCameraLensDistortionAlgo>>& AlgoPair : AlgosMap)
	{
		const UCameraLensDistortionAlgo* Algo = CastChecked<UCameraLensDistortionAlgo>(AlgoPair.Value->GetDefaultObject());

		if (Algo->SupportsModel(LensModel))
		{
			SupportedAlgosMap.Add(AlgoPair);
		}
	}
	if (DistortionWidget)
	{
		DistortionWidget->UpdateAlgosOptions();
	}
}

void ULensDistortionTool::Shutdown()
{
	if (CurrentAlgo)
	{
		if (CalibrationTask.IsValid())
		{
			CurrentAlgo->CancelCalibration();
			CalibrationTask = {};
			ProgressWindow->HideWindow();
		}

		CurrentAlgo->Shutdown();
		CurrentAlgo = nullptr;

		EndCalibrationSession();
	}

	if (TSharedPtr<FCameraCalibrationStepsController> SharedStepsController = CameraCalibrationStepsController.Pin())
	{
		if (ULensFile* const LensFile = SharedStepsController->GetLensFile())
		{
			LensFile->OnLensFileModelChanged().RemoveAll(this);
		}
	}
}

void ULensDistortionTool::Tick(float DeltaTime)
{
	if (CurrentAlgo)
	{
		CurrentAlgo->Tick(DeltaTime);
	}

	// A valid task handle implies that there is an asynchronous calibration happening on another thread.
	// The tool will poll the task to determine when it has finished so that the results can be saved.
	if (CalibrationTask.IsValid())
	{
		if (CalibrationTask.IsCompleted())
		{
			// Extract the return value from the task and release the task resource
			CalibrationResult = CalibrationTask.GetResult();
			CalibrationTask = {};

			if (!CalibrationResult.ErrorMessage.IsEmpty())
			{
				const FText Message = FText::Format(LOCTEXT("CalibrationErrorResult", "Calibration Error: {0}"), CalibrationResult.ErrorMessage);
				ProgressTextWidget->SetText(Message);
			}
			else
			{
				// Update progress window with final reprojection error
				FFormatOrderedArguments Arguments;
				Arguments.Add(FText::FromString(FString::Printf(TEXT("%.3f"), CalibrationResult.ReprojectionError)));

				const FText Message = FText::Format(LOCTEXT("CalibrationTaskResult", "Reprojection Error: {0} pixels"), Arguments);
				ProgressTextWidget->SetText(Message);
			}

			OkayButton->SetEnabled(true);
		}
		else
		{
			FText StatusText = FText::GetEmpty();
			const bool bIsStatusNew = CurrentAlgo->GetCalibrationStatus(StatusText);

			if (bIsStatusNew)
			{
				ProgressTextWidget->SetText(StatusText);
			}
		}
	}
}

bool ULensDistortionTool::OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bIsActive)
	{
		return false;
	}

	if (!CurrentAlgo)
	{
		return false;
	}

	// Block user interaction with the simulcam viewport while an async calibration task is executing
	if (CalibrationTask.IsValid())
	{
		return false;
	}

	return CurrentAlgo->OnViewportClicked(MyGeometry, MouseEvent);
}

TSharedRef<SWidget> ULensDistortionTool::BuildUI()
{
	DistortionWidget = SNew(SLensDistortionToolPanel, this);
	return DistortionWidget.ToSharedRef();
}

void ULensDistortionTool::BuildProgressWindowWidgets()
{
	ProgressWindow = SNew(SWindow)
		.Title(LOCTEXT("ProgressWindowTitle", "Distortion Calibration Progress"))
		.SizingRule(ESizingRule::Autosized)
		.IsTopmostWindow(true)
		.HasCloseButton(false)
		.SupportsMaximize(false)
		.SupportsMinimize(true);

	ProgressTextWidget = SNew(STextBlock).Text(FText::GetEmpty());

	OkayButton = SNew(SButton)
		.IsEnabled(false)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Text(LOCTEXT("OkText", "Ok"))
		.OnClicked_UObject(this, &ULensDistortionTool::OnOkPressed);

	TSharedRef<SWidget> WindowContent = SNew(SVerticalBox)

		// Text widget to display the current progress of the calibration
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.VAlign(EVerticalAlignment::VAlign_Center)
		[
			ProgressTextWidget.ToSharedRef()
		]

		// Ok and Cancel buttons
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.VAlign(EVerticalAlignment::VAlign_Center)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				OkayButton.ToSharedRef()
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("CancelText", "Cancel"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_UObject(this, &ULensDistortionTool::OnCancelPressed)
			]
		];

	ProgressWindow->SetContent(WindowContent);

	// Create the window, but start with it hidden. When the user initiates a calibration, the progress window will be shown.
	FSlateApplication::Get().AddWindow(ProgressWindow.ToSharedRef());
	ProgressWindow->HideWindow();
}

bool ULensDistortionTool::DependsOnStep(UCameraCalibrationStep* Step) const
{
	return Cast<ULensInfoStep>(Step) != nullptr;
}

void ULensDistortionTool::Activate()
{
	// Nothing to do if it is already active.
	if (bIsActive)
	{
		return;
	}

	bIsActive = true;
}

void ULensDistortionTool::Deactivate()
{
	bIsActive = false;
}

UClass* ULensDistortionTool::GetSolverClass()
{
	return SolverClass;
}

void ULensDistortionTool::SetSolverClass(UClass* InSolverClass)
{
	SolverClass = InSolverClass;
}

void ULensDistortionTool::ResetAlgo()
{
	// Remove old Algo
	if (CurrentAlgo)
	{
		CurrentAlgo->Shutdown();
		CurrentAlgo = nullptr;

		EndCalibrationSession();
	}

	// Set the tool overlay pass' material to the MID associate with the current algo
	if (TSharedPtr<FCameraCalibrationStepsController> StepsController = CameraCalibrationStepsController.Pin())
	{
		StepsController->SetOverlayEnabled(false);
		StepsController->SetOverlayMaterial(nullptr);
	}
}

void ULensDistortionTool::SetAlgo(const FName& AlgoName)
{
	// Find the algo class

	//@todo replace with Find to avoid double search

	if (!AlgosMap.Contains(AlgoName))
	{
		return;
	}

	TSubclassOf<UCameraLensDistortionAlgo>& AlgoClass = AlgosMap[AlgoName];
		
	// If it is the same as the existing one, do nothing.
	if (!CurrentAlgo && !AlgoClass)
	{
		return;
	}
	else if (CurrentAlgo && (CurrentAlgo->GetClass() == AlgoClass))
	{
		return;
	}

	// Remove old Algo
	if (CurrentAlgo)
	{
		CurrentAlgo->Shutdown();
		CurrentAlgo = nullptr;

		EndCalibrationSession();
	}

	// If AlgoClass is none, we're done here.
	if (!AlgoClass)
	{
		return;
	}

	// Create new algo
	CurrentAlgo = NewObject<UCameraLensDistortionAlgo>(
		GetTransientPackage(),
		AlgoClass,
		MakeUniqueObjectName(GetTransientPackage(), AlgoClass));

	if (CurrentAlgo)
	{
		CurrentAlgo->Initialize(this);
	}

	// Set the tool overlay pass' material to the MID associate with the current algo
	if (CameraCalibrationStepsController.IsValid())
	{
		TSharedPtr<FCameraCalibrationStepsController> StepsController = CameraCalibrationStepsController.Pin();
		StepsController->SetOverlayEnabled(false);
		StepsController->SetOverlayMaterial(GetOverlayMID());
	}
}

UCameraLensDistortionAlgo* ULensDistortionTool::GetAlgo() const
{
	return CurrentAlgo;
}

void ULensDistortionTool::OnSaveCurrentCalibrationData()
{
	if (!CurrentAlgo)
	{
		return;
	}

	const FText TitleError = LOCTEXT("LensCalibrationError", "Lens Calibration Error");
	const FText UnknownError = LOCTEXT("UnknownError", "An unknown error occurred initiating the distortion calibration. Check the output log for details.");

	if (CurrentAlgo->SupportsAsyncCalibration())
	{
		FText ErrorMessage;
		CalibrationTask = CurrentAlgo->BeginCalibration(ErrorMessage);

		if (!CalibrationTask.IsValid())
		{
			if (!ErrorMessage.IsEmpty())
			{
				FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, TitleError);
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, UnknownError, TitleError);
			}
			return;
		}

		// TODO: This text is temporary. Update to register a delegate with the algo to provide implementation-specific status text
		ProgressTextWidget->SetText(LOCTEXT("CalibrationProgressText", "Calibrating Lens Distortion..."));

		// Ensure that the Ok button is disabled and show the progress window
		OkayButton->SetEnabled(false);
		ProgressWindow->ShowWindow();

		DistortionWidget->SetEnabled(false);
	}
	else
	{
		TSubclassOf<ULensModel> LensModel;
		bool bResult = CurrentAlgo->GetLensDistortion(
			CalibrationResult.EvaluatedFocus, 
			CalibrationResult.EvaluatedZoom, 
			CalibrationResult.Parameters, 
			CalibrationResult.FocalLength, 
			CalibrationResult.ImageCenter, 
			LensModel, 
			CalibrationResult.ReprojectionError, 
			CalibrationResult.ErrorMessage);

		if (!bResult)
		{
			if (CalibrationResult.ErrorMessage.IsEmpty())
			{
				CalibrationResult.ErrorMessage = UnknownError;
			}

			FMessageDialog::Open(EAppMsgType::Ok, CalibrationResult.ErrorMessage, TitleError);
			return;
		}

		// Update progress window with final reprojection error
		ProgressWindow->ShowWindow();

		FFormatOrderedArguments Arguments;
		Arguments.Add(FText::FromString(FString::Printf(TEXT("%.3f"), CalibrationResult.ReprojectionError)));

		const FText Message = FText::Format(LOCTEXT("ReprojectionError", "Reprojection Error: {0} pixels"), Arguments);
		ProgressTextWidget->SetText(Message);

		OkayButton->SetEnabled(true);
	}
}

FReply ULensDistortionTool::OnCancelPressed()
{
	CurrentAlgo->CancelCalibration();

	CalibrationTask = {};
	ProgressWindow->HideWindow();
	DistortionWidget->SetEnabled(true);

	return FReply::Handled();
}

FReply ULensDistortionTool::OnOkPressed()
{
	SaveCalibrationResult();

	ProgressWindow->HideWindow();
	DistortionWidget->SetEnabled(true);

	return FReply::Handled();
}

void ULensDistortionTool::SaveCalibrationResult()
{
	ULensFile* LensFile = CameraCalibrationStepsController.Pin()->GetLensFile();

	if (!LensFile)
	{
		return;
	}

	// If the calibration result contains the name of an ST Map file on disk instead of a UTexture, then we attempt to import it for the user
	if (!CalibrationResult.STMap.DistortionMap && !CalibrationResult.STMapFullPath.IsEmpty())
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

		TArray<FString> TextureFileNames;
		TextureFileNames.Add(CalibrationResult.STMapFullPath);
		TArray<UObject*> ImportedImages = AssetToolsModule.Get().ImportAssets(TextureFileNames, FPaths::ProjectContentDir());

		CalibrationResult.STMap.DistortionMap = (ImportedImages.Num() > 0) ? Cast<UTexture>(ImportedImages[0]) : nullptr;
	}

	// Depending on the algo, it is possible that the result feature calibrated distortion parameters or an ST Map.
	// If the result contains any distortion parameters, then the results will be written as a distortion point in the Lens File
	// Otherwise, if the result contains a valid ST Map, then it will be added to the Lens File
	if (CalibrationResult.Parameters.Parameters.Num() > 0)
	{
		if (LensFile->DataMode != ELensDataMode::Parameters)
		{
			LensFile->DataMode = ELensDataMode::Parameters;
			UE_LOG(LogCameraCalibrationEditor, Log, TEXT("The LensFile's data mode was set to ST Map, but the latest calibration result returned distortion parameters. Data mode will change to Parameters."));
		}

		FScopedTransaction Transaction(LOCTEXT("SaveCurrentDistortionData", "Save Current Distortion Data"));
		LensFile->Modify();

		LensFile->AddDistortionPoint(CalibrationResult.EvaluatedFocus, CalibrationResult.EvaluatedZoom, CalibrationResult.Parameters, CalibrationResult.FocalLength);
		LensFile->AddImageCenterPoint(CalibrationResult.EvaluatedFocus, CalibrationResult.EvaluatedZoom, CalibrationResult.ImageCenter);
	}
	else if (CalibrationResult.STMap.DistortionMap)
	{
		if (LensFile->DataMode != ELensDataMode::STMap)
		{
			LensFile->DataMode = ELensDataMode::STMap;
			UE_LOG(LogCameraCalibrationEditor, Log, TEXT("The LensFile's data mode was set to Parameters, but the latest calibration result returned an ST Map. Data mode will change to ST Map."));
		}

		FScopedTransaction Transaction(LOCTEXT("SaveCurrentDistortionData", "Save Current Distortion Data"));
		LensFile->Modify();

		LensFile->AddSTMapPoint(CalibrationResult.EvaluatedFocus, CalibrationResult.EvaluatedZoom, CalibrationResult.STMap);
		LensFile->AddFocalLengthPoint(CalibrationResult.EvaluatedFocus, CalibrationResult.EvaluatedZoom, CalibrationResult.FocalLength);
		LensFile->AddImageCenterPoint(CalibrationResult.EvaluatedFocus, CalibrationResult.EvaluatedZoom, CalibrationResult.ImageCenter);
	}

	if (UCameraLensDistortionAlgo* Algo = GetAlgo())
	{
		Algo->OnDistortionSavedToLens();
	}
}

FCameraCalibrationStepsController* ULensDistortionTool::GetCameraCalibrationStepsController() const
{
	if (!CameraCalibrationStepsController.IsValid())
	{
		return nullptr;
	}

	return CameraCalibrationStepsController.Pin().Get();
}

TArray<FName> ULensDistortionTool::GetAlgos() const
{
	TArray<FName> OutKeys;
	SupportedAlgosMap.GetKeys(OutKeys);
	return OutKeys;
}

bool ULensDistortionTool::IsActive() const
{
	return bIsActive;
}

UMaterialInstanceDynamic* ULensDistortionTool::GetOverlayMID() const
{
	if (CurrentAlgo)
	{
		return AlgoOverlayMIDs.FindRef(CurrentAlgo->FriendlyName()).Get();
	}

	return nullptr;
}

bool ULensDistortionTool::IsOverlayEnabled() const
{
	if (CurrentAlgo)
	{
		return CurrentAlgo->IsOverlayEnabled();
	}

	return false;
}

void ULensDistortionTool::StartCalibrationSession()
{
	if (!SessionInfo.bIsActive)
	{
		SessionInfo.bIsActive = true;
		SessionInfo.StartTime = FDateTime::Now();
	}
}

void ULensDistortionTool::EndCalibrationSession()
{
	if (SessionInfo.bIsActive)
	{
		SessionInfo.bIsActive = false;
		SessionInfo.RowIndex = -1;
	}
}

uint32 ULensDistortionTool::AdvanceSessionRowIndex()
{
	SessionInfo.RowIndex += 1;
	return SessionInfo.RowIndex;
}

FString ULensDistortionTool::GetSessionSaveDir() const
{
	using namespace UE::CameraCalibration::Private;

	const FString SessionDateString = SessionInfo.StartTime.ToString(TEXT("%Y-%m-%d"));
	const FString SessionTimeString = SessionInfo.StartTime.ToString(TEXT("%H-%M-%S"));
	const FString DatasetPrefix = TEXT("Dataset-") + CurrentAlgo->ShortName().ToString() + TEXT("Algorithm-");
	const FString DatasetDir = DatasetPrefix + SessionTimeString;

	const FString ProjectSaveDir = FPaths::ProjectSavedDir() / TEXT("CameraCalibration") / TEXT("LensDistortion");

	return ProjectSaveDir / SessionDateString / DatasetDir;
}

FString ULensDistortionTool::GetRowFilename(int32 RowIndex) const
{
	const FString RowNumString = TEXT("Row") + FString::FromInt(RowIndex) + TEXT("-");
	return RowNumString;
}

void ULensDistortionTool::DeleteExportedRow(const int32& RowIndex) const
{
	if (!GetDefault<UCameraCalibrationSettings>()->IsCalibrationDatasetImportExportEnabled())
	{
		return;
	}

	// Find all files in the directory of the currently active session
	const FString PathName = GetSessionSaveDir();
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *PathName);

	const FString RowNumString = GetRowFilename(RowIndex);

	// Delete any files containing that row number from the session directory
	for (const FString& File : FoundFiles)
	{
		if (File.Contains(RowNumString))
		{
			const FString FullPath = PathName / File;
			IFileManager::Get().Delete(*FullPath);
			UE_LOG(LogCameraCalibrationEditor, Verbose, TEXT("Lens Distortion Tool removed calibration dataset file: %s"), *FullPath);
		}
	}
}

void ULensDistortionTool::ExportCalibrationRow(int32 RowIndex, const TSharedRef<FJsonObject>& RowObject, const FImageView& RowImage)
{
	if (!GetDefault<UCameraCalibrationSettings>()->IsCalibrationDatasetImportExportEnabled())
	{
		return;
	}

	// Start a calibration session (if one is not currently active)
	StartCalibrationSession();

	// Assemble the path and filename for this row based on the session and row index
	const FString PathName = GetSessionSaveDir();
	const FString FileName = GetRowFilename(RowIndex) + FDateTime::Now().ToString(TEXT("%H-%M-%S"));

	const FString JsonFileName = PathName / FileName + TEXT(".json");
	const FString ImageFileName = PathName / FileName + TEXT(".png");

	// Create and open a new Json file for writing, and initialize a JsonWriter to serialize the contents
	if (TUniquePtr<FArchive> FileWriter = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*JsonFileName)))
	{
		TSharedRef< TJsonWriter<> > JsonWriter = TJsonWriterFactory<>::Create(FileWriter.Get());

		// Write the Json row data out and save the file
		FJsonSerializer::Serialize(RowObject, JsonWriter);
		FileWriter->Close();

		UE_LOG(LogCameraCalibrationEditor, Verbose, TEXT("Lens Distortion Tool wrote to dataset row file: %s"), *JsonFileName);
	}

	// If the row has an image to export, save it out to a file
	if (RowImage.RawData != nullptr)
	{
		FImageUtils::SaveImageByExtension(*ImageFileName, RowImage);
	}
}

void ULensDistortionTool::ExportSessionData(const TSharedRef<FJsonObject>& SessionDataObject)
{
	using namespace UE::CameraCalibration::Private;

	if (!GetDefault<UCameraCalibrationSettings>()->IsCalibrationDatasetImportExportEnabled())
	{
		return;
	}

	// Start a calibration session (if one is not currently active)
	StartCalibrationSession();

	// Assemble the path and filename for this row based on the session and row index
	const FString PathName = GetSessionSaveDir();
	const FString FileName = TEXT("SessionData");

	const FString SessionFileName = PathName / FileName + TEXT(".ucamcalib");

	// Delete the existing session data file (if it exists)
	if (IFileManager::Get().FileExists(*SessionFileName))
	{
		IFileManager::Get().Delete(*SessionFileName);
	}

	// Create and open a new Json file for writing, and initialize a JsonWriter to serialize the contents
	if (TUniquePtr<FArchive> FileWriter = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*SessionFileName)))
	{
		TSharedRef< TJsonWriter<> > JsonWriter = TJsonWriterFactory<>::Create(FileWriter.Get());

		const FString SessionDateTimeString = SessionInfo.StartTime.ToString(TEXT("%Y-%m-%d")) + TEXT("-") + SessionInfo.StartTime.ToString(TEXT("%H-%M-%S"));

		SessionDataObject->SetStringField(LensDistortionTool::SessionDateTimeField, SessionDateTimeString);
		SessionDataObject->SetStringField(LensDistortionTool::AlgoNameField, CurrentAlgo->FriendlyName().ToString());

		// Write the Json row data out and save the file
		FJsonSerializer::Serialize(SessionDataObject, JsonWriter);
		FileWriter->Close();

		UE_LOG(LogCameraCalibrationEditor, Verbose, TEXT("Lens Distortion Tool wrote to dataset session file: %s"), *SessionFileName);
	}
}

void ULensDistortionTool::ImportCalibrationDataset()
{
	using namespace UE::CameraCalibration::Private;

	if (!GetDefault<UCameraCalibrationSettings>()->IsCalibrationDatasetImportExportEnabled())
	{
		return;
	}

	// If there is existing calibration data that will be overwritten during import, ask the user to confirm that they want to continue
	if (CurrentAlgo->HasCalibrationData())
	{
		const FText ConfirmationMessage = LOCTEXT(
			"ImportDatasetConfirmationMessage",
			"There are existing calibration rows which will be removed during the import process. Do you want to proceed with the import?");

		if (FMessageDialog::Open(EAppMsgType::YesNo, ConfirmationMessage) == EAppReturnType::No)
		{
			return;
		}
	}

	// Open a file dialog to select a .ucamcalib session data file 
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const FString Title = TEXT("Import Camera Calibration Dataset");
	const FString DefaultPath = FPaths::ProjectSavedDir() / TEXT("CameraCalibration") / TEXT("LensDistortion");;
	const FString DefaultFile = TEXT("");
	const FString FileTypes = TEXT("Camera Calibration Dataset|*.ucamcalib");
	const uint32 OpenFileFlags = 0;

	// Note, OpenFileFlags is not set to "Multiple" so we only expect one file to be selected
	TArray<FString> SelectedFileNames;
	const bool bFileSelected = DesktopPlatform->OpenFileDialog(ParentWindowHandle, Title, DefaultPath, DefaultFile, FileTypes, OpenFileFlags, SelectedFileNames);

	// Early-out if no calibration file was selected
	if (!bFileSelected || SelectedFileNames.Num() < 1)
	{
		return;
	}

	// Parse the session data filename and the directory from the full path
	const FString SessionFileName = FPaths::GetCleanFilename(SelectedFileNames[0]);
	const FString SelectedDirectory = FPaths::GetPath(SelectedFileNames[0]);

	// Find all json files in the selected directory (this will not include the .ucamcalib session date file)
	TArray<FString> FoundFiles;
	const FString FileExtension = TEXT(".json");
	IFileManager::Get().FindFiles(FoundFiles, *SelectedDirectory, *FileExtension);

	// Early-out if selected directory has no json files to import
	if (FoundFiles.Num() < 1)
	{
		const FText ErrorMessage = LOCTEXT("NoJsonFilesFound", "The selected directory has no .json files to import.");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	// Import the session data
	FDateTime ImportedSessionDateTime = FDateTime::Now();
	{
		const FString SessionFile = SelectedDirectory / SessionFileName;

		// Open the Json file for reading, and initialize a JsonReader to parse the contents
		if (TUniquePtr<FArchive> FileReader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*SessionFile)))
		{
			TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(FileReader.Get());

			// Deserialize the row data from the Json file into a Json object
			TSharedPtr<FJsonObject> JsonSessionData = MakeShared<FJsonObject>();
			if (FJsonSerializer::Deserialize(JsonReader, JsonSessionData))
			{
				// Import the session date/time so that we can restore the imported session
				FString SessionDateTimeString;
				if (JsonSessionData->TryGetStringField(LensDistortionTool::SessionDateTimeField, SessionDateTimeString))
				{
					ensureMsgf(FDateTime::Parse(SessionDateTimeString, ImportedSessionDateTime), TEXT("Failed to parse imported session date and time"));
				}
				else
				{
					UE_LOG(LogCameraCalibrationEditor, Verbose, TEXT("Lens Distortion Tool failed to deserialize the date/time from the session file: %s"), *SessionFile);
				}

				// Import the algo name so we can set the appropriate algo before importing the row data
				FString AlgoString;
				if (JsonSessionData->TryGetStringField(LensDistortionTool::AlgoNameField, AlgoString))
				{
					// Ensure that the algo name matches one of the algos for this tool
					const FName AlgoName = FName(*AlgoString);
					if (AlgosMap.Contains(AlgoName))
					{
						SetAlgo(AlgoName);
					}
					else
					{
						const FText ErrorMessage = LOCTEXT("UnknownCalibrationAlgo", "The selected dataset does not represent a lens distortion calibration. Choose a different .ucamcalib dataset file from a lens distortion calibration.");
						FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
						return;
					}
				}
				else
				{
					UE_LOG(LogCameraCalibrationEditor, Verbose, TEXT("Lens Distortion Tool failed to deserialize the algo name from the session file: %s"), *SessionFile);
				}

				CurrentAlgo->ImportSessionData(JsonSessionData.ToSharedRef());
			}
			else
			{
				UE_LOG(LogCameraCalibrationEditor, Verbose, TEXT("Lens Distortion Tool failed to deserialize dataset session file: %s"), *SessionFile);
			}
		}
	}

	CurrentAlgo->PreImportCalibrationData();

	// Initialize a maximum row index which will be used to set the current session row index if the user wants to add additional rows after importing
	int32 MaxRowIndex = -1;

	for (const FString& File : FoundFiles)
	{
		const FString JsonFileName = SelectedDirectory / File;
		const FString ImageFileName = JsonFileName.Replace(TEXT(".json"), TEXT(".png"));

		// Load the PNG image file for this row into an FImage
		FImage RowImage;
		if (IFileManager::Get().FileExists(*ImageFileName))
		{
			FImageUtils::LoadImage(*ImageFileName, RowImage);
		}

		// Open the Json file for reading, and initialize a JsonReader to parse the contents
		if (TUniquePtr<FArchive> FileReader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*JsonFileName)))
		{
			TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(FileReader.Get());

			// Deserialize the row data from the Json file into a Json object
			TSharedPtr<FJsonObject> JsonRowData = MakeShared<FJsonObject>();
			if (FJsonSerializer::Deserialize(JsonReader, JsonRowData))
			{
				int32 RowNum = CurrentAlgo->ImportCalibrationRow(JsonRowData.ToSharedRef(), RowImage);
				MaxRowIndex = FMath::Max(MaxRowIndex, RowNum);
			}
			else
			{
				UE_LOG(LogCameraCalibrationEditor, Verbose, TEXT("Lens Distortion Tool failed to deserialize the dataset row file: %s"), *JsonFileName);
			}
		}
	}

	CurrentAlgo->PostImportCalibrationData();

	// Set the current session's start date/time and row index to match what was just imported to support adding/deleting rows
	SessionInfo.bIsActive = true;
	SessionInfo.StartTime = ImportedSessionDateTime;
	SessionInfo.RowIndex = MaxRowIndex;
}

#undef LOCTEXT_NAMESPACE
