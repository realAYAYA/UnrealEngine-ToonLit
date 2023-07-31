// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Binary;

namespace EpicGames.Core
{
	/// <summary>
	/// Interface for reading from data in memory
	/// </summary>
	public interface IMemoryReader
	{
		/// <summary>
		/// Gets a block of memory with at least the given size
		/// </summary>
		/// <param name="minSize">Minimum size of the returned data</param>
		/// <returns>Memory of at least the given size</returns>
		ReadOnlyMemory<byte> GetMemory(int minSize);

		/// <summary>
		/// Updates the current position within the input buffer
		/// </summary>
		/// <param name="length">Number of bytes to advance by</param>
		void Advance(int length);
	}

	/// <summary>
	/// Reads data from a memory buffer
	/// </summary>
	public class MemoryReader : IMemoryReader
	{
		/// <summary>
		/// The memory to read from
		/// </summary>
		public ReadOnlyMemory<byte> Memory
		{
			get; private set;
		}

		/// <summary>
		/// Returns the memory at the current offset
		/// </summary>
		public ReadOnlySpan<byte> Span => Memory.Span;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="memory">The memory to read from</param>
		public MemoryReader(ReadOnlyMemory<byte> memory)
		{
			Memory = memory;
		}

		/// <summary>
		/// Checks that we've used the exact buffer length
		/// </summary>
		public void CheckEmpty()
		{
			if (Memory.Length > 0)
			{
				throw new Exception($"Serialization is not at expected offset within the input buffer ({Memory.Length} bytes unused)");
			}
		}

		/// <inheritdoc/>
		public ReadOnlyMemory<byte> GetMemory(int minSize) => Memory;

		/// <inheritdoc/>
		public void Advance(int length) => Memory = Memory.Slice(length);
	}

	/// <summary>
	/// Extension methods for <see cref="IMemoryReader"/>
	/// </summary>
	public static class MemoryReaderExtensions
	{
		/// <summary>
		/// Gets a span of data with the given length
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <param name="minSize">Minimum size of the returned span</param>
		/// <returns>Span of at least the given size</returns>
		public static ReadOnlySpan<byte> GetSpan(this IMemoryReader reader, int minSize) => reader.GetMemory(minSize).Span;

		/// <summary>
		/// Read a boolean from the buffer
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The value read from the buffer</returns>
		public static bool ReadBoolean(this IMemoryReader reader)
		{
			return ReadUInt8(reader) != 0;
		}

		/// <summary>
		/// Reads a byte from the buffer
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The value read from the buffer</returns>
		public static sbyte ReadInt8(this IMemoryReader reader)
		{
			sbyte value = (sbyte)reader.GetSpan(1)[0];
			reader.Advance(1);
			return value;
		}

		/// <summary>
		/// Reads a byte from the buffer
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The value read from the buffer</returns>
		public static byte ReadUInt8(this IMemoryReader reader)
		{
			byte value = reader.GetSpan(1)[0];
			reader.Advance(1);
			return value;
		}

		/// <summary>
		/// Reads an int16
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The value read from the buffer</returns>
		public static short ReadInt16(this IMemoryReader reader)
		{
			short value = BinaryPrimitives.ReadInt16LittleEndian(reader.GetSpan(sizeof(short)));
			reader.Advance(sizeof(short));
			return value;
		}

		/// <summary>
		/// Reads a uint16
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The value read from the buffer</returns>
		public static ushort ReadUInt16(this IMemoryReader reader)
		{
			ushort value = BinaryPrimitives.ReadUInt16LittleEndian(reader.GetSpan(sizeof(ushort)));
			reader.Advance(sizeof(ushort));
			return value;
		}

		/// <summary>
		/// Reads an int32
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The value that was read from the buffer</returns>
		public static int ReadInt32(this IMemoryReader reader)
		{
			int value = BinaryPrimitives.ReadInt32LittleEndian(reader.GetSpan(sizeof(int)));
			reader.Advance(sizeof(int));
			return value;
		}

		/// <summary>
		/// Reads a uint32
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The value that was read from the buffer</returns>
		public static uint ReadUInt32(this IMemoryReader reader)
		{
			uint value = BinaryPrimitives.ReadUInt32LittleEndian(reader.GetSpan(sizeof(uint)));
			reader.Advance(sizeof(uint));
			return value;
		}

		/// <summary>
		/// Reads an int64
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The value that was read from the buffer</returns>
		public static long ReadInt64(this IMemoryReader reader)
		{
			long value = BinaryPrimitives.ReadInt64LittleEndian(reader.GetSpan(sizeof(long)));
			reader.Advance(sizeof(long));
			return value;
		}

		/// <summary>
		/// Reads an int64
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The value that was read from the buffer</returns>
		public static ulong ReadUInt64(this IMemoryReader reader)
		{
			ulong value = BinaryPrimitives.ReadUInt64LittleEndian(reader.GetSpan(sizeof(ulong)));
			reader.Advance(sizeof(ulong));
			return value;
		}

		/// <summary>
		/// Reads a DateTime from the memory buffer
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		public static DateTime ReadDateTime(this IMemoryReader reader)
		{
			ulong encoded = reader.ReadUnsignedVarInt();
			return new DateTime((long)encoded >> 2, (DateTimeKind)(encoded & 3UL));
		}

		/// <summary>
		/// Reads a sequence of bytes from the buffer
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>Sequence of bytes</returns>
		public static ReadOnlyMemory<byte> ReadVariableLengthBytes(this IMemoryReader reader)
		{
			int length = (int)reader.ReadUnsignedVarInt();
			return ReadFixedLengthBytes(reader, length);
		}

		/// <summary>
		/// Writes a variable length span of bytes
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		public static ReadOnlyMemory<byte> ReadVariableLengthBytesWithInt32Length(this IMemoryReader reader)
		{
			int length = ReadInt32(reader);
			return ReadFixedLengthBytes(reader, length);
		}

		/// <summary>
		/// Reads a sequence of bytes from the buffer
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <param name="length">Number of bytes to read</param>
		/// <returns>Sequence of bytes</returns>
		public static ReadOnlyMemory<byte> ReadFixedLengthBytes(this IMemoryReader reader, int length)
		{
			ReadOnlyMemory<byte> bytes = reader.GetMemory(length).Slice(0, length);
			reader.Advance(length);
			return bytes;
		}

		/// <summary>
		/// Reads a variable length array
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <param name="readItem">Delegate to write an individual item</param>
		public static T[] ReadVariableLengthArray<T>(this IMemoryReader reader, Func<T> readItem)
		{
			int length = (int)reader.ReadUnsignedVarInt();
			return ReadFixedLengthArray(reader, length, readItem);
		}

		/// <summary>
		/// Reads a variable length array
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <param name="readItem">Delegate to write an individual item</param>
		public static T[] ReadVariableLengthArrayWithInt32Length<T>(this IMemoryReader reader, Func<T> readItem)
		{
			int length = ReadInt32(reader);
			return ReadFixedLengthArray(reader, length, readItem);
		}

		/// <summary>
		/// Writes a fixed length array
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <param name="length">Length of the array to read</param>
		/// <param name="readItem">Delegate to read an individual item</param>
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE0060:Remove unused parameter")]
		public static T[] ReadFixedLengthArray<T>(this IMemoryReader reader, int length, Func<T> readItem)
		{
			T[] array = new T[length];
			for (int idx = 0; idx < length; idx++)
			{
				array[idx] = readItem();
			}
			return array;
		}
	}
}
