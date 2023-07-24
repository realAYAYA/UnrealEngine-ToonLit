// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Serialization;
using Microsoft.Extensions.Options;
using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Reflection.Metadata;
using System.Threading;
using System.Threading.Tasks;
using System.Xml.Linq;

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
		public ChunkingOptionsForNodeType LeafOptions { get; set; }

		/// <summary>
		/// Options for creating interior nodes
		/// </summary>
		public ChunkingOptionsForNodeType InteriorOptions { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public ChunkingOptions()
		{
			LeafOptions = new ChunkingOptionsForNodeType(32 * 1024, 256 * 1024, 64 * 1024);
			InteriorOptions = new ChunkingOptionsForNodeType(32 * 1024, 256 * 1024, 64 * 1024);
		}
	}

	/// <summary>
	/// Options for creating a specific type of file nodes
	/// </summary>
	public class ChunkingOptionsForNodeType
	{
		/// <summary>
		/// Minimum chunk size
		/// </summary>
		public int MinSize { get; set; }

		/// <summary>
		/// Maximum chunk size. Chunks will be split on this boundary if another match is not found.
		/// </summary>
		public int MaxSize { get; set; }

		/// <summary>
		/// Target chunk size for content-slicing
		/// </summary>
		public int TargetSize { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="size">Fixed size chunks to use</param>
		public ChunkingOptionsForNodeType(int size)
			: this(size, size, size)
		{
		}

		/// <summary>
		/// Default constructor
		/// </summary>
		public ChunkingOptionsForNodeType(int minSize, int maxSize, int targetSize)
		{
			MinSize = minSize;
			MaxSize = maxSize;
			TargetSize = targetSize;
		}
	}

	/// <summary>
	/// Representation of a data stream, split into chunks along content-aware boundaries using a rolling hash (<see cref="BuzHash"/>).
	/// Chunks are pushed into a tree hierarchy as data is appended to the root, with nodes of the tree also split along content-aware boundaries with <see cref="IoHash.NumBytes"/> granularity.
	/// Once a chunk has been written to storage, it is treated as immutable.
	/// </summary>
	public abstract class FileNode : TreeNode
	{
		/// <summary>
		/// Copies the contents of this node and its children to the given output stream
		/// </summary>
		/// <param name="reader">Reader for nodes in the tree</param>
		/// <param name="outputStream">The output stream to receive the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public abstract Task CopyToStreamAsync(TreeReader reader, Stream outputStream, CancellationToken cancellationToken);

		/// <summary>
		/// Extracts the contents of this node to a file
		/// </summary>
		/// <param name="reader">Reader for nodes in the tree</param>
		/// <param name="file">File to write with the contents of this node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task CopyToFileAsync(TreeReader reader, FileInfo file, CancellationToken cancellationToken)
		{
			using (FileStream stream = file.OpenWrite())
			{
				await CopyToStreamAsync(reader, stream, cancellationToken);
			}
		}

		/// <summary>
		/// Serialize this node and its children into a byte array
		/// </summary>
		/// <param name="reader">Reader for nodes in the tree</param>
		/// <param name="cancellationToken"></param>
		/// <returns>Array of data stored by the tree</returns>
		public async Task<byte[]> ToByteArrayAsync(TreeReader reader, CancellationToken cancellationToken)
		{
			using (MemoryStream stream = new MemoryStream())
			{
				await CopyToStreamAsync(reader, stream, cancellationToken);
				return stream.ToArray();
			}
		}
	}

	/// <summary>
	/// File node that contains a chunk of data
	/// </summary>
	[TreeNode("{B27AFB68-9E20-4A4B-A4D8-788A4098D439}", 1)]
	public sealed class LeafFileNode : FileNode
	{
		/// <summary>
		/// Data for this node
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Create an empty leaf node
		/// </summary>
		public LeafFileNode()
		{
		}

		/// <summary>
		/// Create a leaf node from the given serialized data
		/// </summary>
		public LeafFileNode(ITreeNodeReader reader)
		{
			Data = reader.ReadFixedLengthBytes(reader.Length);
		}

		/// <inheritdoc/>
		public override void Serialize(ITreeNodeWriter writer)
		{
			writer.WriteFixedLengthBytes(Data.Span);
		}

		/// <inheritdoc/>
		public override IEnumerable<TreeNodeRef> EnumerateRefs() => Enumerable.Empty<TreeNodeRef>();

		/// <summary>
		/// Determines how much data to append to an existing leaf node
		/// </summary>
		/// <param name="currentData">Current data in the leaf node</param>
		/// <param name="appendData">Data to be appended</param>
		/// <param name="rollingHash">Current BuzHash of the data</param>
		/// <param name="options">Options for chunking the data</param>
		/// <returns>The number of bytes to append</returns>
		internal static int AppendData(ReadOnlySpan<byte> currentData, ReadOnlySpan<byte> appendData, ref uint rollingHash, ChunkingOptionsForNodeType options)
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
			if(appendLength < appendData.Length && windowSize > appendLength)
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

		/// <inheritdoc/>
		public override async Task CopyToStreamAsync(TreeReader reader, Stream outputStream, CancellationToken cancellationToken)
		{
			await outputStream.WriteAsync(Data, cancellationToken);
		}
	}

	/// <summary>
	/// An interior file node
	/// </summary>
	[TreeNode("{F4DEDDBC-70CB-4C7A-8347-F011AFCCCDB9}", 1)]
	public class InteriorFileNode : FileNode
	{
		/// <summary>
		/// Child nodes
		/// </summary>
		public IReadOnlyList<TreeNodeRef<FileNode>> Children { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="children"></param>
		public InteriorFileNode(IReadOnlyList<TreeNodeRef<FileNode>> children)
		{
			Children = children;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public InteriorFileNode(ITreeNodeReader reader)
		{
			TreeNodeRef<FileNode>[] children = new TreeNodeRef<FileNode>[reader.Length / IoHash.NumBytes];
			for (int idx = 0; idx < children.Length; idx++)
			{
				children[idx] = reader.ReadRef<FileNode>();
			}
			Children = children;
		}

		/// <inheritdoc/>
		public override void Serialize(ITreeNodeWriter writer)
		{
			foreach (TreeNodeRef<FileNode> child in Children)
			{
				writer.WriteRef(child);
			}
		}

		/// <inheritdoc/>
		public override IEnumerable<TreeNodeRef> EnumerateRefs() => Children;

		/// <summary>
		/// Test whether the current node is complete
		/// </summary>
		/// <param name="currentData"></param>
		/// <param name="rollingHash"></param>
		/// <param name="options"></param>
		/// <returns></returns>
		internal static bool IsComplete(ReadOnlySpan<byte> currentData, uint rollingHash, ChunkingOptionsForNodeType options)
		{
			if (currentData.Length + IoHash.NumBytes > options.MaxSize)
			{
				return true;
			}

			if (currentData.Length >= options.MinSize)
			{
				uint rollingHashThreshold = BuzHash.GetThreshold(options.TargetSize);
				if (rollingHash < rollingHashThreshold)
				{
					return true;
				}
			}

			return false;
		}

		/// <summary>
		/// Append a new hash to this interior node
		/// </summary>
		/// <param name="currentData">Current data for the node</param>
		/// <param name="hash">Hash of the child node</param>
		/// <param name="rollingHash">Current rolling hash for the node</param>
		/// <param name="options">Options for chunking the node</param>
		/// <returns>True if the hash could be appended, false otherwise</returns>
		internal static void AppendData(ReadOnlySpan<byte> currentData, IoHash hash, ref uint rollingHash, ChunkingOptionsForNodeType options)
		{
			Span<byte> hashData = stackalloc byte[IoHash.NumBytes];
			hash.CopyTo(hashData);

			rollingHash = BuzHash.Add(rollingHash, hashData);

			int windowSize = options.MinSize - (options.MinSize % IoHash.NumBytes);
			if (currentData.Length > windowSize)
			{
				ReadOnlySpan<byte> removeData = currentData.Slice(currentData.Length - windowSize, IoHash.NumBytes);
				rollingHash = BuzHash.Sub(rollingHash, removeData, windowSize + IoHash.NumBytes);
			}
		}

		/// <inheritdoc/>
		public override async Task CopyToStreamAsync(TreeReader reader, Stream outputStream, CancellationToken cancellationToken)
		{
			foreach (TreeNodeRef<FileNode> childNodeRef in Children)
			{
				FileNode childNode = await childNodeRef.ExpandAsync(reader, cancellationToken);
				await childNode.CopyToStreamAsync(reader, outputStream, cancellationToken);
			}
		}
	}

	/// <summary>
	/// Utility class for generating FileNode data directly into <see cref="TreeWriter"/> instances, without constructing node representations first.
	/// </summary>
	public class FileNodeWriter
	{
		class InteriorNodeState
		{
			public InteriorNodeState? _parent;
			public readonly ArrayMemoryWriter _data;
			public readonly List<NodeHandle> _children = new List<NodeHandle>();

			public uint _rollingHash;

			public InteriorNodeState(int maxSize)
			{
				_data = new ArrayMemoryWriter(maxSize);
			}

			public void Reset()
			{
				_rollingHash = 0;
				_data.Clear();
				_children.Clear();
			}

			public void Write(NodeHandle handle)
			{
				_children.Add(handle);
				_data.WriteIoHash(handle.Hash);
			}
		}

		static readonly BundleType s_leafNodeType = TreeNodeExtensions.GetBundleType(typeof(LeafFileNode));
		static readonly BundleType s_interiorNodeType = TreeNodeExtensions.GetBundleType(typeof(InteriorFileNode));

		readonly TreeWriter _writer;
		readonly ChunkingOptions _options;

		// Tree state
		InteriorNodeState? _topInteriorNode = null;
		long _totalLength;
		readonly Stack<InteriorNodeState> _freeInteriorNodes = new Stack<InteriorNodeState>();

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
		public FileNodeWriter(TreeWriter writer, ChunkingOptions options)
		{
			_writer = writer;
			_options = options;
		}

		/// <summary>
		/// Reset the current state
		/// </summary>
		public void Reset()
		{
			FreeInteriorNodes();
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
		/// Creates a new interior node state
		/// </summary>
		/// <returns>State object</returns>
		InteriorNodeState CreateInteriorNode()
		{
			InteriorNodeState? result;
			if (!_freeInteriorNodes.TryPop(out result))
			{
				result = new InteriorNodeState(_options.InteriorOptions.MaxSize);
			}
			return result;
		}

		/// <summary>
		/// Free all the current interior nodes
		/// </summary>
		void FreeInteriorNodes()
		{
			while (_topInteriorNode != null)
			{
				InteriorNodeState current = _topInteriorNode;
				_topInteriorNode = _topInteriorNode._parent;
				current.Reset();
				_freeInteriorNodes.Push(current);
			}
		}

		/// <summary>
		/// Creates data for the given file
		/// </summary>
		/// <param name="fileInfo">File to append</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<NodeHandle> CreateAsync(FileInfo fileInfo, CancellationToken cancellationToken)
		{
			using (FileStream stream = fileInfo.OpenRead())
			{
				return await CreateAsync(stream, cancellationToken);
			}
		}

		/// <summary>
		/// Creates data from the given stream
		/// </summary>
		/// <param name="stream">Stream to append</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<NodeHandle> CreateAsync(Stream stream, CancellationToken cancellationToken)
		{
			Reset();
			await AppendAsync(stream, cancellationToken);
			return await FlushAsync(cancellationToken);
		}

		/// <summary>
		/// Creates data from the given data
		/// </summary>
		/// <param name="data">Stream to append</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<NodeHandle> CreateAsync(ReadOnlyMemory<byte> data, CancellationToken cancellationToken)
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
			const int BufferLength = 32 * 1024;

			using IMemoryOwner<byte> owner = MemoryPool<byte>.Shared.Rent(BufferLength * 2);
			Memory<byte> buffer = owner.Memory;

			int readBufferOffset = 0;
			Memory<byte> appendBuffer = Memory<byte>.Empty;
			for (; ; )
			{
				// Start a read into memory
				Memory<byte> readBuffer = buffer.Slice(readBufferOffset, BufferLength);
				Task<int> readTask = Task.Run(async () => await stream.ReadAsync(readBuffer, cancellationToken), cancellationToken);

				// In the meantime, append the last data that was read to the tree
				if (appendBuffer.Length > 0)
				{
					await AppendAsync(appendBuffer, cancellationToken);
				}

				// Wait for the read to finish
				int numBytes = await readTask;
				if (numBytes == 0)
				{
					break;
				}

				// Switch the buffers around
				appendBuffer = readBuffer.Slice(0, numBytes);
				readBufferOffset ^= BufferLength;
			}
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
				// Append data to the current leaf node
				int appendLength = LeafFileNode.AppendData(buffer.Span.Slice(0, _leafLength), data.Span, ref _leafHash, _options.LeafOptions);

				buffer = _writer.GetOutputBuffer(_leafLength, _leafLength + appendLength);
				data.Slice(0, appendLength).CopyTo(buffer.Slice(_leafLength));

				_leafLength += appendLength;
				data = data.Slice(appendLength);

				_totalLength += appendLength;

				if (data.Length == 0)
				{
					break;
				}

				// Flush the leaf node and any interior nodes that are full
				NodeHandle handle = await WriteLeafNodeAsync(cancellationToken);
				ResetLeafState();
				await AddToInteriorNodeAsync(handle, cancellationToken);
			}
		}

		async Task AddToInteriorNodeAsync(NodeHandle handle, CancellationToken cancellationToken)
		{
			_topInteriorNode ??= CreateInteriorNode();
			await AddToInteriorNodeAsync(_topInteriorNode, handle, cancellationToken);
		}

		async Task AddToInteriorNodeAsync(InteriorNodeState interiorNode, NodeHandle handle, CancellationToken cancellationToken)
		{
			// If the node is already full, flush it
			if (InteriorFileNode.IsComplete(interiorNode._data.WrittenSpan, interiorNode._rollingHash, _options.InteriorOptions))
			{
				NodeHandle interiorNodeHandle = await WriteInteriorNodeAndResetAsync(interiorNode, cancellationToken);
				interiorNode._parent ??= CreateInteriorNode();
				await AddToInteriorNodeAsync(interiorNode._parent, interiorNodeHandle, cancellationToken);
			}

			// Add this handle
			InteriorFileNode.AppendData(interiorNode._data.WrittenSpan, handle.Hash, ref interiorNode._rollingHash, _options.InteriorOptions);
			interiorNode.Write(handle);
		}

		/// <summary>
		/// Complete the current file, and write all open nodes to the underlying writer
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the root node</returns>
		public async Task<NodeHandle> CompleteAsync(CancellationToken cancellationToken)
		{
			NodeHandle handle = await WriteLeafNodeAsync(cancellationToken);
			ResetLeafState();

			for (InteriorNodeState? state = _topInteriorNode; state != null; state = state._parent)
			{
				await AddToInteriorNodeAsync(state, handle, cancellationToken);
				handle = await WriteInteriorNodeAndResetAsync(state, cancellationToken);
			}

			FreeInteriorNodes();
			return handle;
		}

		/// <summary>
		/// Flush the state of the writer
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the root FileNode</returns>
		public async Task<NodeHandle> FlushAsync(CancellationToken cancellationToken)
		{
			NodeHandle handle = await CompleteAsync(cancellationToken);
			await _writer.FlushAsync(cancellationToken);
			return handle;
		}

		/// <summary>
		/// Writes the state of the given interior node to storage
		/// </summary>
		/// <param name="state"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		async ValueTask<NodeHandle> WriteInteriorNodeAndResetAsync(InteriorNodeState state, CancellationToken cancellationToken)
		{
			Memory<byte> buffer = _writer.GetOutputBuffer(0, state._data.Length);
			state._data.WrittenMemory.CopyTo(buffer);

			NodeHandle handle = await _writer.WriteNodeAsync(state._data.Length, state._children, s_interiorNodeType, cancellationToken);
			state.Reset();

			return handle;
		}

		/// <summary>
		/// Writes the contents of the current leaf node to storage
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written leaf node</returns>
		async ValueTask<NodeHandle> WriteLeafNodeAsync(CancellationToken cancellationToken)
		{
			return await _writer.WriteNodeAsync(_leafLength, Array.Empty<NodeHandle>(), s_leafNodeType, cancellationToken);
		}
	}
}
