// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Build.Server;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Storage.Services
{
	using BlobNotFoundException = EpicGames.Horde.Storage.BlobNotFoundException;
	using IRef = EpicGames.Horde.Storage.IRef;

	/// <summary>
	/// Simple implementation of <see cref="IStorageClient"/> which uses the current <see cref="IStorageBackend"/> implementation without garbage collection.
	/// </summary>
	public class BasicStorageClient : IStorageClient
	{
		class RefImpl : IRef
		{
			public string Id { get; set; } = String.Empty;

			[BsonElement("d")]
			public byte[] Data { get; set; } = null!;

			[BsonElement("f")]
			public bool Finalized { get; set; }

			[BsonElement("t")]
			public DateTime LastAccessTime { get; set; } = DateTime.UtcNow;

			public NamespaceId NamespaceId => new NamespaceId(Id.Split('/')[0]);
			public BucketId BucketId => new BucketId(Id.Split('/')[1]);
			public RefId RefId => new RefId(IoHash.Parse(Id.Split('/')[2]));

			public CbObject Value => new CbObject(Data);
		}

		readonly IMongoCollection<RefImpl> _refs;
		readonly IStorageBackend _storageBackend;

		/// <summary>
		/// 
		/// </summary>
		/// <param name="mongoService"></param>
		/// <param name="storageBackend"></param>
		public BasicStorageClient(MongoService mongoService, IStorageBackend<BasicStorageClient> storageBackend)
		{
			_refs = mongoService.GetCollection<RefImpl>("Storage.RefsV2");
			_storageBackend = storageBackend;
		}

		/// <inheritdoc/>
		public async Task<Stream> ReadBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken)
		{
			string path = GetFullBlobPath(namespaceId, hash);

			Stream? stream = await _storageBackend.ReadAsync(path);
			if(stream == null)
			{
				throw new BlobNotFoundException(namespaceId, hash);
			}

			return stream;
		}
		
		/// <inheritdoc/>
		public Task<Stream> ReadCompressedBlobAsync(NamespaceId namespaceId, IoHash uncompressedHash, CancellationToken cancellationToken)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public async Task WriteBlobAsync(NamespaceId namespaceId, IoHash hash, Stream stream, CancellationToken cancellationToken)
		{
			byte[] data;
			using (MemoryStream buffer = new MemoryStream())
			{
				await stream.CopyToAsync(buffer, cancellationToken);
				data = buffer.ToArray();
			}

			IoHash dataHash = IoHash.Compute(data);
			if (hash != dataHash)
			{
				throw new InvalidDataException($"Incorrect hash for data (was {dataHash}, expected {hash})");
			}

			string path = GetFullBlobPath(namespaceId, hash);
			await _storageBackend.WriteBytesAsync(path, data);
		}

		/// <inheritdoc/>
		public async Task<IoHash> WriteBlobAsync(NamespaceId namespaceId, Stream stream, CancellationToken cancellationToken)
		{
			byte[] data;
			using (MemoryStream buffer = new MemoryStream())
			{
				await stream.CopyToAsync(buffer, cancellationToken);
				data = buffer.ToArray();
			}

			IoHash dataHash = IoHash.Compute(data);

			string path = GetFullBlobPath(namespaceId, dataHash);
			await _storageBackend.WriteBytesAsync(path, data);

			return dataHash;
		}

		/// <inheritdoc/>
		public Task WriteCompressedBlobAsync(NamespaceId namespaceId, IoHash uncompressedHash, Stream stream, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public Task<IoHash> WriteCompressedBlobAsync(NamespaceId namespaceId, Stream compressedStream, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public async Task<bool> HasBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken)
		{
			return await _storageBackend.ExistsAsync(GetFullBlobPath(namespaceId, hash));
		}

		/// <inheritdoc/>
		public async Task<HashSet<IoHash>> FindMissingBlobsAsync(NamespaceId namespaceId, HashSet<IoHash> hashes, CancellationToken cancellationToken)
		{
			IEnumerable<Task<(IoHash Hash, bool Exists)>> tasks = hashes.Select(async hash => (Hash: hash, await _storageBackend.ExistsAsync(GetFullBlobPath(namespaceId, hash))));
			await Task.WhenAll(tasks);
			return tasks.Where(x => !x.Result.Exists).Select(x => x.Result.Hash).ToHashSet();
		}

		/// <inheritdoc/>
		public async Task<IRef> GetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken)
		{
			string id = GetFullRefId(namespaceId, bucketId, refId);

			RefImpl? refImpl = await _refs.FindOneAndUpdateAsync<RefImpl>(x => x.Id == id, Builders<RefImpl>.Update.Set(x => x.LastAccessTime, DateTime.UtcNow), new FindOneAndUpdateOptions<RefImpl> { ReturnDocument = ReturnDocument.After }, cancellationToken);
			if (refImpl == null || !refImpl.Finalized)
			{
				throw new RefNotFoundException(namespaceId, bucketId, refId);
			}

			return refImpl;
		}

		/// <inheritdoc/>
		public async Task<bool> HasRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken)
		{
			string id = GetFullRefId(namespaceId, bucketId, refId);
			RefImpl? refImpl = await _refs.Find<RefImpl>(x => x.Id == id).FirstOrDefaultAsync(cancellationToken);
			return refImpl != null;
		}

		/// <inheritdoc/>
		public async Task<List<IoHash>> TrySetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CbObject value, CancellationToken cancellationToken)
		{
			RefImpl newRef = new RefImpl();
			newRef.Id = GetFullRefId(namespaceId, bucketId, refId);
			newRef.Data = value.GetView().ToArray();
			await _refs.ReplaceOneAsync(x => x.Id == newRef.Id, newRef, new ReplaceOptions { IsUpsert = true }, cancellationToken);

			IoHash hash = IoHash.Compute(value.GetView().Span);
			return await TryFinalizeRefAsync(namespaceId, bucketId, refId, hash, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<List<IoHash>> TryFinalizeRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, IoHash hash, CancellationToken cancellationToken)
		{
			string id = GetFullRefId(namespaceId, bucketId, refId);

			RefImpl refImpl = await _refs.Find(x => x.Id == id).FirstAsync(cancellationToken);

			HashSet<IoHash> missingHashes = new HashSet<IoHash>();
			if (!refImpl.Finalized)
			{
				HashSet<IoHash> checkedObjectHashes = new HashSet<IoHash>();
				HashSet<IoHash> checkedBinaryHashes = new HashSet<IoHash>();
				await FinalizeInternalAsync(refImpl.NamespaceId, refImpl.Value, checkedObjectHashes, checkedBinaryHashes, missingHashes);

				if (missingHashes.Count == 0)
				{
					await _refs.UpdateOneAsync(x => x.Id == id, Builders<RefImpl>.Update.Set(x => x.Finalized, true), cancellationToken: cancellationToken);
				}
			}
			return missingHashes.ToList();
		}

		async Task FinalizeInternalAsync(NamespaceId namespaceId, CbObject obj, HashSet<IoHash> checkedObjectHashes, HashSet<IoHash> checkedBinaryHashes, HashSet<IoHash> missingHashes)
		{
			List<IoHash> newObjectHashes = new List<IoHash>();
			List<IoHash> newBinaryHashes = new List<IoHash>();
			obj.IterateAttachments(field =>
			{
				IoHash attachmentHash = field.AsAttachment();
				if (field.IsObjectAttachment())
				{
					if (checkedObjectHashes.Add(attachmentHash))
					{
						newObjectHashes.Add(attachmentHash);
					}
				}
				else
				{
					if (checkedBinaryHashes.Add(attachmentHash))
					{
						newBinaryHashes.Add(attachmentHash);
					}
				}
			});

			foreach (IoHash newHash in newObjectHashes)
			{
				string objPath = GetFullBlobPath(namespaceId, newHash);

				ReadOnlyMemory<byte>? data = await _storageBackend.ReadBytesAsync(objPath);
				if (data == null)
				{
					missingHashes.Add(newHash);
				}
				else
				{
					await FinalizeInternalAsync(namespaceId, new CbObject(data.Value), checkedObjectHashes, checkedBinaryHashes, missingHashes);
				}
			}

			foreach (IoHash newBinaryHash in newBinaryHashes)
			{
				string path = GetFullBlobPath(namespaceId, newBinaryHash);
				if (!await _storageBackend.ExistsAsync(path))
				{
					missingHashes.Add(newBinaryHash);
				}
			}
		}

		/// <inheritdoc/>
		public async Task<bool> DeleteRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken)
		{
			string id = GetFullRefId(namespaceId, bucketId, refId);
			DeleteResult result = await _refs.DeleteOneAsync(x => x.Id == id, cancellationToken);
			return result.DeletedCount > 0;
		}

		/// <inheritdoc/>
		public async Task<List<RefId>> FindMissingRefsAsync(NamespaceId namespaceId, BucketId bucketId, List<RefId> refIds, CancellationToken cancellationToken)
		{
			List<RefId> missingRefIds = new List<RefId>();
			foreach (RefId refId in refIds)
			{
				if(!await HasRefAsync(namespaceId, bucketId, refId, cancellationToken))
				{
					missingRefIds.Add(refId);
				}
			}
			return missingRefIds;
		}

		/// <summary>
		/// Gets the formatted id for a particular ref
		/// </summary>
		static string GetFullRefId(NamespaceId namespaceId, BucketId bucketId, RefId refId)
		{
			return $"{namespaceId}/{bucketId}/{refId}";
		}

		/// <summary>
		/// Gets the path for a blob
		/// </summary>
		static string GetFullBlobPath(NamespaceId namespaceId, IoHash hash)
		{
			string hashText = hash.ToString();
			return $"blobs2/{namespaceId}/{hashText[0..2]}/{hashText[2..4]}/{hashText}.blob";
		}
	}
}
