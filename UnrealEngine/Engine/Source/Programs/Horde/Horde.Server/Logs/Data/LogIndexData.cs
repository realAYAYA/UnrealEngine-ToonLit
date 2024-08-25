// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using EpicGames.Core;
using EpicGames.Horde.Logs;
using Horde.Server.Utilities;
using OpenTelemetry.Trace;

namespace Horde.Server.Logs.Data
{
	/// <summary>
	/// Contains a source block of text to be indexed
	/// </summary>
	public class LogIndexBlock
	{
		/// <summary>
		/// First line within the file
		/// </summary>
		public int LineIndex { get; }

		/// <summary>
		/// Number of lines in this block
		/// </summary>
		public int LineCount { get; }

		/// <summary>
		/// The plain text for this block
		/// </summary>
		public ILogText? CachedPlainText { get; private set; }

		/// <summary>
		/// The compressed plain text data
		/// </summary>
		public ReadOnlyMemory<byte> CompressedPlainText { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="lineIndex">Index of the first line within this block</param>
		/// <param name="lineCount">Number of lines in the block</param>
		/// <param name="cachedPlainText">The decompressed plain text</param>
		/// <param name="compressedPlainText">The compressed text data</param>
		public LogIndexBlock(int lineIndex, int lineCount, ILogText? cachedPlainText, ReadOnlyMemory<byte> compressedPlainText)
		{
			LineIndex = lineIndex;
			LineCount = lineCount;
			CachedPlainText = cachedPlainText;
			CompressedPlainText = compressedPlainText;
		}

		/// <summary>
		/// Accessor for the decompressed plaintext
		/// </summary>
		public ILogText InflatePlainText()
		{
			CachedPlainText ??= new ReadOnlyLogText(CompressedPlainText.DecompressBzip2());
			return CachedPlainText;
		}
	}

	/// <summary>
	/// Extension methods for <see cref="LogIndexBlock"/>
	/// </summary>
	public static class LogIndexBlockExtensions
	{
		/// <summary>
		/// Deserialize an index block
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The new index block</returns>
		public static LogIndexBlock ReadLogIndexBlock(this MemoryReader reader)
		{
			int lineIndex = reader.ReadInt32();
			int lineCount = reader.ReadInt32();
			ReadOnlyMemory<byte> compressedPlainText = reader.ReadVariableLengthBytesWithInt32Length();
			return new LogIndexBlock(lineIndex, lineCount, null, compressedPlainText);
		}

		/// <summary>
		/// Serialize an index block
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="block">The block to serialize</param>
		public static void WriteLogIndexBlock(this MemoryWriter writer, LogIndexBlock block)
		{
			writer.WriteInt32(block.LineIndex);
			writer.WriteInt32(block.LineCount);
			writer.WriteVariableLengthBytesWithInt32Length(block.CompressedPlainText.Span);
		}

		/// <summary>
		/// Gets the serialized size of a block
		/// </summary>
		/// <param name="block">The block to serialize</param>
		public static int GetSerializedSize(this LogIndexBlock block)
		{
			return sizeof(int) + sizeof(int) + (sizeof(int) + block.CompressedPlainText.Length);
		}
	}

	/// <summary>
	/// Data for a log chunk
	/// </summary>
	public class LogIndexData
	{
		/// <summary>
		/// Index for tokens into the block list
		/// </summary>
		ReadOnlyTrie? _cachedTrie;

		/// <summary>
		/// Number of bits in the index devoted to the block index
		/// </summary>
		readonly int _numBlockBits;

		/// <summary>
		/// List of text blocks
		/// </summary>
		readonly LogIndexBlock[] _blocks;

		/// <summary>
		/// Empty index data
		/// </summary>
		public static LogIndexData Empty { get; } = new LogIndexData(ReadOnlyTrie.Empty, 0, Array.Empty<LogIndexBlock>());

		/// <summary>
		/// Number of lines covered by the index
		/// </summary>
		public int LineCount => (_blocks.Length > 0) ? (_blocks[^1].LineIndex + _blocks[^1].LineCount) : 0;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="cachedTrie">Index into the text blocks</param>
		/// <param name="numBlockBits">Number of bits devoted to the block index</param>
		/// <param name="blocks">Bloom filters for this log file</param>
		public LogIndexData(ReadOnlyTrie? cachedTrie, int numBlockBits, LogIndexBlock[] blocks)
		{
			_cachedTrie = cachedTrie;
			_numBlockBits = numBlockBits;
			_blocks = blocks;
		}

		/// <summary>
		/// Updates the blocks in this index relative to a give line index
		/// </summary>
		/// <param name="lineIndex"></param>
		public void SetBaseLineIndex(int lineIndex)
		{
			for (int blockIdx = 0; blockIdx < _blocks.Length; blockIdx++)
			{
				LogIndexBlock block = _blocks[blockIdx];
				_blocks[blockIdx] = new LogIndexBlock(lineIndex, block.LineCount, block.CachedPlainText, block.CompressedPlainText);
				lineIndex += block.LineCount;
			}
		}

		/// <summary>
		/// Builds a trie for this index
		/// </summary>
		/// <returns>The trie for this data</returns>
		ReadOnlyTrie BuildTrie()
		{
			if (_cachedTrie == null)
			{
				ReadOnlyTrieBuilder builder = new ReadOnlyTrieBuilder();
				for (int blockIdx = 0; blockIdx < _blocks.Length; blockIdx++)
				{
					LogIndexBlock block = _blocks[blockIdx];
					LogToken.GetTokens(block.InflatePlainText().Data.Span, token => builder.Add((token << _numBlockBits) | (uint)blockIdx));
				}
				_cachedTrie = builder.Build();
			}
			return _cachedTrie;
		}

		/// <summary>
		/// Create an index from an array of blocks
		/// </summary>
		/// <param name="indexes">List of indexes to merge</param>
		/// <returns>Index data</returns>
		public static LogIndexData Merge(IEnumerable<LogIndexData> indexes)
		{
			using TelemetrySpan span = OpenTelemetryTracers.Horde.StartActiveSpan($"{nameof(LogIndexData)}.{nameof(Merge)}");

			// Create the combined block list
			LogIndexBlock[] newBlocks = indexes.SelectMany(x => x._blocks).ToArray();

			// Figure out how many bits to devote to the block size
			int newNumBlockBits = 0;
			while (newBlocks.Length > (1 << newNumBlockBits))
			{
				newNumBlockBits += 4;
			}

			// Add all the blocks into a combined index
			ReadOnlyTrieBuilder newTrieBuilder = new ReadOnlyTrieBuilder();

			int blockCount = 0;
			foreach (LogIndexData index in indexes)
			{
				ulong blockMask = ((1UL << index._numBlockBits) - 1);
				foreach (ulong value in index.BuildTrie())
				{
					ulong token = value >> index._numBlockBits;
					int blockIdx = (int)(value & blockMask);

					ulong newToken = (token << newNumBlockBits) | (ulong)(long)(blockCount + blockIdx);
					newTrieBuilder.Add(newToken);
				}
				blockCount += index._blocks.Length;
			}

			// Construct the new index data
			ReadOnlyTrie newTrie = newTrieBuilder.Build();
			return new LogIndexData(newTrie, newNumBlockBits, newBlocks);
		}

		/// <summary>
		/// Search for the given text in the index
		/// </summary>
		/// <param name="firstLineIndex">First line index to search from</param>
		/// <param name="text">Text to search for</param>
		/// <param name="stats">Receives stats for the search</param>
		/// <returns>List of line numbers for the text</returns>
		public IEnumerable<int> Search(int firstLineIndex, SearchText text, SearchStats stats)
		{
			int lastBlockCount = 0;
			foreach (int blockIdx in EnumeratePossibleBlocks(text.Bytes, firstLineIndex))
			{
				LogIndexBlock block = _blocks[blockIdx];

				stats.NumScannedBlocks++;
				stats.NumDecompressedBlocks += (block.CachedPlainText == null) ? 1 : 0;

				stats.NumSkippedBlocks += blockIdx - lastBlockCount;
				lastBlockCount = blockIdx + 1;

				// Decompress the text
				ILogText blockText = block.InflatePlainText();

				// Find the initial offset within this block
				int offset = 0;
				if (firstLineIndex > block.LineIndex)
				{
					int lineIndexWithinBlock = firstLineIndex - block.LineIndex;
					offset = blockText.LineOffsets[lineIndexWithinBlock];
				}

				// Search within this block
				for (; ; )
				{
					// Find the next offset
					int nextOffset = blockText.Data.Span.FindNextOcurrence(offset, text);
					if (nextOffset == -1)
					{
						stats.NumScannedBytes += blockText.Length - offset;
						break;
					}

					// Update the stats
					stats.NumScannedBytes += nextOffset - offset;
					offset = nextOffset;

					// Check it's not another match within the same line
					int lineIndexWithinBlock = blockText.GetLineIndexForOffset(offset);
					yield return block.LineIndex + lineIndexWithinBlock;

					// Move to the next line
					offset = blockText.LineOffsets[lineIndexWithinBlock + 1];
				}

				// If the last scanned bytes is zero, we didn't have any matches from this chunk
				if (offset == 0 && _cachedTrie != null)
				{
					stats.NumFalsePositiveBlocks++;
				}
			}
			stats.NumSkippedBlocks += _blocks.Length - lastBlockCount;
		}

		/// <summary>
		/// Search for the given text in the index
		/// </summary>
		/// <param name="text">Text to search for</param>
		/// <param name="lineIndex">The first </param>
		/// <returns>List of line numbers for the text</returns>
		IEnumerable<int> EnumeratePossibleBlocks(ReadOnlyMemory<byte> text, int lineIndex)
		{
			// Find the starting block index
			int blockIdx = _blocks.BinarySearch(x => x.LineIndex, lineIndex);
			if (blockIdx < 0)
			{
				blockIdx = Math.Max(~blockIdx - 1, 0);
			}

			// Make sure we're not starting out of range
			if (blockIdx < _blocks.Length)
			{
				// In order to be considered a possible block for a positive match, the block must contain an exact list of tokens parsed from the
				// search text, along with a set of partial matches (for substrings that are not guaranteed to start on token boundaries). The former
				// is relatively cheap to compute, and done by stepping through all enumerators for each token in a wave, only returning blocks
				// which all contain the token. At the very least, this must contain blocks in the index from BlockIdx onwards.
				List<IEnumerator<int>> enumerators = new List<IEnumerator<int>>();
				enumerators.Add(Enumerable.Range(blockIdx, _blocks.Length - blockIdx).GetEnumerator());

				// The second aspect of the search requires a more expensive search through the trie. This is done by running a set of arbitrary 
				// delegates to filter the matches returned from the enumerators.
				List<Predicate<int>> predicates = new List<Predicate<int>>();

				// If the index has a trie, tokenize the input and generate a list of enumerators and predicates.
				if (_cachedTrie != null)
				{
					// Find a list of filters for matching blocks
					HashSet<ulong> tokens = new HashSet<ulong>();
					for (int tokenPos = 0; tokenPos < text.Length;)
					{
						ReadOnlySpan<byte> token = LogToken.GetTokenText(text.Span, tokenPos);
						if (tokenPos == 0)
						{
							GetUnalignedTokenPredicate(token, token.Length == text.Length, predicates);
						}
						else
						{
							GetAlignedTokenPredicate(token, tokenPos + token.Length == text.Length, tokens, predicates);
						}
						tokenPos += token.Length;
					}

					// Create an enumerator for each token
					foreach (ulong token in tokens)
					{
						ulong minValue = (token << _numBlockBits) | (ulong)(long)blockIdx;
						ulong maxValue = minValue + (1UL << _numBlockBits) - 1;

						IEnumerator<int> enumerator = _cachedTrie.EnumerateRange(minValue, maxValue).Select(x => (int)(x - minValue)).GetEnumerator();
						if (!enumerator.MoveNext())
						{
							yield break;
						}

						enumerators.Add(enumerator);
					}
				}

				// Enumerate the matches
				for (; ; )
				{
					// Advance all the enumerators that are behind the current block index. If they are all equal, we have a match.
					bool match = true;
					foreach (IEnumerator<int> enumerator in enumerators)
					{
						while (enumerator.Current < blockIdx)
						{
							if (!enumerator.MoveNext())
							{
								yield break;
							}
						}

						if (enumerator.Current > blockIdx)
						{
							blockIdx = enumerator.Current;
							match = false;
						}
					}

					// Return the match and move to the next block
					if (match)
					{
						if (predicates.All(predicate => predicate(blockIdx)))
						{
							yield return blockIdx;
						}
						blockIdx++;
					}
				}
			}
		}

		/// <summary>
		/// Gets predicates for matching a token that starts 
		/// </summary>
		/// <param name="text">The token text</param>
		/// <param name="allowPartialMatch">Whether to allow a partial match of the token</param>
		/// <param name="tokens">Set of aligned tokens that are required</param>
		/// <param name="predicates">List of predicates for the search</param>
		void GetAlignedTokenPredicate(ReadOnlySpan<byte> text, bool allowPartialMatch, HashSet<ulong> tokens, List<Predicate<int>> predicates)
		{
			for (int offset = 0; offset < text.Length; offset += LogToken.MaxTokenBytes)
			{
				ulong token = LogToken.GetWindowedTokenValue(text, offset);
				if (offset + LogToken.MaxTokenBytes > text.Length && allowPartialMatch)
				{
					ulong tokenMask = LogToken.GetWindowedTokenMask(text, offset, true);
					predicates.Add(blockIdx => BlockContainsToken(blockIdx, token, tokenMask));
					break;
				}
				tokens.Add(token);
			}
		}

		/// <summary>
		/// Generates a predicate for matching a token which may or may not start on a regular token boundary
		/// </summary>
		/// <param name="text">The token text</param>
		/// <param name="allowPartialMatch">Whether to allow a partial match of the token</param>
		/// <param name="predicates">List of predicates for the search</param>
		void GetUnalignedTokenPredicate(ReadOnlySpan<byte> text, bool allowPartialMatch, List<Predicate<int>> predicates)
		{
			byte[] textCopy = text.ToArray();

			Lazy<HashSet<int>> blocks = new Lazy<HashSet<int>>(() =>
			{
				HashSet<int> union = new HashSet<int>();
				for (int shift = 0; shift < LogToken.MaxTokenBytes; shift++)
				{
					HashSet<int> blocks = new HashSet<int>(BlocksContainingToken(textCopy.AsSpan(), -shift, allowPartialMatch));
					for (int offset = -shift + LogToken.MaxTokenBytes; offset < textCopy.Length && blocks.Count > 0; offset += LogToken.MaxTokenBytes)
					{
						blocks.IntersectWith(BlocksContainingToken(textCopy.AsSpan(), offset, allowPartialMatch));
					}
					if (blocks.Count > 0)
					{
						union.UnionWith(blocks);
					}
				}
				return union;
			});

			predicates.Add(blockIdx => blocks.Value.Contains(blockIdx));
		}

		/// <summary>
		/// Tests whether a block contains a particular token
		/// </summary>
		/// <param name="blockIdx">Index of the block to search</param>
		/// <param name="token">The token to test</param>
		/// <param name="tokenMask">Mask of which bits in the token are valid</param>
		/// <returns>True if the given block contains a token</returns>
		bool BlockContainsToken(int blockIdx, ulong token, ulong tokenMask)
		{
			token = (token << _numBlockBits) | (uint)blockIdx;
			tokenMask = (tokenMask << _numBlockBits) | ((1UL << _numBlockBits) - 1);
			return _cachedTrie!.EnumerateValues((value, valueMask) => (value & tokenMask) == (token & valueMask)).Any();
		}

		/// <summary>
		/// Tests whether a block contains a particular token
		/// </summary>
		/// <param name="text">The token to test</param>
		/// <param name="offset">Offset of the window into the token to test</param>
		/// <param name="allowPartialMatch">Whether to allow a partial match of the token</param>
		/// <returns>True if the given block contains a token</returns>
		IEnumerable<int> BlocksContainingToken(ReadOnlySpan<byte> text, int offset, bool allowPartialMatch)
		{
			ulong token = LogToken.GetWindowedTokenValue(text, offset) << _numBlockBits;
			ulong tokenMask = LogToken.GetWindowedTokenMask(text, offset, allowPartialMatch) << _numBlockBits;
			ulong blockMask = (1UL << _numBlockBits) - 1;
			return _cachedTrie!.EnumerateValues((value, valueMask) => (value & tokenMask) == (token & valueMask)).Select(x => (int)(x & blockMask)).Distinct();
		}

		/// <summary>
		/// Deserialize the index from memory
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>Index data</returns>
		public static LogIndexData Read(MemoryReader reader)
		{
			int version = reader.ReadInt32();
			if (version == 0)
			{
				return LogIndexData.Empty;
			}
			else if (version == 1)
			{
				ReadOnlyTrie index = reader.ReadTrie();
				int numBlockBits = reader.ReadInt32();

				LogIndexBlock[] blocks = new LogIndexBlock[reader.ReadInt32()];
				for (int idx = 0; idx < blocks.Length; idx++)
				{
					int lineIndex = (idx > 0) ? (blocks[idx - 1].LineIndex + blocks[idx - 1].LineCount) : 0;
					int lineCount = reader.ReadInt32();
					ReadOnlyMemory<byte> compressedPlainText = reader.ReadVariableLengthBytesWithInt32Length();
					reader.ReadTrie();
					blocks[idx] = new LogIndexBlock(lineIndex, lineCount, null, compressedPlainText);
				}

				return new LogIndexData(index, numBlockBits, blocks);
			}
			else if (version == 2)
			{
				ReadOnlyTrie index = reader.ReadTrie();
				int numBlockBits = reader.ReadInt32();

				LogIndexBlock[] blocks = new LogIndexBlock[reader.ReadInt32()];
				for (int idx = 0; idx < blocks.Length; idx++)
				{
					int lineIndex = reader.ReadInt32();
					int lineCount = reader.ReadInt32();
					ReadOnlyMemory<byte> compressedPlainText = reader.ReadVariableLengthBytesWithInt32Length();
					reader.ReadTrie();
					blocks[idx] = new LogIndexBlock(lineIndex, lineCount, null, compressedPlainText);
				}

				return new LogIndexData(index, numBlockBits, blocks);
			}
			else if (version == 3)
			{
				ReadOnlyTrie index = reader.ReadTrie();
				int numBlockBits = reader.ReadInt32();
				LogIndexBlock[] blocks = reader.ReadVariableLengthArrayWithInt32Length(() => reader.ReadLogIndexBlock());
				for (int idx = 1; idx < blocks.Length; idx++)
				{
					if (blocks[idx].LineIndex == 0)
					{
						int lineIndex = blocks[idx - 1].LineIndex + blocks[idx - 1].LineCount;
						blocks[idx] = new LogIndexBlock(lineIndex, blocks[idx].LineCount, blocks[idx].CachedPlainText, blocks[idx].CompressedPlainText);
					}
				}
				return new LogIndexData(index, numBlockBits, blocks);
			}
			else
			{
				throw new InvalidDataException($"Invalid index version number {version}");
			}
		}

		/// <summary>
		/// Serialize an index into memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		public void Write(MemoryWriter writer)
		{
			writer.WriteInt32(3);
			writer.WriteTrie(BuildTrie());
			writer.WriteInt32(_numBlockBits);
			writer.WriteVariableLengthArrayWithInt32Length(_blocks, x => writer.WriteLogIndexBlock(x));
		}

		/// <summary>
		/// Deserialize the index from memory
		/// </summary>
		/// <param name="memory">Memory to deserialize from</param>
		/// <returns>Index data</returns>
		public static LogIndexData FromMemory(ReadOnlyMemory<byte> memory)
		{
			MemoryReader reader = new MemoryReader(memory);
			return Read(reader);
		}

		/// <summary>
		/// Serializes the index
		/// </summary>
		/// <returns>Index data</returns>
		public byte[] ToByteArray()
		{
			byte[] buffer = new byte[GetSerializedSize()];

			MemoryWriter writer = new MemoryWriter(buffer);
			Write(writer);
			writer.CheckEmpty();

			return buffer;
		}

		/// <summary>
		/// Gets the serialized size of this index
		/// </summary>
		/// <returns>The serialized size</returns>
		public int GetSerializedSize()
		{
			return sizeof(int) + BuildTrie().GetSerializedSize() + sizeof(int) + (sizeof(int) + _blocks.Sum(x => x.GetSerializedSize()));
		}
	}

	static class LogIndexDataExtensions
	{
		/// <summary>
		/// Deserialize the index from memory
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>Index data</returns>
		public static LogIndexData ReadLogIndexData(this MemoryReader reader)
		{
			return LogIndexData.Read(reader);
		}

		/// <summary>
		/// Serialize an index into memory
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="index">The index to write</param>
		public static void WriteLogIndexData(this MemoryWriter writer, LogIndexData index)
		{
			index.Write(writer);
		}
	}
}
