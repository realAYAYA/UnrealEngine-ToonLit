// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using EpicGames.Tracing.UnrealInsights.Events;

namespace EpicGames.Tracing.UnrealInsights
{
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Naming", "CA1707:Identifiers should not contain underscores")]
	public static class PredefinedEventUid
	{
		public const ushort NewEvent = 0;
		public const ushort AuxData = 1;
		// public const ushort _AuxData_Unused = 2;
		public const ushort AuxDataTerminal = 3;
		public const ushort EnterScope = 4;
		public const ushort LeaveScope = 5;
		// public const ushort _Unused6 = 6;
		// public const ushort _Unused7 = 7;
		public const ushort EnterScope_T = 8;
		// public const ushort _EnterScope_T_Unused0 = 9;
		// public const ushort _EnterScope_T_Unused1 = 10;
		// public const ushort _EnterScope_T_Unused2 = 11;
		public const ushort LeaveScope_T = 12;
		// public const ushort _LeaveScope_T_Unused0 = 13;
		// public const ushort _LeaveScope_T_Unused1 = 14;
		// public const ushort _LeaveScope_T_Unused2 = 15;
		public const ushort _WellKnownNum = 16;
	}
	
	public class UnrealInsightsReader
	{
		public Dictionary<ushort, EventType> EventTypes { get; } = new Dictionary<ushort, EventType>();
		public Dictionary<ushort, List<ITraceEvent>> EventsPerThread { get; } = new Dictionary<ushort, List<ITraceEvent>>();

		internal int NumTransportPacketsRead { get; private set; } = 0;

		public UnrealInsightsReader()
		{
			EventTypes[PredefinedEventUid.EnterScope_T] = new EnterScopeEventTimestamp(0).Type;
			EventTypes[PredefinedEventUid.LeaveScope_T] = new LeaveScopeEventTimestamp(0).Type;
		}

		public void Read(Stream Stream)
		{
			using BinaryReader Reader = new BinaryReader(Stream);
			StreamHeader.Deserialize(Reader);

			Dictionary<ushort, MemoryStream> ThreadStreams = Demux(Reader);
			Reader.EnsureEntireStreamIsConsumed(); // There should be no more trailing bytes once all packets are consumed.
			
			void ReadThreadStream(ushort ThreadId)
			{
				MemoryStream ThreadStream = ThreadStreams[ThreadId];
				using BinaryReader ThreadReader = new BinaryReader(ThreadStream);
				EventsPerThread[ThreadId] = ReadEvents(ThreadId, ThreadReader);
				ThreadReader.EnsureEntireStreamIsConsumed();
				ThreadStream.Dispose();
				ThreadStreams.Remove(ThreadId);
			}

			// Read the NewEvents first to ensure event types are registered
			ReadThreadStream(TransportPacket.ThreadIdEvents);
			
			// Read the important events next
			ReadThreadStream(TransportPacket.ThreadIdImportants);
			
			foreach (ushort ThreadId in ThreadStreams.Keys)
			{
				ReadThreadStream(ThreadId);
			}
		}

		/// <summary>
		/// Demultiplex the stream into one continuous buffer per thread.
		/// This avoids dealing with blocks that span multiple transport packets.
		///
		/// It's a naive and simple enough for our parsing purposes but it definitely won't read larger trace files.
		/// </summary>
		/// <param name="Reader">Reader to consume</param>
		/// <returns>A stream of data per thread</returns>
		public Dictionary<ushort, MemoryStream> Demux(BinaryReader Reader)
		{
			Dictionary<ushort, MemoryStream> ThreadStreams = new Dictionary<ushort, MemoryStream>();
			while (Reader.BaseStream.Position < Reader.BaseStream.Length)
			{
				TransportPacket Packet = TransportPacket.Deserialize(Reader);

				if (!ThreadStreams.TryGetValue(Packet.GetThreadId(), out MemoryStream? ThreadStream))
				{
					ThreadStream = new MemoryStream(5 * 1024 * 1024);
					ThreadStreams[Packet.GetThreadId()] = ThreadStream;
				}

				ThreadStream.Write(Packet.GetData());
				NumTransportPacketsRead++;
			}
			Reader.EnsureEntireStreamIsConsumed();
			
			// Reset stream positions so it can be read from the beginning once returned
			ThreadStreams.Values.ToList().ForEach(Ms => Ms.Position = 0);

			return ThreadStreams;
		}

		public List<ITraceEvent> ReadEvents(int ThreadId, BinaryReader Reader)
		{
			List<ITraceEvent> EventsRead = new List<ITraceEvent>();
			while (Reader.BaseStream.Position != Reader.BaseStream.Length)
			{
				ushort Uid;
				ushort Size = 0;
				if (ThreadId == TransportPacket.ThreadIdEvents || ThreadId == TransportPacket.ThreadIdImportants)
				{
					Uid = Reader.ReadUInt16();
					Size = Reader.ReadUInt16(); // TODO: validate and respect the size
				}
				else
				{
					Uid = Reader.ReadPackedUid(out _);
				}
				
				if (Uid >= PredefinedEventUid._WellKnownNum)
				{
					if (!EventTypes.TryGetValue(Uid, out EventType? EventType))
					{
						throw new Exception($"No event type registered for UID {Uid} / 0x{Uid:X4}");
					}
				
					GenericEvent Event = GenericEvent.Deserialize(Uid, Reader, EventType);
					EventsRead.Add(Event);
				}
				else
				{
					switch (Uid)
					{
						case PredefinedEventUid.NewEvent:
							if (ThreadId != TransportPacket.ThreadIdEvents)
								throw new Exception("NewEvents are only allowed on thread ID " + TransportPacket.ThreadIdEvents);

							(ushort NewEventUid, EventType NewEvent) = EventType.Deserialize(Reader);
							EventTypes[NewEventUid] = NewEvent;
							break;
						
						case PredefinedEventUid.EnterScope:
							break;

						case PredefinedEventUid.LeaveScope:
							break;
						
						case PredefinedEventUid.EnterScope_T:
							Reader.BaseStream.Position -= 1; // Reset so UID can be read inside event's deserialize method
							EventsRead.Add(EnterScopeEventTimestamp.Deserialize(Reader));
							break;

						case PredefinedEventUid.LeaveScope_T:
							Reader.BaseStream.Position -= 1; // Reset so UID can be read inside event's deserialize method
							EventsRead.Add(LeaveScopeEventTimestamp.Deserialize(Reader));
							break;

						default:
							throw new Exception($"Cannot handle unknown UID {Uid}/0x{Uid:X4}");
					}
				}
			}

			return EventsRead;
		}
		
		public Dictionary<ushort, List<ITraceEvent>> GetEventsPerUid()
		{
			Dictionary<ushort, List<ITraceEvent>> EventsPerUid = new Dictionary<ushort, List<ITraceEvent>>();
			
			foreach ((ushort _, List<ITraceEvent> ThreadEvents) in EventsPerThread)
			{
				foreach (ITraceEvent Event in ThreadEvents)
				{
					ushort Uid = EventTypes.First(x => Event.Type.Name == x.Value.Name).Key;
					if (!EventsPerUid.TryGetValue(Uid, out List<ITraceEvent>? Events))
					{
						Events = new List<ITraceEvent>();
						EventsPerUid[Uid] = Events;
					}
					Events.Add(Event);
				}
			}
		
			return EventsPerUid;
		}

		public void PrintEventSummary()
		{
			foreach ((ushort Uid, List<ITraceEvent> Events) in GetEventsPerUid())
			{
				Console.WriteLine($"{Uid,4} {EventTypes[Uid].Name,-45} {Events.Count}");
			}
		}
	}
}