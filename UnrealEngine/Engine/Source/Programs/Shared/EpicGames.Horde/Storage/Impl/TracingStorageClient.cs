// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using Microsoft.Extensions.Logging;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage.Impl
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which writes directly to the local filesystem for testing. Not intended for production use.
	/// </summary>
	public class TracingStorageClient : IStorageClient
	{
		readonly IStorageClient _inner;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		/// <param name="logger"></param>
		public TracingStorageClient(IStorageClient inner, ILogger logger)
		{
			_inner = inner;
			_logger = logger;
		}

		/// <inheritdoc/>
		public Task<Stream> ReadBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Reading blob {NamespaceId}/{Hash}", namespaceId, hash);
			return _inner.ReadBlobAsync(namespaceId, hash, cancellationToken);
		}
		
		/// <inheritdoc/>
		public Task<Stream> ReadCompressedBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Reading compressed blob {NamespaceId}/{Hash}", namespaceId, hash);
			return _inner.ReadCompressedBlobAsync(namespaceId, hash, cancellationToken);
		}

		/// <inheritdoc/>
		public Task WriteBlobAsync(NamespaceId namespaceId, IoHash hash, Stream stream, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Writing blob {NamespaceId}/{Hash}", namespaceId, hash);
			return _inner.WriteBlobAsync(namespaceId, hash, stream, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IoHash> WriteBlobAsync(NamespaceId namespaceId, Stream stream, CancellationToken cancellationToken = default)
		{
			IoHash hash = await _inner.WriteBlobAsync(namespaceId, stream, cancellationToken);
			_logger.LogDebug("Written blob {NamespaceId}/{Hash}", namespaceId, hash);
			return hash;
		}

		/// <inheritdoc/>
		public Task WriteCompressedBlobAsync(NamespaceId namespaceId, IoHash uncompressedHash, Stream stream, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Writing compressed blob {NamespaceId}/{UncompressedHash} (pre-calculated)", namespaceId, uncompressedHash);
			return _inner.WriteCompressedBlobAsync(namespaceId, uncompressedHash, stream, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IoHash> WriteCompressedBlobAsync(NamespaceId namespaceId, Stream compressedStream, CancellationToken cancellationToken = default)
		{
			IoHash uncompressedHash = await _inner.WriteCompressedBlobAsync(namespaceId, compressedStream, cancellationToken);
			_logger.LogDebug("Writing compressed blob {NamespaceId}/{UncompressedHash}", namespaceId, uncompressedHash);
			return uncompressedHash;
		}

		/// <inheritdoc/>
		public Task<bool> HasBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default)
		{
			return _inner.HasBlobAsync(namespaceId, hash, cancellationToken);
		}

		/// <inheritdoc/>
		public Task<HashSet<IoHash>> FindMissingBlobsAsync(NamespaceId namespaceId, HashSet<IoHash> hashes, CancellationToken cancellationToken)
		{
			return _inner.FindMissingBlobsAsync(namespaceId, hashes, cancellationToken);
		}

		/// <inheritdoc/>
		public Task<bool> DeleteRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Deleting ref {NamespaceId}/{BucketId}/{RefId}", namespaceId, bucketId, refId);
			return _inner.DeleteRefAsync(namespaceId, bucketId, refId, cancellationToken);
		}

		/// <inheritdoc/>
		public Task<List<RefId>> FindMissingRefsAsync(NamespaceId namespaceId, BucketId bucketId, List<RefId> refIds, CancellationToken cancellationToken = default)
		{
			return _inner.FindMissingRefsAsync(namespaceId, bucketId, refIds, cancellationToken);
		}

		/// <inheritdoc/>
		public Task<IRef> GetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Getting ref {NamespaceId}/{BucketId}/{RefId}", namespaceId, bucketId, refId);
			return _inner.GetRefAsync(namespaceId, bucketId, refId, cancellationToken);
		}

		/// <inheritdoc/>
		public Task<bool> HasRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default)
		{
			return _inner.HasRefAsync(namespaceId, bucketId, refId, cancellationToken);
		}

		/// <inheritdoc/>
		public Task<List<IoHash>> TryFinalizeRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, IoHash hash, CancellationToken cancellationToken = default)
		{
			return _inner.TryFinalizeRefAsync(namespaceId, bucketId, refId, hash, cancellationToken);
		}

		/// <inheritdoc/>
		public Task<List<IoHash>> TrySetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CbObject value, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Setting ref {NamespaceId}/{BucketId}/{RefId}", namespaceId, bucketId, refId);
			return _inner.TrySetRefAsync(namespaceId, bucketId, refId, value, cancellationToken);
		}
	}
}
