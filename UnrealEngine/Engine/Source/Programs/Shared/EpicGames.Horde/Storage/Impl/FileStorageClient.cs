// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage.Impl
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which writes directly to the local filesystem for testing. Not intended for production use.
	/// </summary>
	public class FileStorageClient : IStorageClient
	{
		class Ref : IRef
		{
			public NamespaceId NamespaceId { get; set; }
			public BucketId BucketId { get; set; }
			public RefId RefId { get; set; }
			public CbObject Value { get; set; }

			public Ref(NamespaceId namespaceId, BucketId bucketId, RefId refId, CbObject value)
			{
				NamespaceId = namespaceId;
				BucketId = bucketId;
				RefId = refId;
				Value = value;
			}
		}

		/// <summary>
		/// Base directory for storing files
		/// </summary>
		public DirectoryReference BaseDir { get; }

		private readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="baseDir"></param>
		/// <param name="logger"></param>
		public FileStorageClient(DirectoryReference baseDir, ILogger logger)
		{
			BaseDir = baseDir;
			_logger = logger;

			DirectoryReference.CreateDirectory(baseDir);
		}

		FileReference GetBlobFile(NamespaceId namespaceId, IoHash hash)
		{
			return FileReference.Combine(BaseDir, namespaceId.ToString(), $"{hash}.blob");
		}

		/// <inheritdoc/>
		public Task<Stream> ReadBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default)
		{
			FileReference file = GetBlobFile(namespaceId, hash);
			_logger.LogInformation("Reading {File} ({Size:n0} bytes)", file, new FileInfo(file.FullName).Length);
			return Task.FromResult<Stream>(FileReference.Open(file, FileMode.Open, FileAccess.Read, FileShare.Read));
		}
		
		/// <inheritdoc/>
		public Task<Stream> ReadCompressedBlobAsync(NamespaceId namespaceId, IoHash uncompressedHash, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public async Task WriteBlobAsync(NamespaceId namespaceId, IoHash hash, Stream stream, CancellationToken cancellationToken = default)
		{
			FileReference file = GetBlobFile(namespaceId, hash);
			DirectoryReference.CreateDirectory(file.Directory);

			_logger.LogInformation("Writing {File} ({Size:n0} bytes)", file, stream.Length);
			using (Stream outputStream = FileReference.Open(file, FileMode.Create, FileAccess.Write, FileShare.Read))
			{
				await stream.CopyToAsync(outputStream, cancellationToken);
			}
		}

		/// <inheritdoc/>
		public async Task<IoHash> WriteBlobAsync(NamespaceId namespaceId, Stream stream, CancellationToken cancellationToken = default)
		{
			byte[] data;
			using (MemoryStream memoryStream = new MemoryStream())
			{
				await stream.CopyToAsync(memoryStream, cancellationToken);
				data = memoryStream.ToArray();
			}
			return await this.WriteBlobFromMemoryAsync(namespaceId, data, cancellationToken);
		}

		/// <inheritdoc/>
		public Task WriteCompressedBlobAsync(NamespaceId namespaceId, IoHash uncompressedHash, Stream stream, CancellationToken cancellationToken = default)
		{
			throw new NotImplementedException();
		}

		/// <inheritdoc/>
		public Task<IoHash> WriteCompressedBlobAsync(NamespaceId namespaceId, Stream compressedStream, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public Task<bool> HasBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default)
		{
			return Task.FromResult(FileReference.Exists(GetBlobFile(namespaceId, hash)));
		}

		/// <inheritdoc/>
		public Task<HashSet<IoHash>> FindMissingBlobsAsync(NamespaceId namespaceId, HashSet<IoHash> hashes, CancellationToken cancellationToken)
		{
			return Task.FromResult(hashes.Where(x => !FileReference.Exists(GetBlobFile(namespaceId, x))).ToHashSet());
		}

		/// <inheritdoc/>
		public Task<bool> DeleteRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(namespaceId, bucketId, refId);
			if (FileReference.Exists(file))
			{
				FileReference.Delete(file);
				return Task.FromResult(true);
			}
			return Task.FromResult(false);
		}

		/// <inheritdoc/>
		public Task<List<RefId>> FindMissingRefsAsync(NamespaceId namespaceId, BucketId bucketId, List<RefId> refIds, CancellationToken cancellationToken = default)
		{
			return Task.FromResult(refIds.Where(x => !FileReference.Exists(GetRefFile(namespaceId, bucketId, x))).ToList());
		}

		FileReference GetRefFile(NamespaceId namespaceId, BucketId bucketId, RefId refId)
		{
			return FileReference.Combine(BaseDir, namespaceId.ToString(), bucketId.ToString(), $"{refId}.ref");
		}

		/// <inheritdoc/>
		public async Task<IRef> GetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(namespaceId, bucketId, refId);
			_logger.LogInformation("Reading {File} ({Size:n0} bytes)", file, new FileInfo(file.FullName).Length);
			byte[] data = await FileReference.ReadAllBytesAsync(file, cancellationToken);
			return new Ref(namespaceId, bucketId, refId, new CbObject(data));
		}

		/// <inheritdoc/>
		public Task<bool> HasRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default)
		{
			return Task.FromResult(FileReference.Exists(GetRefFile(namespaceId, bucketId, refId)));
		}

		/// <inheritdoc/>
		public Task<List<IoHash>> TryFinalizeRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, IoHash hash, CancellationToken cancellationToken = default)
		{
			throw new NotImplementedException();
		}

		/// <inheritdoc/>
		public async Task<List<IoHash>> TrySetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CbObject value, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(namespaceId, bucketId, refId);
			DirectoryReference.CreateDirectory(file.Directory);

			_logger.LogInformation("Writing {File} ({Size:n0} bytes)", file, value.GetView().Length);
			await FileReference.WriteAllBytesAsync(file, value.GetView().ToArray(), cancellationToken);
			return new List<IoHash>();
		}
	}
}
