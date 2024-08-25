// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.ObjectStores
{
	/// <summary>
	/// In-memory implementation of a storage backend
	/// </summary>
	public sealed class MemoryObjectStore : IObjectStore
	{
		readonly ConcurrentDictionary<ObjectKey, byte[]> _keyToData = new ConcurrentDictionary<ObjectKey, byte[]>();

		/// <summary>
		/// Read only access to the stored blobs
		/// </summary>
		public IReadOnlyDictionary<ObjectKey, byte[]> Blobs => _keyToData;

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		/// <inheritdoc/>
		public Task<Stream> OpenAsync(ObjectKey key, int offset, int? length, CancellationToken cancellationToken)
		{
			return Task.FromResult<Stream>(new ReadOnlyMemoryStream(GetData(key, offset, length)));
		}

		/// <inheritdoc/>
		public Task<IReadOnlyMemoryOwner<byte>> ReadAsync(ObjectKey key, int offset, int? length, CancellationToken cancellationToken)
		{
			return Task.FromResult(ReadOnlyMemoryOwner.Create(GetData(key, offset, length)));
		}

		ReadOnlyMemory<byte> GetData(ObjectKey key, int offset, int? length)
		{
			ReadOnlyMemory<byte> data = _keyToData[key].AsMemory(offset);
			if (length != null && length.Value < data.Length)
			{
				data = data.Slice(0, length.Value);
			}
			return data;
		}

		/// <inheritdoc/>
		public async Task WriteAsync(ObjectKey key, Stream stream, CancellationToken cancellationToken = default)
		{
			using (MemoryStream buffer = new MemoryStream())
			{
				await stream.CopyToAsync(buffer, cancellationToken);
				_keyToData[key] = buffer.ToArray();
			}
		}

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(ObjectKey key, CancellationToken cancellationToken)
		{
			return Task.FromResult(_keyToData.ContainsKey(key));
		}

		/// <inheritdoc/>
		public Task DeleteAsync(ObjectKey key, CancellationToken cancellationToken)
		{
			_keyToData.TryRemove(key, out _);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<ObjectKey> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			foreach (ObjectKey key in _keyToData.Keys)
			{
				yield return key;
				cancellationToken.ThrowIfCancellationRequested();
				await Task.Yield();
			}
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(ObjectKey key, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetWriteRedirectAsync(ObjectKey key, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public void GetStats(StorageStats stats) { }
	}
}
