// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGridUtils.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"


bool UE::RenderGrid::Private::FRenderGridUtils::IsImage(const FString& ImagePath)
{
	static IImageWrapperModule* ImageWrapperModule = FModuleManager::LoadModulePtr<IImageWrapperModule>("ImageWrapper");
	if (ImageWrapperModule == nullptr)
	{
		return false;
	}

	EImageFormat ImageFormat = ImageWrapperModule->GetImageFormatFromExtension(*ImagePath);
	if (ImageFormat == EImageFormat::Invalid)
	{
		return false;
	}

	return true;
}

UTexture2D* UE::RenderGrid::Private::FRenderGridUtils::GetImage(const FString& ImagePath, UTexture2D* Texture2D, bool& bOutReusedGivenTexture2D)
{
	static IImageWrapperModule* ImageWrapperModule = FModuleManager::LoadModulePtr<IImageWrapperModule>("ImageWrapper");
	if (ImageWrapperModule == nullptr)
	{
		bOutReusedGivenTexture2D = (Texture2D == nullptr);
		return nullptr;
	}

	EImageFormat ImageFormat = ImageWrapperModule->GetImageFormatFromExtension(*ImagePath);
	if (ImageFormat == EImageFormat::Invalid)
	{
		bOutReusedGivenTexture2D = (Texture2D == nullptr);
		return nullptr;
	}

	TArray<uint8> ImageBytes = GetFileData(ImagePath);
	if (ImageBytes.Num() <= 0)
	{
		bOutReusedGivenTexture2D = (Texture2D == nullptr);
		return nullptr;
	}

	return BytesToExistingImage(ImageBytes, ImageFormat, Texture2D, bOutReusedGivenTexture2D);
}


UTexture2D* UE::RenderGrid::Private::FRenderGridUtils::BytesToImage(const TArray<uint8>& ByteArray, const EImageFormat ImageFormat)
{
	static IImageWrapperModule* ImageWrapperModule = FModuleManager::LoadModulePtr<IImageWrapperModule>("ImageWrapper");
	if (ImageWrapperModule == nullptr)
	{
		return nullptr;
	}

	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule->CreateImageWrapper(ImageFormat);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(ByteArray.GetData(), ByteArray.Num()))
	{
		return nullptr;
	}

	TArray<uint8> Uncompressed;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, Uncompressed))
	{
		return nullptr;
	}

	return DataToTexture2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), Uncompressed.GetData(), Uncompressed.Num());
}

UTexture2D* UE::RenderGrid::Private::FRenderGridUtils::BytesToExistingImage(const TArray<uint8>& ByteArray, const EImageFormat ImageFormat, UTexture2D* Texture2D, bool& bOutReusedGivenTexture2D)
{
	static IImageWrapperModule* ImageWrapperModule = FModuleManager::LoadModulePtr<IImageWrapperModule>("ImageWrapper");
	if (ImageWrapperModule == nullptr)
	{
		bOutReusedGivenTexture2D = (Texture2D == nullptr);
		return nullptr;
	}

	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule->CreateImageWrapper(ImageFormat);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(ByteArray.GetData(), ByteArray.Num()))
	{
		bOutReusedGivenTexture2D = (Texture2D == nullptr);
		return nullptr;
	}

	TArray<uint8> Uncompressed;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, Uncompressed))
	{
		bOutReusedGivenTexture2D = (Texture2D == nullptr);
		return nullptr;
	}

	UTexture2D* NewTexture2D = DataToExistingTexture2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), Uncompressed.GetData(), Uncompressed.Num(), Texture2D, bOutReusedGivenTexture2D);
	bOutReusedGivenTexture2D = (Texture2D == NewTexture2D);
	return NewTexture2D;
}


UTexture2D* UE::RenderGrid::Private::FRenderGridUtils::DataToTexture2D(int32 Width, int32 Height, const void* Src, SIZE_T Count)
{
	UTexture2D* Texture2D = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (Texture2D == nullptr)
	{
		return nullptr;
	}
	Texture2D->bNoTiling = true;

#if WITH_EDITORONLY_DATA
	Texture2D->MipGenSettings = TMGS_NoMipmaps;
#endif

	void* TextureData = Texture2D->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, Src, Count);
	Texture2D->GetPlatformData()->Mips[0].BulkData.Unlock();

	Texture2D->UpdateResource();
	return Texture2D;
}

UTexture2D* UE::RenderGrid::Private::FRenderGridUtils::DataToExistingTexture2D(int32 Width, int32 Height, const void* Src, SIZE_T Count, UTexture2D* Texture2D, bool& bOutReusedGivenTexture2D)
{
	UTexture2D* OriginalTexture2D = Texture2D;

	bOutReusedGivenTexture2D = true;
	if ((Texture2D == nullptr) || (Texture2D->GetSizeX() != Width) || (Texture2D->GetSizeY() != Height) || (Texture2D->GetPixelFormat() != PF_B8G8R8A8))
	{
		bOutReusedGivenTexture2D = false;
		Texture2D = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	}
	if (Texture2D == nullptr)
	{
		bOutReusedGivenTexture2D = (OriginalTexture2D == nullptr);
		return nullptr;
	}
	Texture2D->bNoTiling = true;

#if WITH_EDITORONLY_DATA
	Texture2D->MipGenSettings = TMGS_NoMipmaps;
#endif

	void* TextureData = Texture2D->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, Src, Count);
	Texture2D->GetPlatformData()->Mips[0].BulkData.Unlock();

	Texture2D->UpdateResource();
	return Texture2D;
}


TArray<FString> UE::RenderGrid::Private::FRenderGridUtils::GetFiles(const FString& Directory, const bool bRecursive)
{
	class FRenderGridUtilsGetFilesVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		TArray<FString> Files;
		virtual bool Visit(const TCHAR* FileName, const bool bIsDirectory) override
		{
			if (!bIsDirectory)
			{
				Files.Add(FileName);
			}
			return true;
		}
	};
	FRenderGridUtilsGetFilesVisitor GetFilesVisitor;
	if (bRecursive)
	{
		FPlatformFileManager::Get().GetPlatformFile().IterateDirectoryRecursively(*Directory, GetFilesVisitor);
	}
	else
	{
		FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*Directory, GetFilesVisitor);
	}
	return GetFilesVisitor.Files;
}

TArray<uint8> UE::RenderGrid::Private::FRenderGridUtils::GetFileData(const FString& File)
{
	TArray<uint8> Data;
	IFileHandle* FileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*File);
	if (FileHandle != nullptr)
	{
		int64 FileSize = FileHandle->Size();
		if (FileSize > 0x7fffffff)
		{
			FileSize = 0;
		}
		if (FileSize > 0)
		{
			Data.SetNum(FileSize);
			FileHandle->Read(Data.GetData(), FileSize);
		}
		delete FileHandle;
	}
	return Data;
}


void UE::RenderGrid::Private::FRenderGridUtils::DeleteDirectory(const FString& Directory)
{
	IFileManager::Get().DeleteDirectory(*Directory, false, true);
}

void UE::RenderGrid::Private::FRenderGridUtils::EmptyDirectory(const FString& Directory)
{
	class FRenderGridUtilsEmptyDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
	{
	public:
		virtual bool Visit(const TCHAR* FileName, const bool bIsDirectory) override
		{
			if (bIsDirectory)
			{
				IFileManager::Get().DeleteDirectory(FileName, false, false);
			}
			else
			{
				IFileManager::Get().Delete(FileName, false, true, true);
			}
			return true;
		}
	};
	FRenderGridUtilsEmptyDirectoryVisitor GetFilesVisitor;
	FPlatformFileManager::Get().GetPlatformFile().IterateDirectoryRecursively(*Directory, GetFilesVisitor);
}


FString UE::RenderGrid::Private::FRenderGridUtils::NormalizeOutputDirectory(const FString& OutputDirectory)
{
	FString NewOutputDirectory = OutputDirectory;
	FPaths::NormalizeDirectoryName(NewOutputDirectory);
	FPaths::RemoveDuplicateSlashes(NewOutputDirectory);
	FPaths::CollapseRelativeDirectories(NewOutputDirectory);
	FPaths::RemoveDuplicateSlashes(NewOutputDirectory);
	if (!NewOutputDirectory.EndsWith(TEXT("/"), ESearchCase::CaseSensitive))
	{
		NewOutputDirectory += TEXT("/");
	}
	return NewOutputDirectory;
}


FRenderGridPreviousEngineFpsSettings UE::RenderGrid::Private::FRenderGridUtils::DisableFpsLimit()
{
	FRenderGridPreviousEngineFpsSettings Settings;
	if (GEngine)
	{
		static IConsoleVariable* VSync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
		static IConsoleVariable* VSyncEditor = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSyncEditor"));
		UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();

		Settings.bHasBeenSet = true;
		Settings.bUseFixedFrameRate = GEngine->bUseFixedFrameRate;
		Settings.bForceDisableFrameRateSmoothing = GEngine->bForceDisableFrameRateSmoothing;
		Settings.MaxFps = GEngine->GetMaxFPS();
		Settings.bVSync = VSync->GetBool();
		Settings.bVSyncEditor = VSyncEditor->GetBool();
		Settings.bThrottleCPUWhenNotForeground = EditorPerformanceSettings->bThrottleCPUWhenNotForeground;

		GEngine->bUseFixedFrameRate = false;
		GEngine->bForceDisableFrameRateSmoothing = true;
		GEngine->SetMaxFPS(10000);
		VSync->Set(false);
		VSyncEditor->Set(false);
		EditorPerformanceSettings->bThrottleCPUWhenNotForeground = false;
		EditorPerformanceSettings->PostEditChange();
	}
	return Settings;
}

void UE::RenderGrid::Private::FRenderGridUtils::RestoreFpsLimit(const FRenderGridPreviousEngineFpsSettings& Settings)
{
	if (GEngine && Settings.bHasBeenSet)
	{
		static IConsoleVariable* VSync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
		static IConsoleVariable* VSyncEditor = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSyncEditor"));
		UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();

		GEngine->bUseFixedFrameRate = Settings.bUseFixedFrameRate;
		GEngine->bForceDisableFrameRateSmoothing = Settings.bForceDisableFrameRateSmoothing;
		GEngine->SetMaxFPS(Settings.MaxFps);
		VSync->Set(Settings.bVSync);
		VSyncEditor->Set(Settings.bVSyncEditor);
		EditorPerformanceSettings->bThrottleCPUWhenNotForeground = Settings.bThrottleCPUWhenNotForeground;
		EditorPerformanceSettings->PostEditChange();
	}
}
