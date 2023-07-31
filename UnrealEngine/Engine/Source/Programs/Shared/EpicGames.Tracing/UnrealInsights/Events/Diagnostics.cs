// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;

namespace EpicGames.Tracing.UnrealInsights.Events
{
	public class DiagnosticsSession2Event : ITraceEvent
	{
		public static readonly EventType EventType = new EventType(0, "Diagnostics", "Session2", EventType.FlagImportant | EventType.FlagMaybeHasAux | EventType.FlagNoSync,
			new List<EventTypeField>() {
				new EventTypeField(0, 0, EventTypeField.TypeAnsiString, "Platform"),
				new EventTypeField(0, 0, EventTypeField.TypeAnsiString, "AppName"),
				new EventTypeField(0, 0, EventTypeField.TypeWideString, "CommandLine"),
				new EventTypeField(0, 0, EventTypeField.TypeWideString, "Branch"),
				new EventTypeField(0, 0, EventTypeField.TypeWideString, "BuildVersion"),
				new EventTypeField(0, 4, EventTypeField.TypeInt32, "Changelist"),
				new EventTypeField(4, 1, EventTypeField.TypeInt8, "ConfigurationType"),
				new EventTypeField(5, 1, EventTypeField.TypeInt8, "TargetType"),
			});
		
		public ushort Size => (ushort) (GenericEvent.Size + TraceImportantEventHeader.HeaderSize);
		public EventType Type => EventType;
		private readonly GenericEvent GenericEvent;

		public DiagnosticsSession2Event(string Platform, string AppName, string CommandLine, string Branch, string BuildVersion, int ChangeList, int ConfigurationType, int TargetType)
		{
			Field[] Fields =
			{
				Field.FromString(Platform),
				Field.FromString(AppName),
				Field.FromString(CommandLine),
				Field.FromString(Branch),
				Field.FromString(BuildVersion),
				Field.FromInt(ChangeList),
				Field.FromInt(ConfigurationType),
				Field.FromInt(TargetType),
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