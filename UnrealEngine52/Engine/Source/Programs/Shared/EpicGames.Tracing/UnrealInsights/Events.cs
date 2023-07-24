// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using EpicGames.Tracing.UnrealInsights.Events;

[assembly: InternalsVisibleTo("EpicGames.Tracing.Tests")]

namespace EpicGames.Tracing.UnrealInsights
{
	public static class BinaryReaderExtensions
	{
		public static byte[] ReadBytesStrict(this BinaryReader Reader, int Count)
		{
			byte[] Result = Reader.ReadBytes(Count);
			if (Result.Length != Count)
				throw new EndOfStreamException();

			return Result;
		}

		/// <summary>
		/// Check if bit is set for a given byte
		/// </summary>
		/// <param name="Value">Value to check</param>
		/// <param name="Pos">Pos 0 is least significant bit, pos 7 is most</param>
		/// <returns>True if set</returns>
		internal static bool IsBitSet(byte Value, int Pos)
		{
			return (Value & (1 << Pos)) != 0;
		}

		internal static bool IsTwoByteUid(byte UidLow)
		{
			return IsBitSet(UidLow, 0);
		}

		internal static byte GetHighByte(ushort a)
		{
			return (byte)(a >> 8);
		}

		internal static byte GetLowByte(ushort a)
		{
			return (byte)(a & 0xff);
		}
		
		internal static ushort GetHighWord(uint a)
		{
			return (ushort)(a >> 16);
		}

		internal static ushort GetLowWord(uint a)
		{
			return (ushort)(a & 0xffff);
		}

		internal static ushort MakeShort(byte LowByte, byte HighByte)
		{
			return (ushort)((byte)(LowByte & 0xff) | (ushort)(HighByte & 0xff) << 8);
		}

		internal static ushort GetPackedUid(byte UidLow, byte UidHigh, out bool IsTwoByteUidArg)
		{
			// struct packed_uid
			// {
			// 	uint8   is_two_byte_uid : 1
			// 			uid_low         : 7
			// 	(uint8  uid_high) // if is_two_byte_uid == 1
			// }

			IsTwoByteUidArg = IsTwoByteUid(UidLow);
			byte UidLowNoBit = (byte) (UidLow >> 1); // Strip the is_two_byte_uid bit

			return MakeShort(UidLowNoBit, IsTwoByteUidArg ? UidHigh : (byte) 0x00);
		}

		public static ushort ReadPackedUid(this BinaryReader Reader, out bool IsTwoByteUid)
		{
			byte UidLow = Reader.ReadByte();
			byte UidHigh = Reader.ReadByte();
			Reader.BaseStream.Position -= 1; // UidLow is always consumed, but UidHigh maybe not.
			
			ushort Uid = GetPackedUid(UidLow, UidHigh, out IsTwoByteUid);
			if (IsTwoByteUid)
			{
				Reader.ReadByte();  // Consume UidHigh
			}

			return Uid;
		}
		
		public static void EnsureEntireStreamIsConsumed(this BinaryReader Reader)
		{
			bool IsEntireStreamConsumed = Reader.BaseStream.Position == Reader.BaseStream.Length;
			if (!IsEntireStreamConsumed)
			{
				throw new Exception($"Entire stream/buffer was not consumed. Pos={Reader.BaseStream.Position} Len={Reader.BaseStream.Length}");
			}
		}
		
		public static bool IsEntireStreamConsumed(this BinaryReader Reader)
		{
			return Reader.BaseStream.Position == Reader.BaseStream.Length;
		}
	}

	public static class BinaryWriterExtensions
	{
		public static void WritePackedUid(this BinaryWriter Writer, ushort Uid)
		{
			if (Uid < PredefinedEventUid._WellKnownNum)
			{
				byte UidLow = (byte) Uid;
				UidLow = (byte) (UidLow << 1);
				// LSB is 0 after shifting, indicating a one-byte UID
				Writer.Write(UidLow);
			}
			else if (Uid < 127)
			{
				byte UidLow = BinaryReaderExtensions.GetLowByte(Uid);
				byte UidHigh = BinaryReaderExtensions.GetHighByte(Uid);
				UidLow = (byte) (UidLow << 1);
				UidLow = (byte) (UidLow | 1);
				Writer.Write(UidLow);
				Writer.Write(UidHigh);
			}
			else
			{
				throw new NotImplementedException("Handling of UIDs >= 127 not implemented");
			}
		}
	}

	public interface ITraceEvent
	{
		ushort Size { get; }
		EventType Type { get; }
		public void Serialize(ushort Uid, BinaryWriter Writer);
	}

	public class EnterScopeEvent : ITraceEvent
	{
		public ushort Size => 0;
		public EventType Type => EventType.WellKnown(PredefinedEventUid.EnterScope, "EnterScope");
		public void Serialize(ushort Uid, BinaryWriter Writer) { throw new NotImplementedException(); }
	}
	
	public class LeaveScopeEvent : ITraceEvent
	{
		public ushort Size => 0;
		public EventType Type => EventType.WellKnown(PredefinedEventUid.LeaveScope, "LeaveScope");
		public void Serialize(ushort Uid, BinaryWriter Writer) { throw new NotImplementedException(); }
	}
	
	public class EnterScopeEventTimestamp : ITraceEvent
	{
		public ushort Size => 7;
		public EventType Type => EventType.WellKnown(PredefinedEventUid.EnterScope_T, "EnterScopeTimestamp");
		public ulong Timestamp { get; }

		public EnterScopeEventTimestamp(ulong Timestamp)
		{
			this.Timestamp = Timestamp;
		}

		public void Serialize(ushort Uid, BinaryWriter Writer) { throw new NotImplementedException(); }

		public static EnterScopeEventTimestamp Deserialize(BinaryReader Reader)
		{
			ulong Value = Reader.ReadUInt64();
			ushort UidFound = BinaryReaderExtensions.GetPackedUid((byte) (Value & 0xFF), 0x00, out bool _);

			if (UidFound != PredefinedEventUid.EnterScope_T)
			{
				throw new ArgumentException($"Bad UID found when deserializing 0x{UidFound:X4}/{UidFound}");
			}

			ulong Timestamp = Value >> 8;
			return new EnterScopeEventTimestamp(Timestamp);
		}
	}
	
	public class LeaveScopeEventTimestamp : ITraceEvent
	{
		public ushort Size => 7;
		public EventType Type => EventType.WellKnown(PredefinedEventUid.EnterScope_T, "LeaveScopeTimestamp");
		readonly ulong Timestamp;

		public LeaveScopeEventTimestamp(ulong Timestamp)
		{
			this.Timestamp = Timestamp;
		}

		public void Serialize(ushort Uid, BinaryWriter Writer) { throw new NotImplementedException(); }
		
		public static LeaveScopeEventTimestamp Deserialize(BinaryReader Reader)
		{
			ulong Value = Reader.ReadUInt64();
			ushort UidFound = BinaryReaderExtensions.GetPackedUid((byte) (Value & 0xFF), 0x00, out bool _);

			if (UidFound != PredefinedEventUid.LeaveScope_T)
			{
				throw new ArgumentException($"Bad UID found when deserializing 0x{UidFound:X4}/{UidFound}");
			}

			ulong Timestamp = Value >> 8;
			return new LeaveScopeEventTimestamp(Timestamp);
		}
	}

	public class TraceImportantEventHeader
	{
		public ushort Uid { get; }
		public ushort EventSize { get; }

		public const ushort HeaderSize = sizeof(ushort) + sizeof(ushort); 

		public TraceImportantEventHeader(ushort Uid, ushort EventSize)
		{
#pragma warning disable CA1508 // Avoid dead conditional code
			Debug.Assert(HeaderSize == 4);
#pragma warning restore CA1508 // Avoid dead conditional code
			this.Uid = Uid;
			this.EventSize = EventSize;
		}

		public void Serialize(BinaryWriter Writer)
		{
			Writer.Write(Uid);
			Writer.Write(EventSize);
		}

		public static TraceImportantEventHeader Deserialize(BinaryReader Reader)
		{
			ushort Uid = Reader.ReadUInt16();
			return new TraceImportantEventHeader(Uid, Reader.ReadUInt16());
		}
	}
}