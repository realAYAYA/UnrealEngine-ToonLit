// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;

namespace Horde.Storage.Implementation.Blob;

public class MemoryBlobIndex : IBlobIndex
{
    private class MemoryBlobInfo : BlobInfo
    {

    }

    private readonly ConcurrentDictionary<NamespaceId, ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo>> _index = new ();
    private readonly IOptionsMonitor<JupiterSettings> _jupiterSettings;

    public MemoryBlobIndex(IOptionsMonitor<JupiterSettings> settings)
    {
        _jupiterSettings = settings;
    }

    private ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo> GetNamespaceContainer(NamespaceId ns)
    {
        return _index.GetOrAdd(ns, id => new ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo>());
    }

    public Task AddBlobToIndex(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;
        ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo> index = GetNamespaceContainer(ns);
        index[id] = NewBlobInfo(ns, id, region);
        return Task.CompletedTask;
    }

    public Task<BlobInfo?> GetBlobInfo(NamespaceId ns, BlobIdentifier id)
    {
        ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo> index = GetNamespaceContainer(ns);

        if (!index.TryGetValue(id, out MemoryBlobInfo? blobInfo))
        {
            return Task.FromResult<BlobInfo?>(null);
        }

        return Task.FromResult<BlobInfo?>(blobInfo);
    }

    public Task<bool> RemoveBlobFromIndex(NamespaceId ns, BlobIdentifier id)
    {
        ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo> index = GetNamespaceContainer(ns);

        return Task.FromResult(index.TryRemove(id, out MemoryBlobInfo? _));
    }

    public Task RemoveBlobFromRegion(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;
        ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo> index = GetNamespaceContainer(ns);

        index.AddOrUpdate(id, _ =>
        {
            MemoryBlobInfo info = NewBlobInfo(ns, id, region);
            return info;
        }, (_, info) =>
        {
            info.Regions.Remove(region);
            return info;
        });
        return Task.CompletedTask;
    }

    public async Task<bool> BlobExistsInRegion(NamespaceId ns, BlobIdentifier blobIdentifier)
    {
        BlobInfo? blobInfo = await GetBlobInfo(ns, blobIdentifier);
        return blobInfo?.Regions.Contains(_jupiterSettings.CurrentValue.CurrentSite) ?? false;
    }

    public Task AddRefToBlobs(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier[] blobs)
    {
        foreach (BlobIdentifier id in blobs)
        {
            ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo> index = GetNamespaceContainer(ns);

            index.AddOrUpdate(id, _ =>
            {
                MemoryBlobInfo info = NewBlobInfo(ns, id, _jupiterSettings.CurrentValue.CurrentSite);
                info.References.Add((bucket, key));
                return info;
            }, (_, info) =>
            {
                info.References.Add((bucket, key));
                return info;
            });
        }

        return Task.CompletedTask;
    }

    public async IAsyncEnumerable<BlobInfo> GetAllBlobs()
    {
        await Task.CompletedTask;

        foreach (KeyValuePair<NamespaceId, ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo>> pair in _index)
        {
            foreach ((BlobIdentifier? _, MemoryBlobInfo? blobInfo) in pair.Value)
            {
                yield return blobInfo;
            }
        }
    }

    private static MemoryBlobInfo NewBlobInfo(NamespaceId ns, BlobIdentifier blob, string region)
    {
        MemoryBlobInfo info = new MemoryBlobInfo
        {
            Regions = new HashSet<string> { region },
            Namespace = ns,
            BlobIdentifier = blob,
        };
        return info;
    }
}
