// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.Json;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Logs.Data
{
	/// <summary>
	/// Exception thrown trying to parse log output
	/// </summary>
	public class LogTextParseException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public LogTextParseException(string message, Exception? ex)
			: base(message, ex)
		{
		}
	}

	/// <summary>
	/// Interface for log data
	/// </summary>
	public interface ILogText
	{
		/// <summary>
		/// The raw text data. Contains a complete set of lines followed by newline characters.
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Length of the data in this block
		/// </summary>
		public int Length { get; }

		/// <summary>
		/// Offsets of lines within the data object, including a sentinel for the end of the data (LineCount + 1 entries).
		/// </summary>
		public IReadOnlyList<int> LineOffsets { get; }

		/// <summary>
		/// Number of lines in the 
		/// </summary>
		public int LineCount { get; }

		/// <summary>
		/// Computes the number of bytes used by this object
		/// </summary>
		public int AllocatedSize { get; }
	}

	/// <summary>
	/// Helper methods for parsing log text
	/// </summary>
	public static class LogTextHelpers
	{
		/// <summary>
		/// Finds the number of newline characters in the given span
		/// </summary>
		/// <param name="span">Span of utf-8 characters to scan</param>
		/// <param name="offset">Starting offset within the span</param>
		/// <returns>Number of lines in the given span</returns>
		public static int GetLineCount(ReadOnlySpan<byte> span, int offset)
		{
			int lineCount = 0;
			for (; offset < span.Length; offset++)
			{
				if (span[offset] == (byte)'\n')
				{
					lineCount++;
				}
			}
			return lineCount;
		}

		/// <summary>
		/// Find the offsets of the start of each line within the given span
		/// </summary>
		/// <param name="span">Span to search</param>
		/// <param name="offset">Offset within the span to start searching</param>
		/// <param name="lineOffsets">Receives the list of line offsets</param>
		/// <param name="lineCount">The current line count</param>
		public static void FindLineOffsets(ReadOnlySpan<byte> span, int offset, Span<int> lineOffsets, int lineCount)
		{
			Debug.Assert(span.Length == 0 || span[^1] == (byte)'\n');
			for (; ; )
			{
				// Store the start of this line
				lineOffsets[lineCount++] = offset;

				// Check if we're at the end
				if (offset >= span.Length)
				{
					break;
				}

				// Move to the end of this line
				while (span[offset] != '\n')
				{
					offset++;
				}

				// Move to the start of the next line
				offset++;
			}
		}
	}

	/// <summary>
	/// Immutable block of log text
	/// </summary>
	public class ReadOnlyLogText : ILogText
	{
		/// <inheritdoc/>
		public ReadOnlyMemory<byte> Data { get; private set; }

		/// <inheritdoc/>
		public int Length => Data.Length;

		/// <inheritdoc/>
		public IReadOnlyList<int> LineOffsets { get; private set; }

		/// <inheritdoc/>
		public int LineCount => LineOffsets.Count - 1;

		/// <inheritdoc/>
		public int AllocatedSize => Data.Length + (LineOffsets.Count * sizeof(int));

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="data">Data to construct from</param>
		public ReadOnlyLogText(ReadOnlyMemory<byte> data)
		{
			ReadOnlySpan<byte> span = data.Span;

			int lineCount = LogTextHelpers.GetLineCount(span, 0);

			int[] lineOffsets = new int[lineCount + 1];
			LogTextHelpers.FindLineOffsets(span, 0, lineOffsets, 0);
			Debug.Assert(lineOffsets[lineCount] == data.Length);

			Data = data;
			LineOffsets = lineOffsets;
		}
	}

	/// <summary>
	/// Mutable log text object
	/// </summary>
	public class LogText : ILogText
	{
		/// <summary>
		/// Accessor for Data
		/// </summary>
		byte[] _internalData;

		/// <inheritdoc/>
		public ReadOnlyMemory<byte> Data => _internalData.AsMemory(0, Length);

		/// <summary>
		/// Hacky accessor for InternalData, for serializing to Redis
		/// </summary>
		public byte[] InternalDataHack => _internalData;

		/// <inheritdoc/>
		public int Length
		{
			get;
			private set;
		}

		/// <summary>
		/// Offsets of the start of each line within the data
		/// </summary>
		readonly List<int> _internalLineOffsets = new List<int> { 0 };

		/// <inheritdoc/>
		public IReadOnlyList<int> LineOffsets => _internalLineOffsets;

		/// <inheritdoc/>
		public int LineCount => _internalLineOffsets.Count - 1;

		/// <summary>
		/// Chunk Size
		/// </summary>
		public int MaxLength => _internalData.Length;

		/// <summary>
		/// Computes the size of data used by this object
		/// </summary>
		public int AllocatedSize => _internalData.Length + (_internalLineOffsets.Capacity * sizeof(int));

		/// <summary>
		/// Constructor
		/// </summary>
		public LogText()
		{
			_internalData = Array.Empty<byte>();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="data">Data to initialize this chunk with. Ownership of this array is transferred to the chunk, and its length determines the chunk size.</param>
		/// <param name="length">Number of valid bytes within the initial data array</param>
		public LogText(byte[] data, int length)
		{
			_internalData = data;
			Length = 0; // Updated below

			UpdateLength(length);
		}

		/// <summary>
		/// Create a new chunk data object with the given data appended. The internal buffers are reused, with the assumption that
		/// there is no contention over writing to the same location in the chunk.
		/// </summary>
		/// <param name="textData">The data to append</param>
		/// <returns>New chunk data object</returns>
		public void Append(ReadOnlySpan<byte> textData)
		{
			textData.CopyTo(_internalData.AsSpan(Length, textData.Length));
			UpdateLength(Length + textData.Length);
		}

		/// <summary>
		/// Updates the plain text representation of this chunk
		/// </summary>
		public void AppendPlainText(ILogText srcText, int srcLineIndex, int srcLineCount, ILogger logger)
		{
			// Convert all the lines to plain text
			for (int idx = 0; idx < srcLineCount; idx++)
			{
				int lineOffset = srcText.LineOffsets[srcLineIndex + idx];
				int nextLineOffset = srcText.LineOffsets[srcLineIndex + idx + 1];
				ReadOnlySpan<byte> inputLine = srcText.Data.Slice(lineOffset, nextLineOffset - lineOffset).Span;

				// Make sure the output buffer is large enough
				int requiredSpace = nextLineOffset - lineOffset;
				if (requiredSpace > _internalData.Length - Length)
				{
					int maxLineOffset = srcText.LineOffsets[srcLineIndex + srcLineCount];
					Array.Resize(ref _internalData, _internalData.Length + requiredSpace + (maxLineOffset - nextLineOffset) / 2);
				}

				// Convert the line to plain text
				try
				{
					Length = ConvertToPlainText(inputLine, _internalData, Length);
				}
				catch (Exception ex)
				{
					inputLine.CopyTo(_internalData.AsSpan(Length));
					logger.LogWarning(ex, "Exception while attempting to parse log text as JSON. Line: \"{Line}\"", Encoding.UTF8.GetString(inputLine).Trim());
				}
				_internalLineOffsets.Add(Length);
			}
		}

		/// <summary>
		/// Determines if the given line is empty
		/// </summary>
		/// <param name="input">The input data</param>
		/// <returns>True if the given text is empty</returns>
		public static bool IsEmptyOrWhitespace(ReadOnlySpan<byte> input)
		{
			for (int idx = 0; idx < input.Length; idx++)
			{
				byte v = input[idx];
				if (v != (byte)'\n' && v != '\r' && v != ' ')
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Converts a JSON log line to plain text
		/// </summary>
		/// <param name="input">The JSON data</param>
		/// <param name="output">Output buffer for the converted line</param>
		/// <param name="outputOffset">Offset within the buffer to write the converted data</param>
		/// <returns></returns>
		public static int ConvertToPlainText(ReadOnlySpan<byte> input, byte[] output, int outputOffset)
		{
			if (IsEmptyOrWhitespace(input))
			{
				output[outputOffset] = (byte)'\n';
				return outputOffset + 1;
			}

			Utf8JsonReader reader = new Utf8JsonReader(input);
			if (reader.Read() && reader.TokenType == JsonTokenType.StartObject)
			{
				while (reader.Read() && reader.TokenType == JsonTokenType.PropertyName)
				{
					if (!reader.ValueTextEquals("message"))
					{
						reader.Skip();
						continue;
					}
					if (!reader.Read() || reader.TokenType != JsonTokenType.String)
					{
						reader.Skip();
						continue;
					}

					int unescapedLength = UnescapeUtf8(reader.ValueSpan, output.AsSpan(outputOffset));
					outputOffset += unescapedLength;

					output[outputOffset] = (byte)'\n';
					outputOffset++;

					break;
				}
			}
			return outputOffset;
		}

		/// <summary>
		/// Unescape a json utf8 string
		/// </summary>
		/// <param name="source">Source span of bytes</param>
		/// <param name="target">Target span of bytes</param>
		/// <returns>Length of the converted data</returns>
		static int UnescapeUtf8(ReadOnlySpan<byte> source, Span<byte> target)
		{
			int length = 0;
			for (; ; )
			{
				// Copy up to the next backslash
				int backslash = source.IndexOf((byte)'\\');
				if (backslash == -1)
				{
					source.CopyTo(target);
					length += source.Length;
					break;
				}
				else if (backslash > 0)
				{
					source.Slice(0, backslash).CopyTo(target);
					source = source.Slice(backslash);
					target = target.Slice(backslash);
					length += backslash;
				}

				// Check what the escape code is
				if (source[1] == 'u')
				{
					char[] chars = { (char)((StringUtils.ParseHexByte(source, 2) << 8) | StringUtils.ParseHexByte(source, 4)) };
					int encodedLength = Encoding.UTF8.GetBytes(chars.AsSpan(), target);
					source = source.Slice(6);
					target = target.Slice(encodedLength);
					length += encodedLength;
				}
				else
				{
					target[0] = source[1] switch
					{
						(byte)'\"' => (byte)'\"',
						(byte)'\\' => (byte)'\\',
						(byte)'b' => (byte)'\b',
						(byte)'f' => (byte)'\f',
						(byte)'n' => (byte)'\n',
						(byte)'r' => (byte)'\r',
						(byte)'t' => (byte)'\t',
						_ => source[1]
					};
					source = source.Slice(2);
					target = target.Slice(1);
					length++;
				}
			}
			return length;
		}

		/// <summary>
		/// Generates placeholder data for a missing span
		/// </summary>
		/// <param name="chunkIdx">Index of the chunk</param>
		/// <param name="hostName">Host name of the machine that was holding the data</param>
		/// <param name="targetLength">Desired length of the buffer</param>
		/// <param name="targetLineCount">Desired line count for the buffer</param>
		public void AppendMissingDataInfo(int chunkIdx, string? hostName, int targetLength, int targetLineCount)
		{
			int newLength = Length;

			if (_internalData.Length < targetLength)
			{
				Array.Resize(ref _internalData, targetLength);
			}

			string suffix = $" (server: {hostName})";
			for (int newLineCount = LineCount; newLineCount < targetLineCount; newLineCount++)
			{
				byte[] errorBytes = Encoding.ASCII.GetBytes($"{{ \"level\":\"Error\",\"message\":\"[Missing data at chunk {chunkIdx}, line {newLineCount}{suffix}]\" }}\n");
				if (newLength + errorBytes.Length > targetLength)
				{
					break;
				}

				errorBytes.AsSpan().CopyTo(_internalData.AsSpan(newLength));
				newLength += errorBytes.Length;

				suffix = String.Empty;
			}

			if (newLength < targetLength)
			{
				newLength = Math.Max(newLength - 1, 0);
				_internalData.AsSpan(newLength, (int)((targetLength - 1) - newLength)).Fill((byte)' ');
				_internalData[targetLength - 1] = (byte)'\n';
			}

			UpdateLength((int)targetLength);
		}

		/// <summary>
		/// Shrinks the data allocated by this chunk to the minimum required
		/// </summary>
		public void Shrink()
		{
			if (Length < _internalData.Length)
			{
				Array.Resize(ref _internalData, Length);
			}
		}

		/// <summary>
		/// Updates the length of this chunk, computing all the newline offsets
		/// </summary>
		/// <param name="newLength">New length of the chunk</param>
		private void UpdateLength(int newLength)
		{
			if (newLength > Length)
			{
				// Make sure the data ends with a newline
				if (_internalData[newLength - 1] != '\n')
				{
					throw new InvalidDataException("Chunk data must end with a newline");
				}

				// Calculate the new number of newlines
				ReadOnlySpan<byte> newData = _internalData.AsSpan(0, newLength);
				for (int idx = Length; idx < newLength; idx++)
				{
					if (newData[idx] == '\n')
					{
						_internalLineOffsets.Add(idx + 1);
					}
				}

				// Update the size of the buffer
				Length = newLength;
			}
		}
	}

	/// <summary>
	/// Extension methods for ILogText
	/// </summary>
	public static class LogTextExtensions
	{
		/// <summary>
		/// Find the line index for a particular offset
		/// </summary>
		/// <param name="logText">The text to search</param>
		/// <param name="offset">Offset within the text</param>
		/// <returns>The line index</returns>
		public static int GetLineIndexForOffset(this ILogText logText, int offset)
		{
			int lineIdx = logText.LineOffsets.BinarySearch(offset);
			if (lineIdx < 0)
			{
				lineIdx = ~lineIdx - 1;
			}
			return lineIdx;
		}

		/// <summary>
		/// Converts a log text instance to plain text
		/// </summary>
		/// <param name="logText">The text to convert</param>
		/// <param name="logger">Logger for conversion warnings</param>
		/// <returns>The plain text instance</returns>
		public static ILogText ToPlainText(this ILogText logText, ILogger logger)
		{
			LogText other = new LogText();
			other.AppendPlainText(logText, 0, logText.LineCount, logger);
			return other;
		}
	}
}
