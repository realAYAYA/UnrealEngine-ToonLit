// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Options;

namespace Jupiter.Implementation.Blob;

public class MemoryBlobIndex : IBlobIndex
{
    private class MemoryBlobInfo
    {
        public HashSet<string> Regions { get; init; } = new HashSet<string>();
        public NamespaceId Namespace { get; init; }
        public BlobIdentifier BlobIdentifier { get; init; } = null!;
        public List<BaseBlobReference> References { get; init; } = new List<BaseBlobReference>();
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

    private Task<MemoryBlobInfo?> GetBlobInfo(NamespaceId ns, BlobIdentifier id)
    {
        ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo> index = GetNamespaceContainer(ns);

        if (!index.TryGetValue(id, out MemoryBlobInfo? blobInfo))
        {
            return Task.FromResult<MemoryBlobInfo?>(null);
        }

        return Task.FromResult<MemoryBlobInfo?>(blobInfo);
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

    public async Task<bool> BlobExistsInRegion(NamespaceId ns, BlobIdentifier blobIdentifier, string? region = null)
    {
        string expectedRegion = region ?? _jupiterSettings.CurrentValue.CurrentSite;
        MemoryBlobInfo? blobInfo = await GetBlobInfo(ns, blobIdentifier);
        return blobInfo?.Regions.Contains(expectedRegion) ?? false;
    }

    public async IAsyncEnumerable<BaseBlobReference> GetBlobReferences(NamespaceId ns, BlobIdentifier id)
    {
        MemoryBlobInfo? blobInfo = await GetBlobInfo(ns, id);

        if (blobInfo != null)
        {
            foreach (BaseBlobReference reference in blobInfo.References)
            {
                yield return reference;
            }
        }
    }

    public Task AddRefToBlobs(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier[] blobs)
    {
        foreach (BlobIdentifier id in blobs)
        {
            ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo> index = GetNamespaceContainer(ns);

            index.AddOrUpdate(id, _ =>
            {
                MemoryBlobInfo info = NewBlobInfo(ns, id, _jupiterSettings.CurrentValue.CurrentSite);
                info.References!.Add(new RefBlobReference(bucket, key));
                return info;
            }, (_, info) =>
            {
                info.References!.Add(new RefBlobReference(bucket, key));
                return info;
            });
        }

        return Task.CompletedTask;
    }

    public async IAsyncEnumerable<(NamespaceId, BlobIdentifier)> GetAllBlobs()
    {
        await Task.CompletedTask;

        foreach (KeyValuePair<NamespaceId, ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo>> pair in _index)
        {
            foreach ((BlobIdentifier? _, MemoryBlobInfo? blobInfo) in pair.Value)
            {
                yield return (blobInfo.Namespace, blobInfo.BlobIdentifier);
            }
        }
    }

    public Task RemoveReferences(NamespaceId ns, BlobIdentifier id, List<BaseBlobReference> referencesToRemove)
    {
        ConcurrentDictionary<BlobIdentifier, MemoryBlobInfo> index = GetNamespaceContainer(ns);

        if (index.TryGetValue(id, out MemoryBlobInfo? blobInfo))
        {
            foreach (BaseBlobReference r in referencesToRemove)
            {
                blobInfo.References.Remove(r);
            }
        }

        return Task.CompletedTask;
    }

    public async Task<List<string>> GetBlobRegions(NamespaceId ns, BlobIdentifier blob)
    {
        MemoryBlobInfo? blobInfo = await GetBlobInfo(ns, blob);

        if (blobInfo != null)
        {
            return blobInfo.Regions.ToList();
        }

        throw new BlobNotFoundException(ns, blob);
    }

    public async Task AddBlobReferences(NamespaceId ns, BlobIdentifier sourceBlob, BlobIdentifier targetBlob)
    {
        MemoryBlobInfo? blobInfo = await GetBlobInfo(ns, sourceBlob);

        if (blobInfo == null)
        {
            throw new BlobNotFoundException(ns, sourceBlob);
        }

        blobInfo.References.Add(new BlobToBlobReference(targetBlob));
    }

    private static MemoryBlobInfo NewBlobInfo(NamespaceId ns, BlobIdentifier blob, string region)
    {
        MemoryBlobInfo info = new MemoryBlobInfo
        {
            Regions = new HashSet<string> { region },
            Namespace = ns,
            BlobIdentifier = blob,
            References = new List<BaseBlobReference>()
        };
        return info;
    }
}
