// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// In-memory implementation of a storage backend
	/// </summary>
	public sealed class MemoryStorageBackend : IStorageBackend
	{
		record class AliasListNode(BlobLocator Locator, int Rank, ReadOnlyMemory<byte> Data, AliasListNode? Next);

		readonly ConcurrentDictionary<BlobLocator, ReadOnlyMemory<byte>> _blobs = new ConcurrentDictionary<BlobLocator, ReadOnlyMemory<byte>>();
		readonly ConcurrentDictionary<RefName, BlobRefValue> _refs = new ConcurrentDictionary<RefName, BlobRefValue>();
		readonly ConcurrentDictionary<string, AliasListNode?> _aliases = new ConcurrentDictionary<string, AliasListNode?>(StringComparer.Ordinal);

		/// <summary>
		/// All data stored by the client
		/// </summary>
		public IReadOnlyDictionary<BlobLocator, ReadOnlyMemory<byte>> Blobs => _blobs;

		/// <summary>
		/// Accessor for all refs stored by the client
		/// </summary>
		public IReadOnlyDictionary<RefName, BlobRefValue> Refs => _refs;

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		#region Blobs

		/// <inheritdoc/>
		public Task<Stream> OpenBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default)
		{
			ReadOnlyMemory<byte> memory = GetBlob(locator, offset, length);
			return Task.FromResult<Stream>(new ReadOnlyMemoryStream(memory));
		}

		/// <inheritdoc/>
		public Task<IReadOnlyMemoryOwner<byte>> ReadBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default)
		{
			ReadOnlyMemory<byte> memory = GetBlob(locator, offset, length);
			return Task.FromResult<IReadOnlyMemoryOwner<byte>>(ReadOnlyMemoryOwner.Create<byte>(memory));
		}

		ReadOnlyMemory<byte> GetBlob(BlobLocator locator, int offset, int? length)
		{
			ReadOnlyMemory<byte> memory = _blobs[locator];
			if (offset != 0)
			{
				memory = memory.Slice(offset);
			}
			if (length != null)
			{
				memory = memory.Slice(0, Math.Min(length.Value, memory.Length));
			}
			return memory;
		}

		/// <inheritdoc/>
		public async Task<BlobLocator> WriteBlobAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default)
		{
			BlobLocator locator = StorageHelpers.CreateUniqueLocator(prefix);
			_blobs[locator] = await stream.ReadAllBytesAsync(cancellationToken);
			return locator;
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetBlobReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default)
			=> default;

		/// <inheritdoc/>
		public ValueTask<(BlobLocator, Uri)?> TryGetBlobWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default)
			=> default;

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public Task AddAliasAsync(string name, BlobLocator locator, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			_aliases.AddOrUpdate(name, _ => new AliasListNode(locator, rank, data, null), (_, entry) => new AliasListNode(locator, rank, data, entry));
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public Task RemoveAliasAsync(string name, BlobLocator locator, CancellationToken cancellationToken = default)
		{
			for (; ; )
			{
				AliasListNode? entry;
				if (!_aliases.TryGetValue(name, out entry))
				{
					break;
				}

				AliasListNode? newEntry = RemoveAliasFromList(entry, locator);
				if (entry == newEntry)
				{
					break;
				}

				if (newEntry == null)
				{
					if (_aliases.TryRemove(new KeyValuePair<string, AliasListNode?>(name, entry)))
					{
						break;
					}
				}
				else
				{
					if (_aliases.TryUpdate(name, newEntry, entry))
					{
						break;
					}
				}
			}
			return Task.CompletedTask;
		}

		static AliasListNode? RemoveAliasFromList(AliasListNode? entry, BlobLocator locator)
		{
			if (entry == null)
			{
				return null;
			}
			if (entry.Locator == locator)
			{
				return entry.Next;
			}

			AliasListNode? nextEntry = RemoveAliasFromList(entry.Next, locator);
			if (nextEntry != entry.Next)
			{
				entry = new AliasListNode(entry.Locator, entry.Rank, entry.Data, nextEntry);
			}
			return entry;
		}

		/// <inheritdoc/>
		public Task<BlobAliasLocator[]> FindAliasesAsync(string alias, int? maxResults = null, CancellationToken cancellationToken = default)
		{
			List<BlobAliasLocator> aliases = new List<BlobAliasLocator>();
			if (_aliases.TryGetValue(alias, out AliasListNode? entry))
			{
				for (; entry != null; entry = entry.Next)
				{
					aliases.Add(new BlobAliasLocator(entry.Locator, entry.Rank, entry.Data));
				}
			}
			return Task.FromResult(aliases.ToArray());
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken) => Task.FromResult(_refs.TryRemove(name, out _));

		/// <inheritdoc/>
		public Task<BlobRefValue?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobRefValue? value;
			if (_refs.TryGetValue(name, out value))
			{
				return Task.FromResult<BlobRefValue?>(value);
			}
			else
			{
				return Task.FromResult<BlobRefValue?>(null);
			}
		}

		/// <inheritdoc/>
		public Task WriteRefAsync(RefName name, BlobRefValue value, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			_refs[name] = value;
			return Task.CompletedTask;
		}

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
		{
		}
	}
}
