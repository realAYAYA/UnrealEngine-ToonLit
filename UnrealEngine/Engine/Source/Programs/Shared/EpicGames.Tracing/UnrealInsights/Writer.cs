// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using EpicGames.Tracing.UnrealInsights.Events;

namespace EpicGames.Tracing.UnrealInsights
{
	public class UnrealInsightsWriter
	{
		readonly Dictionary<EventType, ushort> EventTypeToUids = new Dictionary<EventType, ushort>();
		readonly Dictionary<ushort, EventType> UidToEventTypes = new Dictionary<ushort, EventType>();
		readonly Dictionary<ushort, List<ITraceEvent>> ThreadToEvents = new Dictionary<ushort, List<ITraceEvent>>();
		
		private ushort UidCounter = PredefinedEventUid._WellKnownNum; // IDs below 16 are reserved for well-known events
		private object Lock = new object();

		public UnrealInsightsWriter()
		{
			ThreadToEvents[TransportPacket.ThreadIdEvents] = new List<ITraceEvent>();

			EnterScopeEvent EnterScopeEvent = new EnterScopeEvent();
			RegisterEventType(PredefinedEventUid.EnterScope, EnterScopeEvent.Type);
			
			LeaveScopeEvent LeaveScopeEvent = new LeaveScopeEvent();
			RegisterEventType(PredefinedEventUid.LeaveScope, LeaveScopeEvent.Type);
			
			EnterScopeEvent EnterScopeTimestampEvent = new EnterScopeEvent();
			RegisterEventType(PredefinedEventUid.EnterScope_T, EnterScopeTimestampEvent.Type);
			
			LeaveScopeEventTimestamp LeaveScopeTimestampEvent = new LeaveScopeEventTimestamp(0);
			RegisterEventType(PredefinedEventUid.LeaveScope_T, LeaveScopeTimestampEvent.Type);
		}

		public void AddEvent(ushort ThreadId, ITraceEvent Event)
		{
			if (ThreadId == TransportPacket.ThreadIdEvents)
			{
				throw new ArgumentException("Cannot add events directly to new-event thread");
			}
			
			lock (Lock)
			{
				if (!ThreadToEvents.TryGetValue(ThreadId, out List<ITraceEvent>? Events))
				{
					Events = new List<ITraceEvent>();
					ThreadToEvents[ThreadId] = Events;
				}

				if (!EventTypeToUids.ContainsKey(Event.Type))
				{
					ushort NewUid = UidCounter++;
					RegisterEventType(NewUid, Event.Type);
					ThreadToEvents[TransportPacket.ThreadIdEvents].Add(Event);
				}
				
				Events.Add(Event);
			}
		}

		private void RegisterEventType(ushort Uid, EventType EventType)
		{
			UidToEventTypes[Uid] = EventType;
			EventTypeToUids[EventType] = Uid;
		}

		public void Write(BinaryWriter Writer)
		{
			lock (Lock)
			{
				WriteHeader(Writer);
				WriteThread(TransportPacket.ThreadIdImportants, Writer);

				foreach (ushort ThreadId in ThreadToEvents.Keys)
				{
					if (ThreadId == TransportPacket.ThreadIdEvents || ThreadId == TransportPacket.ThreadIdImportants)
						continue;
					
					WriteThread(ThreadId, Writer);
				}
			}
		}

		private void WriteHeader(BinaryWriter Writer)
		{
			StreamHeader.Default().Serialize(Writer);
			TransportPacket TransportPacket = TransportPacket.Create(0, 0);
			IEnumerable<(ushort Uid, ITraceEvent Event)> NewEvents = ThreadToEvents[TransportPacket.ThreadIdEvents].Select(e =>
			{
				ushort NewEventUid = EventTypeToUids[e.Type];
				ITraceEvent MyEventType = e.Type;
				return (NewEventUid, MyEventType);
			});
			TransportPacket.Serialize(Writer, NewEvents);
		}

		private void WriteThread(ushort ThreadId, BinaryWriter Writer)
		{
			foreach (ITraceEvent Event in ThreadToEvents[ThreadId])
			{
				ushort Uid = EventTypeToUids[Event.Type];
				TransportPacket TransportPacket = TransportPacket.Create(0, ThreadId);
				TransportPacket.Serialize(Writer, new [] { (Uid, Event) });
			}
		}
	}
}