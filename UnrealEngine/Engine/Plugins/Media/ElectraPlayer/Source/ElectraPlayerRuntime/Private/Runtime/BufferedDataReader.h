// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Build.h"
#include "Utilities/Utilities.h"
#include "ErrorDetail.h"

namespace Electra
{
	class FBufferedDataReader
	{
	public:
		class IDataProvider
		{
		public:
			enum class EError
			{
				Failed = -1,
				EOS = -2,
				Aborted = -3
			};
			virtual int64 OnReadAssetData(void* Destination, int64 NumBytes, int64 FromOffset, int64* OutTotalSize) = 0;
		};

		FBufferedDataReader(IDataProvider* InDataProvider)
			: DataProvider(InDataProvider)
		{ }

		bool Failed() const
		{
			return LastError.IsSet();
		}
		FErrorDetail GetLastError() const
		{
			return LastError;
		}

		int64 GetCurrentOffset() const
		{ 
			return CurrentOffset; 
		}

		int64 GetTotalDataSize() const
		{
			return TotalDataSize;
		}

		bool PrepareToRead(int64 NumBytes);
		bool ReadU8(uint8& OutValue);
		bool ReadU16LE(uint16& OutValue);
		bool ReadU32LE(uint32& OutValue);
		bool ReadU64LE(uint64& OutValue);
		bool ReadU16BE(uint16& OutValue);
		bool ReadU32BE(uint32& OutValue);
		bool ReadU64BE(uint64& OutValue);
		bool PeekU8(uint8& OutValue);
		bool PeekU16LE(uint16& OutValue);
		bool PeekU32LE(uint32& OutValue);
		bool PeekU64LE(uint64& OutValue);
		bool PeekU16BE(uint16& OutValue);
		bool PeekU32BE(uint32& OutValue);
		bool PeekU64BE(uint64& OutValue);
		bool SkipOver(int64 NumBytes);
		bool ReadByteArray(TArray<uint8>& OutValue, int64 NumBytes);
		bool SeekTo(int64 AbsolutePosition);
		bool IsAtEOS();
	protected:
		enum
		{
			kDefaultReadSize = 65536
		};

		struct FArea
		{
			~FArea()
			{
				FMemory::Free((void*) Data);
			}
			const uint8* Data = nullptr;
			int64 Size = 0;
			int64 StartOffset = 0;
			bool bEOS = false;
		};

		FArea* FindAreaForOffset(int64 Offset);
		void CreateNewArea(int64 NumBytes, int64 FromOffset);
		bool EnlargeCurrentAreaBy(int64 NumBytesToAdd);

		void UpdateReadDataPointer()
		{
			ReadDataPointer = !CurrentArea ? nullptr : CurrentArea->Data + CurrentOffset - CurrentArea->StartOffset;
		}

		IDataProvider* DataProvider = nullptr;
		FErrorDetail LastError;

		TArray<TUniquePtr<FArea>> Areas;
		int64 TotalDataSize = -1;

		FArea* CurrentArea = nullptr;
		int64 BytesRemainingInArea = 0;

		int64 CurrentOffset = 0;
		const uint8* ReadDataPointer = nullptr;
	};

} // namespace Electra
