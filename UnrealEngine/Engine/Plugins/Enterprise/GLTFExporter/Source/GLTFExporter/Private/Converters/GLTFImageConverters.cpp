// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFImageConverters.h"
#include "Converters/GLTFImageUtility.h"
#include "Builders/GLTFConvertBuilder.h"
#include "Misc/FileHelper.h"

FGLTFJsonImage* FGLTFImageConverter::Convert(TGLTFSuperfluous<FString> Name, EGLTFTextureType Type, bool bIgnoreAlpha, FIntPoint Size, TGLTFSharedArray<FColor> Pixels)
{
	const TSharedPtr<FGLTFMemoryArchive> CompressedData = MakeShared<FGLTFMemoryArchive>();

	const EGLTFJsonMimeType MimeType = GetMimeType(Pixels->GetData(), Size, bIgnoreAlpha, Type);
	switch (MimeType)
	{
		case EGLTFJsonMimeType::None:
			return nullptr;

		case EGLTFJsonMimeType::PNG:
			FGLTFImageUtility::CompressToPNG(Pixels->GetData(), Size, *CompressedData);
			break;

		case EGLTFJsonMimeType::JPEG:
			FGLTFImageUtility::CompressToJPEG(Pixels->GetData(), Size, Builder.ExportOptions->TextureImageQuality, *CompressedData);
			break;

		default:
			checkNoEntry();
			break;
	}

	FGLTFJsonImage* JsonImage = Builder.AddImage();

	if (Builder.bIsGLB)
	{
		JsonImage->Name = Name;
		JsonImage->MimeType = MimeType;
		JsonImage->BufferView = Builder.AddBufferView(CompressedData->GetData(), CompressedData->Num());
	}
	else
	{
		const TCHAR* Extension = FGLTFImageUtility::GetFileExtension(MimeType);
		JsonImage->URI = Builder.AddExternalFile(Name + Extension, CompressedData);
	}

	return JsonImage;
}

EGLTFJsonMimeType FGLTFImageConverter::GetMimeType(const FColor* Pixels, FIntPoint Size, bool bIgnoreAlpha, EGLTFTextureType Type) const
{
	switch (Builder.ExportOptions->TextureImageFormat)
	{
		case EGLTFTextureImageFormat::None:
			return EGLTFJsonMimeType::None;

		case EGLTFTextureImageFormat::PNG:
			return EGLTFJsonMimeType::PNG;

		case EGLTFTextureImageFormat::JPEG:
			return
				(Type == EGLTFTextureType::None || !EnumHasAllFlags(static_cast<EGLTFTextureType>(Builder.ExportOptions->NoLossyImageFormatFor), Type)) &&
				(bIgnoreAlpha || FGLTFImageUtility::NoAlphaNeeded(Pixels, Size)) ?
				EGLTFJsonMimeType::JPEG : EGLTFJsonMimeType::PNG;

		default:
			checkNoEntry();
		return EGLTFJsonMimeType::None;
	}
}
