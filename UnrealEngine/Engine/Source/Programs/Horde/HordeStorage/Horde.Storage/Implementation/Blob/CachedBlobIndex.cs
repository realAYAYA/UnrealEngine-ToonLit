// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;

namespace Horde.Storage.Implementation.Blob;

public class CachedBlobIndex : IBlobIndex
{
    private readonly IOptionsMonitor<JupiterSettings> _jupiterSettings;
    private readonly FileSystemStore _fileSystemStore;

    public CachedBlobIndex(IOptionsMonitor<JupiterSettings> jupiterSettings, FileSystemStore fileSystemStore)
    {
        _jupiterSettings = jupiterSettings;
        _fileSystemStore = fileSystemStore;
    }

    public async Task<BlobInfo?> GetBlobInfo(NamespaceId ns, BlobIdentifier id)
    {
        if (await BlobExistsInRegion(ns, id))
        {
            return new BlobInfo
            {
                Namespace = ns,
                BlobIdentifier = id,
                References = new List<(BucketId, IoHashKey)>(),
                Regions = new HashSet<string> {_jupiterSettings.CurrentValue.CurrentSite}
            };
        }

        return null;
    }

    public async Task AddBlobToIndex(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        // We do not actually track any blob information when running in cached mode
        await Task.CompletedTask;
    }

    public async Task<bool> RemoveBlobFromIndex(NamespaceId ns, BlobIdentifier id)
    {
        // We do not actually track any blob information when running in cached mode
        await Task.CompletedTask;
        return false;
    }

    public async Task RemoveBlobFromRegion(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        // We do not actually track any blob information when running in cached mode
        await Task.CompletedTask;
    }

    public async Task<bool> BlobExistsInRegion(NamespaceId ns, BlobIdentifier blobIdentifier)
    {
        return await _fileSystemStore.Exists(ns, blobIdentifier, forceCheck: false);
    }

    public async Task AddRefToBlobs(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier[] blobs)
    {
        // We do not actually track any blob information when running in cached mode
        await Task.CompletedTask;
    }

    public IAsyncEnumerable<BlobInfo> GetAllBlobs()
    {
        throw new NotImplementedException();
    }
}
