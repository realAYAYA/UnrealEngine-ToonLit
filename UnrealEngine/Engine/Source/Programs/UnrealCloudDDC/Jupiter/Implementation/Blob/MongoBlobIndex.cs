// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Options;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Bson.Serialization.Options;
using MongoDB.Driver;

namespace Jupiter.Implementation.Blob;

public class MongoBlobIndex : MongoStore, IBlobIndex
{
    private readonly IOptionsMonitor<JupiterSettings> _jupiterSettings;

    public MongoBlobIndex(IOptionsMonitor<JupiterSettings> jupiterSettings, IOptionsMonitor<MongoSettings> settings) : base(settings)
    {
        _jupiterSettings = jupiterSettings;

        CreateCollectionIfNotExists<MongoBlobIndexModelV0>().Wait();

        IndexKeysDefinitionBuilder<MongoBlobIndexModelV0> indexKeysDefinitionBuilder = Builders<MongoBlobIndexModelV0>.IndexKeys;
        CreateIndexModel<MongoBlobIndexModelV0> indexModel = new CreateIndexModel<MongoBlobIndexModelV0>(
            indexKeysDefinitionBuilder.Combine(
                indexKeysDefinitionBuilder.Ascending(m => m.Ns), 
                indexKeysDefinitionBuilder.Ascending(m => m.BlobId)
            )
            , new CreateIndexOptions()
            {
                Name = "CompoundIndex"
            });

        
        AddIndexFor<MongoBlobIndexModelV0>().CreateMany(new[] { 
            indexModel, 
        });
    }

    private async Task<MongoBlobIndexModelV0?> GetBlobInfo(NamespaceId ns, BlobIdentifier id)
    {
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();
        IAsyncCursor<MongoBlobIndexModelV0>? cursor = await collection.FindAsync(m => m.Ns == ns.ToString() && m.BlobId == id.ToString());
        MongoBlobIndexModelV0? model = await cursor.FirstOrDefaultAsync();
        if (model == null)
        {
            return null;
        }

        return model;
    }

    public async Task AddBlobToIndex(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();
        MongoBlobIndexModelV0 model = new MongoBlobIndexModelV0(ns, id);
        model.Regions.Add(region);
            
        FilterDefinition<MongoBlobIndexModelV0> filter = Builders<MongoBlobIndexModelV0>.Filter.Where(m => m.Ns == ns.ToString() && m.BlobId == id.ToString());
        await collection.FindOneAndReplaceAsync(filter, model, new FindOneAndReplaceOptions<MongoBlobIndexModelV0, MongoBlobIndexModelV0>
        {
            IsUpsert = true
        });
    }

    public async Task RemoveBlobFromRegion(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;

        MongoBlobIndexModelV0? model = await GetBlobInfo(ns, id);
        if (model == null)
        {
            throw new BlobNotFoundException(ns, id);
        }
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();
        model.Regions.Remove(region);
        FilterDefinition<MongoBlobIndexModelV0> filter = Builders<MongoBlobIndexModelV0>.Filter.Where(m => m.Ns == ns.ToString() && m.BlobId == id.ToString());
        await collection.FindOneAndReplaceAsync(filter, model, new FindOneAndReplaceOptions<MongoBlobIndexModelV0, MongoBlobIndexModelV0>
        {
            IsUpsert = true
        });

    }

    public async IAsyncEnumerable<BaseBlobReference> GetBlobReferences(NamespaceId ns, BlobIdentifier id)
    {
        MongoBlobIndexModelV0? blobInfo = await GetBlobInfo(ns, id);
        if (blobInfo == null)
        {
            yield break;
        }

        foreach (Dictionary<string, string> reference in blobInfo.References)
        {
            if (reference.ContainsKey("bucket"))
            {
                string bucket = reference["bucket"];
                string key = reference["key"];
                yield return new RefBlobReference(new BucketId(bucket), new IoHashKey(key));
            } 
            else if (reference.ContainsKey("blob_id"))
            {
                string blobId = reference["blob_id"];
                yield return new BlobToBlobReference(new BlobIdentifier(blobId));
            }
            else
            {
                throw new NotImplementedException();
            }
        }
    }

    public async Task<bool> BlobExistsInRegion(NamespaceId ns, BlobIdentifier blobIdentifier, string? region = null)
    {
        MongoBlobIndexModelV0? blobInfo = await GetBlobInfo(ns, blobIdentifier);
        return blobInfo?.Regions.Contains(_jupiterSettings.CurrentValue.CurrentSite) ?? false;
    }

    public async Task AddRefToBlobs(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier[] blobs)
    {
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();

        string nsAsString = ns.ToString();
        Task[] refUpdateTasks = new Task[blobs.Length];
        for (int i = 0; i < blobs.Length; i++)
        {
            BlobIdentifier id = blobs[i];
            refUpdateTasks[i] = Task.Run(async () =>
            {
                UpdateDefinition<MongoBlobIndexModelV0> update = Builders<MongoBlobIndexModelV0>.Update.AddToSet(m => m.References, new Dictionary<string, string> { {"bucket", bucket.ToString()}, {"key", key.ToString()}});
                FilterDefinition<MongoBlobIndexModelV0> filter = Builders<MongoBlobIndexModelV0>.Filter.Where(m => m.Ns == nsAsString && m.BlobId == id.ToString());

                await collection.FindOneAndUpdateAsync(filter, update);
            });
        }

        await Task.WhenAll(refUpdateTasks);
    }

    public async IAsyncEnumerable<(NamespaceId, BlobIdentifier)> GetAllBlobs()
    {
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();
        IAsyncCursor<MongoBlobIndexModelV0>? cursor = await collection.FindAsync(FilterDefinition<MongoBlobIndexModelV0>.Empty);

        while (await cursor.MoveNextAsync())
        {
            foreach (MongoBlobIndexModelV0 model in cursor.Current)
            {
                yield return (new NamespaceId(model.Ns), new BlobIdentifier(model.BlobId));
            }
        }
    }

    public async Task RemoveReferences(NamespaceId ns, BlobIdentifier id, List<BaseBlobReference> referencesToRemove)
    {
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();

        string nsAsString = ns.ToString();
        List<Dictionary<string, string>> refs = referencesToRemove.Select(reference =>
        {
            if (reference is RefBlobReference refBlobReference)
            {
                return new Dictionary<string, string>
                {
                    { "bucket", refBlobReference.Bucket.ToString() }, { "key", refBlobReference.Key.ToString() }
                };
            }
            else if (reference is BlobToBlobReference blobToBlobReference)
            {
                return new Dictionary<string, string>
                {
                    { "blob_id", blobToBlobReference.Blob.ToString()}
                };
            }
            else
            {
                throw new NotImplementedException();
            }
        }).ToList();
        UpdateDefinition<MongoBlobIndexModelV0> update = Builders<MongoBlobIndexModelV0>.Update.PullAll(m => m.References, refs);
        FilterDefinition<MongoBlobIndexModelV0> filter = Builders<MongoBlobIndexModelV0>.Filter.Where(m => m.Ns == nsAsString && m.BlobId == id.ToString());

        await collection.FindOneAndUpdateAsync(filter, update);
    }

    public async Task<List<string>> GetBlobRegions(NamespaceId ns, BlobIdentifier blob)
    {
        MongoBlobIndexModelV0? blobInfo = await GetBlobInfo(ns, blob);
        if (blobInfo == null)
        {
            throw new BlobNotFoundException(ns, blob);
        }
        return blobInfo.Regions.ToList();
    }

    public async Task AddBlobReferences(NamespaceId ns, BlobIdentifier sourceBlob, BlobIdentifier targetBlob)
    {
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();

        string nsAsString = ns.ToString();

        UpdateDefinition<MongoBlobIndexModelV0> update = Builders<MongoBlobIndexModelV0>.Update.AddToSet(m => m.References, new Dictionary<string, string> {{ "blob_id", targetBlob.ToString()}});
        FilterDefinition<MongoBlobIndexModelV0> filter = Builders<MongoBlobIndexModelV0>.Filter.Where(m => m.Ns == nsAsString && m.BlobId == sourceBlob.ToString());

        await collection.FindOneAndUpdateAsync(filter, update);
    }
}

[BsonDiscriminator("blob-index.v0")]
[BsonIgnoreExtraElements]
[MongoCollectionName("BlobIndex")]
class MongoBlobIndexModelV0
{
    [BsonConstructor]
    public MongoBlobIndexModelV0(string ns, string blobId, List<string> regions, List<Dictionary<string, string>> references)
    {
        Ns = ns;
        BlobId = blobId;
        Regions = regions;
        References = references;
    }

    public MongoBlobIndexModelV0(NamespaceId ns, BlobIdentifier blobId)
    {
        Ns = ns.ToString();
        BlobId = blobId.ToString();
    }

    [BsonRequired]
    public string Ns { get; set; }

    [BsonRequired]
    public string BlobId { get;set; }

    public List<string> Regions { get; set; } = new List<string>();

    [BsonDictionaryOptions(DictionaryRepresentation.Document)]
    public List<Dictionary<string, string>> References { get; set; } = new List<Dictionary<string, string>>();

}
