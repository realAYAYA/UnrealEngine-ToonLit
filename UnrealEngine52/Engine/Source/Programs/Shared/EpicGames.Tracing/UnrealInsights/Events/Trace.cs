// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;

namespace EpicGames.Tracing.UnrealInsights.Events
{
	public class TraceNewTraceEvent : ITraceEvent
	{
		public static readonly EventType EventType = new EventType(0, "$Trace", "NewTrace", EventType.FlagImportant | EventType.FlagNoSync,
			new List<EventTypeField>() {
				new EventTypeField(0, 8, EventTypeField.TypeInt64, "StartCycle"),
				new EventTypeField(8, 8, EventTypeField.TypeInt64, "CycleFrequency"),
				new EventTypeField(16, 2, EventTypeField.TypeInt16, "Endian"),
				new EventTypeField(18, 1, EventTypeField.TypeInt8, "PointerSize")
			});
		
		public ulong StartCycle { get; private set; }
		public ulong CycleFrequency { get; private set; }
		public ushort Endian { get; private set; }
		public byte PointerSize { get; private set; }

		public ushort Size => (ushort) (EventType.GetEventSize() + TraceImportantEventHeader.HeaderSize);
		public EventType Type => EventType;
		
		public TraceNewTraceEvent(ulong StartCycle, ulong CycleFrequency, ushort Endian, byte PointerSize)
		{
			this.StartCycle = StartCycle;
			this.CycleFrequency = CycleFrequency;
			this.Endian = Endian;
			this.PointerSize = PointerSize;
		}
		
		public void Serialize(ushort Uid, BinaryWriter Writer)
		{
			new TraceImportantEventHeader(Uid, EventType.GetEventSize()).Serialize(Writer);
			Writer.Write(StartCycle);
			Writer.Write(CycleFrequency);
			Writer.Write(Endian);
			Writer.Write(PointerSize);
		}
		
		public static TraceNewTraceEvent Deserialize(BinaryReader Reader)
		{
			ulong StartCycle = Reader.ReadUInt64();
			ulong CycleFrequency = Reader.ReadUInt64();
			ushort Endian = Reader.ReadUInt16();
			byte PointerSize = Reader.ReadByte();
			return new TraceNewTraceEvent(StartCycle, CycleFrequency, Endian, PointerSize);
		}
	}

	public class TraceThreadInfoEvent : ITraceEvent
	{
		public static readonly EventType EventType = new EventType(0, "$Trace", "ThreadInfo", EventType.FlagImportant | EventType.FlagMaybeHasAux | EventType.FlagNoSync,
			new List<EventTypeField>() {
				new EventTypeField(0, 4, EventTypeField.TypeInt32, "ThreadId"),
				new EventTypeField(4, 4, EventTypeField.TypeInt32, "SystemId"),
				new EventTypeField(8, 4, EventTypeField.TypeInt32, "SortHint"),
				new EventTypeField(12, 0, EventTypeField.TypeAnsiString, "Name")
			});
			

		public ushort Size => (ushort) (GenericEvent.Size + TraceImportantEventHeader.HeaderSize);
		public EventType Type => EventType;
		private readonly GenericEvent GenericEvent;
		
		public TraceThreadInfoEvent(int ThreadId, int SystemId, int SortHint, string Name)
		{
			Field[] Fields =
			{
				Field.FromInt((int) ThreadId),
				Field.FromInt((int) SystemId),
				Field.FromInt((int) SortHint),
				Field.FromString(Name),
			};

			GenericEvent = new GenericEvent(0, Fields, EventType);
		}

		public void Serialize(ushort Uid, BinaryWriter Writer)
		{
			new TraceImportantEventHeader(Uid, GenericEvent.Size).Serialize(Writer);
			GenericEvent.Serialize(Uid, Writer);
		}
	}
}