// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Cassandra;
using Cassandra.Mapping;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;

namespace Horde.Storage.Implementation.Blob;

public class ScyllaBlobIndex : IBlobIndex
{
    private readonly IScyllaSessionManager _scyllaSessionManager;
    private readonly IOptionsMonitor<JupiterSettings> _jupiterSettings;
    private readonly ISession _session;
    private readonly Mapper _mapper;

    public ScyllaBlobIndex(IScyllaSessionManager scyllaSessionManager, IOptionsMonitor<JupiterSettings> jupiterSettings)
    {
        _scyllaSessionManager = scyllaSessionManager;
        _jupiterSettings = jupiterSettings;
        _session = scyllaSessionManager.GetSessionForReplicatedKeyspace();
        _mapper = new Mapper(_session);

        _session.Execute(new SimpleStatement(@"CREATE TABLE IF NOT EXISTS blob_index (
            namespace text,
            blob_id frozen<blob_identifier>,
            regions set<text>,
            references set<frozen<object_reference>>,
            PRIMARY KEY ((namespace, blob_id))
        );"
        ));
    }

    public async Task AddBlobToIndex(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;
        using IScope scope = Tracer.Instance.StartActive("scylla.insert_blob_index");
        scope.Span.ResourceName = $"{ns}.{id}";

        await _mapper.UpdateAsync<ScyllaBlobIndexTable>("SET regions = regions + ? WHERE namespace = ? AND blob_id = ?",
            new string[] { region }, ns.ToString(), new ScyllaBlobIdentifier(id));
    }

    public async Task<BlobInfo?> GetBlobInfo(NamespaceId ns, BlobIdentifier id)
    {
        using IScope scope = Tracer.Instance.StartActive("scylla.fetch_blob_index");
        scope.Span.ResourceName = $"{ns}.{id}";

        ScyllaBlobIndexTable? blobIndex =
            await _mapper.FirstOrDefaultAsync<ScyllaBlobIndexTable>("WHERE namespace = ? AND blob_id = ?",
                ns.ToString(), new ScyllaBlobIdentifier(id));

        if (blobIndex == null)
        {
            return null;
        }

        return blobIndex.ToBlobInfo();
    }

    public async Task<bool> RemoveBlobFromIndex(NamespaceId ns, BlobIdentifier id)
    {
        using IScope scope = Tracer.Instance.StartActive("scylla.remove_from_blob_index");
        scope.Span.ResourceName = $"{ns}.{id}";

        await _mapper.DeleteAsync<ScyllaBlobIndexTable>("WHERE namespace = ? AND blob_id = ?", ns.ToString(),
            new ScyllaBlobIdentifier(id));

        return true;
    }

    public async Task RemoveBlobFromRegion(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;
        using IScope scope = Tracer.Instance.StartActive("scylla.remove_blob_index_region");
        scope.Span.ResourceName = $"{ns}.{id}";

        await _mapper.UpdateAsync<ScyllaBlobIndexTable>("SET regions = regions - ? WHERE namespace = ? AND blob_id = ?", new string[] { region }, ns.ToString(), new ScyllaBlobIdentifier(id));

        BlobInfo? blobInfo = await GetBlobInfo(ns, id);
        if (blobInfo != null && blobInfo.Regions.Count == 0)
        {
            await RemoveBlobFromIndex(ns, id);
        }
    }

    public async Task<bool> BlobExistsInRegion(NamespaceId ns, BlobIdentifier blobIdentifier)
    {
        BlobInfo? blobInfo = await GetBlobInfo(ns, blobIdentifier);
        return blobInfo?.Regions.Contains(_jupiterSettings.CurrentValue.CurrentSite) ?? false;
    }

    public async Task AddRefToBlobs(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier[] blobs)
    {
        using IScope _ = Tracer.Instance.StartActive("scylla.add_ref_blobs");

        string nsAsString = ns.ToString();
        ScyllaObjectReference reference = new ScyllaObjectReference(bucket, key);
        Task[] refUpdateTasks = new Task[blobs.Length];
        for (int i = 0; i < blobs.Length; i++)
        {
            BlobIdentifier id = blobs[i];
            refUpdateTasks[i] = _mapper.UpdateAsync<ScyllaBlobIndexTable>(
                "SET references = references + ? WHERE namespace = ? AND blob_id = ?",
                new[] { reference },
                nsAsString,
                new ScyllaBlobIdentifier(id));
        }

        await Task.WhenAll(refUpdateTasks);
    }

    public async IAsyncEnumerable<BlobInfo> GetAllBlobs()
    {
        using IScope _ = Tracer.Instance.StartActive("scylla.get_all_blobs");

        string cqlOptions = _scyllaSessionManager.IsScylla ? "BYPASS CACHE" : "";
        foreach (ScyllaBlobIndexTable blobIndex in await _mapper.FetchAsync<ScyllaBlobIndexTable>(cqlOptions))
        {
            yield return blobIndex.ToBlobInfo();
        }
    }
}

[Cassandra.Mapping.Attributes.Table("blob_index")]
class ScyllaBlobIndexTable
{
    public ScyllaBlobIndexTable()
    {
        Namespace = null!;
        BlobId = null!;
        Regions = null;
        References = null;
    }

    public ScyllaBlobIndexTable(string @namespace, BlobIdentifier blobId, HashSet<string> regions, List<ScyllaObjectReference> references)
    {
        Namespace = @namespace;
        BlobId =  new ScyllaBlobIdentifier(blobId);
        Regions = regions;
        References = references;
    }

    [Cassandra.Mapping.Attributes.PartitionKey]
    public string Namespace { get; set; }

    [Cassandra.Mapping.Attributes.PartitionKey]
    [Cassandra.Mapping.Attributes.Column("blob_id")]
    public ScyllaBlobIdentifier BlobId { get;set; }

    [Cassandra.Mapping.Attributes.Column("regions")]
    public HashSet<string>? Regions { get; set; }

    [Cassandra.Mapping.Attributes.Column("references")]
    public List<ScyllaObjectReference>? References { get; set; }

    public BlobInfo ToBlobInfo()
    {
        return new BlobInfo
        {
            Regions = Regions ?? new HashSet<string>(),
            BlobIdentifier = BlobId.AsBlobIdentifier(),
            Namespace = new NamespaceId(Namespace),
            References = References != null ? References.Select(reference => reference.AsTuple()).ToList() : new List<(BucketId, IoHashKey)>()
        };
    }
}
