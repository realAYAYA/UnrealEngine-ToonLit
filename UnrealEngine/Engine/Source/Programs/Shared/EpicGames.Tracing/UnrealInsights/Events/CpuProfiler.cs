// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace EpicGames.Tracing.UnrealInsights.Events
{
	public class CpuProfilerEventSpecEvent : ITraceEvent
	{
		public static readonly EventType EventType = new EventType(0, "CpuProfiler", "EventSpec", EventType.FlagImportant | EventType.FlagMaybeHasAux | EventType.FlagNoSync,
			new List<EventTypeField>() {
				new EventTypeField(0, 4, EventTypeField.TypeInt32, "Id"),
				new EventTypeField(4, 0, EventTypeField.TypeAnsiString, "Name")
			});
		
		public ushort Size => (ushort) (GenericEvent.Size + TraceImportantEventHeader.HeaderSize);
		public EventType Type => EventType;

		public uint Id { get; }
		readonly string Name;

		private readonly GenericEvent GenericEvent;

		public CpuProfilerEventSpecEvent(uint Id, string Name)
		{
			this.Id = Id;
			this.Name = Name;
			
			Field[] Fields =
			{
				Field.FromInt((int) Id),
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
	
	public class CpuProfilerEventBatchEvent : ITraceEvent
	{
		public static readonly EventType EventType = new EventType(0, "CpuProfiler", "EventBatch", EventType.FlagMaybeHasAux | EventType.FlagNoSync,
			new List<EventTypeField>() { new EventTypeField(0, 0, EventTypeField.TypeArray, "Data") });
		
		public ushort Size => (ushort) (GenericEvent.Size + sizeof(ushort));
		public EventType Type => EventType;

		readonly GenericEvent GenericEvent;

		public CpuProfilerEventBatchEvent(byte[] Data)
		{
			Field[] Fields = {Field.FromArray(Data)};
			GenericEvent = new GenericEvent(0, Fields, EventType);
		}
		
		public void Serialize(ushort Uid, BinaryWriter Writer)
		{
			Writer.WritePackedUid(Uid);
			GenericEvent.Serialize(Uid, Writer);
		}
	}
	
	public class CpuProfilerScopeEvent
	{
		public ulong Timestamp { get; }
		public bool IsEnterScope { get; }
		public uint? SpecId { get; }

		public CpuProfilerScopeEvent(ulong Timestamp, bool IsEnterScope, uint? SpecId)
		{
			this.Timestamp = Timestamp;
			this.IsEnterScope = IsEnterScope;
			this.SpecId = SpecId;
		}

		public override string ToString()
		{
			string Name = IsEnterScope ? "EnterScope" : "ExitScope";
			return $"{nameof(CpuProfilerScopeEvent)}({Name} {nameof(Timestamp)}={Timestamp} {nameof(SpecId)}={SpecId})";
		}
	}
	
	/// <summary>
	/// Stateful serializer that can serialize/deserialize the data argument in CpuProfiler.EventBatch events
	/// </summary>
	public class CpuProfilerSerializer
	{
		private readonly ulong CycleFrequency;
		public List<CpuProfilerScopeEvent> ScopeEvents { get; } = new List<CpuProfilerScopeEvent>();

		public CpuProfilerSerializer(ulong CycleFrequency)
		{
			this.CycleFrequency = CycleFrequency;
		}
		
		public void Read(byte[] EventBatchData)
		{
			using MemoryStream Ms = new MemoryStream(EventBatchData);
			using BinaryReader Reader = new BinaryReader(Ms);
			
			ulong AbsTimestamp = 0;
			ulong LastTimestamp = 0;

			while (Reader.BaseStream.Position != Reader.BaseStream.Length)
			{
				ulong Value = TraceUtils.Read7BitUint(Reader);
				(ulong Timestamp, bool IsEnterScope) = DecodeTimestamp(Value);

				if (AbsTimestamp == 0)
				{
					AbsTimestamp = Timestamp; // First timestamp is always absolute
				}
				else
				{
					Timestamp = LastTimestamp + Timestamp * CycleFrequency; // Make timestamp absolute
				}
				LastTimestamp = Timestamp;

				if (IsEnterScope)
				{
					ulong SpecId = TraceUtils.Read7BitUint(Reader);
					CpuProfilerScopeEvent ScopeEvent = new CpuProfilerScopeEvent(Timestamp, true, (uint?) SpecId);
					ScopeEvents.Add(ScopeEvent);
				}
				else
				{
					CpuProfilerScopeEvent ScopeEvent = new CpuProfilerScopeEvent(Timestamp, false, null);
					ScopeEvents.Add(ScopeEvent);
				}
			}
		}
		
		public List<byte[]> Write()
		{
			List<MemoryStream> Streams = new List<MemoryStream>();
			MemoryStream Stream = new MemoryStream();
			BinaryWriter Writer = new BinaryWriter(Stream);
			Streams.Add(Stream);
			
			ulong LastAbsTimestamp = 0;
			foreach (CpuProfilerScopeEvent Scope in ScopeEvents)
			{
				ulong Timestamp;
				if (LastAbsTimestamp == 0)
				{
					Timestamp = Scope.Timestamp;
				}
				else
				{
					Timestamp = (Scope.Timestamp - LastAbsTimestamp) / CycleFrequency;
				}
				LastAbsTimestamp = Scope.Timestamp;
				
				TraceUtils.Write7BitUint(Writer, EncodeTimestamp(Timestamp, Scope.IsEnterScope));
				if (Scope.IsEnterScope)
				{
					TraceUtils.Write7BitUint(Writer, (ulong) Scope.SpecId!);
				}

				// Split buffers after 250 bytes
				if (Stream.Position > 250)
				{
					Writer.Close();
					Stream = new MemoryStream();
					Writer = new BinaryWriter(Stream);
					LastAbsTimestamp = 0;
				}
			}

			Writer.Close();

			return Streams.Select(x => x.ToArray()).ToList();
		}
		
		internal static ulong EncodeTimestamp(ulong Timestamp, bool IsScopeEnter)
		{
			ulong Value = (Timestamp << 1) | (uint) (IsScopeEnter ? 1 : 0);
			return Value;
		}
		
		internal static (ulong Timestamp, bool IsScopeEnter) DecodeTimestamp(ulong Value)
		{
			bool IsScopeEnter = (Value & 1) != 0;
			ulong Timestamp = Value >> 1; // Strip the IsScopeEnter bit
			return (Timestamp, IsScopeEnter);
		}
	}
}