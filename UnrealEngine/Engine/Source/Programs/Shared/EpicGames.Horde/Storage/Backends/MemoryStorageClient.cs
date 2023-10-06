// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which stores data in memory. Not intended for production use.
	/// </summary>
	public class MemoryStorageClient : BundleStorageClient
	{
		record class ExportEntry(BlobHandle Handle, ExportEntry? Next);

		/// <summary>
		/// Map of blob id to blob data
		/// </summary>
		readonly ConcurrentDictionary<BlobLocator, Bundle> _blobs = new ConcurrentDictionary<BlobLocator, Bundle>();

		/// <summary>
		/// Map of ref name to ref data
		/// </summary>
		readonly ConcurrentDictionary<RefName, NodeLocator> _refs = new ConcurrentDictionary<RefName, NodeLocator>();

		/// <summary>
		/// Content addressed data lookup
		/// </summary>
		readonly ConcurrentDictionary<Utf8String, ExportEntry> _exports = new ConcurrentDictionary<Utf8String, ExportEntry>();

		/// <inheritdoc cref="_blobs"/>
		public IReadOnlyDictionary<BlobLocator, Bundle> Blobs => _blobs;

		/// <inheritdoc cref="_refs"/>
		public IReadOnlyDictionary<RefName, NodeLocator> Refs => _refs;

		/// <summary>
		/// Constructor
		/// </summary>
		public MemoryStorageClient() 
			: base(new MemoryCache(new MemoryCacheOptions()), NullLogger.Instance)
		{
		}

		#region Blobs

		/// <inheritdoc/>
		public override Task<Stream> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			Stream stream = new ReadOnlySequenceStream(_blobs[locator].AsSequence());
			return Task.FromResult(stream);
		}

		/// <inheritdoc/>
		public override Task<Stream> ReadBlobRangeAsync(BlobLocator locator, int offset, int length, CancellationToken cancellationToken = default)
		{
			Stream stream = new ReadOnlySequenceStream(_blobs[locator].AsSequence().Slice(offset));
			return Task.FromResult(stream);
		}

		/// <inheritdoc/>
		public override async Task<BlobLocator> WriteBlobAsync(Stream stream, Utf8String prefix = default, CancellationToken cancellationToken = default)
		{
			BlobLocator locator = BlobLocator.Create(HostId.Empty, prefix);
			Bundle bundle = await Bundle.FromStreamAsync(stream, cancellationToken);
			_blobs[locator] = bundle;

			return locator;
		}

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public override Task AddAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken = default)
		{
			_exports.AddOrUpdate(name, _ => new ExportEntry(handle, null), (_, entry) => new ExportEntry(handle, entry));
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public override async IAsyncEnumerable<BlobHandle> FindNodesAsync(Utf8String alias, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			if(_exports.TryGetValue(alias, out ExportEntry? entry))
			{
				for (; entry != null; entry = entry.Next)
				{
					cancellationToken.ThrowIfCancellationRequested();
					await Task.Yield();
					yield return entry.Handle;
				}
			}
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task DeleteRefAsync(RefName name, CancellationToken cancellationToken) => Task.FromResult(_refs.TryRemove(name, out _));

		/// <inheritdoc/>
		public override Task<BlobHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			NodeLocator hashedLocator;
			if (_refs.TryGetValue(name, out hashedLocator))
			{
				return Task.FromResult<BlobHandle?>(new FlushedNodeHandle(TreeReader, hashedLocator)); 
			}
			else
			{
				return Task.FromResult<BlobHandle?>(null);
			}
		}

		/// <inheritdoc/>
		public override async Task WriteRefTargetAsync(RefName name, BlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			_refs[name] = await target.FlushAsync(cancellationToken);
		}

		#endregion
	}
}
