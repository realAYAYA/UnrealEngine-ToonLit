// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;

namespace Jupiter.Implementation.Blob;

public class CachedBlobIndex : IBlobIndex
{
	private readonly FileSystemStore _fileSystemStore;

	public CachedBlobIndex(FileSystemStore fileSystemStore)
	{
		_fileSystemStore = fileSystemStore;
	}

	public async Task AddBlobToIndexAsync(NamespaceId ns, BlobId id, string? region = null)
	{
		// We do not actually track any blob information when running in cached mode
		await Task.CompletedTask;
	}

	public async Task RemoveBlobFromRegionAsync(NamespaceId ns, BlobId id, string? region = null)
	{
		// We do not actually track any blob information when running in cached mode
		await Task.CompletedTask;
	}

	public IAsyncEnumerable<BaseBlobReference> GetBlobReferencesAsync(NamespaceId ns, BlobId id)
	{
		throw new NotImplementedException();
	}

	public async Task<bool> BlobExistsInRegionAsync(NamespaceId ns, BlobId blobIdentifier, string? region = null)
	{
		return await _fileSystemStore.ExistsAsync(ns, blobIdentifier, forceCheck: false);
	}

	public async Task AddRefToBlobsAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId[] blobs)
	{
		// We do not actually track any blob information when running in cached mode
		await Task.CompletedTask;
	}

	public IAsyncEnumerable<(NamespaceId, BlobId)> GetAllBlobsAsync()
	{
		throw new NotImplementedException();
	}

	public Task RemoveReferencesAsync(NamespaceId ns, BlobId id, List<BaseBlobReference>? referencesToRemove)
	{
		// We do not actually track any blob information when running in cached mode
		return Task.CompletedTask;
	}

	public Task<List<string>> GetBlobRegionsAsync(NamespaceId ns, BlobId blob)
	{
		throw new NotImplementedException();
	}

	public Task AddBlobReferencesAsync(NamespaceId ns, BlobId sourceBlob, BlobId targetBlob)
	{
		throw new NotImplementedException();
	}

	public Task AddBlobToBucketListAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobId, long blobSize)
	{
		throw new NotImplementedException();
	}

	public Task RemoveBlobFromBucketListAsync(NamespaceId ns, BucketId bucket, RefId key, List<BlobId> blobIds)
	{
		throw new NotImplementedException();
	}

	public Task<BucketStats> CalculateBucketStatisticsAsync(NamespaceId ns, BucketId bucket)
	{
		throw new NotImplementedException();
	}
}
