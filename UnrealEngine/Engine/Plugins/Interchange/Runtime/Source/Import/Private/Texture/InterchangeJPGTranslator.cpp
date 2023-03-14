// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Texture/InterchangeJPGTranslator.h"

#include "Algo/Find.h"
#include "Containers/StaticArray.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "InterchangeImportLog.h"
#include "InterchangeTextureNode.h"
#include "Memory/SharedBuffer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Texture/TextureTranslatorUtilities.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeJPGTranslator)

static bool GInterchangeEnableJPGImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableJPGImport(
	TEXT("Interchange.FeatureFlags.Import.JPG"),
	GInterchangeEnableJPGImport,
	TEXT("Whether JPG support is enabled."),
	ECVF_Default);

TArray<FString> UInterchangeJPGTranslator::GetSupportedFormats() const
{
	TArray<FString> Formats;

	if (GInterchangeEnableJPGImport || GIsAutomationTesting)
	{
		Formats.Reserve(2);

		Formats.Add(TEXT("jpg;JPEG image"));
		Formats.Add(TEXT("jpeg;JPEG image"));
	}

	return Formats;
}

bool UInterchangeJPGTranslator::Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
{
	return UE::Interchange::FTextureTranslatorUtilities::Generic2DTextureTranslate(GetSourceData(), BaseNodeContainer);
}

TOptional<UE::Interchange::FImportImage> UInterchangeJPGTranslator::GetTexturePayloadData(const UInterchangeSourceData* PayloadSourceData, const FString& PayLoadKey) const
{
	TArray64<uint8> SourceDataBuffer = LoadSourceFile(PayloadSourceData, PayLoadKey);

	if (SourceDataBuffer.IsEmpty())
	{
		return {};
	}

	const bool bImportRaw = false;
	return GetTexturePayloadImplementation(MoveTemp(SourceDataBuffer), PayLoadKey, bImportRaw);

}

bool UInterchangeJPGTranslator::SupportCompressedTexturePayloadData() const
{
	return true;
}

TOptional<UE::Interchange::FImportImage> UInterchangeJPGTranslator::GetCompressedTexturePayloadData(const UInterchangeSourceData* PayloadSourceData, const FString& PayLoadKey) const
{
	TArray64<uint8> SourceDataBuffer = LoadSourceFile(PayloadSourceData, PayLoadKey);

	if (SourceDataBuffer.IsEmpty())
	{
		return {};
	}

	const bool bImportRaw = true;
	return GetTexturePayloadImplementation(MoveTemp(SourceDataBuffer), PayLoadKey, bImportRaw);
}

TArray64<uint8> UInterchangeJPGTranslator::LoadSourceFile(const UInterchangeSourceData* PayloadSourceData, const FString& PayLoadKey) const
{
	check(PayloadSourceData == GetSourceData());

	TArray64<uint8> SourceDataBuffer;

	if (!GetSourceData())
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Failed to import JPEG, bad source data."));
		return SourceDataBuffer;
	}

	FString Filename = GetSourceData()->GetFilename();

	//Make sure the key fit the filename, The key should always be valid
	if (!Filename.Equals(PayLoadKey))
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Failed to import JPEG, wrong payload key. [%s]"), *Filename);
		return SourceDataBuffer;
	}

	if (!FPaths::FileExists(Filename))
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Failed to import JPEG, cannot open file. [%s]"), *Filename);
		return SourceDataBuffer;
	}

	if (!FFileHelper::LoadFileToArray(SourceDataBuffer, *Filename))
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Failed to import JPEG, cannot load file content into an array. [%s]"), *Filename);
	}

	return SourceDataBuffer;
}

TOptional<UE::Interchange::FImportImage> UInterchangeJPGTranslator::GetTexturePayloadImplementation(TArray64<uint8>&& SourceDataBuffer, const FString& Filename, bool bShouldImportRaw) const
{

	const uint8* Buffer = SourceDataBuffer.GetData();
	const uint8* BufferEnd = Buffer + SourceDataBuffer.Num();

	const int32 Length = BufferEnd - Buffer;

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	//
	// JPG
	//
	TSharedPtr<IImageWrapper> JpegImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
	if (!JpegImageWrapper.IsValid() || !JpegImageWrapper->SetCompressed(Buffer, Length))
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Failed to decode JPEG. [%s]"), *Filename);
		return TOptional<UE::Interchange::FImportImage>();
	}

	// Select the texture's source format
	ETextureSourceFormat TextureFormat = TSF_Invalid;
	int32 BitDepth = JpegImageWrapper->GetBitDepth();
	ERGBFormat Format = JpegImageWrapper->GetFormat();

	if (Format == ERGBFormat::Gray)
	{
		if (BitDepth <= 8)
		{
			TextureFormat = TSF_G8;
			Format = ERGBFormat::Gray;
			BitDepth = 8;
		}
	}
	else if (Format == ERGBFormat::RGBA)
	{
		if (BitDepth <= 8)
		{
			TextureFormat = TSF_BGRA8;
			Format = ERGBFormat::BGRA;
			BitDepth = 8;
		}
	}

	if (TextureFormat == TSF_Invalid)
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("JPEG file [%s] contains data in an unsupported format"), *Filename);
		return TOptional<UE::Interchange::FImportImage>();
	}

	UE::Interchange::FImportImage PayloadData;


	const bool bShouldAllocateRawDataBuffer = false;

	PayloadData.Init2DWithParams(
		JpegImageWrapper->GetWidth(),
		JpegImageWrapper->GetHeight(),
		TextureFormat,
		BitDepth < 16,
		bShouldAllocateRawDataBuffer
	);

	// Honor setting from TextureImporter.RetainJpegFormat in Editor.ini if it exists
	const bool bShouldImportRawCache = bShouldImportRaw;
	if (GConfig->GetBool(TEXT("TextureImporter"), TEXT("RetainJpegFormat"), bShouldImportRaw, GEditorIni) && bShouldImportRawCache != bShouldImportRaw)
	{
		UE_LOG(LogInterchangeImport, Log, TEXT("JPEG file [%s]: Pipeline setting 'bPreferCompressedSourceData' has been overridden by Editor setting 'RetainJpegFormat'"), *Filename);
	}

	TArray64<uint8> RawData;
	if (bShouldImportRaw)
	{
		PayloadData.RawData = MakeUniqueBufferFromArray(MoveTemp(SourceDataBuffer));
		PayloadData.RawDataCompressionFormat = ETextureSourceCompressionFormat::TSCF_JPEG;
	}
	else if (JpegImageWrapper->GetRaw(Format, BitDepth, RawData))
	{
		PayloadData.RawData = MakeUniqueBufferFromArray(MoveTemp(RawData));
	}
	else
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Failed to decode JPEG. [%s]"), *Filename);
		return TOptional<UE::Interchange::FImportImage>();
	}

	return PayloadData;
}

