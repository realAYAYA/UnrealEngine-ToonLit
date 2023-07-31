// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using EpicGames.Tracing.UnrealInsights;
using EpicGames.Tracing.UnrealInsights.Events;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Tracing.Tests.UnrealInsights
{
	[TestClass]
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Naming", "CA1707:Identifiers should not contain underscores")]
	public class GenericEventTest
	{
		private readonly string CpuProfilerEventSpecHex = "21 00 77 00 34 25 00 00 01 C1 0D 00 4B 32 4E 6F 64 65 5F 56 61 72 69 61 62 6C 65 47 65 74 20 2F 45 6E 67 69 6E 65 2F 45 6E 67 69 6E 65 53 6B 79 2F 42 50 5F 53 6B 79 5F 53 70 68 65 72 65 2E 42 50 5F 53 6B 79 5F 53 70 68 65 72 65 3A 55 73 65 72 43 6F 6E 73 74 72 75 63 74 69 6F 6E 53 63 72 69 70 74 2E 4B 32 4E 6F 64 65 5F 56 61 72 69 61 62 6C 65 47 65 74 5F 39 38 33 03";
		private readonly string CpuProfilerEventBatchHex1 = "4F 00 02 20 1E 00 87 A7 EB AE 8A D1 0B 01 A7 02 02 04 FB 23 03 A4 0A 61 04 0B 05 F2 80 42 AA 1C EB 5C 06 C0 BC 03 17 07 02 47 08 C2 02 AB 15 09 AA 03 07 0A AE 01 05 0B D8 0D 05 0C B7 01 0D D8 03 00 03 0E A4 03 6F 0F 89 FF 03 10 F3 C4 04 11 60 06 A7 02 10 AB 19 12 B3 04 10 9F 11 13 0A 04 02 04 ED 03 10 91 11 14 80 01 02 97 02 10 A9 D6 0B 15 10 06 DD 03 10 CB 85 04 16 12 06 D3 01 10 89 16 17 0C 02 D6 02 07 18 DA 84 28 99 01 19 18 05 1A 0A A5 10 1B CD 01 1C 06 88 6A C5 49 1D B8 EB 24 A9 17 20 AF 01 10 CD F9 01 21 71 22 F2 AE 12 85 04 23 D1 0D 24 85 74 25 EA 58 91 0D 25 80 E6 02 A9 1C 25 88 CD 06 A7 27 25 D0 DA 09 8B 20 25 DE AD 0B AF 33 25 C0 A9 09 ED 23 25 C4 81 0A F5 2D 25 9A EC 0A F3 35 25 D4 CF 03 FF 19 25 BE 92 01 B1 0D 25 98 22 06";
		// Unused at the moment -- private readonly string CpuProfilerEventBatchHex2 = "4F 00 02 40 1E 00 E4 AD A2 B2 8A D1 0B 17 41 12 0B 41 12 0B 41 04 11 41 2C 0F 41 0E 17 41 14 13 41 10 11 41 26 0F 41 12 B1 01 41 28 0D 41 04 0F 41 18 09 41 04 0F 41 16 0B 41 16 19 41 24 0D 41 14 17 41 0E 0B 41 2A 0B 41 48 31 41 06 15 41 06 5B 41 14 0B 41 0C 25 41 14 25 41 14 25 41 10 15 41 12 2D 41 0E 15 41 04 25 41 10 19 41 0C 17 41 16 17 41 1E 17 41 14 0B 41 0E 2D 41 0E 45 41 04 15 41 14 17 41 14 13 41 18 19 41 06 0D 41 10 11 41 10 0B 41 0E 0B 41 0E 0D 41 10 0B 41 10 0F 41 18 0F 41 12 0B 41 18 19 41 0E 1F 41 16 0D 41 14 1F 41 16 33 41 10 0D 41 24 25 41 18 15 41 04 23 41 06 25 41 04 0B 41 14 0F 41 0E 15 41 12 77 41 12 0D 41 14 23 41 18 0F 41 16 3B 41 18 19 41 20 81 01 41 18 4D 41 16 2B 41 16 1F 41 1C 11 41 10 11 41 10 0B 41 0E 15 41 06";
		private readonly string LoggingLogMessageHex = "35 00 B4 FD CA E1 FB 7F 00 00 A4 AB DB 52 44 17 00 00 02 A2 03 00 02 C2 44 61 00 71 00 50 00 72 00 6F 00 66 00 2E 00 64 00 6C 00 6C 00 00 00 7E 00 00 00 06";
		private readonly string MemoryMemoryScopeHex1 = "25 00 2D 00 00 27 00 00 00";
		private readonly string MemoryMemoryScopeHex2 = "25 00 2E 00 00 27 00 00 00";
		private readonly string MemoryMemoryScopeHex3 = "25 00 3D 59 00 33 00 00 00";
		private readonly string TraceNewTraceHex = "10 00 13 00 D9 6E DA 52 44 17 00 00 80 96 98 00 00 00 00 00 4D 52 08";
		private readonly string DiagnosticsSession2Hex = "22 00 EA 00 DC 26 0A 01 03 04 01 A0 00 00 57 69 6E 36 34 01 81 01 00 55 6E 72 65 61 6C 45 64 69 74 6F 72 01 C2 0F 00 20 00 44 00 3A 00 5C 00 64 00 65 00 70 00 6F 00 74 00 5C 00 73 00 74 00 61 00 72 00 73 00 68 00 69 00 70 00 2D 00 6D 00 61 00 69 00 6E 00 5C 00 51 00 41 00 47 00 61 00 6D 00 65 00 5C 00 51 00 41 00 47 00 61 00 6D 00 65 00 2E 00 75 00 70 00 72 00 6F 00 6A 00 65 00 63 00 74 00 20 00 2D 00 74 00 72 00 61 00 63 00 65 00 3D 00 63 00 70 00 75 00 20 00 2D 00 67 00 61 00 6D 00 65 00 01 83 02 00 2B 00 2B 00 55 00 45 00 35 00 2B 00 4D 00 61 00 69 00 6E 00 01 84 05 00 2B 00 2B 00 55 00 45 00 35 00 2B 00 4D 00 61 00 69 00 6E 00 2D 00 43 00 4C 00 2D 00 31 00 37 00 34 00 34 00 32 00 35 00 32 00 34 00 03";
		private readonly string TraceThreadInfoHex1 = "24 00 1B 00 02 00 00 00 74 9D 02 00 FF FF FF FF 01 43 01 00 47 61 6D 65 54 68 72 65 61 64 03";
		private readonly string TraceThreadInfoHex2 = "24 00 16 00 03 00 00 00 00 00 00 00 FF FF FF 7F 01 A3 00 00 54 72 61 63 65 03";
		private readonly string TraceThreadInfoHex3 = "24 00 25 00 04 00 00 00 D8 BB 06 00 50 00 00 00 01 83 02 00 46 6F 72 65 67 72 6F 75 6E 64 20 57 6F 72 6B 65 72 20 23 30 03";
		
		[TestMethod]
		public void Deserialize_Memory_MemoryScope()
		{
			GenericEvent Event1 = DeserializeAndAssertGenericEvent(MemoryMemoryScopeHex1, MemoryMemoryScopeEvent.EventType, false, 18, 45);
			Assert.AreEqual(0x27, Event1.GetFields()[0].Int!.Value);
			
			GenericEvent Event2 = DeserializeAndAssertGenericEvent(MemoryMemoryScopeHex2, MemoryMemoryScopeEvent.EventType, false, 18, 46);
			Assert.AreEqual(0x27, Event2.GetFields()[0].Int!.Value);
			
			GenericEvent Event3 = DeserializeAndAssertGenericEvent(MemoryMemoryScopeHex3, MemoryMemoryScopeEvent.EventType, false, 18, 5832765);
			Assert.AreEqual(0x33, Event3.GetFields()[0].Int!.Value);
		}
		
		[TestMethod]
		public void Serialize_Memory_MemoryScope()
		{
			SerializeAndAssertGenericEvent(MemoryMemoryScopeHex1, MemoryMemoryScopeEvent.EventType, false, 18, new []
			{
				Field.FromInt(0x27),
			}, 45);
			
			SerializeAndAssertGenericEvent(MemoryMemoryScopeHex2, MemoryMemoryScopeEvent.EventType, false, 18, new []
			{
				Field.FromInt(0x27),
			}, 46);
			
			SerializeAndAssertGenericEvent(MemoryMemoryScopeHex3, MemoryMemoryScopeEvent.EventType, false, 18, new []
			{
				Field.FromInt(0x33),
			}, 5832765);
		}
		
		[TestMethod]
		public void Deserialize_Trace_NewTrace()
		{
			GenericEvent Event = DeserializeAndAssertGenericEvent(TraceNewTraceHex, TraceNewTraceEvent.EventType, true, 16);
			Field[] Fields = Event.GetFields();
			Assert.AreEqual(25582215261913, Fields[0].Long!.Value);
			Assert.AreEqual(10000000, Fields[1].Long!.Value);
			Assert.AreEqual(21069, Fields[2].Int!.Value);
			Assert.AreEqual(8, Fields[3].Int!.Value);
		}
		
		[TestMethod]
		public void Serialize_Trace_NewTrace_Generic()
		{
			SerializeAndAssertGenericEvent(TraceNewTraceHex, TraceNewTraceEvent.EventType, true, 16, new []
			{
				Field.FromLong(25582215261913),
				Field.FromLong(10000000),
				Field.FromInt(21069),
				Field.FromInt(8),
			});
		}
		
		[TestMethod]
		public void Serialize_Trace_NewTrace()
		{
			TraceNewTraceEvent Event = new TraceNewTraceEvent(25582215261913, 10000000, 21069, 8);
			SerializeAndAssertEvent(TraceNewTraceHex, Event, true, 16);
		}
		
		[TestMethod]
		public void Deserialize_Trace_ThreadInfo()
		{
			{
				GenericEvent Event = DeserializeAndAssertGenericEvent(TraceThreadInfoHex1, TraceThreadInfoEvent.EventType, true, 36);
				Field[] Fields = Event.GetFields();
				Assert.AreEqual(2, Fields[0].Int!.Value);
				Assert.AreEqual(171380, Fields[1].Int!.Value);
				Assert.AreEqual(-1, Fields[2].Int!.Value);
				Assert.AreEqual("GameThread", Fields[3].String!);
			}
			
			{
				GenericEvent Event = DeserializeAndAssertGenericEvent(TraceThreadInfoHex2, TraceThreadInfoEvent.EventType, true, 36);
				Field[] Fields = Event.GetFields();
				Assert.AreEqual(3, Fields[0].Int!.Value);
				Assert.AreEqual(0, Fields[1].Int!.Value);
				Assert.AreEqual(2147483647, Fields[2].Int!.Value);
				Assert.AreEqual("Trace", Fields[3].String!);
			}
			
			{
				GenericEvent Event = DeserializeAndAssertGenericEvent(TraceThreadInfoHex3, TraceThreadInfoEvent.EventType, true, 36);
				Field[] Fields = Event.GetFields();
				Assert.AreEqual(4, Fields[0].Int!.Value);
				Assert.AreEqual(441304, Fields[1].Int!.Value);
				Assert.AreEqual(80, Fields[2].Int!.Value);
				Assert.AreEqual("Foreground Worker #0", Fields[3].String!);
			}
		}
		
		[TestMethod]
		public void Serialize_Trace_ThreadInfo()
		{
			{
				TraceThreadInfoEvent Event = new TraceThreadInfoEvent(2, 171380, -1, "GameThread");
				SerializeAndAssertEvent(TraceThreadInfoHex1, Event, true, 36);
			}
			
			{
				TraceThreadInfoEvent Event = new TraceThreadInfoEvent(3, 0, 2147483647, "Trace");
				SerializeAndAssertEvent(TraceThreadInfoHex2, Event, true, 36);
			}
			
			{
				TraceThreadInfoEvent Event = new TraceThreadInfoEvent(4, 441304, 80, "Foreground Worker #0");
				SerializeAndAssertEvent(TraceThreadInfoHex3, Event, true, 36);
			}
		}
		
		[TestMethod]
		public void Deserialize_Diagnostics_Session2()
		{
			GenericEvent Event = DeserializeAndAssertGenericEvent(DiagnosticsSession2Hex, DiagnosticsSession2Event.EventType, true, 34);
			Field[] Fields = Event.GetFields();
			Assert.AreEqual("Win64", Fields[0].String!);
			Assert.AreEqual("UnrealEditor", Fields[1].String!);
			Assert.AreEqual(@" D:\depot\starship-main\QAGame\QAGame.uproject -trace=cpu -game", Fields[2].String!);
			Assert.AreEqual("++UE5+Main", Fields[3].String!);
			Assert.AreEqual("++UE5+Main-CL-17442524", Fields[4].String!);
			Assert.AreEqual(17442524, Fields[5].Int!.Value);
			Assert.AreEqual(3, Fields[6].Int!.Value);
			Assert.AreEqual(4, Fields[7].Int!.Value);
		}
		
		[TestMethod]
		public void Serialize_Diagnostics_Session2_Generic()
		{
			SerializeAndAssertGenericEvent(DiagnosticsSession2Hex, DiagnosticsSession2Event.EventType, true, 34, new []
			{
				Field.FromString("Win64"),
				Field.FromString("UnrealEditor"),
				Field.FromString(@" D:\depot\starship-main\QAGame\QAGame.uproject -trace=cpu -game"),
				Field.FromString("++UE5+Main"),
				Field.FromString("++UE5+Main-CL-17442524"),
				Field.FromInt(17442524),
				Field.FromInt(3),
				Field.FromInt(4)
			});
		}
		
		[TestMethod]
		public void Serialize_Diagnostics_Session2()
		{
			DiagnosticsSession2Event Event = new DiagnosticsSession2Event("Win64", "UnrealEditor", @" D:\depot\starship-main\QAGame\QAGame.uproject -trace=cpu -game", "++UE5+Main", "++UE5+Main-CL-17442524", 17442524, 3, 4);
			SerializeAndAssertEvent(DiagnosticsSession2Hex, Event, true, 34);
		}
		
		[TestMethod]
		public void CpuProfiler_EventSpec_Deserialize()
		{
			GenericEvent Event = DeserializeAndAssertGenericEvent(CpuProfilerEventSpecHex, CpuProfilerEventSpecEvent.EventType, true, 33);
			Assert.AreEqual(9524, Event.GetFields()[0].Int!.Value);
			Assert.AreEqual("K2Node_VariableGet /Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere:UserConstructionScript.K2Node_VariableGet_983", Event.GetFields()[1].String!);
		}
		
		[TestMethod]
		public void CpuProfiler_EventSpec_Serialize_Generic()
		{
			SerializeAndAssertGenericEvent(CpuProfilerEventSpecHex, CpuProfilerEventSpecEvent.EventType, true, 33, new Field[]
			{
				Field.FromInt(9524),
				Field.FromString("K2Node_VariableGet /Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere:UserConstructionScript.K2Node_VariableGet_983")
			});
		}
		
		[TestMethod]
		public void CpuProfiler_EventSpec_Serialize()
		{
			CpuProfilerEventSpecEvent Event = new CpuProfilerEventSpecEvent(9524, "K2Node_VariableGet /Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere:UserConstructionScript.K2Node_VariableGet_983");
			SerializeAndAssertEvent(CpuProfilerEventSpecHex, Event, true, 33);
		}
		
		[TestMethod]
		public void CpuProfiler_EventBatch_Deserialize()
		{
			GenericEvent Event = DeserializeAndAssertGenericEvent(CpuProfilerEventBatchHex1, CpuProfilerEventBatchEvent.EventType, false, 39);
			byte[] EventData = Event.GetFields()[0].GetArray()!;
			Assert.AreEqual(241, EventData.Length);
			
			CpuProfilerSerializer Serializer = new CpuProfilerSerializer(10000000);
			Serializer.Read(EventData);

			byte[] EventDataSerialized = Serializer.Write()[0];
			
			AssertHexString(EventData, EventDataSerialized);
		}
		
		[TestMethod]
		public void CpuProfiler_EventBatch_Serialize()
		{
			byte[] Data = HexStringToBytes("87 A7 EB AE 8A D1 0B 01 A7 02 02 04 FB 23 03 A4 0A 61 04 0B 05 F2 80 42 AA 1C EB 5C 06 C0 BC 03 17 07 02 47 08 C2 02 AB 15 09 AA 03 07 0A AE 01 05 0B D8 0D 05 0C B7 01 0D D8 03 00 03 0E A4 03 6F 0F 89 FF 03 10 F3 C4 04 11 60 06 A7 02 10 AB 19 12 B3 04 10 9F 11 13 0A 04 02 04 ED 03 10 91 11 14 80 01 02 97 02 10 A9 D6 0B 15 10 06 DD 03 10 CB 85 04 16 12 06 D3 01 10 89 16 17 0C 02 D6 02 07 18 DA 84 28 99 01 19 18 05 1A 0A A5 10 1B CD 01 1C 06 88 6A C5 49 1D B8 EB 24 A9 17 20 AF 01 10 CD F9 01 21 71 22 F2 AE 12 85 04 23 D1 0D 24 85 74 25 EA 58 91 0D 25 80 E6 02 A9 1C 25 88 CD 06 A7 27 25 D0 DA 09 8B 20 25 DE AD 0B AF 33 25 C0 A9 09 ED 23 25 C4 81 0A F5 2D 25 9A EC 0A F3 35 25 D4 CF 03 FF 19 25 BE 92 01 B1 0D 25 98 22");
			CpuProfilerEventBatchEvent Event = new CpuProfilerEventBatchEvent(Data);
			SerializeAndAssertEvent(CpuProfilerEventBatchHex1, Event, false, 39);
		}
		
		[TestMethod]
		public void CpuProfiler_EventBatch_EncodeDecodeTimestamp()
		{
			void AssertTimestamp(ulong ExpectedTimestamp, bool ExpectedIsScopeEnter)
			{
				ulong Temp = CpuProfilerSerializer.EncodeTimestamp(ExpectedTimestamp, ExpectedIsScopeEnter);
				(ulong ActualTimestamp, bool ActualIsScopeEnter) = CpuProfilerSerializer.DecodeTimestamp(Temp);
				Assert.AreEqual(ExpectedTimestamp, ActualTimestamp);
				Assert.AreEqual(ExpectedIsScopeEnter, ActualIsScopeEnter);
			}

			AssertTimestamp(0, true);
			AssertTimestamp(0, false);
			AssertTimestamp(1, true);
			AssertTimestamp(1, false);
			AssertTimestamp(14, true);
			AssertTimestamp(14, false);
			AssertTimestamp(255, true);
			AssertTimestamp(255, false);
			AssertTimestamp(256, true);
			AssertTimestamp(256, false);
			AssertTimestamp(4828284612, true);
			AssertTimestamp(4828284612, false);
		}
		
		[TestMethod]
		public void Deserialize_Logging_LogMessage()
		{
			GenericEvent Event = DeserializeAndAssertGenericEvent(LoggingLogMessageHex, LoggingLogMessage.EventType, false, 26);
			Field[] Fields = Event.GetFields();
			Assert.AreEqual(140719801695668, Fields[0].Long!.Value);
			Assert.AreEqual(25582215343012, Fields[1].Long!.Value);
			byte[] Bytes = Fields[2].GetArray()!;
			Assert.AreEqual(29, Bytes.Length);
			Assert.AreEqual(0x02, Bytes[0]);
			Assert.AreEqual(0xC2, Bytes[1]);
			Assert.AreEqual(0x44, Bytes[2]);
			Assert.AreEqual(0x7E, Bytes[25]);
		}
		
		[TestMethod]
		public void SerializeEventWithAuxAndImportant()
		{
			ushort Uid = 33;
			using MemoryStream Ms = new MemoryStream();
			using BinaryWriter Writer = new BinaryWriter(Ms);

			Field[] Fields =
			{
				Field.FromInt(9524),
				Field.FromString("K2Node_VariableGet /Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere:UserConstructionScript.K2Node_VariableGet_983"),
			};
			GenericEvent Event = new GenericEvent(98989, Fields, CpuProfilerEventSpecEvent.EventType);
			Assert.AreEqual(0x77, Event.Size);
			
			TraceImportantEventHeader EventHeader = new TraceImportantEventHeader(Uid, Event.Size);
			EventHeader.Serialize(Writer);
			Event.Serialize(20, Writer);

			AssertHexString(CpuProfilerEventSpecHex, Ms.ToArray());
		}

		[TestMethod]
		public void AuxHeaderDeserialize()
		{
			// Aux header from field "Name" in CpuProfiler.EventSpec
			using MemoryStream Ms = new MemoryStream(new byte[] {0x01, 0xC1, 0x0D, 0x00});
			using BinaryReader Reader = new BinaryReader(Ms);

			var (Uid, FieldIndex, Size) = GenericEvent.DeserializeAuxHeader(Reader.ReadUInt32());
			Assert.AreEqual(PredefinedEventUid.AuxData, Uid);
			Assert.AreEqual(1, FieldIndex);
			Assert.AreEqual(110, Size);
		}

		[TestMethod]
		public void AuxHeaderSerialize()
		{
			uint Header = GenericEvent.SerializeAuxHeader(1, 1, 110);
			using MemoryStream Ms = new MemoryStream();
			using BinaryWriter Writer = new BinaryWriter(Ms);
			Writer.Write(Header);
			byte[] Data = Ms.ToArray();
			Assert.AreEqual(0x01, Data[0]);
			Assert.AreEqual(0xC1, Data[1]);
			Assert.AreEqual(0x0D, Data[2]);
			Assert.AreEqual(0x00, Data[3]);
			
			var (Uid, FieldIndex, Size) = GenericEvent.DeserializeAuxHeader(Header);
			Assert.AreEqual(PredefinedEventUid.AuxData, Uid);
			Assert.AreEqual(1, FieldIndex);
			Assert.AreEqual(110, Size);
		}

		internal static byte[] StringToByteArray(string Hex)
		{
			int NumChars = Hex.Length;
			byte[] Bytes = new byte[NumChars / 2];
			for (int i = 0; i < NumChars; i += 2)
				Bytes[i / 2] = Convert.ToByte(Hex.Substring(i, 2), 16);
			return Bytes;
		}
		
		internal static byte[] HexStringToBytes(string HexData)
		{
			return StringToByteArray(HexData.Replace(" ", "", StringComparison.Ordinal));
		}
		
		public static string ToHexString(byte[] ba)
		{
			return BitConverter.ToString(ba).Replace("-", " ", StringComparison.Ordinal);
		}

		internal static void AssertHexString(string Expected, string Actual)
		{
			if (Expected != Actual)
			{
				Console.WriteLine("Expected:\n" + Expected);
				Console.WriteLine("Actual:\n" + Actual);
			}

			Assert.AreEqual(Expected, Actual);
		}
		
		internal static void AssertHexString(string Expected, byte[] Actual)
		{
			AssertHexString(Expected, ToHexString(Actual));
		}
		
		internal static void AssertHexString(byte[] Expected, byte[] Actual)
		{
			AssertHexString(ToHexString(Expected), ToHexString(Actual));
		}
		
		private static GenericEvent DeserializeAndAssertGenericEvent(string HexData, EventType EventType, bool IsImportant, ushort ExpectedUid, uint? ExpectedSerial = 0)
		{
			byte[] Block = HexStringToBytes(HexData);
			using MemoryStream Ms = new MemoryStream(Block);
			using BinaryReader Reader = new BinaryReader(Ms);

			ushort Uid;
			if (IsImportant)
			{
				TraceImportantEventHeader EventHeader = TraceImportantEventHeader.Deserialize(Reader);
				Assert.AreEqual(ExpectedUid, EventHeader.Uid);
				Assert.AreEqual(Block.Length - TraceImportantEventHeader.HeaderSize, EventHeader.EventSize);
				Uid = EventHeader.Uid;
			}
			else
			{
				Uid = Reader.ReadPackedUid(out _);
				Assert.AreEqual(ExpectedUid, Uid);	
			}
			
			GenericEvent Event = GenericEvent.Deserialize(Uid, Reader, EventType);
			Reader.EnsureEntireStreamIsConsumed();
			
			Assert.AreEqual(ExpectedSerial, Event.Serial);
			Assert.AreEqual(EventType.Fields.Count, Event.GetFields().Length);
			return Event;
		}

		private static void SerializeAndAssertGenericEvent(string ExpectedHexData, EventType EventType, bool IsImportant, ushort Uid, Field[] Fields, uint Serial = 0)
		{
			using MemoryStream Ms = new MemoryStream();
			using BinaryWriter Writer = new BinaryWriter(Ms);

			byte[] ExpectedData = HexStringToBytes(ExpectedHexData);
			GenericEvent Event = new GenericEvent(Serial, Fields, EventType);

			if (IsImportant)
			{
				int ExpectedEventSize = ExpectedData.Length - TraceImportantEventHeader.HeaderSize;
				Assert.AreEqual(ExpectedEventSize, Event.Size);
				TraceImportantEventHeader EventHeader = new TraceImportantEventHeader(Uid, Event.Size);
				EventHeader.Serialize(Writer);
			}
			else
			{
				Writer.WritePackedUid(Uid);
			}
			
			Event.Serialize(20, Writer);
			
			AssertHexString(ExpectedHexData, Ms.ToArray());
		}
		
		private static void SerializeAndAssertEvent(string ExpectedHexData, ITraceEvent Event, bool IsImportant, ushort Uid)
		{
			using MemoryStream Ms = new MemoryStream();
			using BinaryWriter Writer = new BinaryWriter(Ms);
			Event.Serialize(Uid, Writer);
			
			AssertHexString(ExpectedHexData, Ms.ToArray());
		}
	}
}