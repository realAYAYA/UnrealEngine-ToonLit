// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace EpicGames.Tracing.UnrealInsights.Events
{
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Performance", "CA1823:Avoid unused private fields")]
	public class EventTypeField
	{
		public ushort Offset { get; private set; }
		public ushort Size { get; private set; }
		public byte TypeInfo { get; private set; }
		public byte NameSize { get; private set; }
		public string Name { get; private set; } = "<unknown>";
		public const ushort StructSize = 2 + 2 + 1 + 1;
		private static readonly byte Field_CategoryMask = ToOctalByte("300");
		private static readonly byte Field_Integer = ToOctalByte("000");
		private static readonly byte Field_Float = ToOctalByte("100");
		private static readonly byte Field_Array = ToOctalByte("200");

		private static readonly byte Field_Pow2SizeMask = ToOctalByte("003");
		private static readonly byte Field_8 = ToOctalByte("000");
		private static readonly byte Field_16 = ToOctalByte("001");
		private static readonly byte Field_32 = ToOctalByte("002");
		private static readonly byte Field_64 = ToOctalByte("003");
		private static readonly byte Field_Ptr = ToOctalByte("003"); // Assume 64-bit

		private static readonly byte Field_SpecialMask = ToOctalByte("030");
		private static readonly byte Field_Pod = ToOctalByte("000");
		private static readonly byte Field_String = ToOctalByte("010");

		public static readonly byte TypeBool = (byte) (Field_Pod | Field_Integer | Field_8);
		public static readonly byte TypeInt8 = (byte) (Field_Pod | Field_Integer | Field_8);
		public static readonly byte TypeInt16 = (byte) (Field_Pod | Field_Integer | Field_16);
		public static readonly byte TypeInt32 = (byte) (Field_Pod | Field_Integer | Field_32);
		public static readonly byte TypeInt64 = (byte) (Field_Pod | Field_Integer | Field_64);
		public static readonly byte TypePointer = (byte) (Field_Pod | Field_Integer | Field_Ptr);
		public static readonly byte TypeFloat32 = (byte) (Field_Pod | Field_Float | Field_32);
		public static readonly byte TypeFloat64 = (byte) (Field_Pod | Field_Float | Field_64);
		public static readonly byte TypeAnsiString = (byte) (Field_String | Field_Integer | Field_Array | Field_8);
		public static readonly byte TypeWideString = (byte) (Field_String | Field_Integer | Field_Array | Field_16);
		public static readonly byte TypeArray = (byte) (Field_Array);

		public static byte ToOctalByte(string Value)
		{
			return (byte) Convert.ToInt32(Value, 8);
		}

		public static string TypeInfoToString(byte ByteInfo)
		{
			if (ByteInfo == TypeInt8) return "int8";
			if (ByteInfo == TypeInt16) return "int16";
			if (ByteInfo == TypeInt32) return "int32";
			if (ByteInfo == TypeInt64) return "int64";
			if (ByteInfo == TypePointer) return "ptr";
			if (ByteInfo == TypeFloat32) return "float32";
			if (ByteInfo == TypeFloat64) return "float64";
			if (ByteInfo == TypeAnsiString) return "ansi_str";
			if (ByteInfo == TypeWideString) return "wide_str";
			if (ByteInfo == TypeArray) return "array";
			if (ByteInfo == TypeBool) return "bool";
			throw new Exception($"Unable to convert type info {ByteInfo} to string");
		}

		public EventTypeField(ushort Offset, ushort Size, byte TypeInfo, byte NameSize)
		{
			this.Offset = Offset;
			this.Size = Size;
			this.TypeInfo = TypeInfo;
			this.NameSize = NameSize;
			ValidateSize();
		}

		public EventTypeField(ushort Offset, ushort Size, byte TypeInfo, string Name)
		{
			this.Offset = Offset;
			this.Size = Size;
			this.TypeInfo = TypeInfo;
			SetName(Name);
			ValidateSize();
		}

		public bool IsArray()
		{
			return (TypeInfo & Field_Array) != 0;
		}
		
		public bool IsAuxData()
		{
			return IsArray();
		}

		public void SetName(string Name)
		{
			this.NameSize = (byte) Encoding.UTF8.GetBytes(Name).Length;
			this.Name = Name;
		}

		public void ValidateSize()
		{
			if (TypeInfo == TypeAnsiString || TypeInfo == TypeWideString || TypeInfo == TypeArray)
			{
				if (Size != 0)
				{
					// Array data is encoded in aux-data, following the event
					throw new ArgumentException("Array-based field must have a size of 0");
				}
			}
		}

		public override string ToString()
		{
			return $"Field({Name} Offset={Offset} Size={Size} Type={TypeInfoToString(TypeInfo)} TypeInfo={TypeInfo})";
		}
	}

	public class EventType : ITraceEvent
	{
		public ushort NewEventUid { get; set; }
		public string LoggerName { get; private set; }
		public string EventName { get; private set; }
		public byte Flags { get; private set; }
		public List<EventTypeField> Fields { get; private set; }
		
		public string Name => $"{LoggerName}.{EventName}";

		private int EventSize;

		public const byte FlagNone = 0;
		public const byte FlagImportant = 1 << 0;
		public const byte FlagMaybeHasAux = 1 << 1;
		public const byte FlagNoSync = 1 << 2;

		public EventType(ushort NewEventUid, string LoggerName, string EventName, byte Flags, List<EventTypeField> Fields)
		{
			this.NewEventUid = NewEventUid;
			this.LoggerName = LoggerName;
			this.EventName = EventName;
			this.Flags = Flags;
			this.Fields = Fields;
			EventSize += this.Fields.Sum(f => f.Size);
		}

		public EventType(string LoggerName, string EventName, byte Flags)
		{
			this.NewEventUid = 0;
			this.LoggerName = LoggerName;
			this.EventName = EventName;
			this.Flags = Flags;
			this.Fields = new List<EventTypeField>();
		}

		public static EventType WellKnown(ushort Uid, string Name)
		{
			return new EventType(Uid, "WellKnown", Name, 0, new List<EventTypeField>());
		}
		
		private static EventType Self()
		{
			return new EventType(0, "EventType", "Self", 0, new List<EventTypeField>());
		}

		public void AddEventType(ushort Offset, ushort Size, byte TypeInfo, string Name)
		{
			EventTypeField Field = new EventTypeField(Offset, Size, TypeInfo, Name);
			Fields.Add(Field);
			EventSize += Field.Size;
		}
		
		public bool IsWellKnown()
		{
			return NewEventUid < PredefinedEventUid._WellKnownNum;
		}

		public bool IsImportant()
		{
			return (Flags & FlagImportant) != 0;
		}

		public bool MaybeHasAux()
		{
			return (Flags & FlagMaybeHasAux) != 0;
		}

		public bool IsNoSync()
		{
			return (Flags & FlagNoSync) != 0;
		}
		
		public bool HasSerial => !IsNoSync() && !IsImportant();

		public int NumAuxFields => Fields.FindAll(x => x.IsAuxData()).Count;

		/// <summary>
		/// Size of events as seen in the stream
		/// For size of this NewEvent event, see GetSize()
		/// </summary>
		/// <returns></returns>
		public ushort GetEventSize()
		{
			return (ushort) EventSize;
		}

		public override string ToString()
		{
			return $"EventType({Name})";
		}
		
		public string ToStringDetailed()
		{
			string FieldText = string.Join(',', Fields.Select(x => x.ToString()));
			string FlagText = "";
			if (IsImportant()) FlagText += ",Important";
			if (MaybeHasAux()) FlagText += ",MaybeHasAux";
			if (IsNoSync()) FlagText += ",NoSync";
			FlagText = FlagText.Trim(',');
			return $"EventType(Uid={NewEventUid} {LoggerName} {EventName} Flags={FlagText} FlagRaw={Flags} Fields={FieldText})";
		}

		public ushort Size
		{
			get
			{
				byte[] LoggerNameBytes = Encoding.UTF8.GetBytes(LoggerName);
				byte[] EventNameBytes = Encoding.UTF8.GetBytes(EventName);

				ushort FieldArraySize = (ushort) (Fields.Count * EventTypeField.StructSize);
				ushort NamesSize = (ushort) (LoggerNameBytes.Length + EventNameBytes.Length + Fields.Sum(x => x.NameSize));
				ushort NewEventSize = (ushort) (2 + 1 + 1 + 1 + 1 + FieldArraySize + NamesSize);
				
				NewEventSize += TraceImportantEventHeader.HeaderSize;
				return NewEventSize;
			}
		}

		public EventType Type => Self();

		public override int GetHashCode()
		{
			return HashCode.Combine(NewEventUid, LoggerName, EventName, Flags);
		}

		protected bool Equals(EventType other)
		{
			return NewEventUid == other.NewEventUid && LoggerName == other.LoggerName && EventName == other.EventName && Flags == other.Flags;
		}

		public override bool Equals(object? obj)
		{
			if (ReferenceEquals(null, obj)) return false;
			if (ReferenceEquals(this, obj)) return true;
			if (obj.GetType() != this.GetType()) return false;
			return Equals((EventType) obj);
		}

		public void Serialize(ushort Uid, BinaryWriter Writer)
		{
			new TraceImportantEventHeader(PredefinedEventUid.NewEvent, (ushort) (Size - TraceImportantEventHeader.HeaderSize)).Serialize(Writer);
			
			byte[] LoggerNameBytes = Encoding.UTF8.GetBytes(LoggerName);
			byte[] EventNameBytes = Encoding.UTF8.GetBytes(EventName);

			Writer.Write(Uid); // UID of the new event to declare
			Writer.Write((byte) Fields.Count);
			Writer.Write(Flags);
			Writer.Write((byte) LoggerNameBytes.Length);
			Writer.Write((byte) EventNameBytes.Length);

			foreach (EventTypeField Field in Fields)
			{
				Writer.Write(Field.Offset);
				Writer.Write(Field.Size);
				Writer.Write(Field.TypeInfo);
				Writer.Write(Field.NameSize);
			}

			Writer.Write(LoggerNameBytes);
			Writer.Write(EventNameBytes);

			foreach (EventTypeField Field in Fields)
			{
				Writer.Write(Encoding.UTF8.GetBytes(Field.Name));
			}
		}

		public static (ushort, EventType) Deserialize(BinaryReader Reader)
		{
			ushort NewEventUid = Reader.ReadUInt16();
			byte FieldCount = Reader.ReadByte();
			byte Flags = Reader.ReadByte();
			byte LoggerNameSize = Reader.ReadByte();
			byte EventNameSize = Reader.ReadByte();
			
			EventTypeField[] Fields = new EventTypeField[FieldCount];

			for (int i = 0; i < FieldCount; i++)
			{
				ushort Offset = Reader.ReadUInt16();
				ushort Size = Reader.ReadUInt16();
				byte TypeInfo = Reader.ReadByte();
				byte NameSize = Reader.ReadByte();
				Fields[i] = new EventTypeField(Offset, Size, TypeInfo, NameSize);
			}

			string LoggerName = Encoding.UTF8.GetString(Reader.ReadBytesStrict(LoggerNameSize));
			string EventName = Encoding.UTF8.GetString(Reader.ReadBytesStrict(EventNameSize));

			for (int i = 0; i < FieldCount; i++)
			{
				Fields[i].SetName(Encoding.UTF8.GetString(Reader.ReadBytesStrict(Fields[i].NameSize)));
			}

			return (NewEventUid, new EventType(NewEventUid, LoggerName, EventName, Flags, Fields.ToList()));
		}
	}
}