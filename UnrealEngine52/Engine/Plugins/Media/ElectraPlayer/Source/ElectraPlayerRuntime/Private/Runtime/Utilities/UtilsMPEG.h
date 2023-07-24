// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "BitDataStream.h"


namespace Electra
{
	namespace MPEG
	{

		class FESDescriptor
		{
		public:
			FESDescriptor();
			void SetRawData(const void* Data, int64 Size);
			const TArray<uint8>& GetRawData() const;

			bool Parse();

			const TArray<uint8>& GetCodecSpecificData() const;

			uint32 GetBufferSize() const
			{
				return BufferSize;
			}

			uint32 GetMaxBitrate() const
			{
				return MaxBitrate;
			}

			uint32 GetAvgBitrate() const
			{
				return AvgBitrate;
			}

			// See http://mp4ra.org/#/object_types
			enum class FObjectTypeID
			{
				Unknown = 0,
				Text_Stream = 8,
				MPEG4_Video = 0x20,
				H264 = 0x21,
				H264_ParameterSets = 0x22,
				H265 = 0x23,
				MPEG4_Audio = 0x40
			};
			enum class FStreamType
			{
				Unknown = 0,
				VisualStream = 4,
				AudioStream = 5,
			};

			FObjectTypeID GetObjectTypeID() const
			{
				return ObjectTypeID;
			}

			FStreamType GetStreamType() const
			{
				return StreamTypeID;
			}

		private:
			TArray<uint8>						RawData;

			TArray<uint8>						CodecSpecificData;
			FObjectTypeID						ObjectTypeID;
			FStreamType							StreamTypeID;
			uint32								BufferSize;
			uint32								MaxBitrate;
			uint32								AvgBitrate;
			uint16								ESID;
			uint16								DependsOnStreamESID;
			uint8								StreamPriority;
			bool								bDependsOnStream;
		};


	} // namespace MPEG
} // namespace Electra


