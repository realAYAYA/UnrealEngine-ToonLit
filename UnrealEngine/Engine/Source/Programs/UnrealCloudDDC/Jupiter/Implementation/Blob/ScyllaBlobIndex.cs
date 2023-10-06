// Copyright Epic Games, Inc. All Rights Reserved.

using System;
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
    private readonly IOptionsMonitor<ScyllaSettings> _scyllaSettings;
    private readonly Tracer _tracer;
    private readonly ISession _session;
    private readonly Mapper _mapper;

    public ScyllaBlobIndex(IScyllaSessionManager scyllaSessionManager, IOptionsMonitor<JupiterSettings> jupiterSettings, IOptionsMonitor<ScyllaSettings> scyllaSettings, Tracer tracer)
    {
        _scyllaSessionManager = scyllaSessionManager;
        _jupiterSettings = jupiterSettings;
        _scyllaSettings = scyllaSettings;
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

        _session.Execute(new SimpleStatement(@$"CREATE TABLE IF NOT EXISTS blob_index_v2 (
            namespace text,
            blob_id blob,
            region text,
            PRIMARY KEY ((namespace, blob_id), region)
        );"
        ));

        _session.Execute(new SimpleStatement(@$"CREATE TABLE IF NOT EXISTS blob_incoming_references (
            namespace text,
            blob_id blob,
            reference_id blob,
            reference_type smallint,
            bucket_id text,
            PRIMARY KEY ((namespace, blob_id), reference_id)
        );"
        ));
    }

    public async Task AddBlobToIndex(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;
        using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.insert_blob_index").SetAttribute("resource.name", $"{ns}.{id}");

        await _mapper.InsertAsync<ScyllaBlobIndexEntry>(new ScyllaBlobIndexEntry(ns.ToString(), id, region));
    }

    private async Task<List<string>?> GetOldBlobRegions(NamespaceId ns, BlobIdentifier id)
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.fetch_old_blob_index").SetAttribute("resource.name", $"{ns}.{id}");

        if (_scyllaSessionManager.IsScylla)
        {
            ScyllaBlobIndexTable o;
            o = await _mapper.SingleOrDefaultAsync<ScyllaBlobIndexTable>("SELECT namespace, blob_id, regions, references FROM blob_index WHERE namespace = ? AND blob_id = ?", ns.ToString(), new ScyllaBlobIdentifier(id));
            return o?.Regions?.ToList();
        }
        else
        {
            CassandraBlobIndexTable o;
            o = await _mapper.SingleOrDefaultAsync<CassandraBlobIndexTable>("SELECT namespace, blob_id, regions, references FROM blob_index WHERE namespace = ? AND blob_id = ?", ns.ToString(), id.HashData);
            return o?.Regions?.ToList();
        }
    }

    private async Task<List<ScyllaObjectReference>?> GetOldBlobReferences(NamespaceId ns, BlobIdentifier id)
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.fetch_old_blob_index_references").SetAttribute("resource.name", $"{ns}.{id}");

        if (_scyllaSessionManager.IsScylla)
        {
            ScyllaBlobIndexTable o;
            o = await _mapper.SingleOrDefaultAsync<ScyllaBlobIndexTable>("SELECT namespace, blob_id, regions, references FROM blob_index WHERE namespace = ? AND blob_id = ?", ns.ToString(), new ScyllaBlobIdentifier(id));
            return o?.References?.ToList();
        }
        else
        {
            CassandraBlobIndexTable o;
            o = await _mapper.SingleOrDefaultAsync<CassandraBlobIndexTable>("SELECT namespace, blob_id, regions, references FROM blob_index WHERE namespace = ? AND blob_id = ?", ns.ToString(), id.HashData);
            return o?.References?.ToList();
        }
    }

    public async Task RemoveBlobFromRegion(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.remove_blob_index_region").SetAttribute("resource.name", $"{ns}.{id}");

        await _mapper.DeleteAsync<ScyllaBlobIndexEntry>(new ScyllaBlobIndexEntry(ns.ToString(), id, region));
    }

    public async Task<bool> BlobExistsInRegion(NamespaceId ns, BlobIdentifier blobIdentifier, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;
        ScyllaBlobIndexEntry? entry = await _mapper.SingleOrDefaultAsync<ScyllaBlobIndexEntry>("WHERE namespace = ? AND blob_id = ? AND region = ?", ns.ToString(), blobIdentifier.HashData, region);

        bool blobMissing = entry == null;
        if (blobMissing && _scyllaSettings.CurrentValue.MigrateFromOldBlobIndex)
        {
            List<string>? regions = await GetOldBlobRegions(ns, blobIdentifier);
            if (regions == null)
            {
                // blob didn't exist in the old table either
                return false;
            }

            foreach (string oldRegion in regions)
            {
                await AddBlobToIndex(ns, blobIdentifier, oldRegion);
            }

            // blob has been migrated so it existed
            return true;
        }

        return !blobMissing;
    }
    public async IAsyncEnumerable<(NamespaceId, BlobIdentifier)> GetAllBlobs()
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.get_all_blobs");

        string cqlOptions = _scyllaSessionManager.IsScylla ? "BYPASS CACHE" : "";

        foreach (ScyllaBlobIndexEntry blobIndex in await _mapper.FetchAsync<ScyllaBlobIndexEntry>(cqlOptions))
        {
            yield return (new NamespaceId(blobIndex.Namespace), new BlobIdentifier(blobIndex.BlobId));
        }
    }

    public async Task<List<string>> GetBlobRegions(NamespaceId ns, BlobIdentifier blob)
    {
        List<string> regions = new List<string>();
        foreach (ScyllaBlobIndexEntry blobIndex in await _mapper.FetchAsync<ScyllaBlobIndexEntry>("WHERE namespace = ? AND blob_id = ?", ns.ToString(), blob.HashData))
        {
            regions.Add(blobIndex.Region);
        }
        
        if (regions.Any() && _scyllaSettings.CurrentValue.MigrateFromOldBlobIndex)
        {
            List<string>? oldRegions = await GetOldBlobRegions(ns, blob);
            if (oldRegions == null)
            {
                // regions didn't exist in the old table either
                return regions;
            }

            foreach (string oldRegion in oldRegions)
            {
                await AddBlobToIndex(ns, blob, oldRegion);
            }

            // blob has been migrated lets return the old region list
            return oldRegions;
        }
        return regions;
    }

    public async Task AddBlobReferences(NamespaceId ns, BlobIdentifier sourceBlob, BlobIdentifier targetBlob)
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.add_blob_to_blob_ref");

        string nsAsString = ns.ToString();
        ScyllaBlobIncomingReference incomingReference = new ScyllaBlobIncomingReference(nsAsString, sourceBlob, targetBlob);

        await  _mapper.InsertAsync<ScyllaBlobIncomingReference>(incomingReference);
    }

    public async IAsyncEnumerable<BaseBlobReference> GetBlobReferences(NamespaceId ns, BlobIdentifier id)
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.get_blob_references").SetAttribute("resource.name", $"{ns}.{id}");

        // the references are rarely read so we bypass cache to reduce churn in it
        string cqlOptions = _scyllaSessionManager.IsScylla ? "BYPASS CACHE" : "";

        bool noReferencesFound = true;
        foreach (ScyllaBlobIncomingReference incomingReference in await _mapper.FetchAsync<ScyllaBlobIncomingReference>("WHERE namespace = ? AND blob_id = ? " + cqlOptions, ns.ToString(), id.HashData))
        {
            noReferencesFound = false;
            ScyllaBlobIncomingReference.BlobReferenceType referenceType = (ScyllaBlobIncomingReference.BlobReferenceType)incomingReference.ReferenceType;
            if (referenceType == ScyllaBlobIncomingReference.BlobReferenceType.Ref)
            {
                if (incomingReference.BucketId == null)
                {
                    continue;
                }
                yield return new RefBlobReference(new BucketId(incomingReference.BucketId), new IoHashKey(StringUtils.FormatAsHexString(incomingReference.ReferenceId)));
            }
            else if (referenceType == ScyllaBlobIncomingReference.BlobReferenceType.Blob)
            {
                yield return new BlobToBlobReference(new BlobIdentifier(incomingReference.ReferenceId));
            }
            else
            {
                throw new NotImplementedException("Unknown blob reference type");
            }
        }

        if (noReferencesFound && _scyllaSettings.CurrentValue.MigrateFromOldBlobIndex)
        {
            List<ScyllaObjectReference>? oldReferences = await GetOldBlobReferences(ns, id);
            if (oldReferences != null)
            {
                foreach (ScyllaObjectReference scyllaObjectReference in oldReferences)
                {
                    BucketId bucket = new BucketId(scyllaObjectReference.Bucket);
                    IoHashKey key = new IoHashKey(scyllaObjectReference.Key);
                    await AddRefToBlobs(ns, bucket, key, new []{id});
                    yield return new RefBlobReference(bucket, key);
                }
            }
        }
    }

    public async Task AddRefToBlobs(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier[] blobs)
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.add_ref_blobs");

        string nsAsString = ns.ToString();
        Task[] refUpdateTasks = new Task[blobs.Length];
        for (int i = 0; i < blobs.Length; i++)
        {
            BlobIdentifier id = blobs[i];
            ScyllaBlobIncomingReference incomingReference = new ScyllaBlobIncomingReference(nsAsString, id, key, bucket);

            refUpdateTasks[i] = _mapper.InsertAsync<ScyllaBlobIncomingReference>(incomingReference);
        }

        await Task.WhenAll(refUpdateTasks);
    }

    public async Task RemoveReferences(NamespaceId ns, BlobIdentifier id, List<BaseBlobReference> referencesToRemove)
    {
        using TelemetrySpan scope =  _tracer.BuildScyllaSpan("scylla.remove_ref_blobs");

        string nsAsString = ns.ToString();
        int countOfReferencesToRemove = referencesToRemove.Count;
        Task[] refUpdateTasks = new Task[countOfReferencesToRemove];
        for (int i = 0; i < countOfReferencesToRemove; i++)
        {
            BaseBlobReference baseRef = referencesToRemove[i];
            ScyllaBlobIncomingReference incomingReference;

            if (baseRef is RefBlobReference refBlobReference)
            {
                incomingReference = new ScyllaBlobIncomingReference(nsAsString, id, refBlobReference.Key, refBlobReference.Bucket);
            }
            else if (baseRef is BlobToBlobReference blobToBlobReference)
            {
                incomingReference = new ScyllaBlobIncomingReference(nsAsString, id, blobToBlobReference.Blob);
            }
            else
            {
                throw new NotImplementedException("Unknown blob reference type");
            }

            refUpdateTasks[i] =  _mapper.InsertAsync<ScyllaBlobIncomingReference>(incomingReference);
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
}

[Cassandra.Mapping.Attributes.Table("blob_index_v2")]
class ScyllaBlobIndexEntry
{
    public ScyllaBlobIndexEntry()
    {
        Namespace = null!;
        BlobId = null!;
        Region = null!;
    }

    public ScyllaBlobIndexEntry(string @namespace, BlobIdentifier blobId, string region)
    {
        Namespace = @namespace;
        BlobId = blobId.HashData;
        Region = region;
    }

    [Cassandra.Mapping.Attributes.PartitionKey]
    public string Namespace { get; set; }

    [Cassandra.Mapping.Attributes.PartitionKey]
    [Cassandra.Mapping.Attributes.Column("blob_id")]
    public byte[] BlobId { get; set; }

    [Cassandra.Mapping.Attributes.ClusteringKey]
    [Cassandra.Mapping.Attributes.Column("region")]
    public string Region { get; set; }
}

[Cassandra.Mapping.Attributes.Table("blob_incoming_references")]
class ScyllaBlobIncomingReference
{
    public enum BlobReferenceType { Ref = 0, Blob = 1 };

    public ScyllaBlobIncomingReference()
    {
        Namespace = null!;
        BlobId = null!;
        ReferenceId = null!;
        BucketId = null!;
    }

    public ScyllaBlobIncomingReference(string @namespace, BlobIdentifier blobId, BlobIdentifier referenceId)
    {
        Namespace = @namespace;
        BlobId = blobId.HashData;

        BucketId = null;
        ReferenceId = referenceId.HashData;
        ReferenceType = (int)BlobReferenceType.Blob;
    }

    public ScyllaBlobIncomingReference(string @namespace, BlobIdentifier blobId, IoHashKey referenceId, BucketId bucketId)
    {
        Namespace = @namespace;
        BlobId = blobId.HashData;

        BucketId = bucketId.ToString();
        ReferenceId = StringUtils.ToHashFromHexString(referenceId.ToString());
        ReferenceType = (int)BlobReferenceType.Ref;
    }

    [Cassandra.Mapping.Attributes.PartitionKey]
    public string Namespace { get; set; }

    [Cassandra.Mapping.Attributes.PartitionKey]
    [Cassandra.Mapping.Attributes.Column("blob_id")]
    public byte[] BlobId { get; set; }

    [Cassandra.Mapping.Attributes.ClusteringKey]
    [Cassandra.Mapping.Attributes.Column("reference_id")]
    public byte[] ReferenceId { get; set; }

    [Cassandra.Mapping.Attributes.Column("bucket_id")]
    public string? BucketId { get; set; }

    [Cassandra.Mapping.Attributes.Column("reference_type")]
    public short ReferenceType { get; set; }
}
