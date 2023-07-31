// Copyright Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreAugmentedImageDatabase.h"

#include "GoogleARCoreAPI.h"
#include "GoogleARCoreBaseLogCategory.h"
#include "GoogleARCoreCookSupport.h"
#include "GoogleARCoreDevice.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/AssertionMacros.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Containers/StringConv.h"
#include "RenderUtils.h"

#include "png.h"

int UGoogleARCoreAugmentedImageDatabase::AddRuntimeAugmentedImageFromTexture(UTexture2D* ImageTexture, FName ImageName, float ImageWidthInMeter /*= 0*/)
{
	EPixelFormat PixelFormat = ImageTexture->GetPixelFormat();

	if (PixelFormat == EPixelFormat::PF_B8G8R8A8 || PixelFormat == EPixelFormat::PF_G8)
	{
		ensure(ImageTexture->GetNumMips() > 0);
		FTexture2DMipMap* Mip0 = &ImageTexture->GetPlatformData()->Mips[0];
		FByteBulkData* RawImageData = &Mip0->BulkData;

		int ImageWidth = ImageTexture->GetSizeX();
		int ImageHeight = ImageTexture->GetSizeY();

		TArray<uint8> GrayscaleBuffer;
		int PixelNum = ImageWidth * ImageHeight;
		uint8* RawBytes = static_cast<uint8*>(RawImageData->Lock(LOCK_READ_ONLY));
		if (PixelFormat == EPixelFormat::PF_B8G8R8A8)
		{
			GrayscaleBuffer.SetNumUninitialized(PixelNum);
			ensureMsgf(RawImageData->GetBulkDataSize() == ImageWidth * ImageHeight * 4,
				TEXT("Unsupported texture data in UGoogleARCoreAugmentedImageDatabase::AddRuntimeAugmentedImage"));

			for (int i = 0; i < PixelNum; i++)
			{
				uint8 R = RawBytes[i * 4 + 2];
				uint8 G = RawBytes[i * 4 + 1];
				uint8 B = RawBytes[i * 4];
				GrayscaleBuffer[i] = 0.2126 * R + 0.7152 * G + 0.0722 * B;
			}
		}
		else
		{
			ensureMsgf(RawImageData->GetBulkDataSize() == ImageWidth * ImageHeight,
				TEXT("Unsupported texture data in UGoogleARCoreAugmentedImageDatabase::AddRuntimeAugmentedImage"));
			GrayscaleBuffer = TArray<uint8>(RawBytes, PixelNum);
		}
		RawImageData->Unlock();

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return AddRuntimeAugmentedImage(GrayscaleBuffer, ImageWidth, ImageHeight, ImageName, ImageWidthInMeter, ImageTexture);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	UE_LOG(LogGoogleARCore, Warning, TEXT("Failed to add runtime augmented image: Unsupported texture format: %s. ARCore only support PF_B8G8R8A8 or PF_G8 for now for adding runtime Augmented Image"), GetPixelFormatString(PixelFormat));
	return -1;
}

int UGoogleARCoreAugmentedImageDatabase::AddRuntimeAugmentedImage(const TArray<uint8>& ImageGrayscalePixels,
	int ImageWidth, int ImageHeight, FName ImageName, float ImageWidthInMeter /*= 0*/, UTexture2D* ImageTexture /*= nullptr*/)
{
	int NewImageIndex = FGoogleARCoreDevice::GetInstance()->AddRuntimeAugmentedImage(this, ImageGrayscalePixels,
		ImageWidth, ImageHeight, ImageName.ToString(), ImageWidthInMeter);

	if (NewImageIndex == -1)
	{
		return -1;
	}

	FGoogleARCoreAugmentedImageDatabaseEntry NewEntry;
	NewEntry.Name = ImageName;
	NewEntry.ImageAsset = ImageTexture;
	NewEntry.Width = ImageWidthInMeter;

	Entries.Add(NewEntry);

	return NewImageIndex;
}

void UGoogleARCoreAugmentedImageDatabase::Serialize(FArchive& Ar)
{
	FString OutStdout;
	FString OutStderr;
	int32 OutReturnCode = 0;

#if !PLATFORM_ANDROID && WITH_EDITORONLY_DATA

	if (!Ar.IsLoading() && Ar.IsCooking()) {
		SerializedDatabase.Empty();

		if (Entries.Num()) {

			FString PathToDbTool =
				FPaths::Combine(
					*FPaths::EnginePluginsDir(),
					TEXT("Runtime"),
					TEXT("AR"),
					TEXT("Google"),
					TEXT("GoogleARCore"),
					TEXT("Binaries"),
					TEXT("ThirdParty"),
					TEXT("Google"),
					TEXT("ARCoreImg"),
					*UGameplayStatics::GetPlatformName(),
#if PLATFORM_LINUX
					TEXT("arcoreimg")
#elif PLATFORM_WINDOWS
					TEXT("arcoreimg.exe")
#elif PLATFORM_MAC
					TEXT("ptdbtool_macos_lipobin")
#endif
					);

			FString TempDir =
				FPaths::ConvertRelativePathToFull(
					FPaths::Combine(
						*FPaths::EnginePluginsDir(),
						TEXT("Runtime"),
						TEXT("AR"),
						TEXT("Google"),
						TEXT("GoogleARCore"),
						TEXT("Intermediate"),
						TEXT("ARCoreImgTemp")));

			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if (!PlatformFile.DirectoryExists(*TempDir))
			{
				PlatformFile.CreateDirectory(*TempDir);
			}

			TArray<FString> CleanupList;

			FString ImageListFileContents;

			for (int32 i = 0; i < Entries.Num(); i++) {
				UTexture2D *Tex = Entries[i].ImageAsset;

				if (Tex) {

					FString PNGFilename =
						FPaths::Combine(TempDir, Tex->GetName() + FString(".png"));

					if (FGoogleARCoreSessionConfigCookSupport::SaveTextureToPNG(
							Tex,
							PNGFilename))
					{
						// "|" is used as a delimeter in the image
						// list, and there doesn't seem to be any way
						// to escape them, so they will be replaced
						// with underscores.
						FString TmpName =
							Entries[i].Name.ToString().Replace(TEXT("|"), TEXT("_"));

						ImageListFileContents +=
							TmpName +
							FString("|") +
							PNGFilename;

						CleanupList.Add(PNGFilename);

						if (Entries[i].Width > 0.0f) {
							ImageListFileContents +=
								FString("|") +
								FString::SanitizeFloat(Entries[i].Width);
						}

						ImageListFileContents += FString("\n");
					}

				}
			}

			FString PathToImageList =
				FPaths::Combine(TempDir, TEXT("image_list.txt"));
			FString PathToImageDb =
				FPaths::Combine(TempDir, TEXT("image_list.imgdb"));

			CleanupList.Add(PathToImageList);
			CleanupList.Add(PathToImageDb);

			FFileHelper::SaveStringToFile(
				ImageListFileContents,
				*PathToImageList);

			OutStderr = "";
			OutStdout = "";
			OutReturnCode = 0;

#if PLATFORM_LINUX || PLATFORM_MAC
			FGoogleARCoreSessionConfigCookSupport::PlatformSetExecutable(*PathToDbTool, true);
#endif

			FPlatformProcess::ExecProcess(
				*PathToDbTool,
				*(FString("build-db --input_image_list_path=\"")+ PathToImageList +
				  FString("\" --output_db_path=\"") + PathToImageDb + "\""),
				&OutReturnCode,
				&OutStdout,
				&OutStderr);

			if (OutReturnCode)
			{
				Ar.SetCriticalError();
				UE_LOG(LogGoogleARCoreAPI, Error, TEXT("Failed to build augmented image database: %s"), *OutStderr);
			}
			else
			{
				FFileHelper::LoadFileToArray(SerializedDatabase, *PathToImageDb, 0);
				UE_LOG(LogGoogleARCoreAPI, Log,
					   TEXT("Augmented image database created. Size: %d bytes. Tool output: %s"),
					   SerializedDatabase.Num(), *OutStdout);
			}

			for (int32 i = 0; i < CleanupList.Num(); i++)
			{
				UE_LOG(LogGoogleARCoreAPI, Log, TEXT("Cleaning up: %s"), *CleanupList[i]);
				PlatformFile.DeleteFile(*CleanupList[i]);
			}

			PlatformFile.DeleteDirectory(*TempDir);
		}
	}

#endif

	// Must happen AFTER database generation, because we rely on the
	// UPROPERTY serialization to actually save the data.
	Super::Serialize(Ar);
}
