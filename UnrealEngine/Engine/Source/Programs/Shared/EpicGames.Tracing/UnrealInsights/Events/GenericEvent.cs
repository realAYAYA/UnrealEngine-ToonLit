// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;

namespace EpicGames.Tracing.UnrealInsights.Events
{
	public class Field
	{
		public bool? Bool { get; }
		public int? Int { get; }
		public long? Long { get; }
		public double? Float { get; }
		public string? String { get; }
		readonly byte[]? Array;
		public byte[]? GetArray()
		{
			return Array;
		}

		private Field(bool Value) { Bool = Value; }
		private Field(int Value) { Int = Value; }
		private Field(long Value) { Long = Value; }
		private Field(float Value) { Float = Value; }
		private Field(double Value) { Float = Value; }
		private Field(string Value) { String = Value; }
		private Field(byte[] Value) { Array = Value; }
		public static Field FromBool(bool Value) => new Field(Value);
		public static Field FromInt(int Value) => new Field(Value);
		public static Field FromLong(long Value) => new Field(Value);
		public static Field FromFloat(float Value) => new Field(Value);
		public static Field FromDouble(double Value) => new Field(Value);
		public static Field FromString(string Value) => new Field(Value);
		public static Field FromArray(byte[] Value) => new Field(Value);
	}

	/// <summary>
	/// Represents a generic event in the stream with fields stored dynamically
	/// </summary>
	public class GenericEvent : ITraceEvent
	{
		public uint Serial { get; }
		readonly Field[] Fields;
		public Field[] GetFields() => Fields;
		public EventType Type => EventType;
		private EventType EventType;

		public GenericEvent(uint Serial, Field[] Fields, EventType EventType)
		{
			this.Serial = Serial;
			this.Fields = Fields;
			this.EventType = EventType;
		}

		public ushort Size
		{
			get
			{
				ushort TotalSize = 0;
				if (EventType.HasSerial) TotalSize += 3; // 24-bit serial
				TotalSize += EventType.GetEventSize();
				if (EventType.MaybeHasAux())
				{
					for (int i = 0; i < EventType.Fields.Count; i++)
					{
						EventTypeField FieldType = EventType.Fields[i];
						if (!FieldType.IsAuxData())
							continue;

						ushort AuxDataSize;
						if (FieldType.TypeInfo == EventTypeField.TypeAnsiString)
						{
							AuxDataSize = (ushort) Fields[i].String!.Length;
						}
						else if (FieldType.TypeInfo == EventTypeField.TypeWideString)
						{
							AuxDataSize = (ushort) (Fields[i].String!.Length * 2);
						}
						else
						{
							AuxDataSize = (ushort) Fields[i].GetArray()!.Length;
						}

						TotalSize += sizeof(uint); // AuxHeader
						TotalSize += AuxDataSize;
					}
					TotalSize += sizeof(byte); // AuxTerminal UID
				}
				
				return TotalSize;
			}
		}

		public void Serialize(ushort Uid, BinaryWriter Writer)
		{
			if (EventType.HasSerial)
			{
				byte SerialLow = (byte) (Serial & 0xFF);
				Writer.Write(SerialLow);
				Writer.Write(BinaryReaderExtensions.GetHighWord(Serial));
			}
			
			for (int i = 0; i < EventType.Fields.Count; i++)
			{
				EventTypeField FieldType = EventType.Fields[i];

				if (FieldType.TypeInfo == EventTypeField.TypeInt8) { Writer.Write((byte)Fields[i].Int!); }
				else if (FieldType.TypeInfo == EventTypeField.TypeInt16) { Writer.Write((ushort)Fields[i].Int!); }
				else if (FieldType.TypeInfo == EventTypeField.TypeInt32) { Writer.Write((uint)Fields[i].Int!); }
				else if (FieldType.TypeInfo == EventTypeField.TypeInt64) { Writer.Write((ulong)Fields[i].Long!); }
				else if (FieldType.TypeInfo == EventTypeField.TypePointer) { Writer.Write((ulong)Fields[i].Long!); }
				else if (FieldType.TypeInfo == EventTypeField.TypeFloat32) { Writer.Write((float)Fields[i].Float!); }
				else if (FieldType.TypeInfo == EventTypeField.TypeFloat64) { Writer.Write((double)Fields[i].Float!); }
				else if (FieldType.TypeInfo == EventTypeField.TypeAnsiString) {  } // Write later as aux-data
				else if (FieldType.TypeInfo == EventTypeField.TypeWideString) {  } // Write later as aux-data
				else if (FieldType.TypeInfo == EventTypeField.TypeArray) {  } // Write later as aux-data
				else if (FieldType.TypeInfo == EventTypeField.TypeBool) { Writer.Write(Fields[i].Bool!.Value ? (byte)1 : (byte)0); }
				else { throw new Exception($"Found unknown TypeInfo {FieldType.TypeInfo}"); }
			}
			
			if (EventType.MaybeHasAux())
			{
				for (int i = 0; i < EventType.Fields.Count; i++)
				{
					EventTypeField FieldType = EventType.Fields[i];

					if (FieldType.Size > 0)
						continue; // Skip any non-aux fields

					ushort AuxUid = PredefinedEventUid.AuxData;
					if (!EventType.IsImportant())
					{
						// Well-known UIDs are only shifted in non-important events
						AuxUid = (ushort) (AuxUid << 1);
					}

					byte[] Data;
					if (FieldType.TypeInfo == EventTypeField.TypeAnsiString)
					{
						Data = Encoding.ASCII.GetBytes(Fields[i].String!);
					}
					else if (FieldType.TypeInfo == EventTypeField.TypeWideString)
					{
						Data = Encoding.Unicode.GetBytes(Fields[i].String!);
					}
					else
					{
						Data = Fields[i].GetArray()!;
					}

					uint AuxHeader = 0;
					AuxHeader = SetBits(AuxHeader, AuxUid, 0, 8);
					AuxHeader = SetBits(AuxHeader, (uint) i, 8, 5);
					AuxHeader = SetBits(AuxHeader, (uint) Data.Length, 13, 19);
					
					Writer.Write(AuxHeader);
					Writer.Write(Data);
				}
				
				ushort AuxDataTerminalUid = PredefinedEventUid.AuxDataTerminal;
				
				if (!EventType.IsImportant())
				{
					// Well-known UIDs are only shifted in non-important events
					AuxDataTerminalUid = (ushort) (AuxDataTerminalUid << 1);
				}
				Writer.Write((byte)AuxDataTerminalUid);
			}
		}

		public static GenericEvent Deserialize(ushort Uid, BinaryReader Reader, EventType EventType)
		{
			Field[] Fields = new Field[EventType.Fields.Count];
			uint Serial = 0;
			
			if (!EventType.IsNoSync() && !EventType.IsImportant())
			{
				// Read uint24 as serial
				byte SerialLow = Reader.ReadByte();
				ushort SerialHigh = Reader.ReadUInt16();
				Serial = SerialLow | ((uint)SerialHigh << 16);
			}

			for (int i = 0; i < EventType.Fields.Count; i++)
			{
				EventTypeField FieldType = EventType.Fields[i];

				if (FieldType.TypeInfo == EventTypeField.TypeInt8) { Fields[i] = Field.FromInt(Reader.ReadByte()); }
				else if (FieldType.TypeInfo == EventTypeField.TypeInt16) { Fields[i] = Field.FromInt(Reader.ReadUInt16()); }
				else if (FieldType.TypeInfo == EventTypeField.TypeInt32) { Fields[i] = Field.FromInt((int)Reader.ReadUInt32()); }
				else if (FieldType.TypeInfo == EventTypeField.TypeInt64) { Fields[i] = Field.FromLong((long) Reader.ReadUInt64()); }
				else if (FieldType.TypeInfo == EventTypeField.TypePointer) { Fields[i] = Field.FromLong((long) Reader.ReadUInt64()); }
				else if (FieldType.TypeInfo == EventTypeField.TypeFloat32) { Fields[i] = Field.FromFloat(Reader.ReadSingle()); }
				else if (FieldType.TypeInfo == EventTypeField.TypeFloat64) { Fields[i] = Field.FromDouble(Reader.ReadDouble()); }
				else if (FieldType.TypeInfo == EventTypeField.TypeAnsiString) {  } // Read later as aux-data
				else if (FieldType.TypeInfo == EventTypeField.TypeWideString) {  } // Read later as aux-data
				else if (FieldType.TypeInfo == EventTypeField.TypeArray) {  } // Read later as aux-data
				else if (FieldType.TypeInfo == EventTypeField.TypeBool) { Fields[i] = Field.FromBool(Reader.ReadByte() == 1); }
				else { throw new Exception($"Found unknown TypeInfo {FieldType.TypeInfo}"); }
			}

			if (EventType.MaybeHasAux())
			{
				for (;;)
				{
					ushort AuxUid = Reader.ReadByte();
					if (!EventType.IsImportant())
					{
						// Well-known UIDs are only shifted in non-important events
						AuxUid = (ushort) (AuxUid >> 1);
					}

					if (AuxUid == PredefinedEventUid.AuxData)
					{
						Reader.BaseStream.Position -= 1; // Include the UID for parsing rest of header as a single uint32
						uint AuxHeader = Reader.ReadUInt32();

						int FieldIndex = (int) ReadBits(AuxHeader, 8, 5);
						uint Size = ReadBits(AuxHeader, 13, 19);

						byte[] AuxData = Reader.ReadBytesStrict((int) Size);
						
						if (EventType.Fields[FieldIndex].TypeInfo == EventTypeField.TypeAnsiString)
						{
							Fields[FieldIndex] = Field.FromString(Encoding.ASCII.GetString(AuxData));
						}
						else if (EventType.Fields[FieldIndex].TypeInfo == EventTypeField.TypeWideString)
						{
							Fields[FieldIndex] = Field.FromString(Encoding.Unicode.GetString(AuxData));
						}
						else
						{
							Fields[FieldIndex] = Field.FromArray(AuxData);
						}
					}
					else if (AuxUid == PredefinedEventUid.AuxDataTerminal)
					{
						break;
					}
					else
					{
						throw new Exception($"Invalid AuxUid found: {AuxUid} / 0x{AuxUid:X4}");
					}
				}
			}

			return new GenericEvent(Serial, Fields, EventType);
		}
		
		private static uint SetBits(uint Word, uint Value, int Pos, int Size)
		{
			uint mask = (((uint)1 << Size) - 1) << Pos;
			Word &= ~mask;
			Word |= (Value << Pos) & mask;
			return Word;
		}

		private static uint ReadBits(uint Word, int Pos, int Size)
		{
			uint Mask = (((uint)1 << Size) - 1) << Pos;
			return (Word & Mask) >> Pos;
		}

		internal static (ushort Uid, int FieldIndex, int Size) DeserializeAuxHeader(uint Header)
		{
			ushort Uid = (ushort) ReadBits(Header, 0, 8);
			int FieldIndex = (int) ReadBits(Header, 8, 5);
			int Size = (int) ReadBits(Header, 13, 19);
			return (Uid, FieldIndex, Size);
		}
		
		internal static uint SerializeAuxHeader(ushort Uid, int FieldIndex, int Size)
		{
			uint Header = 0;
			Header = SetBits(Header, Uid, 0, 8);
			Header = SetBits(Header, (uint) FieldIndex, 8, 5);
			Header = SetBits(Header, (uint) Size, 13, 19);
			return Header;
		}
	}
}