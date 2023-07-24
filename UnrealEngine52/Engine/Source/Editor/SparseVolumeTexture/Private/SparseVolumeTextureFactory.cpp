// Copyright Epic Games, Inc. All Rights Reserved.

#include "SparseVolumeTextureFactory.h"

#include "SparseVolumeTexture/SparseVolumeTexture.h"

#if WITH_EDITOR

#include "SparseVolumeTextureOpenVDB.h"
#include "SparseVolumeTextureOpenVDBUtility.h"

#include "Serialization/EditorBulkDataWriter.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/FileHelper.h"

#include "Editor.h"

#include "OpenVDBImportWindow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "USparseVolumeTextureFactory"

DEFINE_LOG_CATEGORY_STATIC(LogSparseVolumeTextureFactory, Log, All);

struct FOpenVDBPreviewData
{
	TArray<uint8> LoadedFile;
	TArray<FOpenVDBGridInfo> GridInfo;
	TArray<TSharedPtr<FOpenVDBGridComponentInfo>> GridComponentInfoPtrs;
	FString FileInfoString;
};

static bool LoadOpenVDBPreviewData(const FString& Filename, FOpenVDBPreviewData* OutPreviewData)
{
	FOpenVDBPreviewData& Result = *OutPreviewData;
	Result = {};

	if (!FFileHelper::LoadFileToArray(Result.LoadedFile, *Filename))
	{
		UE_LOG(LogSparseVolumeTextureFactory, Error, TEXT("OpenVDB file could not be loaded: %s"), *Filename);
		return false;
	}
	if (!GetOpenVDBGridInfo(Result.LoadedFile, true /*bCreateStrings*/, &Result.GridInfo))
	{
		UE_LOG(LogSparseVolumeTextureFactory, Error, TEXT("Failed to read OpenVDB file: %s"), *Filename);
		return false;
	}
	if (Result.GridInfo.IsEmpty())
	{
		UE_LOG(LogSparseVolumeTextureFactory, Error, TEXT("OpenVDB file contains no grids: %s"), *Filename);
		return false;
	}

	// We need a <None> option to leave channels empty
	FOpenVDBGridComponentInfo NoneGridComponentInfo;
	NoneGridComponentInfo.Index = INDEX_NONE;
	NoneGridComponentInfo.ComponentIndex = INDEX_NONE;
	NoneGridComponentInfo.Name = TEXT("<None>");
	NoneGridComponentInfo.DisplayString = TEXT("<None>");

	Result.GridComponentInfoPtrs.Add(MakeShared<FOpenVDBGridComponentInfo>(NoneGridComponentInfo));

	// Create individual entries for each component of all valid source grids.
	// This is an array of TSharedPtr because SComboBox requires its input to be wrapped in TSharedPtr.
	bool bFoundSupportedGridType = false;
	for (const FOpenVDBGridInfo& Grid : Result.GridInfo)
	{
		// Append all grids to the string, even if we don't actually support them
		Result.FileInfoString.Append(Grid.DisplayString);
		Result.FileInfoString.AppendChar(TEXT('\n'));

		if (Grid.Type == EOpenVDBGridType::Unknown || !IsOpenVDBGridValid(Grid, Filename))
		{
			continue;
		}

		bFoundSupportedGridType = true;

		// Create one entry per component
		for (uint32 ComponentIdx = 0; ComponentIdx < Grid.NumComponents; ++ComponentIdx)
		{
			FOpenVDBGridComponentInfo ComponentInfo;
			ComponentInfo.Index = Grid.Index;
			ComponentInfo.ComponentIndex = ComponentIdx;
			ComponentInfo.Name = Grid.Name;

			const TCHAR* ComponentNames[] = { TEXT(".X"), TEXT(".Y"),TEXT(".Z"),TEXT(".W") };
			FStringFormatOrderedArguments FormatArgs;
			FormatArgs.Add(ComponentInfo.Index);
			FormatArgs.Add(ComponentInfo.Name);
			FormatArgs.Add(Grid.NumComponents == 1 ? TEXT("") : ComponentNames[ComponentIdx]);

			ComponentInfo.DisplayString = FString::Format(TEXT("{0}. {1}{2}"), FormatArgs);

			Result.GridComponentInfoPtrs.Add(MakeShared<FOpenVDBGridComponentInfo>(MoveTemp(ComponentInfo)));
		}
	}

	if (!bFoundSupportedGridType)
	{
		UE_LOG(LogSparseVolumeTextureFactory, Error, TEXT("OpenVDB file contains no grids of supported type: %s"), *Filename);
		return false;
	}

	return true;
}

struct FOpenVDBImportOptions
{
	FSparseVolumeRawSourcePackedData PackedDataA;
	FSparseVolumeRawSourcePackedData PackedDataB;
	bool bIsSequence;
	bool bShouldImport;
};

static FOpenVDBImportOptions ShowOpenVDBImportWindow(const FString& Filename, const FString& FileInfoString, TArray<TSharedPtr<FOpenVDBGridComponentInfo>>& GridComponentInfo)
{
	FOpenVDBImportOptions ImportOptions{};

	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	// Compute centered window position based on max window size, which include when all categories are expanded
	const float ImportWindowWidth = 450.0f;
	const float ImportWindowHeight = 750.0f;
	FVector2D ImportWindowSize = FVector2D(ImportWindowWidth, ImportWindowHeight); // Max window size it can get based on current slate


	FSlateRect WorkAreaRect = FSlateApplicationBase::Get().GetPreferredWorkArea();
	FVector2D DisplayTopLeft(WorkAreaRect.Left, WorkAreaRect.Top);
	FVector2D DisplaySize(WorkAreaRect.Right - WorkAreaRect.Left, WorkAreaRect.Bottom - WorkAreaRect.Top);

	float ScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayTopLeft.X, DisplayTopLeft.Y);
	ImportWindowSize *= ScaleFactor;

	FVector2D WindowPosition = (DisplayTopLeft + (DisplaySize - ImportWindowSize) / 2.0f) / ScaleFactor;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("UnrealEd", "OpenVDBImportOptionsTitle", "OpenVDB Import Options"))
		.SizingRule(ESizingRule::Autosized)
		.AutoCenter(EAutoCenter::None)
		.ClientSize(ImportWindowSize)
		.ScreenPosition(WindowPosition);

	TArray<TSharedPtr<ESparseVolumePackedDataFormat>> SupportedFormats =
	{
		MakeShared<ESparseVolumePackedDataFormat>(ESparseVolumePackedDataFormat::Float32),
		MakeShared<ESparseVolumePackedDataFormat>(ESparseVolumePackedDataFormat::Float16),
		MakeShared<ESparseVolumePackedDataFormat>(ESparseVolumePackedDataFormat::Unorm8)
	};

	TSharedPtr<SOpenVDBImportWindow> OpenVDBOptionWindow;
	Window->SetContent
	(
		SAssignNew(OpenVDBOptionWindow, SOpenVDBImportWindow)
		.PackedDataA(&ImportOptions.PackedDataA)
		.PackedDataB(&ImportOptions.PackedDataB)
		.OpenVDBGridComponentInfo(&GridComponentInfo)
		.FileInfoString(FileInfoString)
		.OpenVDBSupportedTargetFormats(&SupportedFormats)
		.WidgetWindow(Window)
		.FullPath(FText::FromString(Filename))
		.MaxWindowHeight(ImportWindowHeight)
		.MaxWindowWidth(ImportWindowWidth)
	);

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	ImportOptions.bShouldImport = OpenVDBOptionWindow->ShouldImport();
	ImportOptions.bIsSequence = OpenVDBOptionWindow->ShouldImportAsSequence();

	return ImportOptions;
}

USparseVolumeTextureFactory::USparseVolumeTextureFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = true;
	SupportedClass = USparseVolumeTexture::StaticClass();

	Formats.Add(TEXT("vdb;OpenVDB Format"));
}

FText USparseVolumeTextureFactory::GetDisplayName() const
{
	return LOCTEXT("SparseVolumeTextureFactoryDescription", "Sparse Volume Texture");
}

bool USparseVolumeTextureFactory::ConfigureProperties()
{
	return true;
}

bool USparseVolumeTextureFactory::ShouldShowInNewMenu() const
{
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// Create asset


bool USparseVolumeTextureFactory::CanCreateNew() const
{
	return false;	// To be able to import files and call 
}

UObject* USparseVolumeTextureFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	USparseVolumeTexture* Object = NewObject<USparseVolumeTexture>(InParent, InClass, InName, Flags);

	// SVT_TODO initialize similarly to UTexture2DFactoryNew

	return Object;
}


///////////////////////////////////////////////////////////////////////////////
// Import asset


bool USparseVolumeTextureFactory::DoesSupportClass(UClass* Class)
{
	return Class == USparseVolumeTexture::StaticClass();
}

UClass* USparseVolumeTextureFactory::ResolveSupportedClass()
{
	return USparseVolumeTexture::StaticClass();
}

bool USparseVolumeTextureFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);
	if (Extension == TEXT("vdb"))
	{
		return true;
	}
	return false;
}

void USparseVolumeTextureFactory::CleanUp()
{
	Super::CleanUp();
}

UObject* USparseVolumeTextureFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename,
	const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{

#if OPENVDB_AVAILABLE

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Parms);
	TArray<UObject*> ResultAssets;

	// Load file and get info about each contained grid
	FOpenVDBPreviewData PreviewData;
	if (!LoadOpenVDBPreviewData(Filename, &PreviewData))
	{
		return nullptr;
	}

	// Show dialog for import options
	FOpenVDBImportOptions ImportOptions = ShowOpenVDBImportWindow(Filename, PreviewData.FileInfoString, PreviewData.GridComponentInfoPtrs);
	if (!ImportOptions.bShouldImport)
	{
		bOutOperationCanceled = true;
		return nullptr;
	}

	// Utility function for computing the bounding box encompassing the bounds of all frames in the SVT.
	// Accounts for padding caused by rounding up to a multiple of the page resolution.
	auto ExpandVolumeBounds = [](const FOpenVDBImportOptions& ImportOptions, const TArray<FOpenVDBGridInfo>& GridInfoArray, FBox& VolumeBounds)
	{
		const FSparseVolumeRawSourcePackedData* PackedData[] = { &ImportOptions.PackedDataA, &ImportOptions.PackedDataB };
		for (uint32 PackedDataIdx = 0; PackedDataIdx < 2; ++PackedDataIdx)
		{
			for (uint32 CompIdx = 0; CompIdx < 4; ++CompIdx)
			{
				const uint32 GridIdx = PackedData[PackedDataIdx]->SourceGridIndex[CompIdx];
				if (GridIdx != INDEX_NONE)
				{
					const FOpenVDBGridInfo& GridInfo = GridInfoArray[GridIdx];
					VolumeBounds.Min.X = FMath::Min(VolumeBounds.Min.X, GridInfo.VolumeActiveAABBMin.X);
					VolumeBounds.Min.Y = FMath::Min(VolumeBounds.Min.Y, GridInfo.VolumeActiveAABBMin.Y);
					VolumeBounds.Min.Z = FMath::Min(VolumeBounds.Min.Z, GridInfo.VolumeActiveAABBMin.Z);

					const FVector PageTableVolumeResolution = FMath::DivideAndRoundUp(GridInfo.VolumeActiveDim, FVector(SPARSE_VOLUME_TILE_RES));
					const FVector Max = GridInfo.VolumeActiveAABBMin + (PageTableVolumeResolution * SPARSE_VOLUME_TILE_RES);
					VolumeBounds.Max.X = FMath::Max(VolumeBounds.Max.X, Max.X);
					VolumeBounds.Max.Y = FMath::Max(VolumeBounds.Max.Y, Max.Y);
					VolumeBounds.Max.Z = FMath::Max(VolumeBounds.Max.Z, Max.Z);
				}
			}
		}
	};

	// Import as either single static SVT or a sequence of frames, making up an animated SVT
	if (!ImportOptions.bIsSequence)
	{
		// Import as a static sparse volume texture asset.

		FScopedSlowTask ImportTask(1.0f, LOCTEXT("ImportingVDBStatic", "Importing static OpenVDB"));
		ImportTask.MakeDialog(true);

		FName NewName(InName.ToString() + TEXT("VDB"));
		UStaticSparseVolumeTexture* StaticSVTexture = NewObject<UStaticSparseVolumeTexture>(InParent, UStaticSparseVolumeTexture::StaticClass(), NewName, Flags);

		FBox VolumeBounds(FVector(FLT_MAX), FVector(-FLT_MAX));
		ExpandVolumeBounds(ImportOptions, PreviewData.GridInfo, VolumeBounds);
		StaticSVTexture->VolumeBounds = VolumeBounds;

		FSparseVolumeRawSource SparseVolumeRawSource{};
		SparseVolumeRawSource.PackedDataA = ImportOptions.PackedDataA;
		SparseVolumeRawSource.PackedDataB = ImportOptions.PackedDataB;
		SparseVolumeRawSource.SourceAssetFile = MoveTemp(PreviewData.LoadedFile);
		PreviewData.LoadedFile.Reset();

		// Serialize the raw source data into the asset object.
		{
			UE::Serialization::FEditorBulkDataWriter RawDataArchiveWriter(StaticSVTexture->StaticFrame.RawData);
			SparseVolumeRawSource.Serialize(RawDataArchiveWriter);
		}

		if (ImportTask.ShouldCancel())
		{
			return nullptr;
		}
		ImportTask.EnterProgressFrame(1.0f, LOCTEXT("ConvertingVDBStatic", "Converting static OpenVDB"));

		ResultAssets.Add(StaticSVTexture);
	}
	else
	{
		// Import as an animated sparse volume texture asset.
		
		// Data from original file is no longer needed; we iterate over all frames later
		PreviewData.LoadedFile.Empty();

		FName NewName(InName.ToString() + TEXT("VDBAnim"));
		UAnimatedSparseVolumeTexture* AnimatedSVTexture = NewObject<UAnimatedSparseVolumeTexture>(InParent, UAnimatedSparseVolumeTexture::StaticClass(), NewName, Flags);
		
		// The file is potentially a sequence if the character before the `.vdb` is a number.
		const bool bIsFilePotentiallyPartOfASequence = FChar::IsDigit(Filename[Filename.Len() - 5]);

		// Collect filenames of all files to be imported
		TArray<FString> SequenceFileNames;
		if (!bIsFilePotentiallyPartOfASequence)
		{
			SequenceFileNames.Add(Filename);
		}
		else
		{
			const FString FilenameWithoutExt = Filename.LeftChop(4);
			const int32 LastNonDigitIndex = FilenameWithoutExt.FindLastCharByPredicate([](TCHAR Letter) { return !FChar::IsDigit(Letter); }) + 1;
			const int32 DigitCount = FilenameWithoutExt.Len() - LastNonDigitIndex;
			FString FilenameWithoutSuffix = FilenameWithoutExt.LeftChop(FilenameWithoutExt.Len() - LastNonDigitIndex);
			TCHAR LastDigit = FilenameWithoutExt[FilenameWithoutExt.Len() - 5];

			bool bIndexStartsAtOne = false;
			auto GetOpenVDBFileNameForFrame = [&](int32 FrameIndex)
			{
				FString IndexString = FString::FromInt(FrameIndex + (bIndexStartsAtOne ? 1 : 0));
				// User must select a frame with index in [0-9] so that we can count leading 0s
				check(DigitCount == 1 || (DigitCount > 1 && IndexString.Len() <= DigitCount));
				const int32 MissingLeadingZeroCount = DigitCount - IndexString.Len();
				const FString StringZero = FString::FromInt(0);
				for (int32 i = 0; i < MissingLeadingZeroCount; ++i)
				{
					IndexString = StringZero + IndexString;
				}
				return FString(FilenameWithoutSuffix + IndexString) + TEXT(".vdb");
			};

			const FString VDBFileAt0 = GetOpenVDBFileNameForFrame(0);
			const FString VDBFileAt1 = GetOpenVDBFileNameForFrame(1);
			const bool VDBFileAt0Exists = FPaths::FileExists(VDBFileAt0);
			const bool VDBFileAt1Exists = FPaths::FileExists(VDBFileAt1);
			if (!VDBFileAt0Exists && !VDBFileAt1Exists)
			{
				UE_LOG(LogSparseVolumeTextureFactory, Error, TEXT("An OpenVDB animated sequence must start at index 0 or 1: %s or %s not found."), *VDBFileAt0, *VDBFileAt1);
				return nullptr;
			}
			bIndexStartsAtOne = !VDBFileAt0Exists;

			// Go over all the frame index and stop at the first missing one.
			for (int32 FrameIdx = 0; ; ++FrameIdx)
			{
				const FString FrameFileName = GetOpenVDBFileNameForFrame(FrameIdx);
				if (!FPaths::FileExists(FrameFileName))
				{
					break;
				}
				SequenceFileNames.Add(FrameFileName);
			}
		}

		const int32 NumFrames = SequenceFileNames.Num();

		FScopedSlowTask ImportTask(NumFrames, LOCTEXT("ImportingVDBAnim", "Importing OpenVDB animation"));
		ImportTask.MakeDialog(true);

		// Allocate space for each frame
		AnimatedSVTexture->FrameCount = NumFrames;
		AnimatedSVTexture->AnimationFrames.SetNum(NumFrames);

		FBox VolumeBounds(FVector(FLT_MAX), FVector(-FLT_MAX));

		// Load individual frames, check them for compatibility with the first loaded file and append them to the resulting asset
		for (int32 FrameIdx = 0; FrameIdx < NumFrames; ++FrameIdx)
		{
			const FString& FrameFilename = SequenceFileNames[FrameIdx];

			UE_LOG(LogSparseVolumeTextureFactory, Display, TEXT("Loading OpenVDB sequence frame #%i %s."), FrameIdx, *FrameFilename);

			// Load file and get info about each contained grid
			TArray<uint8> LoadedFrameFile;
			if (!FFileHelper::LoadFileToArray(LoadedFrameFile, *FrameFilename))
			{
				UE_LOG(LogSparseVolumeTextureFactory, Error, TEXT("OpenVDB file could not be loaded: %s"), *FrameFilename);
				return nullptr;
			}

			TArray<FOpenVDBGridInfo> FrameGridInfo;
			if (!GetOpenVDBGridInfo(LoadedFrameFile, true, &FrameGridInfo))
			{
				UE_LOG(LogSparseVolumeTextureFactory, Error, TEXT("Failed to read OpenVDB file: %s"), *FrameFilename);
				return nullptr;
			}

			// Sanity check for compatibility
			const FSparseVolumeRawSourcePackedData* PackedData[] = { &ImportOptions.PackedDataA, &ImportOptions.PackedDataB };
			for (uint32 PackedDataIdx = 0; PackedDataIdx < 2; ++PackedDataIdx)
			{
				for (uint32 DstCompIdx = 0; DstCompIdx < 4; ++DstCompIdx)
				{
					const uint32 SourceGridIndex = PackedData[PackedDataIdx]->SourceGridIndex[DstCompIdx];
					if (SourceGridIndex != INDEX_NONE)
					{
						if ((int32)SourceGridIndex >= FrameGridInfo.Num())
						{
							UE_LOG(LogSparseVolumeTextureFactory, Error, TEXT("OpenVDB file is incompatible with other frames in the sequence: %s"), *FrameFilename);
							return nullptr;
						}
						const auto& OrigSourceGrid = PreviewData.GridInfo[SourceGridIndex];
						const auto& FrameSourceGrid = FrameGridInfo[SourceGridIndex];
						if (OrigSourceGrid.Type != FrameSourceGrid.Type || OrigSourceGrid.Name != FrameSourceGrid.Name)
						{
							UE_LOG(LogSparseVolumeTextureFactory, Error, TEXT("OpenVDB file is incompatible with other frames in the sequence: %s"), *FrameFilename);
							return nullptr;
						}
					}
				}
			}

			ExpandVolumeBounds(ImportOptions, FrameGridInfo, VolumeBounds);

			FSparseVolumeRawSource SparseVolumeRawSource{};
			SparseVolumeRawSource.PackedDataA = ImportOptions.PackedDataA;
			SparseVolumeRawSource.PackedDataB = ImportOptions.PackedDataB;
			SparseVolumeRawSource.SourceAssetFile = MoveTemp(LoadedFrameFile);

			// Serialize the raw source data from this frame into the asset object.
			{
				UE::Serialization::FEditorBulkDataWriter RawDataArchiveWriter(AnimatedSVTexture->AnimationFrames[FrameIdx].RawData);
				SparseVolumeRawSource.Serialize(RawDataArchiveWriter);
			}

			if (ImportTask.ShouldCancel())
			{
				return nullptr;
			}
			ImportTask.EnterProgressFrame(1.0f, LOCTEXT("ConvertingVDBAnim", "Converting OpenVDB animation"));
		}

		AnimatedSVTexture->VolumeBounds = VolumeBounds;

		ResultAssets.Add(AnimatedSVTexture);
	}

	// Now notify the system about the imported/updated/created assets
	AdditionalImportedObjects.Reserve(ResultAssets.Num());
	for (UObject* Object : ResultAssets)
	{
		if (Object)
		{
			GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Object);
			Object->MarkPackageDirty();
			Object->PostEditChange();
			AdditionalImportedObjects.Add(Object);
		}
	}

	return (ResultAssets.Num() > 0) ? ResultAssets[0] : nullptr;

#else // OPENVDB_AVAILABLE

	// SVT_TODO Make sure we can also import on more platforms such as Linux. See SparseVolumeTextureOpenVDB.h
	UE_LOG(LogSparseVolumeTextureFactory, Error, TEXT("Cannot import OpenVDB asset any platform other than Windows."));
	return nullptr;

#endif // OPENVDB_AVAILABLE

}

#endif // WITH_EDITORONLY_DATA

#undef LOCTEXT_NAMESPACE

#include "Serialization/EditorBulkDataWriter.h"
