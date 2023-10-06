// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;

namespace EpicGames.Tracing.UnrealInsights.Events
{
	public class MemoryMemoryScopeEvent : ITraceEvent
	{
		public static readonly EventType EventType = new EventType(0, "Memory", "MemoryScope", EventType.FlagNone,
			new List<EventTypeField>() { new EventTypeField(0, 4, EventTypeField.TypeInt32, "Id") });

		public ushort Size => throw new NotImplementedException();
		public EventType Type => EventType;
		
		public void Serialize(ushort Uid, BinaryWriter Writer)
		{
			throw new NotImplementedException();
		}
	}
}