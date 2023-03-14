// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScreenShotManager.h"
#include "AutomationWorkerMessages.h"
#include "Async/ParallelFor.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "MessageEndpointBuilder.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "Misc/FilterCollection.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "PlatformInfo.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "ScreenShotComparisonSettings.h"


DEFINE_LOG_CATEGORY(LogScreenShotManager);

class FScreenshotComparisons
{
public:
	FString ApprovedFolder;
	FString UnapprovedFolder;

	TArray<FString> Existing;
	TArray<FString> New;
	TArray<FString> Missing;
};

FScreenShotManager::FScreenShotManager()
{
	FModuleManager::Get().LoadModuleChecked(FName("ImageWrapper"));

	ScreenshotResultsFolder = FPaths::AutomationReportsDir();
	ScreenshotTempDeltaFolder = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Delta/"));

	const UScreenShotComparisonSettings* ScreenshotSettings = GetDefault<UScreenShotComparisonSettings>();
	bUseConfidentialPlatformPaths = ScreenshotSettings->bUseConfidentialPlatformPathsForSavedResults;

	// the automation controller owns the screenshot manager so we don't have to worry about outliving it
	//AutomationController->OnTestsAvailable().AddRaw(this, &FScreenShotManager::OnTestAvailableCallback);
	// #agrant todo - we don't want this dependency and higher level code should clear the folders before running new tests

	BuildFallbackPlatformsListFromConfig(ScreenshotSettings);
}

FScreenShotManager::~FScreenShotManager()
{
}



FString FScreenShotManager::GetPathComponentForRHI(const FAutomationScreenshotMetadata& MetaData) const
{
	FString RHIInfo = MetaData.Rhi;
	
	if (!RHIInfo.IsEmpty())
	{
		RHIInfo += TEXT("_");
	}

	RHIInfo += MetaData.FeatureLevel;

	return RHIInfo;
}


FString FScreenShotManager::GetPathComponentForPlatformAndRHI(const FAutomationScreenshotMetadata& MetaData) const
{
	return FPaths::Combine(MetaData.Platform, GetPathComponentForRHI(MetaData));
}

/**
 * Images are now preferred to be under MapOrContext/ImageName/Plat/RHI etc
 */
FString FScreenShotManager::GetPathComponentForTestImages(const FAutomationScreenshotMetadata& MetaData) const
{
	return FPaths::Combine(MetaData.Context, *MetaData.ScreenShotName);
}

FString FScreenShotManager::GetApprovedFolderForImageWithOptions(const FAutomationScreenshotMetadata& MetaData, EApprovedFolderOptions InOptions) const
{
	// plaform will be something like PS4
	// RHI = SM5
	const FDataDrivenPlatformInfo& PlatInfo = FDataDrivenPlatformInfoRegistry::GetPlatformInfo(MetaData.Platform);

	bool bUsePlatformPath = PlatInfo.bIsConfidential && (InOptions & EApprovedFolderOptions::UsePlatformFolders) == 0;

	// Test folder will be MapOrContext/ImageName
	FString TestFolder = GetPathComponentForTestImages(MetaData);

	FString OutPath = FPaths::ProjectDir();

	if (bUsePlatformPath)
	{
		OutPath = FPaths::Combine(OutPath, TEXT("Platforms"), MetaData.Platform, TEXT("Test/Screenshots/"), TestFolder, GetPathComponentForRHI(MetaData));
	}
	else
	{
		OutPath = FPaths::Combine(OutPath, TEXT("Test/Screenshots/"), TestFolder, MetaData.Platform, GetPathComponentForRHI(MetaData));
	}

	if (!OutPath.EndsWith(TEXT("/")))
	{
		OutPath += TEXT("/");
	}

	return OutPath;
}

FString FScreenShotManager::GetIdealApprovedFolderForImage(const FAutomationScreenshotMetadata& MetaData) const
{
	EApprovedFolderOptions DefaultOptions = bUseConfidentialPlatformPaths ? EApprovedFolderOptions::None : EApprovedFolderOptions::UsePlatformFolders;
	return GetApprovedFolderForImageWithOptions(MetaData, DefaultOptions);
}

TArray<FString> FScreenShotManager::FindApprovedImages(const FAutomationScreenshotMetadata& IncomingMetaData)
{
	TArray<FString> ApprovedImages;

	EApprovedFolderOptions Options = bUseConfidentialPlatformPaths ? EApprovedFolderOptions::None : EApprovedFolderOptions::UsePlatformFolders;

	// check out standard path using whether confidential platforms are in a separate tree
	FString ApprovedPath = GetApprovedFolderForImageWithOptions(IncomingMetaData, Options);
	IFileManager::Get().FindFilesRecursive(ApprovedImages, *ApprovedPath, TEXT("*.png"), true, false);

	// check again, but try legacy paths
	if (!ApprovedImages.Num())
	{
		ApprovedPath = GetApprovedFolderForImageWithOptions(IncomingMetaData, Options | EApprovedFolderOptions::UseLegacyPaths);
		IFileManager::Get().FindFilesRecursive(ApprovedImages, *ApprovedPath, TEXT("*.png"), true, false);
	}

	// if we're a blank and bUseConfidentialPlatformPaths, try without that
	if (ApprovedImages.Num() == 0 && bUseConfidentialPlatformPaths)
	{
		// check legacy paths.
		ApprovedPath = FPaths::GetPath(GetApprovedFolderForImageWithOptions(IncomingMetaData, EApprovedFolderOptions::None));
		IFileManager::Get().FindFilesRecursive(ApprovedImages, *ApprovedPath, TEXT("*.png"), true, false);

		// check again, but try legacy paths
		if (!ApprovedImages.Num())
		{
			ApprovedPath = GetApprovedFolderForImageWithOptions(IncomingMetaData, EApprovedFolderOptions::UseLegacyPaths);
			IFileManager::Get().FindFilesRecursive(ApprovedImages, *ApprovedPath, TEXT("*.png"), true, false);
		}
	}

	// @todo(agrant): find fallback images if they don't exist at this point
	if (ApprovedImages.Num() == 0)
	{
		FString CurrentPlatformRHI = GetPathComponentForPlatformAndRHI(IncomingMetaData);

		UE_LOG(LogScreenShotManager, Log, TEXT("No ideal-image found at %s. Checking fallback images"), *ApprovedPath);

		while (ApprovedImages.Num() == 0)
		{
			FString* FallbackPlatformRHI = FallbackPlatforms.Find(CurrentPlatformRHI);
			if (!FallbackPlatformRHI)
			{
				break;
			}

			CurrentPlatformRHI = *FallbackPlatformRHI;

			TArray<FString> Components;
			CurrentPlatformRHI.ParseIntoArray(Components, TEXT("/"));			

			if (Components.Num() == 2)
			{
				TArray<FString> FeatureLevels;
				Components[1].ParseIntoArray(FeatureLevels, TEXT("_"));

				FAutomationScreenshotMetadata CopiedMetaData = IncomingMetaData;
				CopiedMetaData.Platform = Components[0];
				CopiedMetaData.Rhi = FeatureLevels[0];
				CopiedMetaData.FeatureLevel = FeatureLevels[1];

				ApprovedPath = FPaths::GetPath(GetIdealApprovedFolderForImage(CopiedMetaData));
				IFileManager::Get().FindFilesRecursive(ApprovedImages, *ApprovedPath, TEXT("*.png"), true, false);

				// check again, but try legacy paths
				if (!ApprovedImages.Num())
				{
					ApprovedPath = GetApprovedFolderForImageWithOptions(CopiedMetaData, EApprovedFolderOptions::UseLegacyPaths);
					IFileManager::Get().FindFilesRecursive(ApprovedImages, *ApprovedPath, TEXT("*.png"), true, false);
				}

				if (ApprovedImages.Num())
				{
					UE_LOG(LogScreenShotManager, Log, TEXT("Using fallback images from %s"), *ApprovedPath);
				}
			}
			else
			{
				UE_LOG(LogScreenShotManager, Error, TEXT("Invalid fallback Platform/RHI string %s"), *CurrentPlatformRHI);
			}	
		}
	}

	return ApprovedImages;
}

/* IScreenShotManager event handlers
 *****************************************************************************/

TFuture<FImageComparisonResult> FScreenShotManager::CompareScreenshotAsync(const FString& IncomingPath, const FAutomationScreenshotMetadata& MetaData, const EScreenShotCompareOptions Options)
{
	return Async(EAsyncExecution::Thread, [=] () { return CompareScreenshot(IncomingPath, MetaData, Options); });
}


/**
 * Takes an incoming file and returns a comparison based on comparing it to a reference image that exists in the project.
 * The comparison results (incoming image, approved image, comparison image, report metadata) will be placed in the provided
 * output folder.
 *
 * The incoming path must contain a recognized platform name as a path component, with the feature-level info of that platform
 * as the next component.
 * 
 * E.g. <path> must contain <early-path>/Windows/D3D12_SM5/<later-path>, or <early-path>/PS4/SM5/<later-path> etc and must exist
 * under the path returned by IScreenshotManager::GetIncomingFolder
 *
 * Comparison images are loaded from <project>/Test/<path> unless ScreenShotComparisonSettings.bUseConfidentialPlatformPathsForSavedResults 
 * is set in which case they will come from <project>/Platforms/<platform>/Test
 * 
 * @param InUnapprovedIncomingFilePath Path
 * @param ResultsSubFolder 
 *
 * @return FImageComparisonResult
 */
FImageComparisonResult FScreenShotManager::CompareScreenshot(const FString& InUnapprovedIncomingFilePath, const FAutomationScreenshotMetadata& IncomingMetaData, const EScreenShotCompareOptions Options)
{
	// #agrant todo: Handle bad paths

	// get the ideal path for our approved image. This is the path that a file would be at if it matches our platform and RHI
	FString IdealApprovedFolderPath = GetIdealApprovedFolderForImage(IncomingMetaData);

	FString ResultsSubFolder = GetPathComponentForTestImages(IncomingMetaData);

	// If the metadata for the screenshot does not provide tolerance rules, use these instead.
	FImageTolerance DefaultTolerance = FImageTolerance::DefaultIgnoreLess;
	DefaultTolerance.IgnoreAntiAliasing = true;

	// find all the approved images we can use. This will find fallback images from other platforms if necessary
	TArray<FString> ApprovedDeviceShots = FindApprovedImages(IncomingMetaData);

	TOptional<FAutomationScreenshotMetadata> NearestExistingApprovedImageMetadata;

	// This is copied over later, so don't set any properties...
	FImageComparisonResult ComparisonResult;

	// Use found shots as ground truth
	if (ApprovedDeviceShots.Num() > 0)
	{	
		TOptional<FAutomationScreenshotMetadata> ExistingMetadata;
		{
			// Always read the metadata file from the unapproved location, as we may have introduced new comparison rules.
			FString MetadataFile = FPaths::ChangeExtension(InUnapprovedIncomingFilePath, ".json");

			FString Json;
			if ( FFileHelper::LoadFileToString(Json, *MetadataFile) )
			{
				FAutomationScreenshotMetadata Metadata;
				if ( FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Metadata, 0, 0) )
				{
					ExistingMetadata = Metadata;
				}
			}
		}

		FString NearestExistingApprovedImage;
		

		if ( ExistingMetadata.IsSet() )
		{
			int32 MatchScore = -1;

			for ( FString ApprovedShot : ApprovedDeviceShots )
			{	
				FString ApprovedMetadataFile = FPaths::ChangeExtension(ApprovedShot, ".json");

				FString Json;
				if ( FFileHelper::LoadFileToString(Json, *ApprovedMetadataFile) )
				{
					FAutomationScreenshotMetadata Metadata;
					if ( FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Metadata, 0, 0) )
					{
						int32 Comparison = Metadata.Compare(ExistingMetadata.GetValue());
						if ( Comparison > MatchScore )
						{
							MatchScore = Comparison;
							NearestExistingApprovedImage = ApprovedShot;
							NearestExistingApprovedImageMetadata = Metadata;
						}
					}
				}
			}
		}
		else
		{
			// TODO no metadata how do I pick a good shot?
			NearestExistingApprovedImage = ApprovedDeviceShots[0];
		}

		FImageTolerance Tolerance = DefaultTolerance;

		if ( ExistingMetadata.IsSet() && ExistingMetadata->bHasComparisonRules )
		{
			Tolerance.Red = ExistingMetadata->ToleranceRed;
			Tolerance.Green = ExistingMetadata->ToleranceGreen;
			Tolerance.Blue = ExistingMetadata->ToleranceBlue;
			Tolerance.Alpha = ExistingMetadata->ToleranceAlpha;
			Tolerance.MinBrightness = ExistingMetadata->ToleranceMinBrightness;
			Tolerance.MaxBrightness = ExistingMetadata->ToleranceMaxBrightness;
			Tolerance.IgnoreAntiAliasing = ExistingMetadata->bIgnoreAntiAliasing;
			Tolerance.IgnoreColors = ExistingMetadata->bIgnoreColors;
			Tolerance.MaximumLocalError = ExistingMetadata->MaximumLocalError;
			Tolerance.MaximumGlobalError = ExistingMetadata->MaximumGlobalError;
		}

		// TODO Think about using SSIM, but needs local SSIM as well as Global SSIM, same as the basic comparison.
		//double SSIM = Comparer.CompareStructuralSimilarity(ApprovedFullPath, UnapprovedFullPath, FImageComparer::EStructuralSimilarityComponent::Luminance);
		//printf("%f\n", SSIM);

		FImageComparer Comparer;

		ComparisonResult = Comparer.Compare(NearestExistingApprovedImage, InUnapprovedIncomingFilePath, Tolerance, ScreenshotTempDeltaFolder);
	}
	else
	{
		// We can't find a ground truth, so it's a new comparison.
		ComparisonResult.IncomingFilePath = InUnapprovedIncomingFilePath;
		ComparisonResult.CreationTime = FDateTime::Now();

		UE_LOG(LogScreenShotManager, Log, TEXT("No ideal-image found. Assuming %s is a new test image"), *InUnapprovedIncomingFilePath);
	}

	ComparisonResult.SourcePlatform = IncomingMetaData.Platform;
	ComparisonResult.SourceRHI = IncomingMetaData.Rhi;
	ComparisonResult.IdealApprovedFolderPath = IdealApprovedFolderPath;

	// Result paths should be relative to the project. Note this may be empty, and if it is MakePathRelative returns
	// a non empty relative path... but we want it to stay empty as that's how we signal that no approved file exists
	if (!ComparisonResult.ApprovedFilePath.IsEmpty())
	{
		FPaths::MakePathRelativeTo(ComparisonResult.ApprovedFilePath, *FPaths::ProjectDir());
	}

	if (!ComparisonResult.ComparisonFilePath.IsEmpty())
	{
		FPaths::MakePathRelativeTo(ComparisonResult.ComparisonFilePath, *FPaths::ProjectDir());
	}

	// these two must exist...
	FPaths::MakePathRelativeTo(ComparisonResult.IncomingFilePath, *FPaths::ProjectDir());
	FPaths::MakePathRelativeTo(ComparisonResult.IdealApprovedFolderPath, *FPaths::ProjectDir());

	// Report path is something like Test/Context/ that can be freely moved around and relocated with the relative paths
	// to the data remaining intact
	FString ReportPathOnDisk = FPaths::Combine(ScreenshotResultsFolder, ResultsSubFolder, IncomingMetaData.Platform, GetPathComponentForRHI(IncomingMetaData), TEXT("/"));

	FString ProjectDir = FPaths::GetPath(FPaths::GetProjectFilePath());

	/*
		Now create copies of all three images for the report. We use copies (a move in the case of the delta image) so that the report folders
		can be moved around without any external references. This is vital for CIS where we want the results to be served up via HTTP or just 
		by browsing without needing the matching project directory from the time or any other saved data.
	*/
	
	// files we write/copy to for reports
	ComparisonResult.ReportApprovedFilePath = FPaths::Combine(ReportPathOnDisk, TEXT("Approved.png")); 
	ComparisonResult.ReportIncomingFilePath = FPaths::Combine(ReportPathOnDisk, TEXT("Incoming.png"));
	ComparisonResult.ReportComparisonFilePath = FPaths::Combine(ReportPathOnDisk, TEXT("Delta.png"));

	// If not ideal the make the approved name contain the source for clarity, since it will be under a Plat/RHI/Feature level folder for the platform being tested
	if (!ComparisonResult.IsIdeal() && !ComparisonResult.IsNew())
	{
		check(NearestExistingApprovedImageMetadata.IsSet());
		FString ApprovedName = FString::Printf(TEXT("Approved_%s.png"), *GetPathComponentForPlatformAndRHI(*NearestExistingApprovedImageMetadata).Replace(TEXT("/"), TEXT("_")));
		ComparisonResult.ReportApprovedFilePath = FPaths::Combine(ReportPathOnDisk, ApprovedName);
	}

	/*
		First we need the incoming file in the report. If the calling code wants to keep the image then do a copy, 
		otherwise move it
	*/
	// #agrant todo - code in AutomationControllerManager requires these paths and at the moment does not have enough info to use the report versions. Need to also change
	// the delta image below back to a move as well
//	bool CanMoveImage = false; // Options != EScreenShotCompareOptions::KeepImage
//	if (CanMoveImage == false)
	{
		// copy the incoming file to the report path 
		FString IncomingFileFullPath = *FPaths::Combine(ProjectDir, ComparisonResult.IncomingFilePath);

		if (IFileManager::Get().Copy(*ComparisonResult.ReportIncomingFilePath, *IncomingFileFullPath, true, true) == COPY_OK)
		{
			IFileManager::Get().Copy(*FPaths::ChangeExtension(ComparisonResult.ReportIncomingFilePath, ".json"), *FPaths::ChangeExtension(IncomingFileFullPath, ".json"), true, true);
		}
		else
		{
			UE_LOG(LogScreenShotManager, Error, TEXT("Failed to copy incoming image to %s"), *ComparisonResult.ReportApprovedFilePath);
		}
	}
/*	else
	{
		// we can discard the incoming image, so just move it...
		FString IncomingFileFullPath = *FPaths::Combine(ProjectDir, ComparisonResult.IncomingFilePath);

		if (IFileManager::Get().Move(*ComparisonResult.ReportIncomingFilePath, *IncomingFileFullPath, true, true))
		{
			IFileManager::Get().Move(*FPaths::ChangeExtension(ComparisonResult.ReportIncomingFilePath, ".json"), *FPaths::ChangeExtension(IncomingFileFullPath, ".json"), true, true);
		}
		else
		{
			UE_LOG(LogScreenShotManager, Error, TEXT("Failed to copy incoming image to %s"), *ComparisonResult.ReportApprovedFilePath);
		}
	}*/

	// move any incoming renderdoc file if one existed
	FString IncomingTraceFile = FPaths::ChangeExtension(InUnapprovedIncomingFilePath, TEXT(".rdc"));
	if (IFileManager::Get().FileExists(*IncomingTraceFile))
	{
		IFileManager::Get().Move(*(FPaths::Combine(ReportPathOnDisk, TEXT("Incoming.rdc"))), *IncomingTraceFile, true, true);
	}

	// copy the comparison image if we generated one
	if (ComparisonResult.ComparisonFilePath.Len())
	{
		FString ComparisonFileFullPath = *FPaths::Combine(ProjectDir, ComparisonResult.ComparisonFilePath);

		if (IFileManager::Get().Copy(*ComparisonResult.ReportComparisonFilePath, *ComparisonFileFullPath, true, true) == COPY_OK)
		{
			// nothing else to move for this case
		}
		else
		{
			UE_LOG(LogScreenShotManager, Error, TEXT("Failed to move delta image to %s"), *ComparisonResult.ReportComparisonFilePath);
		}
	}

	/*
		Make a copy of the approved file if it exists
	*/
	if (ComparisonResult.ApprovedFilePath.Len())
	{
		// Make a copy of the approved png called 'approved.png'
		FString ApprovedFileFullPath = *FPaths::Combine(ProjectDir, ComparisonResult.ApprovedFilePath);

		if (IFileManager::Get().Copy(*ComparisonResult.ReportApprovedFilePath, *ApprovedFileFullPath, true, true) == COPY_OK)
		{
			IFileManager::Get().Copy(*FPaths::ChangeExtension(ComparisonResult.ReportApprovedFilePath, ".json"), *FPaths::ChangeExtension(ApprovedFileFullPath, ".json"), true, true);
		}
		else
		{
			UE_LOG(LogScreenShotManager, Error, TEXT("Failed to copy approved image to %s"), *ComparisonResult.ReportApprovedFilePath);
		}
	}
		
	// make all these paths relative to the report path so the report can be relocated outside of the project directory
	// and the files still found.
	FPaths::MakePathRelativeTo(ComparisonResult.ReportApprovedFilePath, *ScreenshotResultsFolder);
	FPaths::MakePathRelativeTo(ComparisonResult.ReportIncomingFilePath, *ScreenshotResultsFolder);
	FPaths::MakePathRelativeTo(ComparisonResult.ReportComparisonFilePath, *ScreenshotResultsFolder);

	// save the result at to a report
	FString Json;
	if ( FJsonObjectConverter::UStructToJsonObjectString(ComparisonResult, Json) )
	{
		FString ComparisonReportFile = FPaths::Combine(ReportPathOnDisk, TEXT("Report.json"));
		FFileHelper::SaveStringToFile(Json, *ComparisonReportFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

		UE_LOG(LogScreenShotManager, Log, TEXT("Saved report to %s"), *ComparisonReportFile);
	}

	return ComparisonResult;
}


FScreenshotExportResult FScreenShotManager::ExportScreenshotComparisonResult(FString ScreenshotName, FString RootExportFolder)
{
	FPaths::NormalizeDirectoryName(RootExportFolder);

	if (RootExportFolder.IsEmpty())
	{
		RootExportFolder = GetDefaultExportDirectory();
	}

	FScreenshotExportResult Results;
	Results.Success = false;
	Results.ExportPath = RootExportFolder;

	FString Destination = RootExportFolder / ScreenshotName;
	if (!IFileManager::Get().MakeDirectory(*Destination, /*Tree =*/true))
	{
		return Results;
	}

	CopyDirectory(Destination, ScreenshotResultsFolder / ScreenshotName);

	Results.Success = true;
	return Results;
}

bool FScreenShotManager::OpenComparisonReports(FString ImportPath, TArray<FComparisonReport>& OutReports)
{
	OutReports.Reset();

	FPaths::NormalizeDirectoryName(ImportPath);
	ImportPath += TEXT("/");

	TArray<FString> ComparisonReportPaths;
	IFileManager::Get().FindFilesRecursive(ComparisonReportPaths, *ImportPath, TEXT("Report.json"), /*Files=*/true, /*Directories=*/false, /*bClearFileNames=*/ false);

	for ( const FString& ReportPath : ComparisonReportPaths )
	{
		FString JsonString;
		if ( FFileHelper::LoadFileToString(JsonString, *ReportPath) )
		{
			TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);

			TSharedPtr<FJsonObject> JsonComparisonReport;
			if ( !FJsonSerializer::Deserialize(JsonReader, JsonComparisonReport) )
			{
				return false;
			}

			FImageComparisonResult ComparisonResult;
			ComparisonResult.SetInvalid();
			if ( FJsonObjectConverter::JsonObjectToUStruct(JsonComparisonReport.ToSharedRef(), &ComparisonResult, 0, 0) )
			{
				if (ComparisonResult.IsValid())
				{
					FComparisonReport Report(ImportPath, ReportPath);
					Report.SetComparisonResult(ComparisonResult);
					OutReports.Add(Report);
				}
				else
				{
					UE_LOG(LogScreenShotManager, Error, TEXT("Report %s has invalid version '%d' (Current Version=%d)"), *ReportPath, ComparisonResult.Version, int32(ComparisonResult.CurrentVersion));
				}
			}
		}
	}

	return true;
}

FString FScreenShotManager::GetDefaultExportDirectory() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Exported/imageCompare"));
}

void FScreenShotManager::CopyDirectory(const FString& DestDir, const FString& SrcDir)
{
	TArray<FString> FilesToCopy;

	FString NormalizedSrc = SrcDir;
	FPaths::NormalizeDirectoryName(NormalizedSrc);

	IFileManager::Get().FindFilesRecursive(FilesToCopy, *SrcDir, TEXT("*"), /*Files=*/true, /*Directories=*/false);

	ParallelFor(FilesToCopy.Num(), [&](int32 Index)
		{
			const FString& SourceFilePath = FilesToCopy[Index];
			FString DestFilePath = FPaths::Combine(DestDir, SourceFilePath.RightChop(SrcDir.Len()));
			IFileManager::Get().Copy(*DestFilePath, *SourceFilePath, true, true);
		});
}

void FScreenShotManager::BuildFallbackPlatformsListFromConfig(const UScreenShotComparisonSettings* InSettings)
{
	FallbackPlatforms.Empty();

	if (InSettings->ScreenshotFallbackPlatforms.Num() > 0)
	{
		for (const FScreenshotFallbackEntry& Entry : InSettings->ScreenshotFallbackPlatforms)
		{
			FString Parent = Entry.Parent;
			FString Child = Entry.Child;

			if (Parent.StartsWith(TEXT("/")))
			{
				UE_LOG(LogScreenShotManager, Warning, TEXT("Fallback parent is malformed and has a leading '/'. This will be stripped but please correct. '%s'"), *Parent);
				Parent = Parent.RightChop(1);
			}

			// These are used as folders so ensure they match the expected layout
			if (Child.StartsWith(TEXT("/")))
			{
				UE_LOG(LogScreenShotManager, Warning, TEXT("Fallback child is malformed and has a leading '/'. This will be stripped but please correct. '%s'"), *Child);
				Child = Child.RightChop(1);
			}

			FString& ParentValue = FallbackPlatforms.FindOrAdd(Child);
			ParentValue = Parent;
		}
	}

	// legacy path
	if (GConfig)
	{
		for (const FString& ConfigFilename : GConfig->GetFilenames())
		{
			FConfigSection* FallbackSection = GConfig->GetSectionPrivate(TEXT("AutomationTestFallbackHierarchy"), false, true, ConfigFilename);
			if (FallbackSection)
			{
				UE_LOG(LogScreenShotManager, Warning, TEXT("Please move FallbackPlatform entries in [AutomationTestFallbackHierarchy] to +ScreenshotFallbackPlatforms= under[/Script/ScreenShotComparisonTools.ScreenShotComparisonSettings] in DefaultEngine.ini"));

				// Parse all fallback definitions of the format "FallbackPlatform=(Child=/Platform/RHI, Parent=/Platform/RHI)"
				for (FConfigSection::TIterator Section(*FallbackSection); Section; ++Section)
				{
					if (Section.Key() == TEXT("FallbackPlatform"))
					{
						FString FallbackValue = Section.Value().GetValue();
						FString Child, Parent;
						bool bSuccess = false;

						if (FParse::Value(*FallbackValue, TEXT("Child="), Child, true) && FParse::Value(*FallbackValue, TEXT("Parent="), Parent, true))
						{
							if (Parent.StartsWith(TEXT("/")))
							{
								UE_LOG(LogScreenShotManager, Warning, TEXT("Fallback parent is malformed and has a leading '/'. This will be stripped but please correct. '%s'"), *Parent);
								Parent = Parent.RightChop(1);
							}

							// These are used as folders so ensure they match the expected layout
							if (Child.StartsWith(TEXT("/")))
							{
								UE_LOG(LogScreenShotManager, Warning, TEXT("Fallback child is malformed and has a leading '/'. This will be stripped but please correct. '%s'"), *Child);
								Child = Child.RightChop(1);
							}				
				
							// Append or override, could error here instead
							FString& Fallback = FallbackPlatforms.FindOrAdd(Child);
							Fallback = Parent;
							bSuccess = true;
						}
						
						if (!bSuccess)
						{
							UE_LOG(LogScreenShotManager, Error, TEXT("Invalid fallback platform definition: '%s'"), *FallbackValue);
						}
					}
				}
			}
		}
	}
}
