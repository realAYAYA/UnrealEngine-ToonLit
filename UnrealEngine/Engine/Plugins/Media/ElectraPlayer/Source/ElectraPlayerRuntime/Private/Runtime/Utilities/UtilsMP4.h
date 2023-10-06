// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Misc/Variant.h>
#include "Utils/MPEG/ElectraUtilsMP4.h"
#include "MediaStreamMetadata.h"

namespace Electra
{

class UtilsMP4
{
public:
	/**
	 * This class parses metadata embedded in an mp4 / ISO14496-12 file.
	 * Presently only the structure as used and defined by Apple iTunes is supported.
	 */
	class FMetadataParser
	{
	public:
		FMetadataParser();
		enum class EResult
		{
			Success,
			NotSupported,
			MissingBox
		};
		struct FBoxInfo
		{
			FBoxInfo(uint32 InType, const void* InData, uint32 InSize) : Type(InType), Data(InData), Size(InSize)
			{}
			uint32 Type = 0;
			const void* Data = nullptr;
			uint32 Size = 0;
		};
		EResult Parse(uint32 InHandler, uint32 InHandlerReserved0, const TArray<FBoxInfo>& InBoxes);
		bool IsDifferentFrom(const FMetadataParser& Other);
		FString GetAsJSON() const;
		TSharedPtr<TMap<FString, TArray<TSharedPtr<IMediaStreamMetadata::IItem, ESPMode::ThreadSafe>>>, ESPMode::ThreadSafe> GetMediaStreamMetadata() const;
	private:
		FString PrintableBoxAtom(const uint32 InAtom);
		void Parse(const FBoxInfo& InBox);
		void ParseBoxDataList(const FString& AsCategory, const uint8* InBoxData, uint32 InBoxSize);
		void ParseBoxDataiTunes(const uint8* InBoxData, uint32 InBoxSize);

		class FItem : public IMediaStreamMetadata::IItem
		{
		public:
			~FItem() {}
			const FString& GetLanguageCode() const override
			{ return Language; }
			const FString& GetMimeType() const override
			{ return MimeType; }
			const FVariant& GetValue() const override
			{ return Value; }

			FString Language;				// ISO 639-2; if not set (all zero) the default entry for all languages
			FString MimeType;
			int32 Type = 0;					// Well-known data type (see Quicktime reference)
			FVariant Value;
			FString ToJSONValue() const;
			static TArray<TCHAR> CharsToEscapeInJSON;
			bool operator != (const FItem& Other) const
			{
				return Type != Other.Type || Language != Other.Language || Value != Other.Value;
			}
			bool operator == (const FItem& Other) const
			{
				return !(*this != Other);
			}
		};

		TMap<uint32, FString> WellKnownItems;
		TMap<FString, TArray<TSharedPtr<FItem, ESPMode::ThreadSafe>>> Items;
		uint32 NumTotalItems = 0;
	};


};

} // namespace Electra
