// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Horde.Build.Server;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Storage
{
	/// <summary>
	/// Implementation of <see cref="IBlobStore"/> using Mongo for storing refs, and an <see cref="IStorageBackend"/> implementation for bulk data.
	/// </summary>
	public class BasicBlobStore : IBlobStore
	{
		class RefDocument
		{
			[BsonId]
			public RefName Name { get; set; }
			public BlobId Value { get; set; }

			public RefDocument()
			{
				Name = RefName.Empty;
				Value = BlobId.Empty;
			}

			public RefDocument(RefName name, BlobId value)
			{
				Name = name;
				Value = value;
			}
		}

		class CachedRefValue
		{
			public RefName Name { get; }
			public BlobId Value { get; }
			public DateTime Time { get; }

			public CachedRefValue(RefName name, BlobId value)
			{
				Name = name;
				Value = value;
				Time = DateTime.UtcNow;
			}
		}

		readonly ServerId _serverId = ServerId.Empty;

		readonly IMongoCollection<RefDocument> _refs;
		readonly IStorageBackend _backend;
		readonly IMemoryCache _cache;

		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The mongo service implementation</param>
		/// <param name="backend">Backend to use for storing data</param>
		/// <param name="cache">Cache for ref values</param>
		/// <param name="logger">Logger for this instance</param>
		public BasicBlobStore(MongoService mongoService, IStorageBackend backend, IMemoryCache cache, ILogger<BasicBlobStore> logger)
		{
			_refs = mongoService.GetCollection<RefDocument>("Refs");
			_backend = backend;
			_cache = cache;
			_logger = logger;
 		}

		#region Blobs

		static string GetBlobPath(BlobId id) => $"{id.GetContentId()}.blob";

		/// <inheritdoc/>
		public async Task<IBlob> ReadBlobAsync(BlobId id, CancellationToken cancellationToken = default)
		{
			string path = GetBlobPath(id);

			ReadOnlyMemory<byte>? data = await _backend.ReadBytesAsync(path, cancellationToken);
			if (data == null)
			{
				throw new KeyNotFoundException($"Unable to read data from {path}");
			}

			return Blob.Deserialize(id, data.Value);
		}

		/// <inheritdoc/>
		public async Task<BlobId> WriteBlobAsync(ReadOnlySequence<byte> data, IReadOnlyList<BlobId> references, Utf8String prefix = default, CancellationToken cancellationToken = default)
		{
			BlobId id = BlobId.Create(_serverId, prefix);
			string path = GetBlobPath(id);

			ReadOnlySequence<byte> sequence = Blob.Serialize(data, references);
			using (ReadOnlySequenceStream stream = new ReadOnlySequenceStream(sequence))
			{
				await _backend.WriteAsync(path, stream, cancellationToken);
			}
			return id;
		}

		#endregion

		#region Refs

		CachedRefValue AddRefToCache(RefName name, BlobId value)
		{
			CachedRefValue cacheValue = new CachedRefValue(name, value);
			using (ICacheEntry newEntry = _cache.CreateEntry(name))
			{
				newEntry.Value = cacheValue;
				newEntry.SetSize(name.Text.Length + value.Inner.Length);
				newEntry.SetSlidingExpiration(TimeSpan.FromMinutes(5.0));
			}
			return cacheValue;
		}

		/// <inheritdoc/>
		public async Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
		{
			await _refs.DeleteOneAsync(x => x.Name == name, cancellationToken);
			AddRefToCache(name, BlobId.Empty);
		}

		/// <inheritdoc/>
		public async Task<IBlob?> TryReadRefAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobId blobId = await TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			if (blobId.IsValid())
			{
				try
				{
					return await ReadBlobAsync(blobId, cancellationToken);
				}
				catch (Exception ex)
				{
					_logger.LogWarning(ex, "Unable to read blob {BlobId} referenced by ref {RefName}", blobId, name);
				}
			}
			return null;
		}

		/// <inheritdoc/>
		public async Task<BlobId> TryReadRefTargetAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			CachedRefValue entry;
			if (!_cache.TryGetValue(name, out entry) || entry.Time < cacheTime)
			{
				RefDocument? refDocument = await _refs.Find(x => x.Name == name).FirstOrDefaultAsync(cancellationToken);
				BlobId value = (refDocument == null) ? BlobId.Empty : refDocument.Value;
				entry = AddRefToCache(name, value);
			}
			return entry.Value;
		}

		/// <inheritdoc/>
		public async Task<BlobId> WriteRefAsync(RefName name, ReadOnlySequence<byte> data, IReadOnlyList<BlobId> references, CancellationToken cancellationToken = default)
		{
			BlobId blobId = await WriteBlobAsync(data, references, name.Text, cancellationToken);
			await WriteRefTargetAsync(name, blobId, cancellationToken);
			return blobId;
		}

		/// <inheritdoc/>
		public async Task WriteRefTargetAsync(RefName name, BlobId blobId, CancellationToken cancellationToken = default)
		{
			RefDocument refDocument = new RefDocument(name, blobId);
			await _refs.ReplaceOneAsync(x => x.Name == name, refDocument, new ReplaceOptions { IsUpsert = true }, cancellationToken);
			AddRefToCache(name, blobId);
		}

		#endregion
	}
}
