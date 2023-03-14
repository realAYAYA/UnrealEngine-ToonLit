// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;

namespace EpicGames.Serialization
{
	/// <summary>
	/// Exception for <see cref="CbWriter"/>
	/// </summary>
	public class CbWriterException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="message"></param>
		public CbWriterException(string message)
			: base(message)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="message"></param>
		/// <param name="ex"></param>
		public CbWriterException(string message, Exception? ex)
			: base(message, ex)
		{
		}
	}

	/// <summary>
	/// Forward-only writer for compact binary objects
	/// </summary>
	public class CbWriter
	{
		/// <summary>
		/// Stores information about an object or array scope within the written buffer which requires a header to be inserted containing
		/// the size or number of elements when copied to an output buffer
		/// </summary>
		class Scope
		{
			public CbFieldType _fieldType;
			public CbFieldType _uniformFieldType;
			public int _offset; // Offset to insert the length/count
			public int _length; // Excludes the size of this field's headers, and child fields' headers.
			public int _count;
			public List<Scope> _children = new List<Scope>();
			public int _sizeOfChildHeaders; // Sum of additional headers for child items, recursively.

			public Scope(CbFieldType fieldType, CbFieldType uniformFieldType, int offset)
			{
				Reset(fieldType, uniformFieldType, offset);
			}

			public void Reset(CbFieldType fieldType, CbFieldType uniformFieldType, int offset)
			{
				_fieldType = fieldType;
				_uniformFieldType = uniformFieldType;
				_offset = offset;
				_length = 0;
				_count = 0;
				_children.Clear();
				_sizeOfChildHeaders = 0;
			}
		}

		/// <summary>
		/// Chunk of written data. Chunks are allocated as needed and chained together with scope annotations to produce the output data.
		/// </summary>
		class Chunk
		{
			public int _offset;
			public int _length;
			public byte[] _data;
			public List<Scope> _scopes = new List<Scope>();

			public Chunk(int offset, int maxLength)
			{
				_data = new byte[maxLength];
				Reset(offset);
			}

			public void Reset(int offset)
			{
				_offset = offset;
				_length = 0;
				_scopes.Clear();
			}
		}

		const int DefaultChunkSize = 1024;

		readonly List<Chunk> _chunks = new List<Chunk>();
		readonly Stack<Scope> _openScopes = new Stack<Scope>();
		Chunk CurrentChunk => _chunks[^1];
		Scope CurrentScope => _openScopes.Peek();
		int _currentOffset;
		readonly List<Chunk> _freeChunks = new List<Chunk>();
		readonly List<Scope> _freeScopes = new List<Scope>();

		/// <summary>
		/// Constructor
		/// </summary>
		public CbWriter()
			: this(DefaultChunkSize)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="reserve">Amount of data to reserve for output</param>
		public CbWriter(int reserve)
		{
			_chunks.Add(new Chunk(0, reserve));
			_openScopes.Push(new Scope(CbFieldType.Array, CbFieldType.None, 0));
		}

		/// <summary>
		/// 
		/// </summary>
		public void Clear()
		{
			foreach (Chunk chunk in _chunks)
			{
				FreeChunk(chunk);
			}

			_currentOffset = 0;

			_chunks.Clear();
			_chunks.Add(AllocChunk(0, DefaultChunkSize));

			_openScopes.Clear();
			_openScopes.Push(AllocScope(CbFieldType.Array, CbFieldType.None, 0));
		}

		/// <summary>
		/// Allocate a new chunk object
		/// </summary>
		/// <param name="offset">Offset of the chunk</param>
		/// <param name="maxLength">Maximum length of the chunk</param>
		/// <returns>New chunk object</returns>
		Chunk AllocChunk(int offset, int maxLength)
		{
			for(int idx = _freeChunks.Count - 1; idx >= 0; idx--)
			{
				Chunk chunk = _freeChunks[idx];
				if (chunk._data.Length >= maxLength)
				{
					_freeChunks.RemoveAt(idx);
					chunk.Reset(offset);
					return chunk;
				}
			}
			return new Chunk(offset, maxLength);
		}

		/// <summary>
		/// Adds a chunk to the free list
		/// </summary>
		/// <param name="chunk"></param>
		void FreeChunk(Chunk chunk)
		{
			// Add the scopes to the free list
			_freeScopes.AddRange(chunk._scopes);
			chunk._scopes.Clear();

			// Insert it into the free list, sorted by descending size
			for (int idx = 0; ; idx++)
			{
				if (idx == _freeChunks.Count || chunk._data.Length >= _freeChunks[idx]._data.Length)
				{
					_freeChunks.Insert(idx, chunk);
					break;
				}
			}
		}

		/// <summary>
		/// Allocate a scope object
		/// </summary>
		/// <param name="fieldType"></param>
		/// <param name="uniformFieldType"></param>
		/// <param name="offset"></param>
		/// <returns></returns>
		Scope AllocScope(CbFieldType fieldType, CbFieldType uniformFieldType, int offset)
		{
			if (_freeScopes.Count > 0)
			{
				Scope scope = _freeScopes[^1];
				scope.Reset(fieldType, uniformFieldType, offset);
				_freeScopes.RemoveAt(_freeScopes.Count - 1);
				return scope;
			}
			return new Scope(fieldType, uniformFieldType, offset);
		}

		/// <summary>
		/// Ensure that a block of contiguous memory of the given length is available in the output buffer
		/// </summary>
		/// <param name="length"></param>
		/// <returns>The allocated memory</returns>
		Memory<byte> Allocate(int length)
		{
			Chunk lastChunk = CurrentChunk;
			if (lastChunk._length + length > lastChunk._data.Length)
			{
				int chunkSize = Math.Max(length, DefaultChunkSize);
				lastChunk = AllocChunk(_currentOffset, chunkSize);
				_chunks.Add(lastChunk);
			}

			Memory<byte> buffer = lastChunk._data.AsMemory(lastChunk._length, length);
			lastChunk._length += length;
			_currentOffset += length;
			return buffer;
		}

		/// <summary>
		/// Insert a new scope
		/// </summary>
		/// <param name="fieldType"></param>
		/// <param name="uniformFieldType"></param>
		void PushScope(CbFieldType fieldType, CbFieldType uniformFieldType)
		{
			Scope newScope = AllocScope(fieldType, uniformFieldType, _currentOffset);
			CurrentScope._children.Add(newScope);
			_openScopes.Push(newScope);

			CurrentChunk._scopes.Add(newScope);
		}

		/// <summary>
		/// Pop a scope from the current open list
		/// </summary>
		void PopScope()
		{
			Scope scope = CurrentScope;
			scope._length = _currentOffset - scope._offset;
			scope._sizeOfChildHeaders = ComputeSizeOfChildHeaders(scope);
			_openScopes.Pop();
		}

		/// <summary>
		/// Writes the header for an unnamed field
		/// </summary>
		/// <param name="type"></param>
		void WriteFieldHeader(CbFieldType type)
		{
			Scope scope = CurrentScope;
			if (!CbFieldUtils.IsArray(scope._fieldType))
			{
				throw new CbWriterException($"Anonymous fields are not allowed within fields of type {scope._fieldType}");
			}
			
			if (scope._uniformFieldType == CbFieldType.None)
			{
				Allocate(1).Span[0] = (byte)type;
			}
			else if (scope._uniformFieldType != type)
			{
				throw new CbWriterException($"Mismatched type for uniform array - expected {scope._uniformFieldType}, not {type}");
			}
			scope._count++;
		}

		/// <summary>
		/// Writes the header for a named field
		/// </summary>
		/// <param name="type"></param>
		/// <param name="name"></param>
		void WriteFieldHeader(CbFieldType type, Utf8String name)
		{
			Scope scope = CurrentScope;
			if (!CbFieldUtils.IsObject(scope._fieldType))
			{
				throw new CbWriterException($"Named fields are not allowed within fields of type {scope._fieldType}");
			}

			int nameVarIntLength = VarInt.MeasureUnsigned(name.Length);
			if (scope._uniformFieldType == CbFieldType.None)
			{
				Span<byte> buffer = Allocate(1 + nameVarIntLength + name.Length).Span;
				buffer[0] = (byte)(type | CbFieldType.HasFieldName);
				WriteBinaryPayload(buffer[1..], name.Span);
			}
			else
			{
				if (scope._uniformFieldType != type)
				{
					throw new CbWriterException($"Mismatched type for uniform object - expected {scope._uniformFieldType}, not {type}");
				}
				WriteBinaryPayload(name.Span);
			}
			scope._count++;
		}

		/// <summary>
		/// Copies an entire field value to the output
		/// </summary>
		/// <param name="field"></param>
		public void WriteFieldValue(CbField field)
		{
			WriteFieldHeader(field.GetType());
			int size = (int)field.GetPayloadSize();
			field.GetPayloadView().CopyTo(Allocate(size));
		}

		/// <summary>
		/// Copies an entire field value to the output, using the name from the field
		/// </summary>
		/// <param name="field"></param>
		public void WriteField(CbField field) => WriteField(field.GetName(), field);

		/// <summary>
		/// Copies an entire field value to the output
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="field"></param>
		public void WriteField(Utf8String name, CbField field)
		{
			WriteFieldHeader(field.GetType(), name);
			int size = (int)field.GetPayloadSize();
			field.GetPayloadView().CopyTo(Allocate(size));
		}

		/// <summary>
		/// Begin writing an object field
		/// </summary>
		public void BeginObject()
		{
			WriteFieldHeader(CbFieldType.Object);
			PushScope(CbFieldType.Object, CbFieldType.None);
		}

		/// <summary>
		/// Begin writing an object field
		/// </summary>
		/// <param name="name">Name of the field</param>
		public void BeginObject(Utf8String name)
		{
			WriteFieldHeader(CbFieldType.Object, name);
			PushScope(CbFieldType.Object, CbFieldType.None);
		}

		/// <summary>
		/// End the current object
		/// </summary>
		public void EndObject()
		{
			PopScope();
		}

		/// <summary>
		/// Begin writing an array field
		/// </summary>
		public void BeginArray()
		{
			WriteFieldHeader(CbFieldType.Array);
			PushScope(CbFieldType.Array, CbFieldType.None);
		}

		/// <summary>
		/// Begin writing a named array field
		/// </summary>
		/// <param name="name"></param>
		public void BeginArray(Utf8String name)
		{
			WriteFieldHeader(CbFieldType.Array, name);
			PushScope(CbFieldType.Array, CbFieldType.None);
		}

		/// <summary>
		/// End the current array
		/// </summary>
		public void EndArray()
		{
			PopScope();
		}

		/// <summary>
		/// Begin writing a uniform array field
		/// </summary>
		/// <param name="fieldType">The field type for elements in the array</param>
		public void BeginUniformArray(CbFieldType fieldType)
		{
			WriteFieldHeader(CbFieldType.UniformArray);
			PushScope(CbFieldType.UniformArray, fieldType);
			Allocate(1).Span[0] = (byte)fieldType;
		}

		/// <summary>
		/// Begin writing a named uniform array field
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="fieldType">The field type for elements in the array</param>
		public void BeginUniformArray(Utf8String name, CbFieldType fieldType)
		{
			WriteFieldHeader(CbFieldType.UniformArray, name);
			PushScope(CbFieldType.UniformArray, fieldType);
			Allocate(1).Span[0] = (byte)fieldType;
		}

		/// <summary>
		/// End the current array
		/// </summary>
		public void EndUniformArray()
		{
			PopScope();
		}

		/// <summary>
		/// Write a null field
		/// </summary>
		public void WriteNullValue()
		{
			WriteFieldHeader(CbFieldType.Null);
		}

		/// <summary>
		/// Write a named null field
		/// </summary>
		/// <param name="name">Name of the field</param>
		public void WriteNull(Utf8String name)
		{
			WriteFieldHeader(CbFieldType.Null, name);
		}

		/// <summary>
		/// Writes a boolean value
		/// </summary>
		/// <param name="value"></param>
		public void WriteBoolValue(bool value)
		{
			WriteFieldHeader(value? CbFieldType.BoolTrue : CbFieldType.BoolFalse);
		}

		/// <summary>
		/// Writes a boolean value
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value"></param>
		public void WriteBool(Utf8String name, bool value)
		{
			WriteFieldHeader(value ? CbFieldType.BoolTrue : CbFieldType.BoolFalse, name);
		}

		/// <summary>
		/// Writes the payload for an integer
		/// </summary>
		/// <param name="value">Value to write</param>
		void WriteIntegerPayload(ulong value)
		{
			int length = VarInt.MeasureUnsigned(value);
			Span<byte> buffer = Allocate(length).Span;
			VarInt.WriteUnsigned(buffer, value);
		}

		/// <summary>
		/// Writes an unnamed integer field
		/// </summary>
		/// <param name="value">Value to be written</param>
		public void WriteIntegerValue(int value)
		{
			WriteIntegerValue((long)value);
		}

		/// <summary>
		/// Writes an unnamed integer field
		/// </summary>
		/// <param name="value">Value to be written</param>
		public void WriteIntegerValue(long value)
		{
			if (value >= 0)
			{
				WriteFieldHeader(CbFieldType.IntegerPositive);
				WriteIntegerPayload((ulong)value);
			}
			else
			{
				WriteFieldHeader(CbFieldType.IntegerNegative);
				WriteIntegerPayload((ulong)-value);
			}
		}

		/// <summary>
		/// Writes an named integer field
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteInteger(Utf8String name, int value)
		{
			WriteInteger(name, (long)value);
		}

		/// <summary>
		/// Writes an named integer field
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteInteger(Utf8String name, long value)
		{
			if (value >= 0)
			{
				WriteFieldHeader(CbFieldType.IntegerPositive, name);
				WriteIntegerPayload((ulong)value);
			}
			else
			{
				WriteFieldHeader(CbFieldType.IntegerNegative, name);
				WriteIntegerPayload((ulong)-value);
			}
		}

		/// <summary>
		/// Writes an unnamed integer field
		/// </summary>
		/// <param name="value">Value to be written</param>
		public void WriteIntegerValue(ulong value)
		{
			WriteFieldHeader(CbFieldType.IntegerPositive);
			WriteIntegerPayload(value);
		}

		/// <summary>
		/// Writes a named integer field
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteInteger(Utf8String name, ulong value)
		{
			WriteFieldHeader(CbFieldType.IntegerPositive, name);
			WriteIntegerPayload(value);
		}

		/// <summary>
		/// Writes an unnamed double field
		/// </summary>
		/// <param name="value">Value to be written</param>
		public void WriteDoubleValue(double value)
		{
			WriteFieldHeader(CbFieldType.Float64);
			BinaryPrimitives.WriteInt64BigEndian(Allocate(sizeof(double)).Span, BitConverter.DoubleToInt64Bits(value));
		}

		/// <summary>
		/// Writes a named double field
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteDouble(Utf8String name, double value)
		{
			WriteFieldHeader(CbFieldType.Float64, name);
			BinaryPrimitives.WriteInt64BigEndian(Allocate(sizeof(double)).Span, BitConverter.DoubleToInt64Bits(value));
		}

		/// <summary>
		/// Writes the payload for a <see cref="DateTime"/> value
		/// </summary>
		/// <param name="dateTime">The value to write</param>
		void WriteDateTimePayload(DateTime dateTime)
		{
			Span<byte> buffer = Allocate(sizeof(long)).Span;
			BinaryPrimitives.WriteInt64BigEndian(buffer, dateTime.Ticks);
		}

		/// <summary>
		/// Writes an unnamed <see cref="DateTime"/> field
		/// </summary>
		/// <param name="value">Value to be written</param>
		public void WriteDateTimeValue(DateTime value)
		{
			WriteFieldHeader(CbFieldType.DateTime);
			WriteDateTimePayload(value);
		}

		/// <summary>
		/// Writes a named <see cref="DateTime"/> field
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteDateTime(Utf8String name, DateTime value)
		{
			WriteFieldHeader(CbFieldType.DateTime, name);
			WriteDateTimePayload(value);
		}

		/// <summary>
		/// Writes the payload for a hash
		/// </summary>
		/// <param name="hash"></param>
		void WriteHashPayload(IoHash hash)
		{
			Span<byte> buffer = Allocate(IoHash.NumBytes).Span;
			hash.CopyTo(buffer);
		}

		/// <summary>
		/// Writes an unnamed <see cref="IoHash"/> field
		/// </summary>
		/// <param name="hash"></param>
		public void WriteHashValue(IoHash hash)
		{
			WriteFieldHeader(CbFieldType.Hash);
			WriteHashPayload(hash);
		}

		/// <summary>
		/// Writes a named <see cref="IoHash"/> field
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteHash(Utf8String name, IoHash value)
		{
			WriteFieldHeader(CbFieldType.Hash, name);
			WriteHashPayload(value);
		}

		/// <summary>
		/// Writes an unnamed reference to a binary attachment
		/// </summary>
		/// <param name="hash">Hash of the attachment</param>
		public void WriteBinaryAttachmentValue(IoHash hash)
		{
			WriteFieldHeader(CbFieldType.BinaryAttachment);
			WriteHashPayload(hash);
		}

		/// <summary>
		/// Writes a named reference to a binary attachment
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="hash">Hash of the attachment</param>
		public void WriteBinaryAttachment(Utf8String name, IoHash hash)
		{
			WriteFieldHeader(CbFieldType.BinaryAttachment, name);
			WriteHashPayload(hash);
		}

		/// <summary>
		/// Writes the payload for an object to the buffer
		/// </summary>
		/// <param name="obj"></param>
		void WriteObjectPayload(CbObject obj)
		{
			CbField field = obj.AsField();
			Memory<byte> buffer = Allocate(field.Payload.Length);
			field.Payload.CopyTo(buffer);
		}

		/// <summary>
		/// Writes an object directly into the writer
		/// </summary>
		/// <param name="obj">Object to write</param>
		public void WriteObject(CbObject obj)
		{
			WriteFieldHeader(CbFieldType.Object);
			WriteObjectPayload(obj);
		}

		/// <summary>
		/// Writes an object directly into the writer
		/// </summary>
		/// <param name="name">Name of the object</param>
		/// <param name="obj">Object to write</param>
		public void WriteObject(Utf8String name, CbObject obj)
		{
			WriteFieldHeader(CbFieldType.Object, name);
			WriteObjectPayload(obj);
		}

		/// <summary>
		/// Writes an unnamed reference to an object attachment
		/// </summary>
		/// <param name="hash">Hash of the attachment</param>
		public void WriteObjectAttachmentValue(IoHash hash)
		{
			WriteFieldHeader(CbFieldType.ObjectAttachment);
			WriteHashPayload(hash);
		}

		/// <summary>
		/// Writes a named reference to an object attachment
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="hash">Hash of the attachment</param>
		public void WriteObjectAttachment(Utf8String name, IoHash hash)
		{
			WriteFieldHeader(CbFieldType.ObjectAttachment, name);
			WriteHashPayload(hash);
		}

		/// <summary>
		/// Writes the payload for a binary value
		/// </summary>
		/// <param name="output">Output buffer</param>
		/// <param name="value">Value to be written</param>
		static void WriteBinaryPayload(Span<byte> output, ReadOnlySpan<byte> value)
		{
			int varIntLength = VarInt.WriteUnsigned(output, value.Length);
			output = output[varIntLength..];

			value.CopyTo(output);
			CheckSize(output, value.Length);
		}

		/// <summary>
		/// Writes the payload for a binary value
		/// </summary>
		/// <param name="value">Value to be written</param>
		void WriteBinaryPayload(ReadOnlySpan<byte> value)
		{
			int valueVarIntLength = VarInt.MeasureUnsigned(value.Length);
			Span<byte> buffer = Allocate(valueVarIntLength + value.Length).Span;
			WriteBinaryPayload(buffer, value);
		}

		/// <summary>
		/// Writes an unnamed string value
		/// </summary>
		/// <param name="value">Value to be written</param>
		public void WriteStringValue(string value) => WriteUtf8StringValue(value);

		/// <summary>
		/// Writes a named string value
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteString(Utf8String name, string? value)
		{
			if(value != null)
			{
				WriteUtf8String(name, value);
			}
		}

		/// <summary>
		/// Writes an unnamed string value
		/// </summary>
		/// <param name="value">Value to be written</param>
		public void WriteUtf8StringValue(Utf8String value)
		{
			WriteFieldHeader(CbFieldType.String);
			WriteBinaryPayload(value.Span);
		}

		/// <summary>
		/// Writes a named string value
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteUtf8String(Utf8String name, Utf8String value)
		{
			if (value.Length > 0)
			{
				WriteFieldHeader(CbFieldType.String, name);
				WriteBinaryPayload(value.Span);
			}
		}

		/// <summary>
		/// Writes an unnamed binary value
		/// </summary>
		/// <param name="value">Value to be written</param>
		public void WriteBinarySpanValue(ReadOnlySpan<byte> value)
		{
			WriteFieldHeader(CbFieldType.Binary);
			WriteBinaryPayload(value);
		}

		/// <summary>
		/// Writes a named binary value
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteBinarySpan(Utf8String name, ReadOnlySpan<byte> value)
		{
			WriteFieldHeader(CbFieldType.Binary, name);
			WriteBinaryPayload(value);
		}

		/// <summary>
		/// Writes an unnamed binary value
		/// </summary>
		/// <param name="value">Value to be written</param>
		public void WriteBinaryValue(ReadOnlyMemory<byte> value)
		{
			WriteBinarySpanValue(value.Span);
		}

		/// <summary>
		/// Writes a named binary value
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteBinary(Utf8String name, ReadOnlyMemory<byte> value)
		{
			WriteBinarySpan(name, value.Span);
		}

		/// <summary>
		/// Writes an unnamed binary value
		/// </summary>
		/// <param name="value">Value to be written</param>
		public void WriteBinaryArrayValue(byte[] value)
		{
			WriteBinarySpanValue(value.AsSpan());
		}

		/// <summary>
		/// Writes a named binary value
		/// </summary>
		/// <param name="name">Name of the field</param>
		/// <param name="value">Value to be written</param>
		public void WriteBinaryArray(Utf8String name, byte[] value)
		{
			WriteBinarySpan(name, value.AsSpan());
		}

		/// <summary>
		/// Check that the given span is the required size
		/// </summary>
		/// <param name="span"></param>
		/// <param name="expectedSize"></param>
		static void CheckSize(Span<byte> span, int expectedSize)
		{
			if (span.Length != expectedSize)
			{
				throw new Exception("Size of buffer is not correct");
			}
		}

		/// <summary>
		/// Computes the hash for this object
		/// </summary>
		/// <returns>Hash for the object</returns>
		public IoHash ComputeHash()
		{
			using (Blake3.Hasher hasher = Blake3.Hasher.New())
			{
				foreach (ReadOnlyMemory<byte> segment in EnumerateSegments())
				{
					hasher.Update(segment.Span);
				}
				return IoHash.FromBlake3(hasher);
			}
		}

		/// <summary>
		/// Gets the size of the serialized data
		/// </summary>
		/// <returns></returns>
		public int GetSize()
		{
			if (_openScopes.Count != 1)
			{
				throw new CbWriterException("Unfinished scope in writer");
			}

			return _currentOffset + ComputeSizeOfChildHeaders(CurrentScope);
		}

		/// <summary>
		/// Gets the contents of this writer as a stream
		/// </summary>
		/// <returns>New stream for the contents of this object</returns>
		public Stream AsStream() => new ReadStream(EnumerateSegments().GetEnumerator(), GetSize());

		private IEnumerable<ReadOnlyMemory<byte>> EnumerateSegments()
		{
			byte[] scopeHeader = new byte[64];

			int sourceOffset = 0;
			foreach (Chunk chunk in _chunks)
			{
				foreach (Scope scope in chunk._scopes)
				{
					ReadOnlyMemory<byte> sourceData = chunk._data.AsMemory(sourceOffset - chunk._offset, scope._offset - sourceOffset);
					yield return sourceData;

					sourceOffset += sourceData.Length;

					int headerLength = WriteScopeHeader(scopeHeader, scope);
					yield return scopeHeader.AsMemory(0, headerLength);
				}

				ReadOnlyMemory<byte> lastSourceData = chunk._data.AsMemory(sourceOffset - chunk._offset, (chunk._offset + chunk._length) - sourceOffset);
				yield return lastSourceData;

				sourceOffset += lastSourceData.Length;
			}
		}

		/// <summary>
		/// Copy the data from this writer to a buffer
		/// </summary>
		/// <param name="buffer"></param>
		public void CopyTo(Span<byte> buffer)
		{
			int bufferOffset = 0;

			int sourceOffset = 0;
			foreach (Chunk chunk in _chunks)
			{
				foreach (Scope scope in chunk._scopes)
				{
					ReadOnlySpan<byte> sourceData = chunk._data.AsSpan(sourceOffset - chunk._offset, scope._offset - sourceOffset);
					sourceData.CopyTo(buffer.Slice(bufferOffset));

					bufferOffset += sourceData.Length;
					sourceOffset += sourceData.Length;

					bufferOffset += WriteScopeHeader(buffer.Slice(bufferOffset), scope);
				}

				ReadOnlySpan<byte> lastSourceData = chunk._data.AsSpan(sourceOffset - chunk._offset, (chunk._offset + chunk._length) - sourceOffset);
				lastSourceData.CopyTo(buffer.Slice(bufferOffset));
				bufferOffset += lastSourceData.Length;
				sourceOffset += lastSourceData.Length;
			}
		}

		class ReadStream : Stream
		{
			readonly IEnumerator<ReadOnlyMemory<byte>> _enumerator;
			ReadOnlyMemory<byte> _segment;
			long _positionInternal;

			public ReadStream(IEnumerator<ReadOnlyMemory<byte>> enumerator, long length)
			{
				_enumerator = enumerator;
				Length = length;
			}

			/// <inheritdoc/>
			public override bool CanRead => true;

			/// <inheritdoc/>
			public override bool CanSeek => false;

			/// <inheritdoc/>
			public override bool CanWrite => false;

			/// <inheritdoc/>
			public override long Length { get; }

			/// <inheritdoc/>
			public override long Position
			{
				get => _positionInternal;
				set => throw new NotSupportedException();
			}

			/// <inheritdoc/>
			public override void Flush() { }

			/// <inheritdoc/>
			public override int Read(Span<byte> buffer)
			{
				int readLength = 0;
				while (readLength < buffer.Length)
				{
					while (_segment.Length == 0)
					{
						if (!_enumerator.MoveNext())
						{
							return readLength;
						}
						_segment = _enumerator.Current;
					}

					int copyLength = Math.Min(_segment.Length, buffer.Length);

					_segment.Span.Slice(0, copyLength).CopyTo(buffer.Slice(readLength));
					_segment = _segment.Slice(copyLength);

					_positionInternal += copyLength;
					readLength += copyLength;
				}
				return readLength;
			}

			/// <inheritdoc/>
			public override int Read(byte[] buffer, int offset, int count) => Read(buffer.AsSpan(offset, count));

			/// <inheritdoc/>
			public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();

			/// <inheritdoc/>
			public override void SetLength(long value) => throw new NotSupportedException();

			/// <inheritdoc/>
			public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
		}

		/// <summary>
		/// Convert the data into a compact binary object
		/// </summary>
		/// <returns></returns>
		public CbObject ToObject()
		{
			return new CbObject(ToByteArray());
		}

		/// <summary>
		/// Convert the data into a flat array
		/// </summary>
		/// <returns></returns>
		public byte[] ToByteArray()
		{
			byte[] buffer = new byte[GetSize()];
			CopyTo(buffer);
			return buffer;
		}

		/// <summary>
		/// Comptues the size of any child headers
		/// </summary>
		/// <param name="scope"></param>
		static int ComputeSizeOfChildHeaders(Scope scope)
		{
			int sizeOfChildHeaders = 0;
			foreach (Scope childScope in scope._children)
			{
				switch (childScope._fieldType)
				{
					case CbFieldType.Object:
					case CbFieldType.UniformObject:
						sizeOfChildHeaders += childScope._sizeOfChildHeaders + VarInt.MeasureUnsigned(childScope._length + childScope._sizeOfChildHeaders);
						break;
					case CbFieldType.Array:
					case CbFieldType.UniformArray:
						int arrayCountLength = VarInt.MeasureUnsigned(childScope._count);
						sizeOfChildHeaders += childScope._sizeOfChildHeaders + VarInt.MeasureUnsigned(childScope._length + childScope._sizeOfChildHeaders + arrayCountLength) + arrayCountLength;
						break;
					default:
						throw new InvalidOperationException();
				}
			}
			return sizeOfChildHeaders;
		}

		/// <summary>
		/// Writes the header for a particular scope
		/// </summary>
		/// <param name="span"></param>
		/// <param name="scope"></param>
		/// <returns></returns>
		static int WriteScopeHeader(Span<byte> span, Scope scope)
		{
			switch (scope._fieldType)
			{
				case CbFieldType.Object:
				case CbFieldType.UniformObject:
					return VarInt.WriteUnsigned(span, scope._length + scope._sizeOfChildHeaders);
				case CbFieldType.Array:
				case CbFieldType.UniformArray:
					int numItemsLength = VarInt.MeasureUnsigned(scope._count);
					int offset = VarInt.WriteUnsigned(span, scope._length + scope._sizeOfChildHeaders + numItemsLength);
					return offset + VarInt.WriteUnsigned(span.Slice(offset), scope._count);
				default:
					throw new InvalidOperationException();
			}
		}
	}
}