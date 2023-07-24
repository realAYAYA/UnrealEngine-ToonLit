// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;

namespace EpicGames.Tracing.UnrealInsights.Events
{
	public class LoggingLogMessage : ITraceEvent
	{
		public static readonly EventType EventType = new EventType(0, "Logging", "LogMessage", EventType.FlagMaybeHasAux | EventType.FlagNoSync,
			new List<EventTypeField>() {
				new EventTypeField(0, 8, EventTypeField.TypeInt64, "LogPoint"),
				new EventTypeField(8, 8, EventTypeField.TypeInt64, "Cycle"),
				new EventTypeField(16, 0, EventTypeField.TypeArray, "FormatArgs")
			});

		public ushort Size => throw new NotImplementedException();
		public EventType Type => EventType;

		public void Serialize(ushort Uid, BinaryWriter Writer)
		{
			throw new NotImplementedException();
		}
	}
}