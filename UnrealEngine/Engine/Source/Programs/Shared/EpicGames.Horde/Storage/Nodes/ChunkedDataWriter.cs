// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Options for creating file nodes
	/// </summary>
	public class ChunkingOptions
	{
		/// <summary>
		/// Options for creating leaf nodes
		/// </summary>
		public LeafChunkedDataNodeOptions LeafOptions { get; set; }

		/// <summary>
		/// Options for creating interior nodes
		/// </summary>
		public InteriorChunkedDataNodeOptions InteriorOptions { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public ChunkingOptions()
		{
			LeafOptions = new LeafChunkedDataNodeOptions(32 * 1024, 256 * 1024, 64 * 1024);
			InteriorOptions = new InteriorChunkedDataNodeOptions(1, 5, 10);
		}
	}

	/// <summary>
	/// Options for creating a specific type of file nodes
	/// </summary>
	/// <param name="MinSize">Minimum chunk size</param>
	/// <param name="MaxSize">Maximum chunk size. Chunks will be split on this boundary if another match is not found.</param>
	/// <param name="TargetSize">Target chunk size for content-slicing</param>
	public record class LeafChunkedDataNodeOptions(int MinSize, int MaxSize, int TargetSize)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="size">Fixed size chunks to use</param>
		public LeafChunkedDataNodeOptions(int size)
			: this(size, size, size)
		{
		}
	}

	/// <summary>
	/// Utility class for generating FileNode data directly into <see cref="BundleWriter"/> instances, without constructing node representations first.
	/// </summary>
	public class ChunkedDataWriter
	{
		/// <summary>
		/// Default buffer length when calling CreateAsync/AppendAsync
		/// </summary>
		public const int DefaultBufferLength = 32 * 1024;

		static readonly BlobType s_leafNodeType = Node.GetNodeType<LeafChunkedDataNode>();

		readonly IStorageWriter _writer;
		readonly ChunkingOptions _options;

		// Tree state
		long _totalLength;
		readonly List<NodeRef<ChunkedDataNode>> _leafHandles = new List<NodeRef<ChunkedDataNode>>();

		// Leaf node state
		uint _leafHash;
		int _leafLength;

		/// <summary>
		/// Length of the file so far
		/// </summary>
		public long Length => _totalLength;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="writer">Writer for new nodes</param>
		/// <param name="options">Chunking options</param>
		public ChunkedDataWriter(IStorageWriter writer, ChunkingOptions options)
		{
			_writer = writer;
			_options = options;
		}

		/// <summary>
		/// Reset the current state
		/// </summary>
		public void Reset()
		{
			_leafHandles.Clear();
			ResetLeafState();
			_totalLength = 0;
		}

		/// <summary>
		/// Resets the state of the current leaf node
		/// </summary>
		void ResetLeafState()
		{
			_leafHash = 0;
			_leafLength = 0;
		}

		/// <summary>
		/// Creates data for the given file
		/// </summary>
		/// <param name="fileInfo">File to append</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<NodeRef<ChunkedDataNode>> CreateAsync(FileInfo fileInfo, CancellationToken cancellationToken)
		{
			return await CreateAsync(fileInfo, DefaultBufferLength, cancellationToken);
		}

		/// <summary>
		/// Creates data for the given file
		/// </summary>
		/// <param name="fileInfo">File to append</param>
		/// <param name="bufferLength">Size of the read buffer</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<NodeRef<ChunkedDataNode>> CreateAsync(FileInfo fileInfo, int bufferLength, CancellationToken cancellationToken)
		{
			using (FileStream stream = fileInfo.OpenRead())
			{
				return await CreateAsync(stream, bufferLength, cancellationToken);
			}
		}

		/// <summary>
		/// Creates data from the given stream
		/// </summary>
		/// <param name="stream">Stream to append</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<NodeRef<ChunkedDataNode>> CreateAsync(Stream stream, CancellationToken cancellationToken)
		{
			return await CreateAsync(stream, DefaultBufferLength, cancellationToken);
		}

		/// <summary>
		/// Creates data from the given stream
		/// </summary>
		/// <param name="stream">Stream to append</param>
		/// <param name="bufferLength">Size of the read buffer</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<NodeRef<ChunkedDataNode>> CreateAsync(Stream stream, int bufferLength, CancellationToken cancellationToken)
		{
			Reset();
			await AppendAsync(stream, bufferLength, cancellationToken);
			return await CompleteAsync(cancellationToken);
		}

		/// <summary>
		/// Creates data from the given data
		/// </summary>
		/// <param name="data">Stream to append</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<NodeRef<ChunkedDataNode>> CreateAsync(ReadOnlyMemory<byte> data, CancellationToken cancellationToken)
		{
			Reset();
			await AppendAsync(data, cancellationToken);
			return await FlushAsync(cancellationToken);
		}

		/// <summary>
		/// Appends data to the current file
		/// </summary>
		/// <param name="stream">Stream containing data to append</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task AppendAsync(Stream stream, CancellationToken cancellationToken)
		{
			await AppendAsync(stream, DefaultBufferLength, cancellationToken);
		}

		/// <summary>
		/// Appends data to the current file
		/// </summary>
		/// <param name="stream">Stream containing data to append</param>
		/// <param name="bufferLength">Size of the read buffer</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task AppendAsync(Stream stream, int bufferLength, CancellationToken cancellationToken)
		{
			await stream.ReadAllBytesAsync(bufferLength, async (x) => await AppendAsync(x, cancellationToken), cancellationToken);
		}

		/// <summary>
		/// Appends data to the current file
		/// </summary>
		/// <param name="data">Data to append</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task AppendAsync(ReadOnlyMemory<byte> data, CancellationToken cancellationToken)
		{
			Memory<byte> buffer = _writer.GetOutputBuffer(_leafLength, _leafLength);
			for (; ; )
			{
				int appendLength = AppendToLeafNode(buffer.Span.Slice(0, _leafLength), data.Span, ref _leafHash, _options.LeafOptions);

				buffer = _writer.GetOutputBuffer(_leafLength, _leafLength + appendLength);
				data.Slice(0, appendLength).CopyTo(buffer.Slice(_leafLength));

				_leafLength += appendLength;
				data = data.Slice(appendLength);

				_totalLength += appendLength;

				if (data.Length == 0)
				{
					break;
				}

				await FlushLeafNodeAsync(cancellationToken);
			}
		}

		/// <summary>
		/// Determines how much data to append to an existing leaf node
		/// </summary>
		/// <param name="currentData">Current data in the leaf node</param>
		/// <param name="appendData">Data to be appended</param>
		/// <param name="rollingHash">Current BuzHash of the data</param>
		/// <param name="options">Options for chunking the data</param>
		/// <returns>The number of bytes to append</returns>
		static int AppendToLeafNode(ReadOnlySpan<byte> currentData, ReadOnlySpan<byte> appendData, ref uint rollingHash, LeafChunkedDataNodeOptions options)
		{
			// If the target option sizes are fixed, just chunk the data along fixed boundaries
			if (options.MinSize == options.TargetSize && options.MaxSize == options.TargetSize)
			{
				return Math.Min(appendData.Length, options.MaxSize - (int)currentData.Length);
			}

			// Cap the append data span to the maximum amount we can add
			int maxAppendLength = options.MaxSize - currentData.Length;
			if (maxAppendLength < appendData.Length)
			{
				appendData = appendData.Slice(0, maxAppendLength);
			}

			// Length of the data to be appended
			int appendLength = 0;

			// Fast path for appending data to the buffer up to the chunk window size
			int windowSize = options.MinSize;
			if (currentData.Length < windowSize)
			{
				appendLength = Math.Min(windowSize - (int)currentData.Length, appendData.Length);
				rollingHash = BuzHash.Add(rollingHash, appendData.Slice(0, appendLength));
			}

			// Get the threshold for the rolling hash
			uint rollingHashThreshold = (uint)((1L << 32) / options.TargetSize);

			// Step through the part of the data where the tail of the window is in currentData, and the head of the window is in appendData.
			if (appendLength < appendData.Length && windowSize > appendLength)
			{
				int overlap = windowSize - appendLength;
				int overlapLength = Math.Min(appendData.Length - appendLength, overlap);

				ReadOnlySpan<byte> tailSpan = currentData.Slice(currentData.Length - overlap, overlapLength);
				ReadOnlySpan<byte> headSpan = appendData.Slice(appendLength, overlapLength);

				int count = BuzHash.Update(tailSpan, headSpan, rollingHashThreshold, ref rollingHash);
				if (count != -1)
				{
					appendLength += count;
					return appendLength;
				}

				appendLength += headSpan.Length;
			}

			// Step through the rest of the data which is completely contained in appendData.
			if (appendLength < appendData.Length)
			{
				Debug.Assert(appendLength >= windowSize);

				ReadOnlySpan<byte> tailSpan = appendData.Slice(appendLength - windowSize, appendData.Length - windowSize);
				ReadOnlySpan<byte> headSpan = appendData.Slice(appendLength);

				int count = BuzHash.Update(tailSpan, headSpan, rollingHashThreshold, ref rollingHash);
				if (count != -1)
				{
					appendLength += count;
					return appendLength;
				}

				appendLength += headSpan.Length;
			}

			return appendLength;
		}

		/// <summary>
		/// Complete the current file, and write all open nodes to the underlying writer
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the root node</returns>
		public async Task<NodeRef<ChunkedDataNode>> CompleteAsync(CancellationToken cancellationToken)
		{
			await FlushLeafNodeAsync(cancellationToken);
			NodeRef<ChunkedDataNode> rootHandle = await InteriorChunkedDataNode.CreateTreeAsync(_leafHandles, _options.InteriorOptions, _writer, cancellationToken);
			return rootHandle;
		}

		/// <summary>
		/// Flush the state of the writer
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the root FileNode</returns>
		public async Task<NodeRef<ChunkedDataNode>> FlushAsync(CancellationToken cancellationToken)
		{
			NodeRef<ChunkedDataNode> handle = await CompleteAsync(cancellationToken);
			await _writer.FlushAsync(cancellationToken);
			return handle;
		}

		/// <summary>
		/// Writes the contents of the current leaf node to storage
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written leaf node</returns>
		async ValueTask FlushLeafNodeAsync(CancellationToken cancellationToken)
		{
			BlobHandle handle = await _writer.WriteNodeAsync(_leafLength, Array.Empty<BlobHandle>(), s_leafNodeType, cancellationToken);
			_leafHandles.Add(new NodeRef<ChunkedDataNode>(handle));
			ResetLeafState();
		}
	}
}
