// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Bundles
{
	/// <summary>
	/// Base class for packet handles
	/// </summary>
	public abstract class BundleHandle
	{
		/// <summary>
		/// Flush contents of the bundle to storage
		/// </summary>
		public abstract ValueTask FlushAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Open the bundle for reading
		/// </summary>
		public abstract Task<Stream> OpenAsync(int offset = 0, int? length = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Get data for a packet of data from the bundle
		/// </summary>
		public abstract ValueTask<IReadOnlyMemoryOwner<byte>> ReadAsync(int offset, int? length, CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempt to get a locator for this bundle
		/// </summary>
		public abstract bool TryGetLocator(out BlobLocator locator);
	}

	/// <summary>
	/// Generic flushed bundle handle; can be either V1 or V2 format.
	/// </summary>
	class FlushedBundleHandle : BundleHandle
	{
		readonly BundleStorageClient _storageClient;
		readonly BlobLocator _locator;

		/// <summary>
		/// Constructor
		/// </summary>
		public FlushedBundleHandle(BundleStorageClient storageClient, BlobLocator locator)
		{
			_storageClient = storageClient;
			_locator = locator;
		}

		/// <inheritdoc/>
		public override ValueTask FlushAsync(CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public override Task<Stream> OpenAsync(int offset = 0, int? length = null, CancellationToken cancellationToken = default)
			=> _storageClient.Backend.OpenBlobAsync(_locator, offset, length, cancellationToken);

		/// <inheritdoc/>
		public override async ValueTask<IReadOnlyMemoryOwner<byte>> ReadAsync(int offset, int? length, CancellationToken cancellationToken = default)
			=> await _storageClient.Backend.ReadBlobAsync(_locator, offset, length, cancellationToken);

		/// <inheritdoc/>
		public override bool TryGetLocator(out BlobLocator locator)
		{
			locator = _locator;
			return true;
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is FlushedBundleHandle other && _locator == other._locator;

		/// <inheritdoc/>
		public override int GetHashCode() => _locator.GetHashCode();

		/// <inheritdoc/>
		public override string ToString() => _locator.ToString();
	}
}
