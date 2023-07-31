// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/UtilsMPEG.h"
#include "BitDataStream.h"



namespace Electra
{
	namespace MPEG
	{

		class FMP4BitReader : public FBitDataStream
		{
		public:
			FMP4BitReader(const void* pData, int64 nData)
				: FBitDataStream(pData, nData)
			{
			}
			int32 ReadMP4Length()
			{
				int32 Length = 0;
				for (int32 i = 0; i < 4; ++i)
				{
					uint32 Bits = GetBits(8);
					Length = (Length << 7) + (Bits & 0x7f);
					if ((Bits & 0x80) == 0)
						break;
				}
				return Length;
			}
		};


		FESDescriptor::FESDescriptor()
			: ObjectTypeID(FObjectTypeID::Unknown)
			, StreamTypeID(FStreamType::Unknown)
			, BufferSize(0)
			, MaxBitrate(0)
			, AvgBitrate(0)
			, ESID(0)
			, DependsOnStreamESID(0)
			, StreamPriority(16)
			, bDependsOnStream(false)
		{
		}

		void FESDescriptor::SetRawData(const void* Data, int64 Size)
		{
			RawData.Empty();
			if (Size)
			{
				RawData.Reserve((uint32)Size);
				RawData.SetNumUninitialized((uint32)Size);
				FMemory::Memcpy(RawData.GetData(), Data, Size);
			}
		}

		const TArray<uint8>& FESDescriptor::GetRawData() const
		{
			return RawData;
		}

		const TArray<uint8>& FESDescriptor::GetCodecSpecificData() const
		{
			return CodecSpecificData;
		}


		bool FESDescriptor::Parse()
		{
			CodecSpecificData.Empty();

			FMP4BitReader BitReader(RawData.GetData(), RawData.Num());

			if (BitReader.GetBits(8) != 3)
			{
				return false;
			}

			int32 ESSize = BitReader.ReadMP4Length();

			ESID = (uint16)BitReader.GetBits(16);
			bDependsOnStream = BitReader.GetBits(1) != 0;
			bool bURLFlag = BitReader.GetBits(1) != 0;
			bool bOCRflag = BitReader.GetBits(1) != 0;
			StreamPriority = (uint8)BitReader.GetBits(5);
			if (bDependsOnStream)
			{
				DependsOnStreamESID = BitReader.GetBits(16);
			}
			if (bURLFlag)
			{
				// Skip over the URL
				uint32 urlLen = BitReader.GetBits(8);
				BitReader.SkipBytes(urlLen);
			}
			if (bOCRflag)
			{
				// Skip the OCR ES ID
				BitReader.SkipBits(16);
			}

			// Parse the config descriptor
			if (BitReader.GetBits(8) != 4)
			{
				return false;
			}
			int32 ConfigDescrSize = BitReader.ReadMP4Length();
			ObjectTypeID = static_cast<FObjectTypeID>(BitReader.GetBits(8));
			StreamTypeID = static_cast<FStreamType>(BitReader.GetBits(6));
			// Skip upstream flag
			BitReader.SkipBits(1);
			if (BitReader.GetBits(1) != 1)	// reserved, must be 1
			{
				return false;
			}
			BufferSize = BitReader.GetBits(24);
			MaxBitrate = BitReader.GetBits(32);
			AvgBitrate = BitReader.GetBits(32);
			if (ConfigDescrSize > 13)
			{
				// Optional codec specific descriptor
				if (BitReader.GetBits(8) != 5)
				{
					return false;
				}
				int32 CodecSize = BitReader.ReadMP4Length();
				CodecSpecificData.Reserve(CodecSize);
				for (int32 i = 0; i < CodecSize; ++i)
				{
					CodecSpecificData.Push(BitReader.GetBits(8));
				}
			}

			// SL config (we do not need it, we require it to be there though as per the standard)
			if (BitReader.GetBits(8) != 6)
			{
				return false;
			}
			int32 nSLSize = BitReader.ReadMP4Length();
			if (nSLSize != 1)
			{
				return false;
			}
			if (BitReader.GetBits(8) != 2)
			{
				return false;
			}

			return true;
		}

	} // namespace MPEG
} // namespace Electra

