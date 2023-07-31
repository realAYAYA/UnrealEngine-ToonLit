// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Texture/InterchangeIESTranslator.h"

#include "Engine/Texture.h"
#include "Engine/TextureLightProfile.h"
#include "IESConverter.h"
#include "InterchangeImportLog.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Texture/TextureTranslatorUtilities.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeIESTranslator)

#define LOCTEXT_NAMESPACE "InterchangeIESTranslator"

static bool GInterchangeEnableIESImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableIESImport(
	TEXT("Interchange.FeatureFlags.Import.IES"),
	GInterchangeEnableIESImport,
	TEXT("Whether IES support is enabled."),
	ECVF_Default);

namespace UE::Interchange::IESTranslator::Private
{
	void LogError(const UInterchangeIESTranslator& IESTranslator, const FString& UniqueID, FText&& ErrorText)
	{
		check(IESTranslator.GetSourceData() != nullptr);

		UInterchangeResultError_Generic* ErrorMessage = IESTranslator.AddMessage<UInterchangeResultError_Generic>();
		ErrorMessage->SourceAssetName = IESTranslator.GetSourceData()->GetFilename();
		ErrorMessage->AssetType = UTextureLightProfile::StaticClass();
		ErrorMessage->InterchangeKey = UniqueID;
		ErrorMessage->Text = MoveTemp(ErrorText);
	}
}

TArray<FString> UInterchangeIESTranslator::GetSupportedFormats() const
{
	if (GInterchangeEnableIESImport || GIsAutomationTesting)
	{
		TArray<FString> Formats{ TEXT("ies;IES light profile") };
		return Formats;
	}
	else
	{
		return TArray<FString>{};
	}
}

bool UInterchangeIESTranslator::Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
{
	return UE::Interchange::FTextureTranslatorUtilities::GenericTextureLightProfileTranslate(GetSourceData(), BaseNodeContainer);
}

TOptional<UE::Interchange::FImportLightProfile> UInterchangeIESTranslator::GetLightProfilePayloadData(const UInterchangeSourceData* PayloadSourceData, const FString& PayLoadKey) const
{
	using namespace UE::Interchange;

	check(GetSourceData() == PayloadSourceData);

	if (!GetSourceData())
	{
		IESTranslator::Private::LogError(*this, PayLoadKey,
			LOCTEXT("IESImportFailure_BadData", "Failed to import IES, bad source data."));

		return {};
	}

	TArray64<uint8> SourceDataBuffer;
	FString Filename = GetSourceData()->GetFilename();

	//Make sure the key fit the filename, The key should always be valid
	if (!Filename.Equals(PayLoadKey))
	{
		IESTranslator::Private::LogError(*this, PayLoadKey,
			LOCTEXT("IESImportFailure_BadPayloadKey", "Failed to import IES, wrong payload key."));

		return {};
	}

	if (!FPaths::FileExists(Filename))
	{
		IESTranslator::Private::LogError(*this, PayLoadKey,
			LOCTEXT("IESImportFailure_OpenFile", "Failed to import IES, cannot open file."));

		return {};
	}

	if (!FFileHelper::LoadFileToArray(SourceDataBuffer, *Filename))
	{
		IESTranslator::Private::LogError(*this, PayLoadKey,
			LOCTEXT("IESImportFailure_LoadFile", "Failed to import IES, cannot load file content into an array."));

		return {};
	}

	const uint8* Buffer = SourceDataBuffer.GetData();

	FIESConverter IESConverter(Buffer, SourceDataBuffer.Num());

	if(IESConverter.IsValid())
	{
		UE::Interchange::FImportLightProfile Payload;

		Payload.Init2DWithParams(
			IESConverter.GetWidth(),
			IESConverter.GetHeight(),
			TSF_RGBA16F,
			false
			);

		Payload.CompressionSettings = TC_HDR;
		Payload.Brightness = IESConverter.GetBrightness();
		Payload.TextureMultiplier = IESConverter.GetMultiplier();


		const TArray<uint8>&  RawData = IESConverter.GetRawData();;

		FPlatformMemory::Memcpy(Payload.RawData.GetData(), RawData.GetData(), Payload.RawData.GetSize());

		return Payload;
	}
	else
	{
		IESTranslator::Private::LogError(*this, PayLoadKey,
			FText::Format(LOCTEXT("IESImportFailure_Generic", "IES import failed: \"{0}\""), FText::FromString(IESConverter.GetError())));
	}

	return {};
}

#undef LOCTEXT_NAMESPACE