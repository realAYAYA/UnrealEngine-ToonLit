// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Logs.Data
{
	/// <summary>
	/// Pending data for a sub-chunk
	/// </summary>
	public class LogSubChunkData
	{
		/// <summary>
		/// Type of data stored in this subchunk
		/// </summary>
		public LogType Type { get; }

		/// <summary>
		/// Offset within the file of this sub-chunk
		/// </summary>
		public long Offset { get; }

		/// <summary>
		/// Length of this sub-chunk
		/// </summary>
		public int Length { get; }

		/// <summary>
		/// Index of the first line in this sub-chunk
		/// </summary>
		public int LineIndex { get; }

		/// <summary>
		/// Gets the number of lines in this sub-chunk
		/// </summary>
		public int LineCount { get; }

		/// <summary>
		/// Text data
		/// </summary>
		ILogText? _textInternal;

		/// <summary>
		/// Compressed text data
		/// </summary>
		ReadOnlyMemory<byte> _compressedTextInternal;

		/// <summary>
		/// The index for this sub-chunk
		/// </summary>
		LogIndexData? _indexInternal;

		/// <summary>
		/// The log text
		/// </summary>
		public ILogText InflateText()
		{
			_textInternal ??= new ReadOnlyLogText(_compressedTextInternal.DecompressBzip2());
			return _textInternal;
		}

		/// <summary>
		/// The compressed log text
		/// </summary>
		public ReadOnlyMemory<byte> DeflateText()
		{
			if (_compressedTextInternal.IsEmpty)
			{
				_compressedTextInternal = _textInternal!.Data.CompressBzip2();
			}
			return _compressedTextInternal;
		}

		/// <summary>
		/// Index for tokens in this chunk
		/// </summary>
		public LogIndexData BuildIndex(ILogger logger)
		{
			if (_indexInternal == null)
			{
				ILogText plainText = InflateText();
				if (Type != LogType.Text)
				{
					plainText = plainText.ToPlainText(logger);
				}

				LogIndexBlock[] blocks = new LogIndexBlock[1];
				blocks[0] = new LogIndexBlock(LineIndex, LineCount, plainText, plainText.Data.CompressBzip2());
				_indexInternal = new LogIndexData(null, 0, blocks);
			}
			return _indexInternal;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type">Type of data stored in this subchunk</param>
		/// <param name="offset">Offset within the file of this sub-chunk</param>
		/// <param name="lineIndex">Index of the first line in this sub-chunk</param>
		/// <param name="text">Text to add</param>
		public LogSubChunkData(LogType type, long offset, int lineIndex, ILogText text)
		{
			Type = type;
			Offset = offset;
			Length = text.Data.Length;
			LineIndex = lineIndex;
			LineCount = text.LineCount;
			_textInternal = text;
		}

		/// <summary>
		/// Constructor for raw data
		/// </summary>
		/// <param name="type">Type of data stored in this subchunk</param>
		/// <param name="offset">Offset within the file of this sub-chunk</param>
		/// <param name="length">Length of the uncompressed data</param>
		/// <param name="lineIndex">Index of the first line</param>
		/// <param name="lineCount">Number of lines in the uncompressed text</param>
		/// <param name="compressedText">Compressed text data</param>
		/// <param name="index">Index data</param>
		public LogSubChunkData(LogType type, long offset, int length, int lineIndex, int lineCount, ReadOnlyMemory<byte> compressedText, LogIndexData? index)
		{
			Type = type;
			Offset = offset;
			Length = length;
			LineIndex = lineIndex;
			LineCount = lineCount;
			_compressedTextInternal = compressedText;
			_indexInternal = index;
		}

		/// <summary>
		/// Constructs a sub-chunk from a block of memory. Uses slices of the given memory buffer rather than copying the data.
		/// </summary>
		/// <param name="reader">The reader to read from</param>
		/// <param name="offset">Offset of the sub-chunk within this file</param>
		/// <param name="lineIndex">Index of the first line in this subchunk</param>
		/// <returns>New subchunk data</returns>
		public static LogSubChunkData Read(MemoryReader reader, long offset, int lineIndex)
		{
			int version = reader.ReadInt32(); // Version placeholder
			if (version == 0)
			{
				LogType type = (LogType)reader.ReadInt32();
				int length = reader.ReadInt32();
				int lineCount = reader.ReadInt32();
				ReadOnlyMemory<byte> compressedText = reader.ReadVariableLengthBytesWithInt32Length();
				reader.ReadVariableLengthBytesWithInt32Length();
				return new LogSubChunkData(type, offset, length, lineIndex, lineCount, compressedText, null);
			}
			else if (version == 1)
			{
				LogType type = (LogType)reader.ReadInt32();
				int length = reader.ReadInt32();
				int lineCount = reader.ReadInt32();
				ReadOnlyMemory<byte> compressedText = reader.ReadVariableLengthBytesWithInt32Length();
				ReadOnlyMemory<byte> compressedPlainText = reader.ReadVariableLengthBytesWithInt32Length();
				ReadOnlyTrie trie = reader.ReadTrie();
				LogIndexBlock indexBlock = new LogIndexBlock(lineIndex, lineCount, null, compressedPlainText);
				LogIndexData index = new LogIndexData(trie, 0, new[] { indexBlock });
				return new LogSubChunkData(type, offset, length, lineIndex, lineCount, compressedText, index);
			}
			else
			{
				LogType type = (LogType)reader.ReadInt32();
				int length = reader.ReadInt32();
				int lineCount = reader.ReadInt32();
				ReadOnlyMemory<byte> compressedText = reader.ReadVariableLengthBytesWithInt32Length();
				LogIndexData index = reader.ReadLogIndexData();
				index.SetBaseLineIndex(lineIndex); // Fix for incorrectly saved data
				return new LogSubChunkData(type, offset, length, lineIndex, lineCount, compressedText, index);
			}
		}

		/// <summary>
		/// Serializes the sub-chunk to a stream
		/// </summary>
		/// <param name="writer">Writer to output to</param>
		/// <param name="logger">Logger for output</param>
		public void Write(MemoryWriter writer, ILogger logger)
		{
			writer.WriteInt32(2); // Version placeholder

			writer.WriteInt32((int)Type);
			writer.WriteInt32(Length);
			writer.WriteInt32(LineCount);

			writer.WriteVariableLengthBytesWithInt32Length(DeflateText().Span);
			writer.WriteLogIndexData(BuildIndex(logger));
		}

		/// <summary>
		/// Serializes this object to a byte array
		/// </summary>
		/// <returns>Byte array</returns>
		public byte[] ToByteArray(ILogger logger)
		{
			byte[] data = new byte[GetSerializedSize(logger)];

			MemoryWriter writer = new MemoryWriter(data);
			Write(writer, logger);
			writer.CheckEmpty();

			return data;
		}

		/// <summary>
		/// Determines the size of serialized data
		/// </summary>
		public int GetSerializedSize(ILogger logger)
		{
			return sizeof(int) + sizeof(int) + sizeof(int) + sizeof(int) + (sizeof(int) + DeflateText().Length) + BuildIndex(logger).GetSerializedSize();
		}
	}

	/// <summary>
	/// Pending data for a sub-chunk
	/// </summary>
	public static class LogSubChunkDataExtensions
	{
		/// <summary>
		/// Constructs a sub-chunk from a block of memory. Uses slices of the given memory buffer rather than copying the data.
		/// </summary>
		/// <param name="reader">The reader to read from</param>
		/// <param name="offset">Offset of this sub-chunk within the file</param>
		/// <param name="lineIndex">Index of the first line within this chunk</param>
		/// <returns>New subchunk data</returns>
		public static LogSubChunkData ReadLogSubChunkData(this MemoryReader reader, long offset, int lineIndex)
		{
			return LogSubChunkData.Read(reader, offset, lineIndex);
		}

		/// <summary>
		/// Serializes the sub-chunk to a stream
		/// </summary>
		/// <param name="writer">Writer to output to</param>
		/// <param name="subChunkData">The sub-chunk data to write</param>
		/// <param name="logger">Logger for output</param>
		public static void WriteLogSubChunkData(this MemoryWriter writer, LogSubChunkData subChunkData, ILogger logger)
		{
			subChunkData.Write(writer, logger);
		}
	}
}
