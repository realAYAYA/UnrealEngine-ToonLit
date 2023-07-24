// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace EpicGames.Tracing.UnrealInsights
{
	public static class TraceUtils
	{
		public static ulong Read7BitUint(BinaryReader Reader)
		{
			ulong Value = 0;
			ulong ByteIndex = 0;
			bool HasMoreBytes;
			
			do
			{
				byte ByteValue = Reader.ReadByte();
				HasMoreBytes = (ByteValue & 0x80) != 0;
				Value |= (ulong)(ByteValue & 0x7f) << (int)(ByteIndex * 7);
				++ByteIndex;
			} while (HasMoreBytes);
			
			return Value;
		}

		public static int Write7BitUint(BinaryWriter Writer, ulong Value)
		{
			int NumBytesWritten = 0;
			do
			{
				byte HasMoreBytesBit = (byte) (Value > 0x7F ? 1 : 0);
				byte HasMoreBytes = (byte)(HasMoreBytesBit << 7);
				byte ByteToWrite = (byte)((Value & 0x7F) | HasMoreBytes);
				
				Writer.Write(ByteToWrite);
				Value >>= 7;
				NumBytesWritten++;
				
			} while (Value > 0);

			return NumBytesWritten;
		}
	}
}