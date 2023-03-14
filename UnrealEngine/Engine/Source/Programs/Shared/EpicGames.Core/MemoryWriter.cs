// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Text;

namespace EpicGames.Core
{
	/// <summary>
	/// Interface for serializing to a memory buffer
	/// </summary>
	public interface IMemoryWriter
	{
		/// <summary>
		/// Gets a block of memory with at least the given size
		/// </summary>
		/// <param name="minSize">Minimum size of the returned data</param>
		/// <returns>Memory of at least the given size</returns>
		Memory<byte> GetMemory(int minSize);

		/// <summary>
		/// Updates the current position within the input buffer
		/// </summary>
		/// <param name="length">Number of bytes to advance by</param>
		void Advance(int length);
	}

	/// <summary>
	/// Writes into a fixed size memory block
	/// </summary>
	public class MemoryWriter : IMemoryWriter
	{
		/// <summary>
		/// The memory block to write to
		/// </summary>
		Memory<byte> _memory;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="memory">Memory to write to</param>
		public MemoryWriter(Memory<byte> memory)
		{
			_memory = memory;
		}

		/// <summary>
		/// Checks that we've used the exact buffer length
		/// </summary>
		public void CheckEmpty()
		{
			if (_memory.Length > 0)
			{
				throw new Exception($"Serialization is not at expected offset within the output buffer ({_memory.Length} bytes unused)");
			}
		}

		/// <inheritdoc/>
		public Memory<byte> GetMemory(int length) => _memory;

		/// <inheritdoc/>
		public void Advance(int length) => _memory = _memory.Slice(length);
	}

	/// <summary>
	/// Extension methods for <see cref="IMemoryWriter"/>
	/// </summary>
	public static class MemoryWriterExtensions
	{
		/// <summary>
		/// Writes a byte to memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="minSize">Minimum size for the returned span</param>
		public static Span<byte> GetSpan(this IMemoryWriter writer, int minSize) => writer.GetMemory(minSize).Span;

		/// <summary>
		/// Writes a boolean to memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteBoolean(this IMemoryWriter writer, bool value)
		{
			WriteUInt8(writer, value ? (byte)1 : (byte)0);
		}

		/// <summary>
		/// Writes a byte to memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteInt8(this IMemoryWriter writer, sbyte value)
		{
			WriteUInt8(writer, (byte)value);
		}

		/// <summary>
		/// Writes a byte to memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteUInt8(this IMemoryWriter writer, byte value)
		{
			writer.GetSpan(1)[0] = (byte)value;
			writer.Advance(1);
		}

		/// <summary>
		/// Writes an int16 to the memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteInt16(this IMemoryWriter writer, short value)
		{
			Span<byte> span = GetSpan(writer, sizeof(short));
			BinaryPrimitives.WriteInt16LittleEndian(span, value);
			writer.Advance(sizeof(short));
		}

		/// <summary>
		/// Writes a uint16 to the memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteUInt16(this IMemoryWriter writer, ushort value)
		{
			Span<byte> span = GetSpan(writer, sizeof(ushort));
			BinaryPrimitives.WriteUInt16LittleEndian(span, value);
			writer.Advance(sizeof(ushort));
		}

		/// <summary>
		/// Writes an int32 to the memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteInt32(this IMemoryWriter writer, int value)
		{
			Span<byte> span = GetSpan(writer, sizeof(int));
			BinaryPrimitives.WriteInt32LittleEndian(span, value);
			writer.Advance(sizeof(int));
		}

		/// <summary>
		/// Writes a uint32 to the memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteUInt32(this IMemoryWriter writer, uint value)
		{
			Span<byte> span = GetSpan(writer, sizeof(uint));
			BinaryPrimitives.WriteUInt32LittleEndian(span, value);
			writer.Advance(sizeof(uint));
		}

		/// <summary>
		/// Writes an int64 to the memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteInt64(this IMemoryWriter writer, long value)
		{
			Span<byte> span = GetSpan(writer, sizeof(long));
			BinaryPrimitives.WriteInt64LittleEndian(span, value);
			writer.Advance(sizeof(long));
		}

		/// <summary>
		/// Writes a uint64 to the memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteUInt64(this IMemoryWriter writer, ulong value)
		{
			Span<byte> span = GetSpan(writer, sizeof(ulong));
			BinaryPrimitives.WriteUInt64LittleEndian(span, value);
			writer.Advance(sizeof(ulong));
		}

		/// <summary>
		/// Writes a DateTime to the memory writer
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to write</param>
		public static void WriteDateTime(this IMemoryWriter writer, DateTime value)
		{
			ulong encoded = ((ulong)value.Ticks << 2) | (ulong)value.Kind;
			writer.WriteUnsignedVarInt(encoded);
		}

		/// <summary>
		/// Appends a sequence of bytes to the buffer
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="sequence">Sequence to append</param>
		public static void WriteSequence(this IMemoryWriter writer, ReadOnlySequence<byte> sequence)
		{
			Memory<byte> memory = writer.GetMemory((int)sequence.Length);
			sequence.CopyTo(memory.Span);
		}

		/// <summary>
		/// Writes a variable length span of bytes
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="bytes">The bytes to write</param>
		public static void WriteVariableLengthBytes(this IMemoryWriter writer, ReadOnlySpan<byte> bytes)
		{
			int lengthBytes = VarInt.MeasureUnsigned(bytes.Length);
			Span<byte> span = GetSpan(writer, lengthBytes + bytes.Length);
			VarInt.WriteUnsigned(span, bytes.Length);
			bytes.CopyTo(span[lengthBytes..]);
			writer.Advance(lengthBytes + bytes.Length);
		}

		/// <summary>
		/// Writes a variable length span of bytes
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="bytes">The bytes to write</param>
		public static void WriteVariableLengthBytesWithInt32Length(this IMemoryWriter writer, ReadOnlySpan<byte> bytes)
		{
			writer.WriteInt32(bytes.Length);
			writer.WriteFixedLengthBytes(bytes);
		}

		/// <summary>
		/// Write a fixed-length sequence of bytes to the buffer
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="bytes">The bytes to write</param>
		public static void WriteFixedLengthBytes(this IMemoryWriter writer, ReadOnlySpan<byte> bytes)
		{
			Span<byte> span = GetSpan(writer, bytes.Length);
			bytes.CopyTo(span);
			writer.Advance(bytes.Length);
		}

		/// <summary>
		/// Writes a variable length array
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="list">The array to write</param>
		/// <param name="writeItem">Delegate to write an individual item</param>
		public static void WriteVariableLengthArray<T>(this IMemoryWriter writer, IReadOnlyList<T> list, Action<T> writeItem)
		{
			writer.WriteUnsignedVarInt(list.Count);
			WriteFixedLengthArray(writer, list, writeItem);
		}

		/// <summary>
		/// Writes a variable length array
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="array">The array to write</param>
		/// <param name="writeItem">Delegate to write an individual item</param>
		public static void WriteVariableLengthArrayWithInt32Length<T>(this IMemoryWriter writer, T[] array, Action<T> writeItem)
		{
			WriteInt32(writer, array.Length);
			WriteFixedLengthArray(writer, array, writeItem);
		}

		/// <summary>
		/// Writes a fixed length array
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="list">The array to write</param>
		/// <param name="writeItem">Delegate to write an individual item</param>
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE0060:Remove unused parameter")]
		public static void WriteFixedLengthArray<T>(this IMemoryWriter writer, IReadOnlyList<T> list, Action<T> writeItem)
		{
			for (int idx = 0; idx < list.Count; idx++)
			{
				writeItem(list[idx]);
			}
		}

		/// <summary>
		/// Read a string from memory
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The string that was read</returns>
		public static string ReadString(this IMemoryReader reader) => ReadString(reader, Encoding.UTF8);

		/// <summary>
		/// Read a string from memory
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <param name="encoding">Encoding to use for the string</param>
		/// <returns>The string that was read</returns>
		public static string ReadString(this IMemoryReader reader, Encoding encoding)
		{
			int length = (int)reader.ReadUnsignedVarInt();

			ReadOnlySpan<byte> span = reader.GetSpan(length).Slice(0, length);
			string str = encoding.GetString(span);
			reader.Advance(length);

			return str;
		}

		/// <summary>
		/// Write a string to memory
		/// </summary>
		/// <param name="writer">Reader to deserialize from</param>
		/// <param name="str">The string to be written</param>
		public static void WriteString(this IMemoryWriter writer, string str) => WriteString(writer, str, Encoding.UTF8);

		/// <summary>
		/// Write a string to memory
		/// </summary>
		/// <param name="writer">Reader to deserialize from</param>
		/// <param name="str">The string to be written</param>
		/// <param name="encoding">Encoding to use for the string</param>
		public static void WriteString(this IMemoryWriter writer, string str, Encoding encoding)
		{
			int stringBytes = encoding.GetByteCount(str);
			int lengthBytes = VarInt.MeasureUnsigned(stringBytes);

			Span<byte> span = writer.GetSpan(lengthBytes + stringBytes);
			VarInt.WriteUnsigned(span, stringBytes);
			encoding.GetBytes(str, span[lengthBytes..]);

			writer.Advance(lengthBytes + stringBytes);
		}
	}
}
