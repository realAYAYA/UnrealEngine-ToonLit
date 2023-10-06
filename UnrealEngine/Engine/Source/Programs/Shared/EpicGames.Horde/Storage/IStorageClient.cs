// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Base exception for the storage service
	/// </summary>
	public class StorageException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public StorageException(string message)
			: base(message)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageException(string message, Exception? innerException)
			: base(message, innerException)
		{
		}
	}

	/// <summary>
	/// Exception for a ref not existing
	/// </summary>
	public sealed class RefNameNotFoundException : StorageException
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
		/// Creates a new writer for storage nodes
		/// </summary>
		/// <param name="refName">Name of the ref being written.</param>
		/// <returns>New writer instance. Must be disposed after use.</returns>
		IStorageWriter CreateWriter(RefName refName = default);

		#endregion

		#region Aliases

		/// <summary>
		/// Adds an alias to a given node
		/// </summary>
		/// <param name="name">Alias for the node</param>
		/// <param name="handle">Locator for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task AddAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken = default);

		/// <summary>
		/// Removes an alias from a node
		/// </summary>
		/// <param name="name">Name of the alias</param>
		/// <param name="handle">Locator for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task RemoveAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds nodes with the given alias. Unlike refs, aliases do not serve as GC roots.
		/// </summary>
		/// <param name="name">Alias for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Nodes matching the given handle</returns>
		IAsyncEnumerable<BlobHandle> FindNodesAsync(Utf8String name, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <summary>
		/// Reads data for a ref from the store
		/// </summary>
		/// <param name="name">The ref name</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node pointed to by the ref</returns>
		Task<BlobHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a new ref to the store
		/// </summary>
		/// <param name="name">Ref to write</param>
		/// <param name="handle">Handle to the target node</param>
		/// <param name="options">Options for the new ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Unique identifier for the blob</returns>
		Task WriteRefTargetAsync(RefName name, BlobHandle handle, RefOptions? options = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads data for a ref from the store
		/// </summary>
		/// <param name="name">The ref identifier</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default);

		#endregion
	}

	/// <summary>
	/// Interface for writing new nodes to the store
	/// </summary>
	public interface IStorageWriter : IAsyncDisposable
	{
		/// <summary>
		/// Create another writer instance, allowing multiple threads to write in parallel.
		/// </summary>
		/// <returns>New writer instance</returns>
		IStorageWriter Fork();

		/// <summary>
		/// Flush any pending nodes to storage
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task FlushAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets an output buffer for writing.
		/// </summary>
		/// <param name="usedSize">Current size in the existing buffer that has been written to</param>
		/// <param name="desiredSize">Desired size of the returned buffer</param>
		/// <returns>Buffer to be written into.</returns>
		Memory<byte> GetOutputBuffer(int usedSize, int desiredSize);

		/// <summary>
		/// Finish writing a node.
		/// </summary>
		/// <param name="size">Used size of the buffer</param>
		/// <param name="references">References to other nodes</param>
		/// <param name="type">Type of the node that was written</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written node</returns>
		ValueTask<BlobHandle> WriteNodeAsync(int size, IReadOnlyList<BlobHandle> references, BlobType type, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes the reference using the given target node
		/// </summary>
		/// <param name="target">The target node</param>
		/// <param name="options">Options for the new ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		ValueTask WriteRefAsync(BlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default);
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
	/// Indicates the maximum age of a entry returned from a cache in the hierarchy
	/// </summary>
	/// <param name="Utc">Oldest allowed timestamp for a returned result</param>
	public record struct RefCacheTime(DateTime Utc)
	{
		/// <summary>
		/// Maximum age for a cached value to be returned
		/// </summary>
		public TimeSpan MaxAge => DateTime.UtcNow - Utc;

		/// <summary>
		/// Sets the earliest time at which the entry must have been valid
		/// </summary>
		/// <param name="age">Maximum age of any returned cache value. Taken from the moment that this object was created.</param>
		public RefCacheTime(TimeSpan age) : this(DateTime.UtcNow - age) { }

		/// <summary>
		/// Tests whether this value is set
		/// </summary>
		public bool IsSet() => Utc != default;

		/// <summary>
		/// Determines if this cache time deems a particular cache entry stale
		/// </summary>
		/// <param name="entryTime">Time at which the cache entry was valid</param>
		/// <param name="cacheTime">Maximum cache time to test against</param>
		public static bool IsStaleCacheEntry(DateTime entryTime, RefCacheTime cacheTime) => cacheTime.IsSet() && cacheTime.Utc < entryTime;

		/// <summary>
		/// Implicit conversion operator from datetime values.
		/// </summary>
		public static implicit operator RefCacheTime(DateTime time) => new RefCacheTime(time);

		/// <summary>
		/// Implicit conversion operator from timespan values.
		/// </summary>
		public static implicit operator RefCacheTime(TimeSpan age) => new RefCacheTime(age);
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageClient"/>
	/// </summary>
	public static class StorageClientExtensions
	{
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
		public static async Task<bool> HasRefAsync(this IStorageClient store, RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobHandle? target = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			return target != null;
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="cacheTime">Minimum coherency of any cached result</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The ref target</returns>
		public static async Task<BlobHandle> ReadRefTargetAsync(this IStorageClient store, RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobHandle? refTarget = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			if (refTarget == null)
			{
				throw new RefNameNotFoundException(name);
			}
			return refTarget;
		}

		#endregion
	}
}
