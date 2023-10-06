// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Base class for an implementation of <see cref="IStorageClient"/>, providing implementations for some common functionality using bundles.
	/// </summary>
	public abstract class BundleStorageClient : IStorageClient
	{
		/// <summary>
		/// Reader for node data
		/// </summary>
		protected BundleReader TreeReader { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		protected BundleStorageClient(IMemoryCache? memoryCache, ILogger logger)
		{
			TreeReader = new BundleReader(this, memoryCache, logger);
		}

		#region Blobs

		/// <inheritdoc/>
		public abstract Task<Stream> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task<Stream> ReadBlobRangeAsync(BlobLocator locator, int offset, int length, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task<BlobLocator> WriteBlobAsync(Stream stream, Utf8String prefix = default, CancellationToken cancellationToken = default);

		#endregion

		#region Nodes

		/// <inheritdoc/>
		public BundleWriter CreateWriter(RefName refName = default, BundleOptions? options = null)
		{
			return new BundleWriter(this, TreeReader, refName, options);
		}

		/// <inheritdoc/>
		IStorageWriter IStorageClient.CreateWriter(RefName refName) => CreateWriter(refName);

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public abstract Task AddAliasAsync(Utf8String name, BlobHandle locator, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task RemoveAliasAsync(Utf8String name, BlobHandle locator, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract IAsyncEnumerable<BlobHandle> FindNodesAsync(Utf8String name, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <inheritdoc/>
		public abstract Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task<BlobHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task WriteRefTargetAsync(RefName name, BlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default);

		#endregion
	}

	/// <summary>
	/// Extension methods for serializing bundles
	/// </summary>
	public static class BundleStorageClientExtensions
	{
		#region Bundles

		/// <summary>
		/// Reads a bundle from the given blob id, or retrieves it from the cache
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="locator"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async Task<Bundle> ReadBundleAsync(this IStorageClient store, BlobLocator locator, CancellationToken cancellationToken = default)
		{
			using (Stream stream = await store.ReadBlobAsync(locator, cancellationToken))
			{
				return await Bundle.FromStreamAsync(stream, cancellationToken);
			}
		}

		/// <summary>
		/// Writes a new bundle to the store
		/// </summary>
		/// <param name="store">The store instance to write to</param>
		/// <param name="bundle">Bundle data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <param name="prefix">Prefix for blob names. While the returned BlobId is guaranteed to be unique, this name can be used as a prefix to aid debugging.</param>
		/// <returns>Unique identifier for the blob</returns>
		public static async Task<BlobLocator> WriteBundleAsync(this IStorageClient store, Bundle bundle, Utf8String prefix = default, CancellationToken cancellationToken = default)
		{
			using ReadOnlySequenceStream stream = new ReadOnlySequenceStream(bundle.AsSequence());
			return await store.WriteBlobAsync(stream, prefix, cancellationToken);
		}

		#endregion
	}
}
