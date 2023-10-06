// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// Representation of a data stream, split into chunks along content-aware boundaries using a rolling hash (<see cref="BuzHash"/>).
	/// Chunks are pushed into a tree hierarchy as data is appended to the root, with nodes of the tree also split along content-aware boundaries with <see cref="IoHash.NumBytes"/> granularity.
	/// Once a chunk has been written to storage, it is treated as immutable.
	/// </summary>
	public abstract class ChunkedDataNode : Node
	{
		/// <summary>
		/// Copies the contents of this node and its children to the given output stream
		/// </summary>
		/// <param name="outputStream">The output stream to receive the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public abstract Task CopyToStreamAsync(Stream outputStream, CancellationToken cancellationToken);

		static readonly Guid s_leafNodeGuid = Node.GetNodeType<LeafChunkedDataNode>().Guid;
		static readonly Guid s_interiorNodeGuid = Node.GetNodeType<InteriorChunkedDataNode>().Guid;

		/// <summary>
		/// Copy the contents of the node to the output stream without creating the intermediate FileNodes
		/// </summary>
		/// <param name="handle">Handle to the data to read</param>
		/// <param name="outputStream">The output stream to receive the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task CopyToStreamAsync(BlobHandle handle, Stream outputStream, CancellationToken cancellationToken)
		{
			BlobData nodeData = await handle.ReadAsync(cancellationToken);
			if (nodeData.Type.Guid == s_leafNodeGuid)
			{
				await LeafChunkedDataNode.CopyToStreamAsync(nodeData, outputStream, cancellationToken);
			}
			else if (nodeData.Type.Guid == s_interiorNodeGuid)
			{
				await InteriorChunkedDataNode.CopyToStreamAsync(nodeData, outputStream, cancellationToken);
			}
			else
			{
				throw new ArgumentException($"Unexpected {nameof(ChunkedDataNode)} type found.");
			}
		}

		/// <summary>
		/// Extracts the contents of this node to a file
		/// </summary>
		/// <param name="file">File to write with the contents of this node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task CopyToFileAsync(FileInfo file, CancellationToken cancellationToken)
		{
			if(file.Exists && (file.Attributes & FileAttributes.ReadOnly) != 0)
			{
				file.Attributes &= ~FileAttributes.ReadOnly;
			}
			using (FileStream stream = file.Open(FileMode.Create, FileAccess.Write, FileShare.Read))
			{
				await CopyToStreamAsync(stream, cancellationToken);
			}
		}

		/// <summary>
		/// Serialize this node and its children into a byte array
		/// </summary>
		/// <param name="cancellationToken"></param>
		/// <returns>Array of data stored by the tree</returns>
		public async Task<byte[]> ToByteArrayAsync(CancellationToken cancellationToken)
		{
			using (MemoryStream stream = new MemoryStream())
			{
				await CopyToStreamAsync(stream, cancellationToken);
				return stream.ToArray();
			}
		}
	}

	/// <summary>
	/// File node that contains a chunk of data
	/// </summary>
	[NodeType("{B27AFB68-9E20-4A4B-A4D8-788A4098D439}", 1)]
	public sealed class LeafChunkedDataNode : ChunkedDataNode
	{
		/// <summary>
		/// Data for this node
		/// </summary>
		public ReadOnlyMemory<byte> Data { get; }

		/// <summary>
		/// Create an empty leaf node
		/// </summary>
		public LeafChunkedDataNode()
		{
		}

		/// <summary>
		/// Create a leaf node from the given serialized data
		/// </summary>
		public LeafChunkedDataNode(NodeReader reader)
		{
			// Keep this code in sync with CopyToStreamAsync
			Data = reader.ReadFixedLengthBytes(reader.Length);
		}

		/// <inheritdoc/>
		public override void Serialize(NodeWriter writer)
		{
			writer.WriteFixedLengthBytes(Data.Span);
		}

		/// <inheritdoc/>
		public override async Task CopyToStreamAsync(Stream outputStream, CancellationToken cancellationToken)
		{
			await outputStream.WriteAsync(Data, cancellationToken);
		}

		/// <summary>
		/// Copy the contents of the node to the output stream without creating the intermediate FileNodes
		/// </summary>
		/// <param name="nodeData">The raw node data</param>
		/// <param name="outputStream">The output stream to receive the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task CopyToStreamAsync(BlobData nodeData, Stream outputStream, CancellationToken cancellationToken)
		{
			// Keep this code in sync with the constructor
			await outputStream.WriteAsync(nodeData.Data, cancellationToken);
		}

		/// <summary>
		/// Creates nodes from the given file
		/// </summary>
		/// <param name="writer"></param>
		/// <param name="file"></param>
		/// <param name="options"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async Task<List<NodeRef<ChunkedDataNode>>> CreateFromFileAsync(IStorageWriter writer, FileReference file, LeafChunkedDataNodeOptions options, CancellationToken cancellationToken)
		{
			using (FileStream stream = FileReference.Open(file, FileMode.Open, FileAccess.Read, FileShare.Read))
			{
				return await CreateFromStreamAsync(writer, stream, options, cancellationToken);
			}
		}

		/// <summary>
		/// Creates nodes from the given file
		/// </summary>
		/// <param name="writer"></param>
		/// <param name="file"></param>
		/// <param name="options"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async Task<List<NodeRef<ChunkedDataNode>>> CreateFromFileAsync(IStorageWriter writer, FileInfo file, LeafChunkedDataNodeOptions options, CancellationToken cancellationToken)
		{
			using (FileStream stream = file.Open(FileMode.Open, FileAccess.Read, FileShare.Read))
			{
				return await CreateFromStreamAsync(writer, stream, options, cancellationToken);
			}
		}

		/// <summary>
		/// Creates nodes from the given file
		/// </summary>
		/// <param name="writer"></param>
		/// <param name="stream"></param>
		/// <param name="options"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async Task<List<NodeRef<ChunkedDataNode>>> CreateFromStreamAsync(IStorageWriter writer, Stream stream, LeafChunkedDataNodeOptions options, CancellationToken cancellationToken)
		{
			List<NodeRef<ChunkedDataNode>> handles = new List<NodeRef<ChunkedDataNode>>();

			using IMemoryOwner<byte> readBuffer = MemoryPool<byte>.Shared.Rent(options.MaxSize);

			int size = 0;
			for (; ; )
			{
				size += await stream.ReadGreedyAsync(readBuffer.Memory.Slice(size), cancellationToken);
				if (size == 0)
				{
					break;
				}

				int nextLength = GetChunkLength(readBuffer.Memory.Span.Slice(0, size), options);

				Memory<byte> outputBuffer = writer.GetOutputBuffer(0, nextLength);
				readBuffer.Memory.Slice(0, nextLength).CopyTo(outputBuffer);

				BlobHandle handle = await writer.WriteNodeAsync(nextLength, Array.Empty<BlobHandle>(), GetNodeType<LeafChunkedDataNode>(), cancellationToken);
				handles.Add(new NodeRef<ChunkedDataNode>(handle));

				readBuffer.Memory.Slice(nextLength, size - nextLength).CopyTo(readBuffer.Memory);
				size -= nextLength;
			}

			return handles;
		}

		/// <summary>
		/// Determines how much data to append to an existing leaf node
		/// </summary>
		/// <param name="inputData">Data to be appended</param>
		/// <param name="options">Options for chunking the data</param>
		/// <returns>The number of bytes to append</returns>
		public static int GetChunkLength(ReadOnlySpan<byte> inputData, LeafChunkedDataNodeOptions options)
		{
			// If the target option sizes are fixed, just chunk the data along fixed boundaries
			if (options.MinSize == options.TargetSize && options.MaxSize == options.TargetSize)
			{
				return Math.Min(inputData.Length, options.MaxSize);
			}

			// Cap the append data span to the maximum amount we can add
			int maxLength = options.MaxSize;
			if (maxLength < inputData.Length)
			{
				inputData = inputData.Slice(0, maxLength);
			}

			int windowSize = options.MinSize;

			// Fast path for appending data to the buffer up to the chunk window size
			int length = Math.Min(windowSize, inputData.Length);
			uint rollingHash = BuzHash.Add(0, inputData.Slice(0, length));

			// Get the threshold for the rolling hash to split the output
			uint rollingHashThreshold = (uint)((1L << 32) / options.TargetSize);

			// Step through the rest of the data which is completely contained in appendData.
			if (length < inputData.Length)
			{
				Debug.Assert(length >= windowSize);

				ReadOnlySpan<byte> tailSpan = inputData.Slice(length - windowSize, inputData.Length - windowSize);
				ReadOnlySpan<byte> headSpan = inputData.Slice(length);

				int count = BuzHash.Update(tailSpan, headSpan, rollingHashThreshold, ref rollingHash);
				if (count != -1)
				{
					length += count;
					return length;
				}

				length += headSpan.Length;
			}

			return length;
		}
	}

	/// <summary>
	/// Options for creating interior nodes
	/// </summary>
	/// <param name="MinChildCount">Minimum number of children in each node</param>
	/// <param name="TargetChildCount">Target number of children in each node</param>
	/// <param name="MaxChildCount">Maximum number of children in each node</param>
	/// <param name="SliceThreshold">Threshold hash value for splitting interior nodes</param>
	public record class InteriorChunkedDataNodeOptions(int MinChildCount, int TargetChildCount, int MaxChildCount, uint SliceThreshold)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public InteriorChunkedDataNodeOptions(int minChildCount, int targetChildCount, int maxChildCount)
			: this(minChildCount, targetChildCount, maxChildCount, (uint)((1UL << 32) / (uint)targetChildCount))
		{
		}
	}

	/// <summary>
	/// An interior file node
	/// </summary>
	[NodeType("{F4DEDDBC-70CB-4C7A-8347-F011AFCCCDB9}", 1)]
	public class InteriorChunkedDataNode : ChunkedDataNode
	{
		/// <summary>
		/// Child nodes
		/// </summary>
		public IReadOnlyList<NodeRef<ChunkedDataNode>> Children { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="children"></param>
		public InteriorChunkedDataNode(IReadOnlyList<NodeRef<ChunkedDataNode>> children)
		{
			Children = children;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public InteriorChunkedDataNode(NodeReader reader)
		{
			// Keep this code in sync with CopyToStreamAsync
			NodeRef<ChunkedDataNode>[] children = new NodeRef<ChunkedDataNode>[reader.Length / IoHash.NumBytes];
			for (int idx = 0; idx < children.Length; idx++)
			{
				children[idx] = reader.ReadNodeRef<ChunkedDataNode>();
			}
			Children = children;
		}

		/// <inheritdoc/>
		public override void Serialize(NodeWriter writer)
		{
			foreach (NodeRef<ChunkedDataNode> child in Children)
			{
				writer.WriteNodeRef(child);
			}
		}

		/// <summary>
		/// Create a tree of nodes from the given list of handles, splitting nodes in each layer based on the hash of the last node.
		/// </summary>
		/// <param name="handles">List of leaf handles</param>
		/// <param name="options">Options for splitting the tree</param>
		/// <param name="writer">Output writer for new interior nodes</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the root node of the tree</returns>
		public static async Task<NodeRef<ChunkedDataNode>> CreateTreeAsync(List<NodeRef<ChunkedDataNode>> handles, InteriorChunkedDataNodeOptions options, IStorageWriter writer, CancellationToken cancellationToken)
		{
			List<NodeRef<ChunkedDataNode>> handleBuffer = new List<NodeRef<ChunkedDataNode>>();

			List<InteriorChunkedDataNode> interiorNodes = new List<InteriorChunkedDataNode>();
			while (handles.Count > 1)
			{
				interiorNodes.Clear();
				CreateTreeLayer(handles, options, interiorNodes);

				handleBuffer.Clear();
				foreach (InteriorChunkedDataNode interiorNode in interiorNodes)
				{
					NodeRef<ChunkedDataNode> handle = await writer.WriteNodeAsync<ChunkedDataNode>(interiorNode, cancellationToken);
					handleBuffer.Add(handle);
				}

				handles = handleBuffer;
			}

			return handles[0];
		}
		
		/// <summary>
		/// Split a list of handles into a layer of interior nodes
		/// </summary>
		static void CreateTreeLayer(List<NodeRef<ChunkedDataNode>> handles, InteriorChunkedDataNodeOptions options, List<InteriorChunkedDataNode> interiorNodes)
		{
			Span<byte> buffer = stackalloc byte[IoHash.NumBytes];

			for (int index = 0; index < handles.Count; )
			{
				int minIndex = index;
				int maxIndex = Math.Min(minIndex + options.MaxChildCount, handles.Count);

				index = Math.Min(index + options.MinChildCount, handles.Count);
				for (; index < maxIndex; index++)
				{
					handles[index].Handle.Hash.CopyTo(buffer);

					uint value = BinaryPrimitives.ReadUInt32LittleEndian(buffer);
					if (value < options.SliceThreshold)
					{
						break;
					}
				}

				NodeRef<ChunkedDataNode>[] children = new NodeRef<ChunkedDataNode>[index - minIndex];
				for (int childIndex = minIndex; childIndex < index; childIndex++)
				{
					children[childIndex - minIndex] = new NodeRef<ChunkedDataNode>(handles[childIndex]);
				}

				interiorNodes.Add(new InteriorChunkedDataNode(children));
			}
		}

		/// <inheritdoc/>
		public override async Task CopyToStreamAsync(Stream outputStream, CancellationToken cancellationToken)
		{
			foreach (NodeRef<ChunkedDataNode> childNodeRef in Children)
			{
				ChunkedDataNode childNode = await childNodeRef.ExpandAsync(cancellationToken);
				await childNode.CopyToStreamAsync(outputStream, cancellationToken);
			}
		}

		/// <summary>
		/// Copy the contents of the node to the output stream without creating the intermediate FileNodes
		/// </summary>
		/// <param name="nodeData">Source data</param>
		/// <param name="outputStream">The output stream to receive the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task CopyToStreamAsync(BlobData nodeData, Stream outputStream, CancellationToken cancellationToken)
		{
			NodeReader nodeReader = new NodeReader(nodeData);
			while (nodeReader.GetMemory(0).Length > 0)
			{
				NodeRef nodeRef = nodeReader.ReadNodeRef();
				await ChunkedDataNode.CopyToStreamAsync(nodeRef.Handle!, outputStream, cancellationToken);
			}
		}
	}
}
