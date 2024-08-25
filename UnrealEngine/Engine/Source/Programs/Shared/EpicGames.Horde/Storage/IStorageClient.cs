// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Options for a new ref
	/// </summary>
	/// <param name="Lifetime">Time until a ref is expired</param>
	/// <param name="Extend">Whether to extend the remaining lifetime of a ref whenever it is fetched. Defaults to true.</param>
	public record class RefOptions(TimeSpan? Lifetime = null, bool? Extend = null);

	/// <summary>
	/// Interface for the storage system.
	/// </summary>
	public interface IStorageClient : IDisposable
	{
		#region Blobs

		/// <summary>
		/// Creates a new blob handle by parsing a locator
		/// </summary>
		/// <param name="locator">Path to the blob</param>
		/// <returns>New handle to the blob</returns>
		IBlobHandle CreateBlobHandle(BlobLocator locator);

		/// <summary>
		/// Creates a new blob reference from a locator and hash
		/// </summary>
		/// <param name="hash">Hash of the target blob</param>
		/// <param name="locator">Path to the blob</param>
		/// <returns>New handle to the blob</returns>
		IBlobRef CreateBlobRef(IoHash hash, BlobLocator locator);

		/// <summary>
		/// Creates a new blob reference from a locator and hash
		/// </summary>
		/// <param name="hash">Hash of the target blob</param>
		/// <param name="locator">Path to the blob</param>
		/// <param name="serializerOptions">Options for deserializing the blob</param>
		/// <returns>New handle to the blob</returns>
		IBlobRef<T> CreateBlobRef<T>(IoHash hash, BlobLocator locator, BlobSerializerOptions? serializerOptions = null);

		/// <summary>
		/// Creates a new writer for storage blobs
		/// </summary>
		/// <param name="basePath">Base path for any nodes written from the writer.</param>
		/// <param name="serializerOptions">Options for serializing classes</param>
		/// <returns>New writer instance. Must be disposed after use.</returns>
		IBlobWriter CreateBlobWriter(string? basePath = null, BlobSerializerOptions? serializerOptions = null);

		#endregion

		#region Aliases

		/// <summary>
		/// Adds an alias to a given blob
		/// </summary>
		/// <param name="name">Alias for the blob</param>
		/// <param name="handle">Locator for the blob</param>
		/// <param name="rank">Rank for this alias. In situations where an alias has multiple mappings, the alias with the highest rank will be returned by default.</param>
		/// <param name="data">Additional data to be stored inline with the alias</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task AddAliasAsync(string name, IBlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default);

		/// <summary>
		/// Removes an alias from a blob
		/// </summary>
		/// <param name="name">Name of the alias</param>
		/// <param name="handle">Locator for the blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task RemoveAliasAsync(string name, IBlobHandle handle, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds blobs with the given alias. Unlike refs, aliases do not serve as GC roots.
		/// </summary>
		/// <param name="name">Alias for the blob</param>
		/// <param name="maxResults">Maximum number of aliases to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Blobs matching the given handle</returns>
		Task<BlobAlias[]> FindAliasesAsync(string name, int? maxResults = null, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <summary>
		/// Reads data for a ref from the store
		/// </summary>
		/// <param name="name">The ref name</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Blob pointed to by the ref</returns>
		Task<IBlobRef?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a new ref to the store
		/// </summary>
		/// <param name="name">Ref to write</param>
		/// <param name="target">Handle to the target blob</param>
		/// <param name="options">Options for the new ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Unique identifier for the blob</returns>
		Task WriteRefAsync(RefName name, IBlobRef target, RefOptions? options = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Reads data for a ref from the store
		/// </summary>
		/// <param name="name">The ref identifier</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default);

		#endregion

		/// <summary>
		/// Gets a snapshot of the stats for the storage client.
		/// </summary>
		void GetStats(StorageStats stats);
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
		/// <returns>Storage client instance. May be null if the namespace does not exist.</returns>
		IStorageClient? TryCreateClient(NamespaceId namespaceId);
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageClientFactory"/>
	/// </summary>
	public static class StorageClientFactoryExtensions
	{
		/// <summary>
		/// Creates a new storage client, throwing an exception if it does not exist
		/// </summary>
		public static IStorageClient CreateClient(this IStorageClientFactory factory, NamespaceId namespaceId)
		{
			IStorageClient? client = factory.TryCreateClient(namespaceId);
			if (client == null)
			{
				throw new InvalidOperationException($"No namespace '{namespaceId}' is configured");
			}
			return client;
		}
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
		public readonly TimeSpan MaxAge => DateTime.UtcNow - Utc;

		/// <summary>
		/// Sets the earliest time at which the entry must have been valid
		/// </summary>
		/// <param name="age">Maximum age of any returned cache value. Taken from the moment that this object was created.</param>
		public RefCacheTime(TimeSpan age) : this(DateTime.UtcNow - age) { }

		/// <summary>
		/// Tests whether this value is set
		/// </summary>
		public readonly bool IsSet() => Utc != default;

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
	/// Stats for the storage system
	/// </summary>
	public class StorageStats
	{
		/// <summary>
		/// Stat name to value
		/// </summary>
		public List<(string, long)> Values { get; } = new List<(string, long)>();

		/// <summary>
		/// Add a new stat to the list
		/// </summary>
		public void Add(string name, long value) => Values.Add((name, value));

		/// <summary>
		/// Prints the table of stats to the logger
		/// </summary>
		public void Print(ILogger logger)
		{
			foreach ((string key, long value) in Values)
			{
				logger.LogInformation("{Key}: {Value:n0}", key, value);
			}
		}

		/// <summary>
		/// Subtract a base set of stats from this one
		/// </summary>
		public static StorageStats GetDelta(StorageStats initial, StorageStats finish)
		{
			StorageStats result = new StorageStats();

			Dictionary<string, long> initialValues = initial.Values.ToDictionary(x => x.Item1, x => x.Item2, StringComparer.Ordinal);
			foreach ((string name, long value) in finish.Values.ToArray())
			{
				initialValues.TryGetValue(name, out long otherValue);
				result.Add(name, value - otherValue);
			}

			return result;
		}
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageClient"/>
	/// </summary>
	public static class StorageClientExtensions
	{
		#region Blobs

		/// <summary>
		/// Create a blob ref from a RefValue
		/// </summary>
		public static IBlobRef CreateBlobRef(this IStorageClient store, BlobRefValue refValue)
			=> store.CreateBlobRef(refValue.Hash, refValue.Locator);

		/// <summary>
		/// Create a typed blob ref from a RefValue
		/// </summary>
		public static IBlobRef<T> CreateBlobRef<T>(this IStorageClient store, BlobRefValue refValue, BlobSerializerOptions? options)
			=> store.CreateBlobRef<T>(refValue.Hash, refValue.Locator, options);

		/// <summary>
		/// Creates a writer using a refname as a base path
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="refName">Ref name to use as a base path</param>
		public static IBlobWriter CreateBlobWriter(this IStorageClient store, RefName refName) => store.CreateBlobWriter(refName.ToString());

		#endregion

		#region Aliases

		/// <summary>
		/// Finds blobs with the given alias. Unlike refs, aliases do not serve as GC roots.
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Alias for the blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Blobs matching the given handle</returns>
		public static async Task<BlobAlias?> FindAliasAsync(this IStorageClient store, string name, CancellationToken cancellationToken = default)
		{
			BlobAlias[] aliases = await store.FindAliasesAsync(name, 1, cancellationToken);
			return aliases.FirstOrDefault();
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
		public static async Task<bool> RefExistsAsync(this IStorageClient store, RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			IBlobHandle? target = await store.TryReadRefAsync(name, cacheTime, cancellationToken);
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
		public static async Task<IBlobRef> ReadRefAsync(this IStorageClient store, RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			IBlobRef? refTarget = await store.TryReadRefAsync(name, cacheTime, cancellationToken);
			return refTarget ?? throw new RefNameNotFoundException(name);
		}

		#endregion

		/// <summary>
		/// Gets a snapshot of the stats for the storage client.
		/// </summary>
		public static StorageStats GetStats(this IStorageClient store)
		{
			StorageStats stats = new StorageStats();
			store.GetStats(stats);
			return stats;
		}
	}
}
