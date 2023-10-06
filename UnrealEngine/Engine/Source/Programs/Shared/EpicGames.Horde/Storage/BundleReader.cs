// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using Microsoft.CodeAnalysis;
using System.Diagnostics;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Writes nodes from bundles in an <see cref="IStorageClient"/> instance.
	/// </summary>
	public class BundleReader
	{
		/// <summary>
		/// Computed information about a bundle
		/// </summary>
		class BundleInfo
		{
			public readonly BlobLocator Locator;
			public readonly BundleHeader Header;
			public readonly int HeaderLength;

			public BundleInfo(BlobLocator locator, BundleHeader header, int headerLength)
			{
				Locator = locator;
				Header = header;
				HeaderLength = headerLength;
			}
		}

		/// <summary>
		/// Bundle header queued to be read
		/// </summary>
		class QueuedHeader
		{
			public readonly BlobLocator Blob;
			public readonly TaskCompletionSource<BundleInfo> CompletionSource = new TaskCompletionSource<BundleInfo>();

			public BlobId BlobId => Blob.BlobId;

			public QueuedHeader(BlobLocator blob)
			{
				Blob = blob;
			}
		}

		/// <summary>
		/// Encoded bundle packet queued to be read
		/// </summary>
		class QueuedPacket
		{
			public readonly BundleInfo Bundle;
			public readonly int PacketIdx;
			public readonly TaskCompletionSource<ReadOnlyMemory<byte>> CompletionSource = new TaskCompletionSource<ReadOnlyMemory<byte>>();

			public BlobId BlobId => Bundle.Locator.BlobId;

			public QueuedPacket(BundleInfo bundle, int packetIdx)
			{
				Bundle = bundle;
				PacketIdx = packetIdx;
			}
		}

		// Size of data to fetch by default. This is larger than the minimum request size to reduce number of reads.
		const int DefaultFetchSize = 15 * 1024 * 1024;

		// When reader is uncached, use a smaller default fetch size
		const int DefaultUncachedFetchSize = 1 * 1024 * 1024;

		readonly IStorageClient _store;
		readonly IMemoryCache? _cache;
		readonly ILogger _logger;

		readonly object _queueLock = new object();
		readonly List<QueuedHeader> _queuedHeaders = new List<QueuedHeader>();
		readonly List<QueuedPacket> _queuedPackets = new List<QueuedPacket>();
		readonly Dictionary<string, Task<ReadOnlyMemory<byte>>> _decodeTasks = new Dictionary<string, Task<ReadOnlyMemory<byte>>>(StringComparer.Ordinal);
		Task? _readTask;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="store"></param>
		/// <param name="cache">Cache for data</param>
		/// <param name="logger">Logger for output</param>
		public BundleReader(IStorageClient store, IMemoryCache? cache, ILogger logger)
		{
			_store = store;
			_cache = cache;
			_logger = logger;
		}

		#region Bundles

		static string GetBundleInfoCacheKey(BlobId blobId) => $"bundle:{blobId}";
		static string GetEncodedPacketCacheKey(BlobId blobId, int packetIdx) => $"encoded-packet:{blobId}#{packetIdx}";
		static string GetDecodedPacketCacheKey(BlobId blobId, int packetIdx) => $"decoded-packet:{blobId}#{packetIdx}";

		/// <summary>
		/// Adds an object to the storage cache
		/// </summary>
		/// <param name="cacheKey">Key for the item</param>
		/// <param name="value">Value to add</param>
		/// <param name="size">Size of the value</param>
		void AddToCache(string cacheKey, object value, int size)
		{
			if (_cache != null)
			{
				using (ICacheEntry entry = _cache.CreateEntry(cacheKey))
				{
					entry.SetValue(value);
					entry.SetSize(size);
				}
			}
		}

		/// <summary>
		/// Starts the background task for reading data from the store
		/// </summary>
		void StartReadTask()
		{
			if (_readTask == null)
			{
				_readTask = Task.Run(() => ServiceReadQueueAsync(CancellationToken.None));
			}
		}

		/// <summary>
		/// Dispatches requests in the read queue
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the background task</param>
		async Task ServiceReadQueueAsync(CancellationToken cancellationToken)
		{
			const int MaxConcurrentReads = 4;

			List<(BlobId BlobId, Task Task)> currentTasks = new List<(BlobId, Task)>();
			for (; ; )
			{
				// Start any new reads
				lock (_queueLock)
				{
					while (currentTasks.Count < MaxConcurrentReads)
					{
						HashSet<BlobId> currentBlobIds = new HashSet<BlobId>(currentTasks.Select(x => x.BlobId));

						// Try to start another header read
						QueuedHeader? queuedHeader = _queuedHeaders.FirstOrDefault(x => !currentBlobIds.Contains(x.BlobId));
						if (queuedHeader != null)
						{
							Task task = Task.Run(() => PerformHeaderReadGuardedAsync(queuedHeader, cancellationToken), cancellationToken);
							currentTasks.Add((queuedHeader.BlobId, task));
							continue;
						}

						// Try to start another packet read
						QueuedPacket? queuedPacket = _queuedPackets.FirstOrDefault(x => !currentBlobIds.Contains(x.BlobId));
						if (queuedPacket != null)
						{
							Task task = Task.Run(() => PerformPacketReadGuardedAsync(queuedPacket, cancellationToken), cancellationToken);
							currentTasks.Add((queuedPacket.BlobId, task));
							continue;
						}

						// If we're not waiting for anything else and there are no more requests, end the task thread.
						if (currentTasks.Count == 0)
						{
							_readTask = null;
							return;
						}

						// Break out of the loop
						break;
					}
				}

				// Wait for any read task to complete
				await Task.WhenAny(currentTasks.Select(x => x.Task));

				// Remove any tasks which are complete
				for (int idx = 0; idx < currentTasks.Count; idx++)
				{
					Task task = currentTasks[idx].Task;
					if (task.IsCompleted)
					{
						if (task.Exception != null)
						{
							_logger.LogError(task.Exception, "Exception while reading from blob {BlobId}.", currentTasks[idx].BlobId);
						}
						currentTasks.RemoveAt(idx--);
					}
				}
			}
		}

		/// <summary>
		/// Reads a bundle header from the queue
		/// </summary>
		/// <param name="queuedHeader">The header to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async Task PerformHeaderReadGuardedAsync(QueuedHeader queuedHeader, CancellationToken cancellationToken)
		{
			try
			{
				await PerformHeaderReadAsync(queuedHeader, cancellationToken);
			}
			catch (Exception ex)
			{
				queuedHeader.CompletionSource.TrySetException(ex);
			}
		}

		/// <summary>
		/// Reads a bundle header from the queue
		/// </summary>
		/// <param name="queuedHeader">The header to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async Task PerformHeaderReadAsync(QueuedHeader queuedHeader, CancellationToken cancellationToken)
		{
			int prefetchSize = _cache != null ? DefaultFetchSize : DefaultUncachedFetchSize;
			for (; ; )
			{
				using (IMemoryOwner<byte> owner = MemoryPool<byte>.Shared.Rent(prefetchSize))
				{
					// Read the prefetch size from the blob
					Memory<byte> memory = owner.Memory.Slice(0, prefetchSize);
					memory = await _store.ReadBlobRangeAsync(queuedHeader.Blob, 0, memory, cancellationToken);

					// Make sure it's large enough to hold the header
					int headerSize = BundleHeader.ReadPrelude(memory.Span);
					if (headerSize > prefetchSize)
					{
						prefetchSize = headerSize;
						continue;
					}

					// Parse the header and construct the bundle info from it
					BundleHeader header = BundleHeader.Read(memory.ToArray());

					// Construct the bundle info
					BundleInfo bundleInfo = new BundleInfo(queuedHeader.Blob, header, headerSize);

					if (_cache != null)
					{
						// Also add any encoded packets we prefetched
						List<ReadOnlyMemory<byte>> packets = new List<ReadOnlyMemory<byte>>();
						for (int packetOffset = headerSize; packets.Count < header.Packets.Count;)
						{
							int packetIdx = packets.Count;

							int packetLength = header.Packets[packetIdx].EncodedLength;
							if (packetOffset + packetLength > memory.Length)
							{
								break;
							}

							ReadOnlyMemory<byte> packetData = memory.Slice(packetOffset, packetLength).ToArray();
							AddToCache(GetEncodedPacketCacheKey(queuedHeader.Blob.BlobId, packetIdx), packetData, packetData.Length);
							packets.Add(packetData);
							packetOffset += packetLength;
						}

						// Add the info to the cache
						string cacheKey = GetBundleInfoCacheKey(queuedHeader.Blob.BlobId);
						AddToCache(cacheKey, bundleInfo, headerSize);
					}

					// Update the task
					queuedHeader.CompletionSource.SetResult(bundleInfo);

					// Remove it from the queue
					lock (_queueLock)
					{
						_queuedHeaders.Remove(queuedHeader);
					}
					break;
				}
			}
		}

		/// <summary>
		/// Reads a packet from storage
		/// </summary>
		/// <param name="queuedPacket">The packet to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async Task PerformPacketReadGuardedAsync(QueuedPacket queuedPacket, CancellationToken cancellationToken)
		{
			try
			{
				await PerformPacketReadAsync(queuedPacket, cancellationToken);
			}
			catch (Exception ex)
			{
				queuedPacket.CompletionSource.SetException(ex);
			}
		}

		/// <summary>
		/// Reads a packet from storage
		/// </summary>
		/// <param name="queuedPacket">The packet to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async Task PerformPacketReadAsync(QueuedPacket queuedPacket, CancellationToken cancellationToken)
		{
			BundleInfo bundleInfo = queuedPacket.Bundle;
			int minPacketIdx = queuedPacket.PacketIdx;

			BundlePacket packet = bundleInfo.Header.Packets[minPacketIdx];
			int readLength = packet.EncodedLength;

			int maxPacketIdx = minPacketIdx + 1;
			for (; maxPacketIdx < bundleInfo.Header.Packets.Count; maxPacketIdx++)
			{
				int nextReadLength = readLength + bundleInfo.Header.Packets[maxPacketIdx].EncodedLength;
				if (nextReadLength > DefaultFetchSize)
				{
					break;
				}
				readLength = nextReadLength;
			}

			using (IMemoryOwner<byte> owner = MemoryPool<byte>.Shared.Rent(readLength))
			{
				Memory<byte> buffer = owner.Memory.Slice(0, readLength);
				buffer = await _store.ReadBlobRangeAsync(bundleInfo.Locator, bundleInfo.HeaderLength + bundleInfo.Header.Packets[minPacketIdx].EncodedOffset, buffer, cancellationToken);

				// Copy all the packets that have been read into separate buffers, so we can cache them individually.
				ReadOnlyMemory<byte>?[] packets = new ReadOnlyMemory<byte>?[maxPacketIdx - minPacketIdx];
				if (_cache != null)
				{
					for (int idx = minPacketIdx; idx < maxPacketIdx; idx++)
					{
						string cacheKey = GetEncodedPacketCacheKey(bundleInfo.Locator.BlobId, idx);
						ReadOnlyMemory<byte> data = buffer.Slice(bundleInfo.Header.Packets[idx].EncodedOffset - bundleInfo.Header.Packets[minPacketIdx].EncodedOffset, bundleInfo.Header.Packets[idx].EncodedLength).ToArray();
						packets[idx - minPacketIdx] = data;
						AddToCache(cacheKey, data, data.Length);
					}
				}

				// Find all the packets we can mark as complete
				List<QueuedPacket> updatePackets = new List<QueuedPacket>();
				lock (_queueLock)
				{
					updatePackets.AddRange(_queuedPackets.Where(x => x.BlobId == bundleInfo.Locator.BlobId && (x.PacketIdx >= minPacketIdx && x.PacketIdx < maxPacketIdx)));
				}

				// Mark them all as complete
				foreach (QueuedPacket updatePacket in updatePackets)
				{
					ReadOnlyMemory<byte>? data = packets[updatePacket.PacketIdx - minPacketIdx];
					if (data == null)
					{
						data = buffer.Slice(bundleInfo.Header.Packets[updatePacket.PacketIdx].EncodedOffset - bundleInfo.Header.Packets[minPacketIdx].EncodedOffset, bundleInfo.Header.Packets[updatePacket.PacketIdx].EncodedLength).ToArray();
					}
					updatePacket.CompletionSource.SetResult((ReadOnlyMemory<byte>)data);
				}

				// Remove all the completed packets from the queue
				lock (_queueLock)
				{
					_queuedPackets.RemoveAll(x => updatePackets.Contains(x));
				}
			}
		}

		/// <summary>
		/// Reads a bundle header from the given blob locator, or retrieves it from the cache
		/// </summary>
		/// <param name="locator"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public async Task<BundleHeader> ReadBundleHeaderAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			BundleInfo info = await GetBundleInfoAsync(locator, cancellationToken);
			return info.Header;
		}

		/// <summary>
		/// Reads the header and structural metadata about the bundle
		/// </summary>
		/// <param name="locator">The bundle location</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the bundle</returns>
		async ValueTask<BundleInfo> GetBundleInfoAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			Debug.Assert(locator.IsValid());

			string cacheKey = GetBundleInfoCacheKey(locator.BlobId);
			if (_cache != null && _cache.TryGetValue(cacheKey, out BundleInfo bundleInfo))
			{
				return bundleInfo;
			}

			QueuedHeader? queuedHeader;
			lock (_queueLock)
			{
				// Check the cache again inside lock scope to avoid races
				if (_cache != null && _cache.TryGetValue(cacheKey, out bundleInfo))
				{
					return bundleInfo;
				}

				// Find or start the read
				queuedHeader = _queuedHeaders.FirstOrDefault(x => x.Blob.BlobId == locator.BlobId);
				if (queuedHeader == null)
				{
					queuedHeader = new QueuedHeader(locator);
					_queuedHeaders.Add(queuedHeader);
					StartReadTask();
				}
			}
			return await queuedHeader.CompletionSource.Task.AbandonOnCancel(cancellationToken);
		}

		/// <summary>
		/// Gets a decoded block from the store
		/// </summary>
		/// <param name="bundleInfo">Information about the bundle</param>
		/// <param name="packetIdx">Index of the packet</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The decoded data</returns>
		async ValueTask<ReadOnlyMemory<byte>> ReadBundlePacketAsync(BundleInfo bundleInfo, int packetIdx, CancellationToken cancellationToken)
		{
			char c = bundleInfo.Locator.BlobId.ToString()[0];
			Debug.Assert(Char.IsLetterOrDigit(c));

			if (packetIdx < 0 || packetIdx >= bundleInfo.Header.Packets.Count)
			{
				throw new ArgumentException("Packet index is out of range", nameof(packetIdx));
			}

			string decodedCacheKey = GetDecodedPacketCacheKey(bundleInfo.Locator.BlobId, packetIdx);
			if (_cache != null && _cache.TryGetValue(decodedCacheKey, out ReadOnlyMemory<byte> decodedPacket))
			{
				return decodedPacket;
			}

			Task<ReadOnlyMemory<byte>>? decodeTask;
			lock (_queueLock)
			{
				// Query the cache again, to eliminate races between cache checks and decode tasks finishing.
				if (_cache != null && _cache.TryGetValue(decodedCacheKey, out decodedPacket))
				{
					return decodedPacket;
				}

				// Create an async task to read the data
				if (!_decodeTasks.TryGetValue(decodedCacheKey, out decodeTask))
				{
					decodeTask = Task.Run(() => ReadAndDecodePacketAsync(bundleInfo, packetIdx), CancellationToken.None);
					_decodeTasks.Add(decodedCacheKey, decodeTask);
				}
			}
			return await decodeTask.AbandonOnCancel(cancellationToken);
		}

		/// <summary>
		/// Reads and decodes a packet from a bundle
		/// </summary>
		/// <param name="bundleInfo">Bundle to read from</param>
		/// <param name="packetIdx">Index of the packet to return</param>
		/// <returns>The decoded packet data</returns>
		async Task<ReadOnlyMemory<byte>> ReadAndDecodePacketAsync(BundleInfo bundleInfo, int packetIdx)
		{
			ReadOnlyMemory<byte> encodedPacket = await ReadEncodedPacketAsync(bundleInfo, packetIdx);

			BundlePacket packet = bundleInfo.Header.Packets[packetIdx];
			byte[] decodedPacket = new byte[packet.DecodedLength];

			BundleData.Decompress(packet.CompressionFormat, encodedPacket, decodedPacket);

			string decodedCacheKey = GetDecodedPacketCacheKey(bundleInfo.Locator.BlobId, packetIdx);
			AddToCache(decodedCacheKey, (ReadOnlyMemory<byte>)decodedPacket, decodedPacket.Length);

			lock (_queueLock)
			{
				_decodeTasks.Remove(decodedCacheKey);
			}

			return decodedPacket;
		}

		/// <summary>
		/// Reads an encoded packet from a bundle
		/// </summary>
		/// <param name="bundleInfo">Bundle to read from</param>
		/// <param name="packetIdx">Index of the packet to return</param>
		/// <returns>The encoded packet data</returns>
		async ValueTask<ReadOnlyMemory<byte>> ReadEncodedPacketAsync(BundleInfo bundleInfo, int packetIdx)
		{
			char c = bundleInfo.Locator.BlobId.ToString()[0];
			Debug.Assert(Char.IsLetterOrDigit(c));

			if (packetIdx < 0 || packetIdx >= bundleInfo.Header.Packets.Count)
			{
				throw new ArgumentException("Packet index is out of range", nameof(packetIdx));
			}

			string encodedCacheKey = GetEncodedPacketCacheKey(bundleInfo.Locator.BlobId, packetIdx);
			if (_cache != null && _cache.TryGetValue(encodedCacheKey, out ReadOnlyMemory<byte> encodedPacket))
			{
				return encodedPacket;
			}

			QueuedPacket? queuedPacket;
			lock (_queueLock)
			{
				// Query the cache again, to eliminate races between cache checks and decode tasks finishing.
				if (_cache != null && _cache.TryGetValue(encodedCacheKey, out encodedPacket))
				{
					return encodedPacket;
				}

				// Add a read to the queue
				queuedPacket = _queuedPackets.FirstOrDefault(x => x.Bundle.Locator.BlobId == bundleInfo.Locator.BlobId && x.PacketIdx == packetIdx);
				if (queuedPacket == null)
				{
					queuedPacket = new QueuedPacket(bundleInfo, packetIdx);
					_queuedPackets.Add(queuedPacket);
					StartReadTask();
				}
			}
			return await queuedPacket.CompletionSource.Task;
		}

		#endregion

		/// <summary>
		/// Reads a node from a bundle
		/// </summary>
		/// <param name="locator">Locator for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node data read from the given bundle</returns>
		public async ValueTask<BlobData> ReadNodeDataAsync(NodeLocator locator, CancellationToken cancellationToken = default)
		{
			BundleInfo bundleInfo = await GetBundleInfoAsync(locator.Blob, cancellationToken);
			BundleExport export = bundleInfo.Header.Exports[locator.ExportIdx];

			List<BlobHandle> refs = new List<BlobHandle>(export.References.Count);
			foreach (BundleExportRef reference in export.References)
			{
				BlobLocator importBlob;
				if (reference.ImportIdx == -1)
				{
					importBlob = locator.Blob;
				}
				else
				{
					importBlob = bundleInfo.Header.Imports[reference.ImportIdx];
				}
				Debug.Assert(importBlob.IsValid());
				refs.Add(new FlushedNodeHandle(this, new NodeLocator(reference.Hash, importBlob, reference.NodeIdx)));
			}

			ReadOnlyMemory<byte> nodeData = ReadOnlyMemory<byte>.Empty;
			if (export.Length > 0)
			{
				ReadOnlyMemory<byte> packetData = await ReadBundlePacketAsync(bundleInfo, export.Packet, cancellationToken);
				nodeData = packetData.Slice(export.Offset, export.Length);
			}

			BlobType nodeType = bundleInfo.Header.Types[export.TypeIdx];
			return new BlobData(nodeType, export.Hash, nodeData, refs);
		}

		/// <summary>
		/// Reads a node from a bundle
		/// </summary>
		/// <param name="locator">Locator for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node data read from the given bundle</returns>
		public async ValueTask<Node> ReadNodeAsync(NodeLocator locator, CancellationToken cancellationToken = default)
		{
			BlobData nodeData = await ReadNodeDataAsync(locator, cancellationToken);
			return Node.Deserialize(nodeData);
		}

		/// <summary>
		/// Reads a node from a bundle
		/// </summary>
		/// <param name="locator">Locator for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node data read from the given bundle</returns>
		public async ValueTask<TNode> ReadNodeAsync<TNode>(NodeLocator locator, CancellationToken cancellationToken = default) where TNode : Node => (TNode)await ReadNodeAsync(locator, cancellationToken);
	}
}
