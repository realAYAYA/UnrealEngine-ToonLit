// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Options;

namespace Jupiter.Implementation.Blob;

public class CachedBlobIndex : IBlobIndex
{
    private readonly IOptionsMonitor<JupiterSettings> _jupiterSettings;
    private readonly FileSystemStore _fileSystemStore;

    public CachedBlobIndex(IOptionsMonitor<JupiterSettings> jupiterSettings, FileSystemStore fileSystemStore)
    {
        _jupiterSettings = jupiterSettings;
        _fileSystemStore = fileSystemStore;
    }

    public async Task AddBlobToIndex(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        // We do not actually track any blob information when running in cached mode
        await Task.CompletedTask;
    }

    public async Task RemoveBlobFromRegion(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        // We do not actually track any blob information when running in cached mode
        await Task.CompletedTask;
    }

    public IAsyncEnumerable<BaseBlobReference> GetBlobReferences(NamespaceId ns, BlobIdentifier id)
    {
        throw new NotImplementedException();
    }

    public async Task<bool> BlobExistsInRegion(NamespaceId ns, BlobIdentifier blobIdentifier, string? region = null)
    {
        return await _fileSystemStore.Exists(ns, blobIdentifier, forceCheck: false);
    }

    public async Task AddRefToBlobs(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier[] blobs)
    {
        // We do not actually track any blob information when running in cached mode
        await Task.CompletedTask;
    }

    public IAsyncEnumerable<(NamespaceId, BlobIdentifier)> GetAllBlobs()
    {
        throw new NotImplementedException();
    }

    public Task RemoveReferences(NamespaceId ns, BlobIdentifier id, List<BaseBlobReference> referencesToRemove)
    {
        // We do not actually track any blob information when running in cached mode
        return Task.CompletedTask;
    }

    public Task<List<string>> GetBlobRegions(NamespaceId ns, BlobIdentifier blob)
    {
        throw new NotImplementedException();
    }

    public Task AddBlobReferences(NamespaceId ns, BlobIdentifier sourceBlob, BlobIdentifier targetBlob)
    {
        throw new NotImplementedException();
    }
}
