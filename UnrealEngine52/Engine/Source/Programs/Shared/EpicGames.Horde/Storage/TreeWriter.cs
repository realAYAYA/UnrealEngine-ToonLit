// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.CodeAnalysis;
using Microsoft.Extensions.Caching.Memory;
using static EpicGames.Horde.Storage.TreeWriter;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Options for configuring a bundle serializer
	/// </summary>
	public class TreeOptions
	{
		/// <summary>
		/// Maximum payload size fo a blob
		/// </summary>
		public int MaxBlobSize { get; set; } = 10 * 1024 * 1024;

		/// <summary>
		/// Compression format to use
		/// </summary>
		public BundleCompressionFormat CompressionFormat { get; set; } = BundleCompressionFormat.LZ4;

		/// <summary>
		/// Minimum size of a block to be compressed
		/// </summary>
		public int MinCompressionPacketSize { get; set; } = 16 * 1024;

		/// <summary>
		/// Maximum amount of data to store in memory. This includes any background writes as well as bundles being built.
		/// </summary>
		public int MaxInMemoryDataLength { get; set; } = 128 * 1024 * 1024;

		/// <summary>
		/// Number of nodes to cache
		/// </summary>
		public int NodeCacheSize { get; set; } = 1024;
	}

	/// <summary>
	/// Unique identifier for a node
	/// </summary>
	/// <param name="Hash">Hash of the node</param>
	/// <param name="Type">Type of the node</param>
	public record NodeKey(IoHash Hash, BundleType Type);

	/// <summary>
	/// Handle to a node. Can be used to reference nodes that have not been flushed yet.
	/// </summary>
	public class NodeHandle
	{
		/// <summary>
		/// Hash of the target node
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Location of the node in storage
		/// </summary>
		public NodeLocator Locator { get; protected set; }

		/// <summary>
		/// Used to track the bundle that is being written to
		/// </summary>
		internal PendingBundle? PendingBundle { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="hash">Hash of the target node</param>
		/// <param name="locator">Location of the node in storage</param>
		public NodeHandle(IoHash hash, NodeLocator locator)
		{
			Hash = hash;
			Locator = locator;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="hash">Hash of the target node</param>
		/// <param name="locator">Location of the node in storage</param>
		/// <param name="exportIdx">Index of the bundle export</param>
		public NodeHandle(IoHash hash, BlobLocator locator, int exportIdx)
			: this(hash, new NodeLocator(locator, exportIdx))
		{
		}

		/// <summary>
		/// Adds a callback to be executed once the node has been written. Triggers immediately if the node has already been written.
		/// </summary>
		/// <param name="callback">Action to be executed after the write</param>
		internal void AddWriteCallback(WriteCallback callback)
		{
			PendingBundle? pendingBundle = PendingBundle;
			if (pendingBundle == null || !pendingBundle.TryAddWriteCallback(callback))
			{
				Debug.Assert(Locator.IsValid());
				callback.OnWrite();
			}
		}

		/// <summary>
		/// Parse a node handle value from a string
		/// </summary>
		/// <param name="text">Text to parse</param>
		/// <returns>Parsed node handle</returns>
		public static NodeHandle Parse(string text)
		{
			int hashLength = IoHash.NumBytes * 2;
			if (text.Length == hashLength)
			{
				return new NodeHandle(IoHash.Parse(text), default);
			}
			else if (text[hashLength] == '@')
			{
				return new NodeHandle(IoHash.Parse(text.Substring(0, hashLength)), NodeLocator.Parse(text.Substring(hashLength + 1)));
			}
			else
			{
				throw new FormatException("Invalid NodeHandle value");
			}
		}

		/// <inheritdoc/>
		public override string ToString()
		{
			if (Locator.IsValid())
			{
				return $"{Hash}@{Locator}";
			}
			else
			{
				return $"{Hash}";
			}
		}
	}

	/// <summary>
	/// Index of known nodes that can be used for deduplication.
	/// </summary>
	public class NodeCache
	{
		readonly object _lockObject = new object();
		readonly int _maxKeys;
		readonly Queue<NodeKey> _nodeKeys = new Queue<NodeKey>();
		readonly Dictionary<NodeKey, NodeHandle> _nodeKeyToHandle = new Dictionary<NodeKey, NodeHandle>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="maxKeys">Maximum number of node keys to keep in the cache</param>
		public NodeCache(int maxKeys)
		{
			_maxKeys = maxKeys;
			_nodeKeys = new Queue<NodeKey>(maxKeys);
			_nodeKeyToHandle = new Dictionary<NodeKey, NodeHandle>(maxKeys);
		}

		/// <summary>
		/// Adds a new node handle to the cache
		/// </summary>
		/// <param name="key">Unique node key</param>
		/// <param name="handle">Handle to the node</param>
		public void Add(NodeKey key, NodeHandle handle)
		{
			lock (_lockObject)
			{
				AddInternal(key, handle);
			}
		}

		/// <summary>
		/// Adds nodes exported from a bundle to the cache
		/// </summary>
		/// <param name="locator">Locator for the bundle</param>
		/// <param name="header">The bundle header</param>
		public void Add(BlobLocator locator, BundleHeader header)
		{
			lock (_lockObject)
			{
				for (int idx = 0; idx < header.Exports.Count; idx++)
				{
					BundleExport export = header.Exports[idx];

					NodeKey key = new NodeKey(export.Hash, header.Types[export.TypeIdx]);
					NodeLocator node = new NodeLocator(locator, idx);
					NodeHandle handle = new NodeHandle(export.Hash, node);

					AddInternal(key, handle);
				}
			}
		}

		void AddInternal(NodeKey key, NodeHandle handle)
		{
			NodeKey? prevKey;
			if (_nodeKeys.Count == _maxKeys && _nodeKeys.TryDequeue(out prevKey))
			{
				_nodeKeyToHandle.Remove(prevKey);
			}
			_nodeKeyToHandle.TryAdd(key, handle);
		}

		/// <summary>
		/// Find a node within the cache
		/// </summary>
		/// <param name="key">Key to look up in the cache</param>
		/// <param name="handle">Handle for the node</param>
		/// <returns>True if the node was found</returns>
		public bool TryGetNode(NodeKey key, [NotNullWhen(true)] out NodeHandle? handle)
		{
			lock (_lockObject)
			{
				return _nodeKeyToHandle.TryGetValue(key, out handle);
			}
		}
	}

	/// <summary>
	/// Writes nodes of a tree to an <see cref="IStorageClient"/>, packed into bundles. Each <see cref="TreeWriter"/> instance is single threaded,
	/// but multiple instances may be written to in parallel.
	/// </summary>
	public sealed class TreeWriter : IDisposable
	{
		/// <summary>
		/// Object to receive notifications on a node being written
		/// </summary>
		internal abstract class WriteCallback
		{
			internal WriteCallback? _next;

			public abstract void OnWrite();
		}

		// Information about a unique output node. Note that multiple node refs may de-duplicate to the same output node.
		internal class PendingNode : NodeHandle
		{
			public readonly NodeKey Key;
			public readonly int Length;
			public readonly NodeHandle[] Refs;

			public PendingNode(NodeKey key, int length, IReadOnlyList<NodeHandle> refs, PendingBundle pendingBundle)
				: base(key.Hash, default)
			{
				Key = key;
				Length = length;
				Refs = refs.ToArray();
				PendingBundle = pendingBundle;
			}

			public void MarkAsWritten(NodeLocator locator)
			{
				Debug.Assert(!Locator.IsValid());
				Locator = locator;
				PendingBundle = null;
			}
		}

		// Information about a bundle being built. Metadata operations are synchronous, compression/writes are asynchronous.
		internal class PendingBundle : IDisposable
		{
			class WriteCallbackSentinel : WriteCallback
			{
				public override void OnWrite() => throw new NotImplementedException();
			}

			static readonly WriteCallbackSentinel s_callbackSentinel = new WriteCallbackSentinel();

			readonly int _maxPacketSize;
			readonly BundleCompressionFormat _compressionFormat;

			// The current packet being built
			IMemoryOwner<byte>? _currentPacket;

			// Length of the packet that has been written
			int _currentPacketLength;

			// Map of keys to nodes in the queue
			public readonly Dictionary<NodeKey, PendingNode> _nodeKeyToInfo = new Dictionary<NodeKey, PendingNode>();

			// Queue of nodes for the current bundle
			readonly List<PendingNode> _queue = new List<PendingNode>();

			// List of packets in the current bundle
			readonly List<BundlePacket> _packets = new List<BundlePacket>();

			// List of compressed packets
			readonly ChunkedMemoryWriter _encodedPacketWriter;

			// Set of all direct dependencies from this bundle
			readonly HashSet<Task> _dependencies = new HashSet<Task>();

			// Total size of uncompressed data in the current bundle
			public int UncompressedLength { get; private set; }

			// Whether all nodes have been added to a bundle
			public bool IsReadOnly { get; private set; }

			// List of post-write callbacks
			WriteCallback? _callbacks = null;

			// Task used to compress data in the background
			Task _writeTask = Task.CompletedTask;

			// Event which is signalled after the bundle is written to storage
			readonly TaskCompletionSource<bool> _completeEvent = new TaskCompletionSource<bool>();

			// Task signalled after the write is complete
			public Task CompleteTask => _completeEvent.Task;

			public PendingBundle(int maxPacketSize, int maxBlobSize, BundleCompressionFormat compressionFormat)
			{
				_maxPacketSize = maxPacketSize;
				_compressionFormat = compressionFormat;
				_encodedPacketWriter = new ChunkedMemoryWriter(maxBlobSize);
			}

			/// <inheritdoc/>
			public void Dispose()
			{
				if (_currentPacket != null)
				{
					_currentPacket.Dispose();
					_currentPacket = null;
				}
				_encodedPacketWriter.Dispose();
			}

			// Whether this bundle has finished writing
			public bool IsComplete() => CompleteTask.IsCompleted;

			// Whether this bundle can be written
			public bool CanComplete() => !IsReadOnly && _dependencies.Count == 0;

			// Add a dependency onto another bundle
			public void AddDependencyOn(PendingBundle bundle)
			{
				if (bundle != this)
				{
					_dependencies.Add(bundle.CompleteTask);
				}
			}

			// Adds a callback after writing
			public bool TryAddWriteCallback(WriteCallback callback)
			{
				for (; ; )
				{
					WriteCallback? tail = _callbacks;
					if (tail == s_callbackSentinel)
					{
						return false;
					}

					callback._next = tail;

					if (Interlocked.CompareExchange(ref _callbacks, callback, tail) == tail)
					{
						return true;
					}
				}
			}

			// Try to add a ref to an existing output node
			public bool TryAddRefToExistingNode(NodeKey nodeKey, [NotNullWhen(true)] out PendingNode? handle)
			{
				PendingNode? pendingNode;
				if (_nodeKeyToInfo.TryGetValue(nodeKey, out pendingNode))
				{
					handle = pendingNode;
					return true;
				}
				else
				{
					handle = null;
					return false;
				}
			}

			// Starts writing a node to the buffer
			public Memory<byte> GetBuffer(int usedSize, int desiredSize)
			{
				if (_currentPacket == null)
				{
					_currentPacket = MemoryPool<byte>.Shared.Rent(Math.Max(_maxPacketSize, desiredSize));
				}
				else if (_currentPacketLength + desiredSize > _maxPacketSize)
				{
					IMemoryOwner<byte> nextPacket = MemoryPool<byte>.Shared.Rent(Math.Max(_maxPacketSize, desiredSize));
					if (usedSize > 0)
					{
						_currentPacket.Memory.Slice(_currentPacketLength, usedSize).CopyTo(nextPacket.Memory);
					}
					FlushPacket();
					_currentPacket = nextPacket;
					return _currentPacket.Memory.Slice(_currentPacketLength, desiredSize);
				}
				return _currentPacket.Memory.Slice(_currentPacketLength, desiredSize);
			}

			// Finish a node write
			public PendingNode WriteNode(NodeKey nodeKey, int size, IReadOnlyList<NodeHandle> refs)
			{
				_currentPacketLength += size;
				UncompressedLength += size;

				PendingNode pendingNode = new PendingNode(nodeKey, (int)size, refs, this);
				_queue.Add(pendingNode);
				_nodeKeyToInfo.Add(nodeKey, pendingNode);

				return pendingNode;
			}

			// Compresses the current packet and schedule it to be written to storage
			void FlushPacket()
			{
				if (_currentPacket != null)
				{
					IMemoryOwner<byte> currentPacket = _currentPacket;
					int currentPacketLength = _currentPacketLength;

					if (currentPacketLength > 0)
					{
						_writeTask = _writeTask.ContinueWith(x => CompressPacket(currentPacket, currentPacketLength), TaskScheduler.Default);
					}
					else
					{
						currentPacket.Dispose();
					}

					_currentPacket = null;
					_currentPacketLength = 0;
				}
			}

			void CompressPacket(IMemoryOwner<byte> packetData, int length)
			{
				int encodedLength = BundleData.Compress(_compressionFormat, packetData.Memory.Slice(0, length), _encodedPacketWriter);

				BundlePacket packet = new BundlePacket(encodedLength, length);
				_packets.Add(packet);

				packetData.Dispose();
			}

			// Mark the bundle as complete
			public void MarkAsComplete(IStorageClient store, Utf8String prefix)
			{
				if (!IsReadOnly)
				{
					FlushPacket();
					Task prevWriteTask = _writeTask;
					_writeTask = Task.Run(() => CompleteAsync(prevWriteTask, store, prefix));
					IsReadOnly = true;
				}
			}

			async Task CompleteAsync(Task prevWriteTask, IStorageClient store, Utf8String prefix)
			{
				try
				{
					await prevWriteTask;
					await Task.WhenAll(_dependencies);

					Bundle bundle = CreateBundle();
					BlobLocator locator = await store.WriteBundleAsync(bundle, prefix);

					for (int idx = 0; idx < _queue.Count; idx++)
					{
						NodeLocator nodeLocator = new NodeLocator(locator, idx);
						_queue[idx].MarkAsWritten(nodeLocator);
					}

					WriteCallback? callback = Interlocked.Exchange(ref _callbacks, s_callbackSentinel);
					while (callback != null)
					{
						callback.OnWrite();
						callback = callback._next;
					}

					_completeEvent.SetResult(true);
				}
				catch (Exception ex)
				{
					_completeEvent.SetException(ex);
				}
			}

			// Flush the current write state
			public Task FlushAsync()
			{
				FlushPacket();
				return _writeTask;
			}

			// Mark the bundle as complete and create a bundle with the current state
			public Bundle CreateBundle()
			{
				// Create a set from the nodes to be written. We use this to determine references that are imported.
				HashSet<NodeHandle> nodeSet = new HashSet<NodeHandle>(_queue);

				// Find all the imported nodes by bundle
				Dictionary<BlobLocator, List<(int, NodeHandle)>> bundleToImports = new Dictionary<BlobLocator, List<(int, NodeHandle)>>();
				foreach (PendingNode nodeInfo in _queue)
				{
					foreach (NodeHandle handle in nodeInfo.Refs)
					{
						if (nodeSet.Add(handle))
						{
							Debug.Assert(handle.Locator.IsValid());
							NodeLocator refLocator = handle.Locator;

							List<(int, NodeHandle)>? importedNodes;
							if (!bundleToImports.TryGetValue(refLocator.Blob, out importedNodes))
							{
								importedNodes = new List<(int, NodeHandle)>();
								bundleToImports.Add(refLocator.Blob, importedNodes);
							}

							importedNodes.Add((refLocator.ExportIdx, handle));
						}
					}
				}

				// Map from node hash to index, with imported nodes first, ordered by blob, and exported nodes second.
				int nodeIdx = 0;
				Dictionary<NodeHandle, int> nodeToIndex = new Dictionary<NodeHandle, int>();

				// Add all the imports and assign them identifiers
				List<BundleImport> imports = new List<BundleImport>();
				foreach ((BlobLocator blobLocator, List<(int, NodeHandle)> importEntries) in bundleToImports)
				{
					importEntries.SortBy(x => x.Item1);

					int[] entries = new int[importEntries.Count];
					for (int idx = 0; idx < importEntries.Count; idx++)
					{
						NodeHandle key = importEntries[idx].Item2;
						nodeToIndex.Add(key, nodeIdx++);
						entries[idx] = importEntries[idx].Item1;
					}

					imports.Add(new BundleImport(blobLocator, entries));
				}

				// List of types in the bundle
				List<BundleType> types = new List<BundleType>();
				Dictionary<BundleType, int> typeToIndex = new Dictionary<BundleType, int>();

				// Create the export list
				List<BundleExport> exports = new List<BundleExport>(_queue.Count);
				foreach (PendingNode nodeInfo in _queue)
				{
					int typeIdx;
					if (!typeToIndex.TryGetValue(nodeInfo.Key.Type, out typeIdx))
					{
						typeIdx = types.Count;
						typeToIndex.Add(nodeInfo.Key.Type, typeIdx);
						types.Add(nodeInfo.Key.Type);
					}

					int[] references = nodeInfo.Refs.Select(x => nodeToIndex[x]).ToArray();
					BundleExport export = new BundleExport(typeIdx, nodeInfo.Key.Hash, nodeInfo.Length, references);
					nodeToIndex[nodeInfo] = nodeIdx;
					exports.Add(export);
					nodeIdx++;
				}

				// Get the memory for each packet
				List<ReadOnlyMemory<byte>> packetData = new List<ReadOnlyMemory<byte>>(_packets.Count);
				foreach (ReadOnlyMemory<byte> segment in _encodedPacketWriter.AsSequence())
				{
					int offset = 0;
					while(packetData.Count < _packets.Count)
					{
						BundlePacket packet = _packets[packetData.Count];
						int next = offset + packet.EncodedLength;

						if (next > segment.Length)
						{
							break;
						}
						packetData.Add(segment.Slice(offset, packet.EncodedLength));
						offset = next;
					}
				}

				// Create the bundle
				BundleHeader header = new BundleHeader(_compressionFormat, types, imports, exports, _packets.ToArray());
				return new Bundle(header, packetData);
			}
		}

		static readonly TreeOptions s_defaultOptions = new TreeOptions();

		readonly IStorageClient _store;
		readonly TreeOptions _options;
		readonly Utf8String _prefix;

		readonly NodeCache _nodeCache;
		readonly Queue<PendingBundle> _writeQueue = new Queue<PendingBundle>();

		PendingBundle? _currentBundle;
		int _memoryFootprint;

		/// <summary>
		/// Accessor for the store backing this writer
		/// </summary>
		public IStorageClient Store => _store;

		/// <summary>
		/// Cache of nodes to deduplicate against
		/// </summary>
		public NodeCache NodeCache => _nodeCache;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="store">Store to write data to</param>
		/// <param name="options">Options for the writer</param>
		/// <param name="prefix">Prefix for blobs written to the store</param>
		/// <param name="nodeCache">Cache of nodes for deduplication</param>
		public TreeWriter(IStorageClient store, TreeOptions? options = null, Utf8String prefix = default, NodeCache? nodeCache = null)
		{
			_store = store;
			_options = options ?? s_defaultOptions;
			_prefix = prefix;
			_nodeCache = nodeCache ?? new NodeCache(_options.NodeCacheSize);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="store">Store to write data to</param>
		/// <param name="refName">Ref being written. Will be used as a prefix for storing blobs.</param>
		/// <param name="options">Options for the writer</param>
		public TreeWriter(IStorageClient store, RefName refName, TreeOptions? options = null)
			: this(store, options, refName.Text)
		{
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="other"></param>
		public TreeWriter(TreeWriter other)
			: this(other._store, other._options, other._prefix, other._nodeCache)
		{
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			if (_currentBundle != null)
			{
				_currentBundle.Dispose();
				_currentBundle = null;
			}
		}

		/// <summary>
		/// Mark this writer as complete, allowing its data to be serialized.
		/// </summary>
		public void Complete()
		{
			if (_currentBundle != null)
			{
				_currentBundle.MarkAsComplete(_store, _prefix);
				_writeQueue.Enqueue(_currentBundle);
				_currentBundle = null;
			}
		}

		/// <summary>
		/// Gets an output buffer for writing.
		/// </summary>
		/// <param name="usedSize">Current size in the existing buffer that has been written to</param>
		/// <param name="desiredSize">Desired size of the returned buffer</param>
		/// <returns>Buffer to be written into.</returns>
		public Memory<byte> GetOutputBuffer(int usedSize, int desiredSize)
		{
			// If the bundle is full, start the process of writing it to disk
			if (_currentBundle != null && _currentBundle.UncompressedLength > 0 && _currentBundle.UncompressedLength + desiredSize > _options.MaxBlobSize)
			{
				Complete();
			}

			// If we don't yet have a bundle in this writer, create one
			PendingBundle currentBundle = GetCurrentBundle();
			return currentBundle.GetBuffer(usedSize, desiredSize);
		}

		/// <summary>
		/// Finish writing a node.
		/// </summary>
		/// <param name="size">Used size of the buffer</param>
		/// <param name="references">References to other nodes</param>
		/// <param name="type">Type of the node that was written</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written node</returns>
		public async ValueTask<NodeHandle> WriteNodeAsync(int size, IReadOnlyList<NodeHandle> references, BundleType type, CancellationToken cancellationToken = default)
		{
			PendingBundle currentBundle = GetCurrentBundle();

			// Get the hash for the new blob
			ReadOnlyMemory<byte> memory = currentBundle.GetBuffer(size, size);
			IoHash hash = IoHash.Compute(memory.Span);

			// Create a unique key for the new node
			NodeKey nodeKey = new NodeKey(hash, type);

			// Check if we have a matching node already in storage
			if (_nodeCache.TryGetNode(nodeKey, out NodeHandle? handle))
			{
				return handle;
			}

			// Append this node data
			PendingNode pendingNode = currentBundle.WriteNode(nodeKey, size, references);
			_memoryFootprint += size;
			_nodeCache.Add(nodeKey, pendingNode);

			// Add dependencies on all bundles containing a dependent node
			foreach (NodeHandle reference in references)
			{
				if (reference.PendingBundle != null)
				{
					currentBundle.AddDependencyOn(reference.PendingBundle);
				}
			}

			// If the bundle is full, start the process of writing it to disk
			if (currentBundle.UncompressedLength > _options.MaxBlobSize)
			{
				Complete();
			}

			// Remove any complete bundle writes
			while (_writeQueue.Count > 0 && (_writeQueue.Peek().IsComplete() || _memoryFootprint > _options.MaxInMemoryDataLength))
			{
				await WaitForWriteAsync(cancellationToken);
			}

			return pendingNode;
		}

		PendingBundle GetCurrentBundle()
		{
			if (_currentBundle == null || _currentBundle.IsReadOnly)
			{
				int bufferSize = (int)(_options.MinCompressionPacketSize * 1.2);
				_currentBundle = new PendingBundle(bufferSize, _options.MaxBlobSize, _options.CompressionFormat);
			}
			return _currentBundle;
		}

		async Task WaitForWriteAsync(CancellationToken cancellationToken)
		{
			PendingBundle writtenBundle = _writeQueue.Dequeue();
			await await Task.WhenAny(writtenBundle.CompleteTask, Task.Delay(-1, cancellationToken));
			_memoryFootprint -= writtenBundle.UncompressedLength;
			writtenBundle.Dispose();
		}

		/// <summary>
		/// Flushes all the current nodes to storage
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task FlushAsync(CancellationToken cancellationToken)
		{
			Complete();
			while (_writeQueue.Count > 0)
			{
				await WaitForWriteAsync(cancellationToken);
			}
		}
	}
}
