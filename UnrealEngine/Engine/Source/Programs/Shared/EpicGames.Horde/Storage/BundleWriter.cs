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
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Options for configuring a bundle serializer
	/// </summary>
	public class BundleOptions
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
	public record NodeKey(IoHash Hash, BlobType Type);

	/// <summary>
	/// Implementation of <see cref="BlobHandle"/> for nodes which can be read from storage
	/// </summary>
	public sealed class FlushedNodeHandle : BlobHandle
	{
		readonly BundleReader _reader;
		readonly NodeLocator _locator;

		/// <summary>
		/// Constructor
		/// </summary>
		public FlushedNodeHandle(BundleReader reader, NodeLocator locator)
			: base(locator.Hash)
		{
			_reader = reader;
			_locator = locator;
		}

		/// <inheritdoc/>
		public override bool HasLocator() => true;

		/// <inheritdoc/>
		public override NodeLocator GetLocator() => _locator;

		/// <inheritdoc/>
		public override void AddWriteCallback(BlobWriteCallback callback) => callback.OnWrite();

		/// <inheritdoc/>
		public override ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default) => _reader.ReadNodeDataAsync(_locator, cancellationToken);

		/// <inheritdoc/>
		public override ValueTask<NodeLocator> FlushAsync(CancellationToken cancellationToken = default) => new ValueTask<NodeLocator>(_locator);
	}

	/// <summary>
	/// Index of known nodes that can be used for deduplication.
	/// </summary>
	public class NodeCache
	{
		readonly object _lockObject = new object();
		readonly int _maxKeys;
		readonly Queue<NodeKey> _nodeKeys = new Queue<NodeKey>();
		readonly Dictionary<NodeKey, BlobHandle> _nodeKeyToHandle = new Dictionary<NodeKey, BlobHandle>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="maxKeys">Maximum number of node keys to keep in the cache</param>
		public NodeCache(int maxKeys)
		{
			_maxKeys = maxKeys;
			_nodeKeys = new Queue<NodeKey>(maxKeys);
			_nodeKeyToHandle = new Dictionary<NodeKey, BlobHandle>(maxKeys);
		}

		/// <summary>
		/// Adds a new node handle to the cache
		/// </summary>
		/// <param name="key">Unique node key</param>
		/// <param name="handle">Handle to the node</param>
		public void Add(NodeKey key, BlobHandle handle)
		{
			lock (_lockObject)
			{
				AddInternal(key, handle);
			}
		}

		void AddInternal(NodeKey key, BlobHandle handle)
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
		public bool TryGetNode(NodeKey key, [NotNullWhen(true)] out BlobHandle? handle)
		{
			lock (_lockObject)
			{
				return _nodeKeyToHandle.TryGetValue(key, out handle);
			}
		}
	}

	/// <summary>
	/// Writes nodes of a tree to an <see cref="IStorageClient"/>, packed into bundles. Each <see cref="BundleWriter"/> instance is single threaded,
	/// but multiple instances may be written to in parallel.
	/// </summary>
	public sealed class BundleWriter : IStorageWriter
	{
		// Information about a unique output node. Note that multiple node refs may de-duplicate to the same output node.
		internal class PendingNode : BlobHandle
		{
			readonly BundleReader _reader;

			object LockObject => _reader;

			NodeLocator _locator;
			PendingBundle? _pendingBundle;

			public readonly NodeKey Key;
			public readonly int Packet;
			public readonly int Offset;
			public readonly int Length;
			public readonly BlobHandle[] Refs;

			public PendingBundle? PendingBundle => _pendingBundle;

			public PendingNode(BundleReader reader, NodeKey key, int packet, int offset, int length, IReadOnlyList<BlobHandle> refs, PendingBundle pendingBundle)
				: base(key.Hash)
			{
				_reader = reader;

				Key = key;
				Packet = packet;
				Offset = offset;
				Length = length;
				Refs = refs.ToArray();

				_pendingBundle = pendingBundle;
			}

			/// <inheritdoc/>
			public override bool HasLocator() => _locator.IsValid();

			/// <inheritdoc/>
			public override NodeLocator GetLocator()
			{
				if (!_locator.IsValid())
				{
					throw new InvalidOperationException();
				}
				return _locator;
			}

			public void MarkAsWritten(NodeLocator locator)
			{
				lock (LockObject)
				{
					Debug.Assert(!_locator.IsValid());
					_locator = locator;
					_pendingBundle = null;
				}
			}

			/// <inheritdoc/>
			public override void AddWriteCallback(BlobWriteCallback callback)
			{
				PendingBundle? pendingBundle = PendingBundle;
				if (pendingBundle == null || !pendingBundle.TryAddWriteCallback(callback))
				{
					Debug.Assert(_locator.IsValid());
					callback.OnWrite();
				}
			}

			/// <inheritdoc/>
			public override async ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
			{
				if (!_locator.IsValid())
				{
					lock (LockObject)
					{
						if (!_locator.IsValid() && _pendingBundle != null)
						{
							ReadOnlyMemory<byte> data = _pendingBundle.GetNodeData(Packet, Offset, Length);
							if (data.Length == Length)
							{
								return new BlobData(Key.Type, Hash, data, Refs);
							}
						}
					}
				}

				return await _reader.ReadNodeDataAsync(_locator, cancellationToken);
			}

			/// <inheritdoc/>
			public override async ValueTask<NodeLocator> FlushAsync(CancellationToken cancellationToken = default)
			{
				if (_locator.IsValid())
				{
					return _locator;
				}

				foreach (BlobHandle nodeRef in Refs)
				{
					await nodeRef.FlushAsync(cancellationToken);
				}

				PendingBundle? pendingBundle = _pendingBundle;
				if (pendingBundle != null)
				{
					await pendingBundle.FlushAsync(cancellationToken);
				}

				return GetLocator();
			}
		}

		// Information about a bundle being built. Metadata operations are synchronous, compression/writes are asynchronous.
		internal class PendingBundle : IDisposable
		{
			class WriteCallbackSentinel : BlobWriteCallback
			{
				public override void OnWrite() => throw new NotImplementedException();
			}

			static readonly WriteCallbackSentinel s_callbackSentinel = new WriteCallbackSentinel();

			static int s_lastBundleId = 0;
			public int BundleId { get; } = Interlocked.Increment(ref s_lastBundleId);

			readonly BundleReader _treeReader;
			readonly BundleWriter _treeWriter;
			readonly int _maxPacketSize;
			readonly BundleCompressionFormat _compressionFormat;

			readonly object _lockObject = new object();

			// The current packet being built
			IMemoryOwner<byte>? _currentPacket;

			// Index of the current packet being written to
			int _currentPacketIdx;

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
			BlobWriteCallback? _callbacks = null;

			// Task used to compress data in the background
			Task _writeTask = Task.CompletedTask;

			// Event which is signalled after the bundle is written to storage
			readonly TaskCompletionSource<bool> _completeEvent = new TaskCompletionSource<bool>();

			// Task signalled after the write is complete
			public Task CompleteTask => _completeEvent.Task;

			public PendingBundle(BundleReader treeReader, BundleWriter treeWriter, int maxPacketSize, int maxBlobSize, BundleCompressionFormat compressionFormat)
			{
				_treeReader = treeReader;
				_treeWriter = treeWriter;
				_maxPacketSize = maxPacketSize;
				_compressionFormat = compressionFormat;
				_encodedPacketWriter = new ChunkedMemoryWriter(maxBlobSize);
			}

			/// <inheritdoc/>
			public void Dispose()
			{
				if (!CompleteTask.IsCompleted)
				{
					_completeEvent.TrySetException(new ObjectDisposedException($"Bundle {BundleId} has been disposed. Make sure to call FlushAsync() on forked writers that may have dependencies before disposing.", $"Bundle {BundleId}"));
				}
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
			public void AddDependencyOn(PendingBundle bundle, ILogger? traceLogger)
			{
				if (bundle != this)
				{
					if (_dependencies.Add(bundle.CompleteTask))
					{
						traceLogger?.LogInformation("Added dependency from bundle {BundleId} on {OtherBundleId}", BundleId, bundle.BundleId);
					}
				}
			}

			// Adds a callback after writing
			public bool TryAddWriteCallback(BlobWriteCallback callback)
			{
				for (; ; )
				{
					BlobWriteCallback? tail = _callbacks;
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
				else if (_currentPacketLength + desiredSize > _currentPacket.Memory.Length)
				{
					IMemoryOwner<byte> nextPacket = MemoryPool<byte>.Shared.Rent(Math.Max(_maxPacketSize, desiredSize));
					if (usedSize > 0)
					{
						_currentPacket.Memory.Slice(_currentPacketLength, usedSize).CopyTo(nextPacket.Memory);
					}
					FlushPacket();
					_currentPacket = nextPacket;
				}
				return _currentPacket.Memory.Slice(_currentPacketLength, desiredSize);
			}

			// Finish a node write
			public PendingNode WriteNode(NodeKey nodeKey, int size, IReadOnlyList<BlobHandle> refs)
			{
				int offset = _currentPacketLength;

				_currentPacketLength += size;
				UncompressedLength += size;

				PendingNode pendingNode = new PendingNode(_treeReader, nodeKey, _currentPacketIdx, offset, (int)size, refs, this);
				_queue.Add(pendingNode);
				_nodeKeyToInfo.Add(nodeKey, pendingNode);

				return pendingNode;
			}

			public ReadOnlyMemory<byte> GetNodeData(int packetIdx, int offset, int length)
			{
				lock (_lockObject)
				{
					if (packetIdx < _packets.Count)
					{
						BundlePacket packet = _packets[packetIdx];

						ReadOnlySequence<byte> sequence = _encodedPacketWriter.AsSequence(packet.EncodedOffset, packet.EncodedLength);
						if (!sequence.IsSingleSegment)
						{
							throw new InvalidOperationException();
						}

						using (IMemoryOwner<byte> decodeBuffer = MemoryPool<byte>.Shared.Rent(packet.DecodedLength))
						{
							BundleData.Decompress(_compressionFormat, sequence.First, decodeBuffer.Memory);
							return decodeBuffer.Memory.Slice(offset, length).ToArray();
						}
					}
					else if (packetIdx == _currentPacketIdx)
					{
						return _currentPacket!.Memory.Slice(offset, length).Slice(offset, length).ToArray();
					}
					else
					{
						return ReadOnlyMemory<byte>.Empty;
					}
				}
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
						_currentPacketIdx++;
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
				lock (_lockObject)
				{
					int encodedLength = BundleData.Compress(_compressionFormat, packetData.Memory.Slice(0, length), _encodedPacketWriter);

					int encodedOffset = (_packets.Count == 0) ? 0 : _packets[^1].EncodedOffset + _packets[^1].EncodedLength;
					BundlePacket packet = new BundlePacket(_compressionFormat, encodedOffset, encodedLength, length);
					_packets.Add(packet);

					packetData.Dispose();
				}
			}

			// Mark the bundle as complete
			public void MarkAsComplete(IStorageClient store, Utf8String prefix, ILogger? traceLogger)
			{
				if (!IsReadOnly)
				{
					traceLogger?.LogInformation("Marking bundle {BundleId} as complete ({NumNodes} nodes); adding to write queue.", BundleId, _queue.Count);
					FlushPacket();
					Task prevWriteTask = _writeTask;
					_writeTask = Task.Run(() => CompleteAsync(prevWriteTask, store, prefix, traceLogger));
					IsReadOnly = true;
				}
			}

			async Task CompleteAsync(Task prevWriteTask, IStorageClient store, Utf8String prefix, ILogger? traceLogger)
			{
				try
				{
					await prevWriteTask;
					if (traceLogger != null)
					{
						foreach (Task dependency in _dependencies)
						{
							if (!dependency.IsCompleted)
							{
								traceLogger.LogInformation("Bundle {BundleId} is stalling waiting for dependencies to flush first", BundleId);
							}
						}
					}
					await Task.WhenAll(_dependencies);

					Bundle bundle = CreateBundle();
					BlobLocator locator = await store.WriteBundleAsync(bundle, prefix);
					traceLogger?.LogInformation("Written bundle {BundleId} as {Locator}", BundleId, locator);

					for (int idx = 0; idx < _queue.Count; idx++)
					{
						NodeLocator nodeLocator = new NodeLocator(_queue[idx].Key.Hash, locator, idx);
						traceLogger?.LogInformation("Updated pending node {Hash} with locator {Locator}", _queue[idx].Key.Hash, nodeLocator);
						_queue[idx].MarkAsWritten(nodeLocator);
					}

					BlobWriteCallback? callback = Interlocked.Exchange(ref _callbacks, s_callbackSentinel);
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
			public Task FlushAsync(CancellationToken cancellationToken) => _treeWriter.FlushAsync(cancellationToken);

			// Mark the bundle as complete and create a bundle with the current state
			public Bundle CreateBundle()
			{
				lock (_lockObject)
				{
					return CreateBundleInternal();
				}
			}

			Bundle CreateBundleInternal()
			{
				// List of imported blobs
				List<BlobLocator> imports = new List<BlobLocator>();
				Dictionary<BlobLocator, int> importToIndex = new Dictionary<BlobLocator, int>();

				// List of types in the bundle
				List<BlobType> types = new List<BlobType>();
				Dictionary<BlobType, int> typeToIndex = new Dictionary<BlobType, int>();

				// Map of node handle to reference
				Dictionary<BlobHandle, BundleExportRef> nodeHandleToExportRef = new Dictionary<BlobHandle, BundleExportRef>();
				for (int exportIdx = 0; exportIdx < _queue.Count; exportIdx++)
				{
					BlobHandle handle = _queue[exportIdx];
					nodeHandleToExportRef[handle] = new BundleExportRef(-1, exportIdx, handle.Hash);
				}

				// Create the export list
				List<BundleExport> exports = new List<BundleExport>(_queue.Count);
				foreach (PendingNode nodeInfo in _queue)
				{
					int typeIdx = FindOrAddItemIndex(nodeInfo.Key.Type, types, typeToIndex);

					List<BundleExportRef> exportRefs = new List<BundleExportRef>();
					foreach (BlobHandle handle in nodeInfo.Refs)
					{
						BundleExportRef exportRef;
						if (!nodeHandleToExportRef.TryGetValue(handle, out exportRef))
						{
							NodeLocator locator = handle.GetLocator();

							int importIdx = FindOrAddItemIndex(locator.Blob, imports, importToIndex);
							exportRef = new BundleExportRef(importIdx, locator.ExportIdx, handle.Hash);
						}
						exportRefs.Add(exportRef);
					}

					BundleExport export = new BundleExport(typeIdx, nodeInfo.Key.Hash, nodeInfo.Packet, nodeInfo.Offset, nodeInfo.Length, exportRefs);
					exports.Add(export);
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
				BundleHeader header = BundleHeader.Create(types, imports, exports, _packets);
				return new Bundle(header, packetData);
			}

			static int FindOrAddItemIndex<TItem>(TItem item, List<TItem> items, Dictionary<TItem, int> itemToIndex) where TItem : notnull
			{
				int index;
				if (!itemToIndex.TryGetValue(item, out index))
				{
					index = items.Count;
					items.Add(item);
					itemToIndex.Add(item, index);
				}
				return index;
			}
		}

		static readonly BundleOptions s_defaultOptions = new BundleOptions();

		readonly IStorageClient _store;
		readonly BundleReader _reader;
		readonly BundleOptions _options;
		readonly RefName _refName;

		readonly NodeCache _nodeCache;
		readonly Queue<PendingBundle> _writeQueue = new Queue<PendingBundle>();

		PendingBundle? _currentBundle;
		int _memoryFootprint;

		bool _disposed;

		internal readonly ILogger? _traceLogger;

		/// <summary>
		/// Cache of nodes to deduplicate against
		/// </summary>
		public NodeCache NodeCache => _nodeCache;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="store">Store to write data to</param>
		/// <param name="reader">Reader for serialized node data</param>
		/// <param name="refName">Name of the ref being written</param>
		/// <param name="options">Options for the writer</param>
		/// <param name="nodeCache">Cache of nodes for deduplication</param>
		/// <param name="traceLogger">Optional logger for trace information</param>
		public BundleWriter(IStorageClient store, BundleReader reader, RefName refName, BundleOptions? options = null, NodeCache? nodeCache = null, ILogger? traceLogger = null)
		{
			_store = store;
			_reader = reader;
			_refName = refName;
			_options = options ?? s_defaultOptions;
			_nodeCache = nodeCache ?? new NodeCache(_options.NodeCacheSize);
			_traceLogger = traceLogger;
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="other"></param>
		public BundleWriter(BundleWriter other)
			: this(other._store, other._reader, other._refName, other._options, other._nodeCache, other._traceLogger)
		{
		}

		/// <inheritdoc/>
		IStorageWriter IStorageWriter.Fork() => new BundleWriter(this);

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			if (!_disposed)
			{
				await FlushAsync();
				_disposed = true;
			}

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
				_currentBundle.MarkAsComplete(_store, _refName.Text, _traceLogger);
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
		public async ValueTask<BlobHandle> WriteNodeAsync(int size, IReadOnlyList<BlobHandle> references, BlobType type, CancellationToken cancellationToken = default)
		{
			PendingBundle currentBundle = GetCurrentBundle();

			// Get the hash for the new blob
			ReadOnlyMemory<byte> memory = currentBundle.GetBuffer(size, size);
			IoHash hash = IoHash.Compute(memory.Span);

			// Create a unique key for the new node
			NodeKey nodeKey = new NodeKey(hash, type);

			// Check if we have a matching node already in storage
			if (_nodeCache.TryGetNode(nodeKey, out BlobHandle? handle))
			{
				_traceLogger?.LogInformation("Returning cached handle for {NodeKey} -> {Handle}", nodeKey, handle);
				return handle;
			}

			// Append this node data
			PendingNode pendingNode = currentBundle.WriteNode(nodeKey, size, references);
			_memoryFootprint += size;
			_nodeCache.Add(nodeKey, pendingNode);
			_traceLogger?.LogInformation("Added new node for {NodeKey} in bundle {BundleId}", nodeKey, currentBundle.BundleId);

			// Add dependencies on all bundles containing a dependent node
			foreach (BlobHandle reference in references)
			{
				PendingNode? pendingReference = reference as PendingNode;
				if (pendingReference?.PendingBundle != null)
				{
					currentBundle.AddDependencyOn(pendingReference.PendingBundle, _traceLogger);
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
				_currentBundle = new PendingBundle(_reader, this, bufferSize, _options.MaxBlobSize, _options.CompressionFormat);
			}
			return _currentBundle;
		}

		async Task WaitForWriteAsync(CancellationToken cancellationToken)
		{
			PendingBundle writtenBundle = _writeQueue.Dequeue();
			_traceLogger?.LogInformation("Waiting for bundle {BundleId} to finish writing", writtenBundle.BundleId);
			await await Task.WhenAny(writtenBundle.CompleteTask, Task.Delay(-1, cancellationToken));
			_memoryFootprint -= writtenBundle.UncompressedLength;
			writtenBundle.Dispose();
		}

		/// <summary>
		/// Flushes all the current nodes to storage
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task FlushAsync(CancellationToken cancellationToken = default)
		{
			if (_disposed)
			{
				throw new ObjectDisposedException(GetType().Name);
			}

			Complete();
			while (_writeQueue.Count > 0)
			{
				await WaitForWriteAsync(cancellationToken);
			}
		}

		/// <inheritdoc/>
		public async ValueTask WriteRefAsync(BlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			await _store.WriteRefTargetAsync(_refName, target, options, cancellationToken);
		}
	}
}
