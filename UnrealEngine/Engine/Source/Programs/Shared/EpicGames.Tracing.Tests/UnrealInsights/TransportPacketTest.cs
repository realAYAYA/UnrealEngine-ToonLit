// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using EpicGames.Tracing.UnrealInsights;
using EpicGames.Tracing.UnrealInsights.Events;

namespace EpicGames.Tracing.Tests.UnrealInsights
{
	[TestClass]
	public class TransportPacketTest
	{
		[TestMethod]
		public void ShortByteHandling()
		{
			ushort Val1 = 0x1234;
			Assert.AreEqual(0x34, BinaryReaderExtensions.GetLowByte(Val1));
			Assert.AreEqual(0x12, BinaryReaderExtensions.GetHighByte(Val1));
			
			ushort Val2 = BinaryReaderExtensions.MakeShort(0x56, 0x78);
			Assert.AreEqual(0x56, BinaryReaderExtensions.GetLowByte(Val2));
			Assert.AreEqual(0x78, BinaryReaderExtensions.GetHighByte(Val2));
			
			ushort Val3 = BinaryReaderExtensions.MakeShort(0x25, 0x00);
			Assert.AreEqual(0x25, Val3);
		}

		[TestMethod]
		public void GetPackedUid()
		{
			void AssertGetPackedUid(ushort ExpectedUid, bool ExpectedIsTwoByteUid, byte UidLow, byte UidHigh)
			{
				Assert.AreEqual(ExpectedUid, BinaryReaderExtensions.GetPackedUid(UidLow, UidHigh, out bool IsTwoByteUid));
				Assert.AreEqual(ExpectedIsTwoByteUid, IsTwoByteUid);
			}

			AssertGetPackedUid(0x05, false, 0x0A, 0x08);
			AssertGetPackedUid(0x13, false, 0x26, 0x00);
			AssertGetPackedUid(4, false, 0x08, 0x25);
			AssertGetPackedUid(4, false, 0x08, 0x29);
			AssertGetPackedUid(5, false, 0x0A, 0x08);
			AssertGetPackedUid(8, false, 0x10, 0xE5);
			AssertGetPackedUid(12, false, 0x18, 0xCA);
			AssertGetPackedUid(5, false, 0x0A, 0x0A);
			AssertGetPackedUid(12, false, 0x18, 0xF3);
			
			AssertGetPackedUid(17, true, 0x23, 0x00);
			AssertGetPackedUid(18, true, 0x25, 0x00);
			AssertGetPackedUid(39, true, 0x4F, 0x00);
			AssertGetPackedUid(41, true, 0x53, 0x00);
			AssertGetPackedUid(0x12, true, 0x25, 0x00);
			AssertGetPackedUid(0x13, true, 0x27, 0x00);
			AssertGetPackedUid(0x4513, true, 0x27, 0x45);
		}
		
		[TestMethod]
		public void WritePackedUid()
		{
			void AssertWriteUid(ushort Uid, bool ExpectTwoByteUid)
			{
				using MemoryStream Ms = new MemoryStream();
				using BinaryWriter Writer = new BinaryWriter(Ms);
				Writer.WritePackedUid(Uid);
				byte[] Data = Ms.ToArray();
				Assert.AreEqual(ExpectTwoByteUid ? 2 : 1, Data.Length);
				byte UidHigh = Data.Length == 2 ? Data[1] : (byte)0xFF;
				ushort DeserializedUid = BinaryReaderExtensions.GetPackedUid(Data[0], UidHigh, out bool IsTwoByteUid);
				Assert.AreEqual(ExpectTwoByteUid, IsTwoByteUid);
				Assert.AreEqual(Uid, DeserializedUid);
			}
			
			AssertWriteUid(0x00, false);
			AssertWriteUid(0x0A, false);
			AssertWriteUid(0x05, false);
		
			AssertWriteUid(17, true);
			AssertWriteUid(18, true);
			AssertWriteUid(126, true);

			Assert.ThrowsException<NotImplementedException>(() => AssertWriteUid(127, true));
			Assert.ThrowsException<NotImplementedException>(() => AssertWriteUid(128, true));
			Assert.ThrowsException<NotImplementedException>(() => AssertWriteUid(4362, true));
		}

		[TestMethod]
		public void Deserialize7Bit()
		{
			void Assert7Bit(ulong ExpectedValue, string ActualBytesHex)
			{
				byte[] ActualBytes = GenericEventTest.StringToByteArray(ActualBytesHex.Replace(" ", "", StringComparison.Ordinal));
				using MemoryStream Ms = new MemoryStream(ActualBytes);
				using BinaryReader Reader = new BinaryReader(Ms);
				Assert.AreEqual(ExpectedValue, TraceUtils.Read7BitUint(Reader));
			}

			Assert7Bit(0, "00");
			Assert7Bit(1, "01");
			Assert7Bit(127, "7F");
			Assert7Bit(128, "80 01");
			Assert7Bit(129, "81 01");
			
			Assert7Bit(16383, "FF 7F");
			Assert7Bit(16384, "80 80 01");
			Assert7Bit(16385, "81 80 01");
			
			Assert7Bit(2097151, "FF FF 7F");
			Assert7Bit(2097152, "80 80 80 01");
			Assert7Bit(2097153, "81 80 80 01");
			
			Assert7Bit(268435455, "FF FF FF 7F");
			Assert7Bit(268435456, "80 80 80 80 01");
			Assert7Bit(268435457, "81 80 80 80 01");
			
			Assert7Bit(34359738367, "FF FF FF FF 7F");
			Assert7Bit(34359738368, "80 80 80 80 80 01");
			Assert7Bit(34359738369, "81 80 80 80 80 01");
			
			Assert7Bit(562949953421311, "FF FF FF FF FF FF 7F");
			Assert7Bit(562949953421312, "80 80 80 80 80 80 80 01");
			Assert7Bit(562949953421313, "81 80 80 80 80 80 80 01");
		}
		
		[TestMethod]
		public void Serialize7Bit()
		{
			void Assert7Bit(ulong Value, string ExpectedBytesHex)
			{
				byte[] ExpectedBytes = GenericEventTest.StringToByteArray(ExpectedBytesHex.Replace(" ", "", StringComparison.Ordinal));
				using MemoryStream Ms = new MemoryStream();
				using BinaryWriter Writer = new BinaryWriter(Ms);

				int ActualNumBytesWritten = TraceUtils.Write7BitUint(Writer, Value);
				byte[] ActualBytes = Ms.ToArray();

				Assert.AreEqual(ExpectedBytes.Length, ActualNumBytesWritten);
				Assert.AreEqual(ExpectedBytes.Length, ActualBytes.Length);
				GenericEventTest.AssertHexString(ExpectedBytesHex, ActualBytes);
			}

			Assert7Bit(0, "00");
			Assert7Bit(1, "01");
			Assert7Bit(127, "7F");
			Assert7Bit(128, "80 01");
			Assert7Bit(129, "81 01");
			
			Assert7Bit(16383, "FF 7F");
			Assert7Bit(16384, "80 80 01");
			Assert7Bit(16385, "81 80 01");
			
			Assert7Bit(2097151, "FF FF 7F");
			Assert7Bit(2097152, "80 80 80 01");
			Assert7Bit(2097153, "81 80 80 01");
			
			Assert7Bit(268435455, "FF FF FF 7F");
			Assert7Bit(268435456, "80 80 80 80 01");
			Assert7Bit(268435457, "81 80 80 80 01");
			
			Assert7Bit(34359738367, "FF FF FF FF 7F");
			Assert7Bit(34359738368, "80 80 80 80 80 01");
			Assert7Bit(34359738369, "81 80 80 80 80 01");
			
			Assert7Bit(562949953421311, "FF FF FF FF FF FF 7F");
			Assert7Bit(562949953421312, "80 80 80 80 80 80 80 01");
			Assert7Bit(562949953421313, "81 80 80 80 80 80 80 01");
		}

		[TestMethod]
		public void SerializeDeserializeWithStubs()
		{
			StubTraceEvent Event1 = new StubTraceEvent(1000, 20);
			StubTraceEvent Event2 = new StubTraceEvent(2000, 30);
			ushort Uid1 = 5555;
			ushort Uid2 = 6666;

			{
				using MemoryStream Ms = new MemoryStream();
				using BinaryWriter Writer = new BinaryWriter(Ms);
				TransportPacket Packet = TransportPacket.Create(0, 0);
				Packet.Serialize(Writer, new [] { (Uid1, Event1 as ITraceEvent), (Uid2, Event2 as ITraceEvent) });
				
				Ms.Position = 0;
				using BinaryReader Reader = new BinaryReader(Ms);

				TransportPacket DeserializedPacket = TransportPacket.Deserialize(Reader);
				Assert.AreEqual(0, DeserializedPacket.ThreadIdAndMarkers);
				Assert.AreEqual(Event1.Size + Event2.Size, DeserializedPacket.GetData().Length);
			}
			
			{
				using MemoryStream Ms = new MemoryStream();
				using BinaryWriter Writer = new BinaryWriter(Ms);
				TransportPacket Packet = TransportPacket.Create(0, 1);
				Packet.Serialize(Writer, Array.Empty<(ushort, ITraceEvent)>());

				Ms.Position = 0;
				using BinaryReader Reader = new BinaryReader(Ms);

				TransportPacket DeserializedPacket = TransportPacket.Deserialize(Reader);
				Assert.AreEqual(1, DeserializedPacket.ThreadIdAndMarkers);
				Assert.AreEqual(0, DeserializedPacket.GetData().Length);
			}
			
			{
				using MemoryStream Ms = new MemoryStream();
				using BinaryWriter Writer = new BinaryWriter(Ms);
				TransportPacket Packet = TransportPacket.Create(0, 12345);
				Packet.Serialize(Writer, Array.Empty<(ushort, ITraceEvent)>());

				Ms.Position = 0;
				using BinaryReader Reader = new BinaryReader(Ms);

				TransportPacket DeserializedPacket = TransportPacket.Deserialize(Reader);
				Assert.AreEqual(12345, DeserializedPacket.ThreadIdAndMarkers);
				Assert.AreEqual(0, DeserializedPacket.GetData().Length);
			}
		}
		
		[TestMethod]
		public void SerializeDeserialize()
		{
			TraceNewTraceEvent Event1 = new TraceNewTraceEvent(25582215261913, 10000000, 21069, 8);
			DiagnosticsSession2Event Event2 = new DiagnosticsSession2Event("Win64", "UnrealEditor", @" D:\depot\starship-main\QAGame\QAGame.uproject -trace=cpu -game", "++UE5+Main", "++UE5+Main-CL-17442524", 17442524, 3, 4);
			CpuProfilerEventSpecEvent Event3 = new CpuProfilerEventSpecEvent(9524, "K2Node_VariableGet /Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere:UserConstructionScript.K2Node_VariableGet_983");

			ushort Uid1 = 10;
			ushort Uid2 = 20;
			ushort Uid3 = 30;

			{
				using MemoryStream Ms = new MemoryStream();
				using BinaryWriter Writer = new BinaryWriter(Ms);
				TransportPacket Packet = TransportPacket.Create(0, 0);
				Packet.Serialize(Writer, new [] { (Uid1, Event1 as ITraceEvent), (Uid2, Event2 as ITraceEvent) });
				
				Ms.Position = 0;
				using BinaryReader Reader = new BinaryReader(Ms);

				TransportPacket DeserializedPacket = TransportPacket.Deserialize(Reader);
				Assert.AreEqual(0, DeserializedPacket.ThreadIdAndMarkers);
				Assert.AreEqual(Event1.Size + Event2.Size, DeserializedPacket.GetData().Length);
				Reader.EnsureEntireStreamIsConsumed();
			}
			
			{
				using MemoryStream Ms = new MemoryStream();
				using BinaryWriter Writer = new BinaryWriter(Ms);
				TransportPacket Packet = TransportPacket.Create(0, 0);
				Packet.Serialize(Writer, new []
				{
					(Uid1, Event1.Type as ITraceEvent),
					(Uid2, Event2.Type as ITraceEvent),
					(Uid3, Event3.Type as ITraceEvent)
				});
				
				Ms.Position = 0;
				using BinaryReader Reader = new BinaryReader(Ms);

				TransportPacket DeserializedPacket = TransportPacket.Deserialize(Reader);
				Assert.AreEqual(0, DeserializedPacket.ThreadIdAndMarkers);
				Assert.AreEqual(Event1.Type.Size + Event2.Type.Size + Event3.Type.Size, DeserializedPacket.GetData().Length);
				Reader.EnsureEntireStreamIsConsumed();
			}
		}
	}
}