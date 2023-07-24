// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Reflection.Metadata;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Exception for a ref not existing
	/// </summary>
	public sealed class RefNameNotFoundException : Exception
	{
		/// <summary>
		/// Name of the missing ref
		/// </summary>
		public RefName Name { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name"></param>
		public RefNameNotFoundException(RefName name)
			: base($"Ref name '{name}' not found")
		{
			Name = name;
		}
	}

	/// <summary>
	/// Options for a new ref
	/// </summary>
	public class RefOptions
	{
		/// <summary>
		/// Time until a ref is expired
		/// </summary>
		public TimeSpan? Lifetime { get; set; }

		/// <summary>
		/// Whether to extend the remaining lifetime of a ref whenever it is fetched. Defaults to true.
		/// </summary>
		public bool? Extend { get; set; }
	}

	/// <summary>
	/// Locates a node in storage
	/// </summary>
	public struct NodeLocator : IEquatable<NodeLocator>
	{
		/// <summary>
		/// Location of the blob containing this node
		/// </summary>
		public BlobLocator Blob { get; }

		/// <summary>
		/// Index of the export within the blob
		/// </summary>
		public int ExportIdx { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public NodeLocator(BlobLocator blob, int exportIdx)
		{
			Blob = blob;
			ExportIdx = exportIdx;
		}

		/// <summary>
		/// Determines if this locator points to a valid entry
		/// </summary>
		public bool IsValid() => Blob.IsValid();

		/// <summary>
		/// Parse a string as a node locator
		/// </summary>
		/// <param name="text">Text to parse</param>
		/// <returns></returns>
		public static NodeLocator Parse(ReadOnlySpan<char> text)
		{
			int hashIdx = text.IndexOf('#');
			int exportIdx = Int32.Parse(text.Slice(hashIdx + 1), NumberStyles.None, CultureInfo.InvariantCulture);
			BlobLocator blobLocator = new BlobLocator(new Utf8String(text.Slice(0, hashIdx)));
			return new NodeLocator(blobLocator, exportIdx);
		}

		/// <inheritdoc/>
		public override bool Equals([NotNullWhen(true)] object? obj) => obj is NodeLocator locator && Equals(locator);

		/// <inheritdoc/>
		public bool Equals(NodeLocator other) => Blob == other.Blob && ExportIdx == other.ExportIdx;

		/// <inheritdoc/>
		public override int GetHashCode() => HashCode.Combine(Blob, ExportIdx);

		/// <inheritdoc/>
		public override string ToString() => $"{Blob}#{ExportIdx}";

		/// <inheritdoc/>
		public static bool operator ==(NodeLocator left, NodeLocator right) => left.Equals(right);

		/// <inheritdoc/>
		public static bool operator !=(NodeLocator left, NodeLocator right) => !left.Equals(right);
	}

	/// <summary>
	/// Base interface for a low-level storage backend. Blobs added to this store are not content addressed, but referenced by <see cref="BlobLocator"/>.
	/// </summary>
	public interface IStorageClient
	{
		#region Blobs

		/// <summary>
		/// Reads raw data for a blob from the store
		/// </summary>
		/// <param name="locator">The blob locator</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream containing the data</returns>
		Task<Stream> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads a ranged chunk from a blob
		/// </summary>
		/// <param name="locator">Locator for the blob</param>
		/// <param name="offset">Starting offset for the data to read</param>
		/// <param name="length">Length of the data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<Stream> ReadBlobRangeAsync(BlobLocator locator, int offset, int length, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a new blob to the store
		/// </summary>
		/// <param name="stream">Blob data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <param name="prefix">Prefix for blob names. While the returned BlobId is guaranteed to be unique, this name can be used as a prefix to aid debugging.</param>
		/// <returns>Unique identifier for the blob</returns>
		Task<BlobLocator> WriteBlobAsync(Stream stream, Utf8String prefix = default, CancellationToken cancellationToken = default);

		#endregion

		#region Nodes

		/// <summary>
		/// Finds nodes with the given alias. Unlike refs, aliases do not serve as GC roots.
		/// </summary>
		/// <param name="name">Alias for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Nodes matching the given handle</returns>
		IAsyncEnumerable<NodeHandle> FindNodesAsync(Utf8String name, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <summary>
		/// Reads data for a ref from the store
		/// </summary>
		/// <param name="name">The ref name</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node pointed to by the ref</returns>
		Task<NodeHandle?> TryReadRefTargetAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a new ref to the store which points to a new blob
		/// </summary>
		/// <param name="name">Ref to write</param>
		/// <param name="bundle">The bundle to write</param>
		/// <param name="exportIdx">Index of the export in the bundle to be the root of the tree</param>
		/// <param name="prefix">Prefix for blob names.</param>
		/// <param name="options">Options for the new ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Unique identifier for the blob</returns>
		Task<NodeHandle> WriteRefAsync(RefName name, Bundle bundle, int exportIdx, Utf8String prefix = default, RefOptions? options = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a new ref to the store
		/// </summary>
		/// <param name="name">Ref to write</param>
		/// <param name="handle">Handle to the target node</param>
		/// <param name="options">Options for the new ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Unique identifier for the blob</returns>
		Task WriteRefTargetAsync(RefName name, NodeHandle handle, RefOptions? options = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads data for a ref from the store
		/// </summary>
		/// <param name="name">The ref identifier</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default);

		#endregion
	}

	/// <summary>
	/// Typed implementation of <see cref="IStorageClient"/> for use with dependency injection
	/// </summary>
	public interface IStorageClient<T> : IStorageClient
	{
	}

	/// <summary>
	/// Allows creating storage clients for different namespaces
	/// </summary>
	public interface IStorageClientFactory
	{
		/// <summary>
		/// Creates a storage client for the given namespace
		/// </summary>
		/// <param name="namespaceId">Namespace to manipulate</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Storage client instance</returns>
		ValueTask<IStorageClient> GetClientAsync(NamespaceId namespaceId, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageClient"/>
	/// </summary>
	public static class StorageClientExtensions
	{
		class TypedStorageClient<T> : IStorageClient<T>
		{
			readonly IStorageClient _inner;

			public TypedStorageClient(IStorageClient inner) => _inner = inner;

			#region Blobs

			/// <inheritdoc/>
			public Task<Stream> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default) => _inner.ReadBlobAsync(locator, cancellationToken);

			/// <inheritdoc/>
			public Task<Stream> ReadBlobRangeAsync(BlobLocator locator, int offset, int length, CancellationToken cancellationToken = default) => _inner.ReadBlobRangeAsync(locator, offset, length, cancellationToken);

			/// <inheritdoc/>
			public Task<BlobLocator> WriteBlobAsync(Stream stream, Utf8String prefix = default, CancellationToken cancellationToken = default) => _inner.WriteBlobAsync(stream, prefix, cancellationToken);

			#endregion

			#region Nodes

			/// <inheritdoc/>
			public IAsyncEnumerable<NodeHandle> FindNodesAsync(Utf8String name, CancellationToken cancellationToken = default) => _inner.FindNodesAsync(name, cancellationToken);

			#endregion

			#region Refs

			/// <inheritdoc/>
			public Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default) => _inner.DeleteRefAsync(name, cancellationToken);

			/// <inheritdoc/>
			public Task<NodeHandle?> TryReadRefTargetAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default) => _inner.TryReadRefTargetAsync(name, cacheTime, cancellationToken);

			/// <inheritdoc/>
			public Task<NodeHandle> WriteRefAsync(RefName name, Bundle bundle, int exportIdx = 0, Utf8String prefix = default, RefOptions? options = null, CancellationToken cancellationToken = default) => _inner.WriteRefAsync(name, bundle, exportIdx, prefix, options, cancellationToken);

			/// <inheritdoc/>
			public Task WriteRefTargetAsync(RefName name, NodeHandle target, RefOptions? options = null, CancellationToken cancellationToken = default) => _inner.WriteRefTargetAsync(name, target, options, cancellationToken);

			#endregion
		}

		/// <summary>
		/// Wraps a <see cref="IStorageClient"/> interface with a type argument
		/// </summary>
		/// <param name="blobStore">Regular blob store instance</param>
		/// <returns></returns>
		public static IStorageClient<T> ForType<T>(this IStorageClient blobStore) => new TypedStorageClient<T>(blobStore);

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

		#region Blobs

		/// <summary>
		/// Utility method to read a blob into a buffer
		/// </summary>
		/// <param name="store">Store to read from</param>
		/// <param name="locator">Blob location</param>
		/// <param name="offset">Offset within the blob</param>
		/// <param name="memory">Buffer to read into</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The data that was read</returns>
		public static async Task<Memory<byte>> ReadBlobRangeAsync(this IStorageClient store, BlobLocator locator, int offset, Memory<byte> memory, CancellationToken cancellationToken = default)
		{
			using (Stream stream = await store.ReadBlobRangeAsync(locator, offset, memory.Length, cancellationToken))
			{
				int length = 0;
				while (length < memory.Length)
				{
					int readBytes = await stream.ReadAsync(memory.Slice(length), cancellationToken);
					if (readBytes == 0)
					{
						break;
					}
					length += readBytes;
				}
				return memory.Slice(0, length);
			}
		}

		#endregion

		#region Refs

		/// <summary>
		/// Checks if the given ref exists
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Name of the reference to look for</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the ref exists, false if it did not exist</returns>
		public static async Task<bool> HasRefAsync(this IStorageClient store, RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			NodeHandle? target = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			return target != null;
		}

		/// <summary>
		/// Checks if the given ref exists
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Name of the reference to look for</param>
		/// <param name="maxAge">Maximum age of any cached ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the ref exists, false if it did not exist</returns>
		public static Task<bool> HasRefAsync(this IStorageClient store, RefName name, TimeSpan maxAge, CancellationToken cancellationToken = default)
		{
			return HasRefAsync(store, name, DateTime.UtcNow - maxAge, cancellationToken);
		}

		/// <summary>
		/// Attempts to reads a ref from the store
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="maxAge">Maximum age of any cached ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The ref target</returns>
		public static Task<NodeHandle?> TryReadRefTargetAsync(this IStorageClient store, RefName name, TimeSpan maxAge, CancellationToken cancellationToken = default)
		{
			return store.TryReadRefTargetAsync(name, DateTime.UtcNow - maxAge, cancellationToken);
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="cacheTime">Minimum coherency of any cached result</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The ref target</returns>
		public static async Task<NodeHandle> ReadRefTargetAsync(this IStorageClient store, RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			NodeHandle? refTarget = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			if (refTarget == null)
			{
				throw new RefNameNotFoundException(name);
			}
			return refTarget;
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="maxAge">Maximum age for any cached result</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The blob instance</returns>
		public static Task<NodeHandle> ReadRefTargetAsync(this IStorageClient store, RefName name, TimeSpan maxAge, CancellationToken cancellationToken = default)
		{
			return ReadRefTargetAsync(store, name, DateTime.UtcNow - maxAge, cancellationToken);
		}

		#endregion
	}
}
