// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace EpicGames.Tracing.UnrealInsights
{
	public class StreamHeader
	{
		public const uint MagicTrc = 1414677317; // TRCE
		public const uint MagicTrc2 = 1414677298; // TRC2

		uint MagicFourCc;
		ushort MetadataSize;
		ushort MetadataField0;
		ushort ControlPort;

		byte TransportVersion;
		byte ProtocolVersion;

		private StreamHeader()
		{
		}

		public static StreamHeader Default()
		{
			StreamHeader Handshake = new StreamHeader();
			Handshake.MagicFourCc = MagicTrc;
			Handshake.TransportVersion = 3;
			Handshake.ProtocolVersion = 5;
			return Handshake;
		}

		public void Serialize(BinaryWriter Writer)
		{
			Writer.Write(MagicTrc);
			Writer.Write(TransportVersion);
			Writer.Write(ProtocolVersion);
		}

		public static StreamHeader Deserialize(BinaryReader Reader)
		{
			StreamHeader Header = new StreamHeader();
			Header.MagicFourCc = Reader.ReadUInt32();
			if (Header.MagicFourCc != MagicTrc && Header.MagicFourCc != MagicTrc2)
				throw new ArgumentException("Only support magic number TRCE and TRC2");

			if (Header.MagicFourCc == MagicTrc2)
			{
				Header.MetadataSize = Reader.ReadUInt16();
				if (Header.MetadataSize != 4)
					throw new ArgumentException("Only support metadata size of 4 bytes (got " + Header.MetadataSize + ")");
				Header.MetadataField0 = Reader.ReadUInt16();
				Header.ControlPort = Reader.ReadUInt16();
			}

			Header.TransportVersion = Reader.ReadByte();
			Header.ProtocolVersion = Reader.ReadByte();
			return Header;
		}
	}
}