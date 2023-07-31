// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using EpicGames.Tracing.UnrealInsights;
using EpicGames.Tracing.UnrealInsights.Events;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using ITraceEvent = EpicGames.Tracing.UnrealInsights.ITraceEvent;
using UnrealInsightsReader = EpicGames.Tracing.UnrealInsights.UnrealInsightsReader;
using UnrealInsightsWriter = EpicGames.Tracing.UnrealInsights.UnrealInsightsWriter;

namespace EpicGames.Tracing.Tests.UnrealInsights
{
	public class StubTraceEvent : ITraceEvent
	{
		readonly uint Val1;
		readonly byte Val2;

		public StubTraceEvent(uint Val1, byte Val2)
		{
			this.Val1 = Val1;
			this.Val2 = Val2;
		}

		public ushort Size => sizeof(uint) + sizeof(byte);
		public EventType Type => EventType.WellKnown(15, "StubEvent");

		public void Serialize(ushort Uid, BinaryWriter Writer)
		{
			Writer.Write(Val1);
			Writer.Write(Val2);
		}
		
		public static StubTraceEvent Deserialize(BinaryReader Reader)
		{
			return new StubTraceEvent(Reader.ReadUInt32(), Reader.ReadByte());
		}
	}
	
	[TestClass]
	public class TraceEventHeaderTest
	{
		[TestMethod]
		public void SerializeDeserialize()
		{
			uint Test1 = 222;
			uint Test2 = 333;
			ulong Test3 = 444;
			ushort EventSize = 4 + 4 + 8;
			
			using MemoryStream Ms = new MemoryStream();
			using BinaryWriter Writer = new BinaryWriter(Ms);
			new TraceImportantEventHeader(1000, EventSize).Serialize(Writer);
			Writer.Write(Test1);
			Writer.Write(Test2);
			Writer.Write(Test3);
			
			Ms.Position = 0;
			using BinaryReader Reader = new BinaryReader(Ms);
			TraceImportantEventHeader EventHeader = TraceImportantEventHeader.Deserialize(Reader);
			Assert.AreEqual(1000, EventHeader.Uid);
			Assert.AreEqual(EventSize, EventHeader.EventSize);
			Assert.AreEqual(Test1, Reader.ReadUInt32());
			Assert.AreEqual(Test2, Reader.ReadUInt32());
			Assert.AreEqual(Test3, Reader.ReadUInt64());
		}
	}

	[TestClass]
	public class UnrealInsightsTest
	{
		[TestMethod]
		public void WriteUtraceFile()
		{
			using MemoryStream Ms = new MemoryStream();
			using BinaryWriter BinaryWriter = new BinaryWriter(Ms);

			UnrealInsightsWriter Writer = new UnrealInsightsWriter();
			
			TraceNewTraceEvent NewTraceEvent1 = new TraceNewTraceEvent(1001, 2001, 31, 8);
			TraceNewTraceEvent NewTraceEvent2 = new TraceNewTraceEvent(1002, 2002, 32, 8);
			TraceNewTraceEvent NewTraceEvent3 = new TraceNewTraceEvent(1003, 2003, 33, 8);
			CpuProfilerEventSpecEvent Cpu1 = new CpuProfilerEventSpecEvent(400, "MyCpuEventSpecEvent1");

			Writer.AddEvent(TransportPacket.ThreadIdImportants, NewTraceEvent1);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, NewTraceEvent2);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, NewTraceEvent3);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, Cpu1);
			Writer.Write(BinaryWriter);

			Ms.Position = 0;

			UnrealInsightsReader Reader = new UnrealInsightsReader();
			Reader.Read(Ms);
			Reader.PrintEventSummary();
			Assert.AreEqual(4, Reader.EventTypes.Count);
			
			Assert.AreEqual(0, Reader.EventsPerThread[TransportPacket.ThreadIdEvents].Count);
			Assert.AreEqual(4, Reader.EventsPerThread[TransportPacket.ThreadIdImportants].Count);
			
			{
				GenericEvent Event = (GenericEvent) Reader.EventsPerThread[1][0];
				Field[] Fields = Event.GetFields();
				Assert.AreEqual(1001, Fields[0].Long!.Value);
				Assert.AreEqual(2001, Fields[1].Long!.Value);
				Assert.AreEqual(31, Fields[2].Int!.Value);
				Assert.AreEqual(8, Fields[3].Int!.Value);
			}
			{
				GenericEvent Event = (GenericEvent) Reader.EventsPerThread[1][1];
				Field[] Fields = Event.GetFields();
				Assert.AreEqual(1002, Fields[0].Long!.Value);
				Assert.AreEqual(2002, Fields[1].Long!.Value);
				Assert.AreEqual(32, Fields[2].Int!.Value);
				Assert.AreEqual(8, Fields[3].Int!.Value);
			}
			{
				GenericEvent Event = (GenericEvent) Reader.EventsPerThread[1][2];
				Field[] Fields = Event.GetFields();
				Assert.AreEqual(1003, Fields[0].Long!.Value);
				Assert.AreEqual(2003, Fields[1].Long!.Value);
				Assert.AreEqual(33, Fields[2].Int!.Value);
				Assert.AreEqual(8, Fields[3].Int!.Value);
			}
			{
				GenericEvent Event = (GenericEvent) Reader.EventsPerThread[1][3];
				Field[] Fields = Event.GetFields();
				Assert.AreEqual(400, Fields[0].Int!.Value);
				Assert.AreEqual("MyCpuEventSpecEvent1", Fields[1].String!);
			}
		}
		
		[TestMethod]
		[Ignore]
		public void WriteUtraceExample()
		{
			string FileName = "d:\\Temp\\mytrace.utrace";
			
			FileStream Fs = File.Open(FileName, FileMode.Create);
			using BinaryWriter BinaryWriter = new BinaryWriter(Fs);
			UnrealInsightsWriter Writer = new UnrealInsightsWriter();
			
			byte[] CpuBatchData1 = GenericEventTest.HexStringToBytes("87 A7 EB AE 8A D1 0B 01 A7 02 02 04 FB 23 03 A4 0A 61 04 0B 05 F2 80 42 AA 1C EB 5C 06 C0 BC 03 17 07 02 47 08 C2 02 AB 15 09 AA 03 07 0A AE 01 05 0B D8 0D 05 0C B7 01 0D D8 03 00 03 0E A4 03 6F 0F 89 FF 03 10 F3 C4 04 11 60 06 A7 02 10 AB 19 12 B3 04 10 9F 11 13 0A 04 02 04 ED 03 10 91 11 14 80 01 02 97 02 10 A9 D6 0B 15 10 06 DD 03 10 CB 85 04 16 12 06 D3 01 10 89 16 17 0C 02 D6 02 07 18 DA 84 28 99 01 19 18 05 1A 0A A5 10 1B CD 01 1C 06 88 6A C5 49 1D B8 EB 24 A9 17 20 AF 01 10 CD F9 01 21 71 22 F2 AE 12 85 04 23 D1 0D 24 85 74 25 EA 58 91 0D 25 80 E6 02 A9 1C 25 88 CD 06 A7 27 25 D0 DA 09 8B 20 25 DE AD 0B AF 33 25 C0 A9 09 ED 23 25 C4 81 0A F5 2D 25 9A EC 0A F3 35 25 D4 CF 03 FF 19 25 BE 92 01 B1 0D 25 98 22");

			ulong CycleFrequency = 10000000;
			TraceNewTraceEvent NewTrace1 = new TraceNewTraceEvent(25582215261913, CycleFrequency, 21069, 8);
			TraceThreadInfoEvent ThreadInfo1 = new TraceThreadInfoEvent(2, 171380, -1, "GameThread");
			TraceThreadInfoEvent ThreadInfo2 = new TraceThreadInfoEvent(3, 0, 2147483647, "Trace");
			DiagnosticsSession2Event DiagnosticsSession = new DiagnosticsSession2Event("Finally it works", "HELLO WORLD", "QAGame.uproject -trace=cpu -game", "++UE5+Main", "++UE5+Main-CL-17442524", 1000, 3, 4);
			CpuProfilerEventSpecEvent CpuEventSpec1 = new CpuProfilerEventSpecEvent(1, "MyCpuEventSpecEvent1");
			CpuProfilerEventSpecEvent CpuEventSpec2 = new CpuProfilerEventSpecEvent(2, "MyCpuEventSpecEvent2");
			CpuProfilerEventSpecEvent CpuEventSpec3 = new CpuProfilerEventSpecEvent(3, "MyCpuEventSpecEvent3");
			CpuProfilerEventSpecEvent CpuEventSpec4 = new CpuProfilerEventSpecEvent(4, "MyCpuEventSpecEvent4");
			CpuProfilerEventSpecEvent CpuEventSpec5 = new CpuProfilerEventSpecEvent(5, "MyCpuEventSpecEvent5");
			CpuProfilerEventSpecEvent CpuEventSpec6 = new CpuProfilerEventSpecEvent(6, "MyCpuEventSpecEvent6");
			CpuProfilerEventSpecEvent CpuEventSpec7 = new CpuProfilerEventSpecEvent(7, "MyCpuEventSpecEvent7");
			CpuProfilerEventSpecEvent CpuEventSpec8 = new CpuProfilerEventSpecEvent(8, "MyCpuEventSpecEvent8");
			CpuProfilerEventSpecEvent CpuEventSpecTesting = new CpuProfilerEventSpecEvent(9, "TestingSome");
			CpuProfilerEventBatchEvent CpuBatch1 = new CpuProfilerEventBatchEvent(CpuBatchData1);

			Writer.AddEvent(TransportPacket.ThreadIdImportants, NewTrace1);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, ThreadInfo1);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, ThreadInfo2);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, DiagnosticsSession);
			
			Writer.AddEvent(TransportPacket.ThreadIdImportants, CpuEventSpec1);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, CpuEventSpec2);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, CpuEventSpec3);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, CpuEventSpec4);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, CpuEventSpec5);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, CpuEventSpec6);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, CpuEventSpec7);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, CpuEventSpec8);
			Writer.AddEvent(TransportPacket.ThreadIdImportants, CpuEventSpecTesting);
			
			Writer.AddEvent(2, CpuBatch1);
			
			CpuProfilerSerializer CpuSerializerThread3 = new CpuProfilerSerializer(CycleFrequency);
			CpuSerializerThread3.ScopeEvents.Add(new CpuProfilerScopeEvent(25582215261913 + 400 * 1000, true, CpuEventSpecTesting.Id));
			CpuSerializerThread3.ScopeEvents.Add(new CpuProfilerScopeEvent(25582215261913 + (CycleFrequency * CycleFrequency), false, null));
			
			List<byte[]> EventBatchData = CpuSerializerThread3.Write();
			foreach (byte[] Data in EventBatchData)
			{
				Writer.AddEvent(3, new CpuProfilerEventBatchEvent(Data));
			}
			Writer.Write(BinaryWriter);

			Fs.Close();

			UnrealInsightsReader Reader = new UnrealInsightsReader();
			using FileStream Stream = File.Open(FileName, FileMode.Open);
			Reader.Read(Stream);
			Reader.PrintEventSummary();

			Assert.AreEqual(0, Reader.EventsPerThread[TransportPacket.ThreadIdEvents].Count);
		}
		
		[TestMethod]
		public void ReadUtraceFile()
		{
			string ExampleDecompTrace = "UnrealInsights/example_trace.decomp.utrace";

			UnrealInsightsReader Reader = new UnrealInsightsReader();
			using FileStream Stream = File.Open(ExampleDecompTrace, FileMode.Open);
			Reader.Read(Stream);
			Reader.PrintEventSummary();
			
			Assert.AreEqual(6281, Reader.NumTransportPacketsRead);
			Dictionary<ushort,List<ITraceEvent>> EventsPerUid = Reader.GetEventsPerUid();

			void AssertEvent(ushort Uid, string ExpectedName, int ExpectedCount)
			{
				Assert.IsTrue(Reader.EventTypes.ContainsKey(Uid));
				Assert.IsTrue(EventsPerUid.ContainsKey(Uid));
				Assert.AreEqual(ExpectedName, Reader.EventTypes[Uid].Name);
				Assert.AreEqual(ExpectedCount, EventsPerUid[Uid].Count);
			}

			AssertEvent(16, "$Trace.NewTrace", 1);
			AssertEvent(17, "$Trace.ThreadTiming", 71);
			AssertEvent(18, "Memory.MemoryScope", 16614);
			AssertEvent(19, "Logging.LogCategory", 741);
			AssertEvent(20, "Memory.MemoryScopeRealloc", 6262);
			AssertEvent(21, "Stats.Spec", 972);
			AssertEvent(22, "CsvProfiler.RegisterCategory", 51);
			AssertEvent(23, "CsvProfiler.DefineDeclaredStat", 159);
			AssertEvent(24, "Counters.Spec", 85);
			AssertEvent(25, "Logging.LogMessageSpec", 6);
			AssertEvent(26, "Logging.LogMessage", 11);
			AssertEvent(27, "CsvProfiler.DefineInlineStat", 1);
			AssertEvent(28, "LoadTime.ClassInfo", 1);
			AssertEvent(29, "SlateTrace.AddWidget", 1);
			AssertEvent(30, "SlateTrace.WidgetInfo", 1);
			AssertEvent(31, "Misc.BookmarkSpec", 2);
			AssertEvent(32, "Misc.Bookmark", 2);
			AssertEvent(33, "CpuProfiler.EventSpec", 13003);
			AssertEvent(34, "Diagnostics.Session2", 1);
			AssertEvent(35, "Trace.ChannelToggle", 32);
			AssertEvent(36, "$Trace.ThreadInfo", 100);
			AssertEvent(37, "Trace.ChannelAnnounce", 34);
			AssertEvent(38, "$Trace.ThreadGroupBegin", 12);
			AssertEvent(39, "CpuProfiler.EventBatch", 64354);
			AssertEvent(40, "$Trace.ThreadGroupEnd", 12);
			AssertEvent(41, "Cpu.CacheResourceShadersForRendering", 109);
			AssertEvent(42, "SourceFilters.FilterClass", 2);
			AssertEvent(43, "WorldSourceFilters.WorldInstance", 1);
			AssertEvent(44, "WorldSourceFilters.WorldOperation", 2);
			AssertEvent(45, "CpuProfiler.EndThread", 66);
		}
	}
}