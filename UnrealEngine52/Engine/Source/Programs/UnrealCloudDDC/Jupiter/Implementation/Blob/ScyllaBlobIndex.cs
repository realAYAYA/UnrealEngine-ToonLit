// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Cassandra;
using Cassandra.Mapping;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation.Blob;

public class ScyllaBlobIndex : IBlobIndex
{
    private readonly IScyllaSessionManager _scyllaSessionManager;
    private readonly IOptionsMonitor<JupiterSettings> _jupiterSettings;
    private readonly Tracer _tracer;
    private readonly ISession _session;
    private readonly Mapper _mapper;

    public ScyllaBlobIndex(IScyllaSessionManager scyllaSessionManager, IOptionsMonitor<JupiterSettings> jupiterSettings, Tracer tracer)
    {
        _scyllaSessionManager = scyllaSessionManager;
        _jupiterSettings = jupiterSettings;
        _tracer = tracer;
        _session = scyllaSessionManager.GetSessionForReplicatedKeyspace();
        _mapper = new Mapper(_session);

        string blobType = scyllaSessionManager.IsCassandra ? "blob" : "frozen<blob_identifier>";
        _session.Execute(new SimpleStatement(@$"CREATE TABLE IF NOT EXISTS blob_index (
            namespace text,
            blob_id {blobType},
            regions set<text>,
            references set<frozen<object_reference>>,
            PRIMARY KEY ((namespace, blob_id))
        );"
        ));
    }

    public async Task AddBlobToIndex(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;
        using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.insert_blob_index").SetAttribute("resource.name", $"{ns}.{id}");

        if (_scyllaSessionManager.IsScylla)
        {
            await _mapper.UpdateAsync<ScyllaBlobIndexTable>("SET regions = regions + ? WHERE namespace = ? AND blob_id = ?",
                new string[] { region }, ns.ToString(), new ScyllaBlobIdentifier(id));
        }
        else
        {
            await _mapper.UpdateAsync<CassandraBlobIndexTable>("SET regions = regions + ? WHERE namespace = ? AND blob_id = ?",
                new string[] { region }, ns.ToString(), id.HashData);
        }
    }

    public async Task<BlobInfo?> GetBlobInfo(NamespaceId ns, BlobIdentifier id, BlobIndexFlags flags = BlobIndexFlags.None)
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.fetch_blob_index").SetAttribute("resource.name", $"{ns}.{id}");

        bool includeReferences = (flags & BlobIndexFlags.IncludeReferences) != 0;

        if (_scyllaSessionManager.IsScylla)
        {
            ScyllaBlobIndexTable o;
            if (includeReferences)
            {
                o = await _mapper.SingleOrDefaultAsync<ScyllaBlobIndexTable>("SELECT namespace, blob_id, regions, references FROM blob_index WHERE namespace = ? AND blob_id = ?", ns.ToString(), new ScyllaBlobIdentifier(id));
            }
            else
            {
                o = await _mapper.SingleOrDefaultAsync<ScyllaBlobIndexTable>("SELECT namespace, blob_id, regions FROM blob_index WHERE namespace = ? AND blob_id = ?", ns.ToString(), new ScyllaBlobIdentifier(id));
            }
            return o?.ToBlobInfo();
        }
        else
        {
            CassandraBlobIndexTable o;
            if (includeReferences)
            {
                o = await _mapper.SingleOrDefaultAsync<CassandraBlobIndexTable>("SELECT namespace, blob_id, regions, references FROM blob_index WHERE namespace = ? AND blob_id = ?", ns.ToString(), id.HashData);
            }
            else
            {
                o = await _mapper.SingleOrDefaultAsync<CassandraBlobIndexTable>("SELECT namespace, blob_id, regions FROM blob_index WHERE namespace = ? AND blob_id = ?", ns.ToString(), id.HashData);
            }
            return o?.ToBlobInfo();
        }
    }

    public async Task<bool> RemoveBlobFromIndex(NamespaceId ns, BlobIdentifier id)
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.remove_from_blob_index").SetAttribute("resource.name", $"{ns}.{id}");

        if (_scyllaSessionManager.IsScylla)
        {
            await _mapper.DeleteAsync<ScyllaBlobIndexTable>("WHERE namespace = ? AND blob_id = ?", ns.ToString(),
                new ScyllaBlobIdentifier(id));
        }
        else
        {
            await _mapper.DeleteAsync<CassandraBlobIndexTable>("WHERE namespace = ? AND blob_id = ?", ns.ToString(),
                id.HashData);
        }

        return true;
    }

    public async Task RemoveBlobFromRegion(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.remove_blob_index_region").SetAttribute("resource.name", $"{ns}.{id}");

        if (_scyllaSessionManager.IsScylla)
        {
            await _mapper.UpdateAsync<ScyllaBlobIndexTable>("SET regions = regions - ? WHERE namespace = ? AND blob_id = ?", new string[] { region }, ns.ToString(), new ScyllaBlobIdentifier(id));
        }
        else
        {
            await _mapper.UpdateAsync<CassandraBlobIndexTable>("SET regions = regions - ? WHERE namespace = ? AND blob_id = ?", new string[] { region }, ns.ToString(), id.HashData);
        }

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
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.add_ref_blobs");

        string nsAsString = ns.ToString();
        ScyllaObjectReference reference = new ScyllaObjectReference(bucket, key);
        Task[] refUpdateTasks = new Task[blobs.Length];
        for (int i = 0; i < blobs.Length; i++)
        {
            BlobIdentifier id = blobs[i];
            Task updateTask;
            if (_scyllaSessionManager.IsScylla)
            {
                updateTask = _mapper.UpdateAsync<ScyllaBlobIndexTable>(
                    "SET references = references + ? WHERE namespace = ? AND blob_id = ?",
                    new[] { reference },
                    nsAsString,
                    new ScyllaBlobIdentifier(id));
            }
            else
            {
                updateTask = _mapper.UpdateAsync<CassandraBlobIndexTable>(
                    "SET references = references + ? WHERE namespace = ? AND blob_id = ?",
                    new[] { reference },
                    nsAsString,
                    id.HashData);
            }

            refUpdateTasks[i] = updateTask;
        }

        await Task.WhenAll(refUpdateTasks);
    }

    public async IAsyncEnumerable<BlobInfo> GetAllBlobs()
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.get_all_blobs");

        string cqlOptions = _scyllaSessionManager.IsScylla ? "BYPASS CACHE" : "";

        if (_scyllaSessionManager.IsScylla)
        {
            foreach (ScyllaBlobIndexTable blobIndex in await _mapper.FetchAsync<ScyllaBlobIndexTable>(cqlOptions))
            {
                yield return blobIndex.ToBlobInfo();
            }
        }
        else
        {
            foreach (CassandraBlobIndexTable blobIndex in await _mapper.FetchAsync<CassandraBlobIndexTable>(cqlOptions))
            {
                yield return blobIndex.ToBlobInfo();
            }
        }
    }

    public async Task RemoveReferences(NamespaceId ns, BlobIdentifier id, List<(BucketId,IoHashKey)> references)
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.remove_ref_blobs");

        string nsAsString = ns.ToString();
        (BucketId, IoHashKey)[] blobs = references.ToArray();
        Task[] refUpdateTasks = new Task[blobs.Length];
        for (int i = 0; i < blobs.Length; i++)
        {
            (BucketId bucket, IoHashKey key) = blobs[i];
            ScyllaObjectReference reference = new ScyllaObjectReference(bucket, key);
            Task updateTask;
            if (_scyllaSessionManager.IsScylla)
            {
                updateTask = _mapper.UpdateAsync<ScyllaBlobIndexTable>(
                    "SET references = references - ? WHERE namespace = ? AND blob_id = ?",
                    new[] { reference },
                    nsAsString,
                    new ScyllaBlobIdentifier(id));
            }
            else
            {
                updateTask = _mapper.UpdateAsync<CassandraBlobIndexTable>(
                    "SET references = references - ? WHERE namespace = ? AND blob_id = ?",
                    new[] { reference },
                    nsAsString,
                    id.HashData);
            }

            refUpdateTasks[i] = updateTask;
        }

        await Task.WhenAll(refUpdateTasks);
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
    public ScyllaBlobIdentifier BlobId { get; set; }

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

[Cassandra.Mapping.Attributes.Table("blob_index")]
class CassandraBlobIndexTable
{
    public CassandraBlobIndexTable()
    {
        Namespace = null!;
        BlobId = null!;
        Regions = null;
        References = null;
    }

    public CassandraBlobIndexTable(string @namespace, BlobIdentifier blobId, HashSet<string> regions, List<ScyllaObjectReference> references)
    {
        Namespace = @namespace;
        BlobId =  blobId.HashData;
        Regions = regions;
        References = references;
    }

    [Cassandra.Mapping.Attributes.PartitionKey]
    public string Namespace { get; set; }

    [Cassandra.Mapping.Attributes.PartitionKey]
    [Cassandra.Mapping.Attributes.Column("blob_id")]
    public byte[] BlobId { get; set; }

    [Cassandra.Mapping.Attributes.Column("regions")]
    public HashSet<string>? Regions { get; set; }

    [Cassandra.Mapping.Attributes.Column("references")]
    public List<ScyllaObjectReference>? References { get; set; }

    public BlobInfo ToBlobInfo()
    {
        return new BlobInfo
        {
            Regions = Regions ?? new HashSet<string>(),
            BlobIdentifier = new BlobIdentifier(BlobId),
            Namespace = new NamespaceId(Namespace),
            References = References != null ? References.Select(reference => reference.AsTuple()).ToList() : new List<(BucketId, IoHashKey)>()
        };
    }
}