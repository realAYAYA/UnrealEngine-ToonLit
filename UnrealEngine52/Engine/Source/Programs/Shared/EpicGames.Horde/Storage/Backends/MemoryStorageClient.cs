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

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which stores data in memory. Not intended for production use.
	/// </summary>
	public class MemoryStorageClient : StorageClientBase
	{
		record class ExportEntry(NodeHandle Handle, ExportEntry? Next);

		/// <summary>
		/// Map of blob id to blob data
		/// </summary>
		readonly ConcurrentDictionary<BlobLocator, Bundle> _blobs = new ConcurrentDictionary<BlobLocator, Bundle>();

		/// <summary>
		/// Map of ref name to ref data
		/// </summary>
		readonly ConcurrentDictionary<RefName, NodeHandle> _refs = new ConcurrentDictionary<RefName, NodeHandle>();

		/// <summary>
		/// Content addressed data lookup
		/// </summary>
		readonly ConcurrentDictionary<Utf8String, ExportEntry> _exports = new ConcurrentDictionary<Utf8String, ExportEntry>();

		/// <inheritdoc cref="_blobs"/>
		public IReadOnlyDictionary<BlobLocator, Bundle> Blobs => _blobs;

		/// <inheritdoc cref="_refs"/>
		public IReadOnlyDictionary<RefName, NodeHandle> Refs => _refs;

		/// <summary>
		/// Constructor
		/// </summary>
		public MemoryStorageClient() 
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

			for (int idx = 0; idx < bundle.Header.Exports.Count; idx++)
			{
				BundleExport export = bundle.Header.Exports[idx];
				if(!export.Alias.IsEmpty)
				{
					NodeHandle handle = new NodeHandle(export.Hash, locator, idx);
					_exports.AddOrUpdate(export.Alias, _ => new ExportEntry(handle, null), (_, entry) => new ExportEntry(handle, entry));
				}
			}

			return locator;
		}

		#endregion

		#region Nodes

		/// <inheritdoc/>
		public override async IAsyncEnumerable<NodeHandle> FindNodesAsync(Utf8String alias, [EnumeratorCancellation] CancellationToken cancellationToken = default)
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
		public override Task<NodeHandle?> TryReadRefTargetAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			NodeHandle? refTarget;
			_refs.TryGetValue(name, out refTarget);
			return Task.FromResult(refTarget);
		}

		/// <inheritdoc/>
		public override Task WriteRefTargetAsync(RefName name, NodeHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			_refs[name] = target;
			return Task.CompletedTask;
		}

		#endregion
	}
}
