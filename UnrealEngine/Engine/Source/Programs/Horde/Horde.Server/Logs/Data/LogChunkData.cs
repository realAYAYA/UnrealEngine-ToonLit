// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Logs.Data
{
	/// <summary>
	/// Data for a log chunk
	/// </summary>
	public class LogChunkData
	{
		/// <summary>
		/// Offset of this chunk
		/// </summary>
		public long Offset { get; }

		/// <summary>
		/// Length of this chunk
		/// </summary>
		public int Length { get; }

		/// <summary>
		/// Line index of this chunk
		/// </summary>
		public int LineIndex { get; }

		/// <summary>
		/// Number of lines in this chunk
		/// </summary>
		public int LineCount { get; }

		/// <summary>
		/// List of sub-chunks
		/// </summary>
		public IReadOnlyList<LogSubChunkData> SubChunks { get; }

		/// <summary>
		/// Offset of each sub-chunk within this chunk
		/// </summary>
		public int[] SubChunkOffset { get; }

		/// <summary>
		/// Total line count of the chunk after each sub-chunk
		/// </summary>
		public int[] SubChunkLineIndex { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="offset">Offset of this chunk</param>
		/// <param name="lineIndex">First line index of this chunk</param>
		/// <param name="subChunks">Sub-chunks for this chunk</param>
		public LogChunkData(long offset, int lineIndex, IReadOnlyList<LogSubChunkData> subChunks)
		{
			Offset = offset;
			LineIndex = lineIndex;
			SubChunks = subChunks;
			SubChunkOffset = CreateSumLookup(subChunks, x => x.Length);
			SubChunkLineIndex = CreateSumLookup(subChunks, x => x.LineCount);

			if (subChunks.Count > 0)
			{
				int lastSubChunkIdx = subChunks.Count - 1;
				Length = SubChunkOffset[lastSubChunkIdx] + subChunks[lastSubChunkIdx].Length;
				LineCount = SubChunkLineIndex[lastSubChunkIdx] + subChunks[lastSubChunkIdx].LineCount;
			}
		}

		/// <summary>
		/// Creates a lookup table by summing a field across a list of subchunks
		/// </summary>
		/// <param name="subChunks"></param>
		/// <param name="field"></param>
		/// <returns></returns>
		static int[] CreateSumLookup(IReadOnlyList<LogSubChunkData> subChunks, Func<LogSubChunkData, int> field)
		{
			int[] total = new int[subChunks.Count];

			int value = 0;
			for (int idx = 0; idx + 1 < subChunks.Count; idx++)
			{
				value += field(subChunks[idx]);
				total[idx + 1] = value;
			}

			return total;
		}

		/// <summary>
		/// Gets the offset of a line within the file
		/// </summary>
		/// <param name="lineIdx">The line index</param>
		/// <returns>Offset of the line within the file</returns>
		public long GetLineOffsetWithinChunk(int lineIdx)
		{
			int subChunkIdx = SubChunkLineIndex.BinarySearch(lineIdx);
			if (subChunkIdx < 0)
			{
				subChunkIdx = ~subChunkIdx - 1;
			}
			return SubChunkOffset[subChunkIdx] + SubChunks[subChunkIdx].InflateText().LineOffsets[lineIdx - SubChunkLineIndex[subChunkIdx]];
		}

		/// <summary>
		/// Gets the sub chunk index for the given line
		/// </summary>
		/// <param name="chunkLineIdx">Line index within the chunk</param>
		/// <returns>Subchunk line index</returns>
		public int GetSubChunkForLine(int chunkLineIdx)
		{
			int subChunkIdx = SubChunkLineIndex.BinarySearch(chunkLineIdx);
			if (subChunkIdx < 0)
			{
				subChunkIdx = ~subChunkIdx - 1;
			}
			return subChunkIdx;
		}

		/// <summary>
		/// Gets the index of the sub-chunk containing the given offset
		/// </summary>
		/// <param name="offset">Offset to search for</param>
		/// <returns>Index of the sub-chunk</returns>
		public int GetSubChunkForOffsetWithinChunk(int offset)
		{
			int subChunkIdx = SubChunkOffset.BinarySearch(offset);
			if (subChunkIdx < 0)
			{
				subChunkIdx = ~subChunkIdx - 1;
			}
			return subChunkIdx;
		}

		/// <summary>
		/// Signature for output data
		/// </summary>
		const int CurrentSignature = 'L' | ('C' << 8) | ('D' << 16);

		/// <summary>
		/// Read a log chunk from the given stream
		/// </summary>
		/// <param name="reader">The reader to read from</param>
		/// <param name="offset">Offset of this chunk within the file</param>
		/// <param name="lineIndex">Line index of this chunk</param>
		/// <returns>New log chunk data</returns>
		public static LogChunkData Read(MemoryReader reader, long offset, int lineIndex)
		{
			int signature = reader.ReadInt32();
			if ((signature & 0xffffff) != CurrentSignature)
			{
				List<LogSubChunkData> subChunks = new List<LogSubChunkData>();
				subChunks.Add(new LogSubChunkData(LogType.Json, offset, lineIndex, new ReadOnlyLogText(reader.RemainingMemory)));
				reader.Advance(reader.RemainingMemory.Length);
				return new LogChunkData(offset, lineIndex, subChunks);
			}

			int version = signature >> 24;
			if (version == 0)
			{
				List<LogSubChunkData> subChunks = ReadSubChunkList(reader, offset, lineIndex);
				reader.ReadVariableLengthBytesWithInt32Length();
				return new LogChunkData(offset, lineIndex, subChunks);
			}
			else
			{
				List<LogSubChunkData> subChunks = ReadSubChunkList(reader, offset, lineIndex);
				return new LogChunkData(offset, lineIndex, subChunks);
			}
		}

		/// <summary>
		/// Read a list of sub-chunks from the given stream
		/// </summary>
		/// <param name="reader">The reader to read from</param>
		/// <param name="subChunkOffset">Offset of this chunk within the file</param>
		/// <param name="subChunkLineIndex">Line index of this chunk</param>
		/// <returns>List of sub-chunks</returns>
		static List<LogSubChunkData> ReadSubChunkList(MemoryReader reader, long subChunkOffset, int subChunkLineIndex)
		{
			int numSubChunks = reader.ReadInt32();

			List<LogSubChunkData> subChunks = new List<LogSubChunkData>();
			for (int idx = 0; idx < numSubChunks; idx++)
			{
				LogSubChunkData subChunkData = reader.ReadLogSubChunkData(subChunkOffset, subChunkLineIndex);
				subChunkOffset += subChunkData.Length;
				subChunkLineIndex += subChunkData.LineCount;
				subChunks.Add(subChunkData);
			}

			return subChunks;
		}

		/// <summary>
		/// Construct an object from flat memory buffer
		/// </summary>
		/// <param name="memory">Memory buffer</param>
		/// <param name="offset">Offset of this chunk within the file</param>
		/// <param name="lineIndex">Line index of this chunk</param>
		/// <returns>Log chunk data</returns>
		public static LogChunkData FromMemory(ReadOnlyMemory<byte> memory, long offset, int lineIndex)
		{
			MemoryReader reader = new MemoryReader(memory);
			LogChunkData chunkData = Read(reader, offset, lineIndex);
			reader.CheckEmpty();
			return chunkData;
		}

		/// <summary>
		/// Write the chunk data to a stream
		/// </summary>
		/// <returns>Serialized data</returns>
		public void Write(MemoryWriter writer, ILogger logger)
		{
			writer.WriteInt32(CurrentSignature | (1 << 24));

			writer.WriteInt32(SubChunks.Count);
			foreach (LogSubChunkData subChunk in SubChunks)
			{
				writer.WriteLogSubChunkData(subChunk, logger);
			}
		}

		/// <summary>
		/// Construct an object from flat memory buffer
		/// </summary>
		/// <param name="logger">Logger for output</param>
		/// <returns>Log chunk data</returns>
		public byte[] ToByteArray(ILogger logger)
		{
			byte[] data = new byte[GetSerializedSize(logger)];
			MemoryWriter writer = new MemoryWriter(data);
			Write(writer, logger);
			writer.CheckEmpty();
			return data;
		}

		/// <summary>
		/// Determines the size of the serialized buffer
		/// </summary>
		public int GetSerializedSize(ILogger logger)
		{
			return sizeof(int) + (sizeof(int) + SubChunks.Sum(x => x.GetSerializedSize(logger)));
		}
	}

	/// <summary>
	/// Extensions for the log chunk data
	/// </summary>
	static class LogChunkDataExtensions
	{
		/// <summary>
		/// Read a log chunk from the given stream
		/// </summary>
		/// <param name="reader">The reader to read from</param>
		/// <param name="offset">Offset of this chunk within the file</param>
		/// <param name="lineIndex">Line index of this chunk</param>
		/// <returns>New log chunk data</returns>
		public static LogChunkData ReadLogChunkData(this MemoryReader reader, long offset, int lineIndex)
		{
			return LogChunkData.Read(reader, offset, lineIndex);
		}

		/// <summary>
		/// Write the chunk data to a stream
		/// </summary>
		/// <returns>Serialized data</returns>
		public static void WriteLogChunkData(this MemoryWriter writer, LogChunkData chunkData, ILogger logger)
		{
			chunkData.Write(writer, logger);
		}
	}
}
