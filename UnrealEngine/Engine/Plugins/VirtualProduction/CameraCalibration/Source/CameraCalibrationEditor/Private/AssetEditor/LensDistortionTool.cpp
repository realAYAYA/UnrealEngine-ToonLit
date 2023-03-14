// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensDistortionTool.h"

#include "CameraCalibrationEditorLog.h"
#include "CameraCalibrationSettings.h"
#include "CameraCalibrationStepsController.h"
#include "CameraCalibrationTypes.h"
#include "CameraLensDistortionAlgo.h"
#include "DesktopPlatformModule.h"
#include "Dom/JsonObject.h"
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
#include "SLensDistortionToolPanel.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "LensDistortionTool"

namespace UE::CameraCalibration::Private::LensDistortionTool
{
	static const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("CameraCalibration") / TEXT("LensDistortion");
	static const FString SessionDateTimeField(TEXT("SessionDateTime"));
	static const FString AlgoNameField(TEXT("AlgoName"));
}

void ULensDistortionTool::Initialize(TWeakPtr<FCameraCalibrationStepsController> InCameraCalibrationStepController)
{
	CameraCalibrationStepsController = InCameraCalibrationStepController;

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
}

void ULensDistortionTool::Shutdown()
{
	if (CurrentAlgo)
	{
		CurrentAlgo->Shutdown();
		CurrentAlgo = nullptr;

		EndCalibrationSession();
	}
}

void ULensDistortionTool::Tick(float DeltaTime)
{
	if (CurrentAlgo)
	{
		CurrentAlgo->Tick(DeltaTime);
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

	return CurrentAlgo->OnViewportClicked(MyGeometry, MouseEvent);
}

TSharedRef<SWidget> ULensDistortionTool::BuildUI()
{
	return SNew(SLensDistortionToolPanel, this);
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
	if (!CameraCalibrationStepsController.IsValid())
	{
		return;
	}

	UCameraLensDistortionAlgo* Algo = GetAlgo();
	
	if (!Algo)
	{
		FText ErrorMessage = LOCTEXT("NoAlgoFound", "No algo found");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}
	
	ULensFile* LensFile = CameraCalibrationStepsController.Pin()->GetLensFile();

	if (!LensFile)
	{
		FText ErrorMessage = LOCTEXT("NoLensFile", "No Lens File");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	const FText TitleError = LOCTEXT("LensCalibrationError", "Lens Calibration Error");
	const FText TitleInfo = LOCTEXT("LensCalibrationInfo", "Lens Calibration Info");

	float Focus;
	float Zoom;
	FDistortionInfo DistortionInfo;
	FFocalLengthInfo FocalLengthInfo;
	FImageCenterInfo ImageCenterInfo;
	TSubclassOf<ULensModel> LensModel;
	double Error;

	// Get distortion value, and if errors, inform the user.
	{
		FText ErrorMessage;

		if (!Algo->GetLensDistortion(Focus, Zoom, DistortionInfo, FocalLengthInfo, ImageCenterInfo, LensModel, Error, ErrorMessage))
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);
			return;
		}
	}

	// Show reprojection error
	{
		FFormatOrderedArguments Arguments;
		Arguments.Add(FText::FromString(FString::Printf(TEXT("%.2f"), Error)));

		const FText Message = FText::Format(LOCTEXT("ReprojectionError", "RMS Reprojection Error: {0} pixels"), Arguments);

		// Allow the user to cancel adding to the LUT if the reprojection error is unacceptable.
		if (FMessageDialog::Open(EAppMsgType::OkCancel, Message, &TitleInfo) != EAppReturnType::Ok)
		{
			return;
		}
	}

	if (LensFile->HasSamples(ELensDataCategory::Distortion) && LensFile->LensInfo.LensModel != LensModel)
	{
		const FText ErrorMessage = LOCTEXT("LensDistortionModelMismatch", "There is a distortion model mismatch between the new and existing samples");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SaveCurrentDistortionData", "Save Current Distortion Data"));

 	LensFile->Modify();

	LensFile->AddDistortionPoint(Focus, Zoom, DistortionInfo, FocalLengthInfo);
	LensFile->AddImageCenterPoint(Focus, Zoom, ImageCenterInfo);

	Algo->OnDistortionSavedToLens();
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
	AlgosMap.GetKeys(OutKeys);
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

	return LensDistortionTool::SaveDir / SessionDateString / DatasetDir;
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
	const FString DefaultPath = LensDistortionTool::SaveDir;
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
