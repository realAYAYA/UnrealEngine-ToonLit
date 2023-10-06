// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.Common;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using System.Threading;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Microsoft.Extensions.Logging;
using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace EpicGames.Horde.Logs
{
	/// <summary>
	/// Format for the log file
	/// </summary>
	public enum LogFormat
	{
		/// <summary>
		/// Text data
		/// </summary>
		Text = 0,

		/// <summary>
		/// Json data
		/// </summary>
		Json = 1,
	}

	/// <summary>
	/// Represents an entire log
	/// </summary>
	[NodeType("{274DF8F7-9E87-4B4F-8AD5-318CDB25AD33}", 1)]
	public class LogNode : Node
	{
		/// <summary>
		/// Format for this log file
		/// </summary>
		public LogFormat Format { get; }

		/// <summary>
		/// Total number of lines
		/// </summary>
		public int LineCount { get; }

		/// <summary>
		/// Length of the log file
		/// </summary>
		public long Length { get; }

		/// <summary>
		/// Text blocks for this chunk
		/// </summary>
		public IReadOnlyList<LogChunkRef> TextChunkRefs { get; }

		/// <summary>
		/// Index for this log
		/// </summary>
		public NodeRef<LogIndexNode> IndexRef { get; }

		/// <summary>
		/// Whether this log is complete
		/// </summary>
		public bool Complete { get; }

		/// <summary>
		/// Deserializing constructor
		/// </summary>
		public LogNode(LogFormat format, int lineCount, long length, IReadOnlyList<LogChunkRef> textChunkRefs, NodeRef<LogIndexNode> indexRef, bool complete)
		{
			Format = format;
			LineCount = lineCount;
			Length = length;
			TextChunkRefs = textChunkRefs.ToArray();
			IndexRef = indexRef;
			Complete = complete;
		}

		/// <summary>
		/// Deserializing constructor
		/// </summary>
		/// <param name="reader">Reader to draw data from</param>
		public LogNode(NodeReader reader)
		{
			Format = (LogFormat)reader.ReadUInt8();
			LineCount = (int)reader.ReadUnsignedVarInt();
			Length = (long)reader.ReadUnsignedVarInt();
			IndexRef = reader.ReadNodeRef<LogIndexNode>();
			TextChunkRefs = reader.ReadList(() => new LogChunkRef(reader));
			Complete = reader.ReadBoolean();
		}

		/// <inheritdoc/>
		public override void Serialize(NodeWriter writer)
		{
			writer.WriteUInt8((byte)Format);
			writer.WriteUnsignedVarInt(LineCount);
			writer.WriteUnsignedVarInt((ulong)Length);
			writer.WriteNodeRef(IndexRef);
			writer.WriteList(TextChunkRefs, x => writer.WriteNodeRef(x));
			writer.WriteBoolean(Complete);
		}
	}

	/// <summary>
	/// Assists building log files through trees of <see cref="LogNode"/>, <see cref="LogIndexNode"/> and <see cref="LogChunkNode"/> nodes.
	/// </summary>
	public class LogBuilder
	{
		/// <summary>
		/// Default maximum size for a log text block
		/// </summary>
		public const int DefaultTextBlockLength = 256 * 1024;

		/// <summary>
		/// Default maximum size for an index text block
		/// </summary>
		public const int DefaultIndexBlockLength = 64 * 1024;

		/// <summary>
		/// Number of lines written to the log
		/// </summary>
		public int LineCount => FlushedLineCount + _textBuilder.LineCount;

		/// <summary>
		/// Number of lines flushed to storage
		/// </summary>
		public int FlushedLineCount => _root?.LineCount ?? 0;

		readonly LogFormat _format;

		// Data for the log file which has been flushed to disk so far
		LogNode? _root;
		LogIndexNode _index = LogIndexNode.Empty;

		// Json data read but not flushed
		readonly LogChunkSequenceBuilder _textBuilder;
		readonly LogChunkSequenceBuilder _indexTextBuilder;

		// Lock object for access to the fields above
		readonly object _lockObject = new object();

		// Inner log device; used for messaging encoding errors.
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="format">Format for data in the log file</param>
		/// <param name="logger"></param>
		public LogBuilder(LogFormat format, ILogger logger)
			: this(format, DefaultTextBlockLength, DefaultIndexBlockLength, logger)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="format">Format of data in the log file</param>
		/// <param name="maxTextBlockLength">maximum size for a regular text block</param>
		/// <param name="maxIndexBlockLength">Maximum size for an index text block</param>
		/// <param name="logger">Logger for conversion errors</param>
		public LogBuilder(LogFormat format, int maxTextBlockLength, int maxIndexBlockLength, ILogger logger)
		{
			_format = format;
			_textBuilder = new LogChunkSequenceBuilder(maxTextBlockLength);
			_indexTextBuilder = new LogChunkSequenceBuilder(maxIndexBlockLength);
			_logger = logger;
		}

		/// <summary>
		/// Read data from the unflushed log tail
		/// </summary>
		/// <param name="firstLineIdx">The first line to read, from the end of the flushed data</param>
		/// <param name="maxLength"></param>
		/// <returns></returns>
		public ReadOnlyMemory<byte> ReadTailData(int firstLineIdx, int maxLength)
		{
			lock (_lockObject)
			{
				// Measure the size of buffer required for the tail data
				int length = 0;
				int lineCount = 0;
				foreach (Utf8String line in _textBuilder.EnumerateLines(firstLineIdx))
				{
					int nextLength = length + line.Length;
					if (length > 0 && nextLength > maxLength)
					{
						break;
					}
					length = nextLength;
					lineCount++;
				}

				// Allocate the buffer
				byte[] buffer = new byte[length];

				// Copy lines into the buffer
				Span<byte> output = buffer.AsSpan();
				foreach (Utf8String line in _textBuilder.EnumerateLines(firstLineIdx).Take(lineCount))
				{
					line.Span.CopyTo(output);
					output = output.Slice(line.Length);
				}
				Debug.Assert(output.Length == 0);

				return buffer;
			}
		}

		/// <summary>
		/// Append JSON data to the end of the log
		/// </summary>
		/// <param name="data">Log data to append</param>
		public void WriteData(ReadOnlyMemory<byte> data)
		{
			lock (_lockObject)
			{
				ReadOnlyMemory<byte> remaining = data;
				for (; ; )
				{
					int newlineIdx = remaining.Span.IndexOf((byte)'\n');
					if (newlineIdx == -1)
					{
						break;
					}

					ReadOnlyMemory<byte> line = remaining.Slice(0, newlineIdx + 1);
					_textBuilder.Append(line.Span);

					if (_format == LogFormat.Json)
					{
						_indexTextBuilder.AppendJsonAsPlainText(line.Span, _logger);
					}
					else
					{
						_indexTextBuilder.Append(line.Span);
					}

					remaining = remaining.Slice(newlineIdx + 1);
				}
			}
		}

		/// <summary>
		/// Flushes the written data to the log
		/// </summary>
		/// <param name="writer">Writer for the output nodes</param>
		/// <param name="complete">Whether the log is complete</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<NodeRef<LogNode>> FlushAsync(IStorageWriter writer, bool complete, CancellationToken cancellationToken)
		{
			// Capture the new data that needs to be written
			IReadOnlyList<LogChunkNode> writeTextChunks;
			IReadOnlyList<LogChunkNode> writeIndexTextChunks;

			lock (_lockObject)
			{
				_textBuilder.Flush();
				writeTextChunks = _textBuilder.Chunks.ToArray();

				_indexTextBuilder.Flush();
				writeIndexTextChunks = _indexTextBuilder.Chunks.ToArray();
			}

			// Flush any complete chunks to storage
			LogIndexNode newIndex = await _index.AppendAsync(writer, writeIndexTextChunks, cancellationToken);
			NodeRef<LogIndexNode> newIndexRef = await writer.WriteNodeAsync(newIndex, cancellationToken);

			List<LogChunkRef> newJsonChunkRefs = new List<LogChunkRef>(_root?.TextChunkRefs ?? Array.Empty<LogChunkRef>());
			int lineCount = _root?.LineCount ?? 0;
			long length = _root?.Length ?? 0;
			foreach (LogChunkNode writeTextChunk in writeTextChunks)
			{
				NodeRef<LogChunkNode> writeTextChunkRef = await writer.WriteNodeAsync(writeTextChunk, cancellationToken);
				newJsonChunkRefs.Add(new LogChunkRef(lineCount, writeTextChunk.LineCount, length, writeTextChunk.Length, writeTextChunkRef));
				lineCount += writeTextChunk.LineCount;
				length += writeTextChunk.Length;
			}

			LogNode newRoot = new LogNode(_format, lineCount, length, newJsonChunkRefs, newIndexRef, complete);
			NodeRef<LogNode> newRootRef = await writer.WriteNodeAsync(newRoot, cancellationToken);

			await writer.FlushAsync(cancellationToken);

			// Update the new state
			lock (_lockObject)
			{
				_root = newRoot;
				_index = newIndex;
				_textBuilder.Remove(writeTextChunks.Count);
				_indexTextBuilder.Remove(writeIndexTextChunks.Count);
			}

			return newRootRef;
		}
	}

	/// <summary>
	/// Extension methods
	/// </summary>
	public static class LogNodeExtensions
	{
		/// <summary>
		/// Reads lines from a line
		/// </summary>
		/// <param name="logNode">Log to read from</param>
		/// <param name="reader">Reader to pull nodes from</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Sequence of line buffers</returns>
		public static async IAsyncEnumerable<ReadOnlyMemory<byte>> ReadLogAsync(this LogNode logNode, BundleReader reader, [EnumeratorCancellation] CancellationToken cancellationToken)
		{
			foreach (LogChunkRef textChunkRef in logNode.TextChunkRefs)
			{
				LogChunkNode textChunk = await textChunkRef.ExpandAsync(cancellationToken);
				yield return textChunk.Data;
			}
		}

		/// <summary>
		/// Reads lines from a line
		/// </summary>
		/// <param name="logNode">Log to read from</param>
		/// <param name="index">Zero-based index of the first line to read from</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Sequence of line buffers</returns>
		public static async IAsyncEnumerable<ReadOnlyMemory<byte>> ReadLogLinesAsync(this LogNode logNode, int index, [EnumeratorCancellation] CancellationToken cancellationToken)
		{
			foreach (LogChunkRef textChunkRef in logNode.TextChunkRefs)
			{
				int lineIdx = Math.Max(index - textChunkRef.LineIndex, 0);
				if (lineIdx < textChunkRef.LineCount)
				{
					LogChunkNode textChunk = await textChunkRef.ExpandAsync(cancellationToken);

					int offset = textChunk.LineOffsets[lineIdx];
					for (; lineIdx < textChunk.LineCount; lineIdx++)
					{
						int nextOffset = textChunk.LineOffsets[lineIdx + 1];
						ReadOnlyMemory<byte> line = textChunk.Data.Slice(offset, nextOffset - offset);
						yield return line;
						offset = nextOffset;
					}
				}
			}
		}
	}
}
