// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Implementation of <see cref="IBlobStore"/> which stores data in memory. Not intended for production use.
	/// </summary>
	public class InMemoryBlobStore : IBlobStore
	{
		/// <summary>
		/// Map of blob id to blob data
		/// </summary>
		readonly ConcurrentDictionary<BlobId, IBlob> _blobs = new ConcurrentDictionary<BlobId, IBlob>();

		/// <summary>
		/// Map of ref name to ref data
		/// </summary>
		readonly ConcurrentDictionary<RefName, BlobId> _refs = new ConcurrentDictionary<RefName, BlobId>();

		/// <inheritdoc cref="_blobs"/>
		public IReadOnlyDictionary<BlobId, IBlob> Blobs => _blobs;

		/// <inheritdoc cref="_refs"/>
		public IReadOnlyDictionary<RefName, BlobId> Refs => _refs;

		#region Blobs

		/// <inheritdoc/>
		public Task<IBlob> ReadBlobAsync(BlobId blobId, CancellationToken cancellationToken = default)
		{
			return Task.FromResult(_blobs[blobId]);
		}

		/// <inheritdoc/>
		public Task<BlobId> WriteBlobAsync(ReadOnlySequence<byte> data, IReadOnlyList<BlobId> references, Utf8String prefix = default, CancellationToken cancellationToken = default)
		{
			BlobId blobId = BlobId.Create(ServerId.Empty, prefix);

			IBlob blob = Blob.FromMemory(blobId, data.ToArray(), references);
			_blobs[blobId] = blob;

			return Task.FromResult(blobId);
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public Task DeleteRefAsync(RefName name, CancellationToken cancellationToken) => Task.FromResult(_refs.TryRemove(name, out _));

		/// <inheritdoc/>
		public Task<bool> HasRefAsync(RefName name, CancellationToken cancellationToken) => Task.FromResult(_refs.ContainsKey(name));

		/// <inheritdoc/>
		public Task<IBlob?> TryReadRefAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			IBlob? blob = null;
			if (_refs.TryGetValue(name, out BlobId blobId))
			{
				_blobs.TryGetValue(blobId, out blob);
			}
			return Task.FromResult(blob);
		}

		/// <inheritdoc/>
		public Task<BlobId> TryReadRefTargetAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			if (_refs.TryGetValue(name, out BlobId blobId))
			{
				return Task.FromResult(blobId);
			}
			else
			{
				return Task.FromResult(BlobId.Empty);
			}
		}

		/// <inheritdoc/>
		public async Task<BlobId> WriteRefAsync(RefName name, ReadOnlySequence<byte> data, IReadOnlyList<BlobId> references, CancellationToken cancellationToken)
		{
			BlobId blobId = await WriteBlobAsync(data, references, name.Text, cancellationToken);
			_refs[name] = blobId;
			return blobId;
		}

		/// <inheritdoc/>
		public Task WriteRefTargetAsync(RefName name, BlobId blobId, CancellationToken cancellationToken = default)
		{
			_refs[name] = blobId;
			return Task.CompletedTask;
		}

		#endregion
	}
}
