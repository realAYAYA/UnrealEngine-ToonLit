// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Bson.Serialization.Options;
using MongoDB.Driver;

namespace Horde.Storage.Implementation.Blob;

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

    public async Task<BlobInfo?> GetBlobInfo(NamespaceId ns, BlobIdentifier id)
    {
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();
        IAsyncCursor<MongoBlobIndexModelV0>? cursor = await collection.FindAsync(m => m.Ns == ns.ToString() && m.BlobId == id.ToString());
        MongoBlobIndexModelV0? model = await cursor.FirstOrDefaultAsync();
        if (model == null)
        {
            return null;
        }

        return model.ToBlobInfo();
    }

    public async Task<bool> RemoveBlobFromIndex(NamespaceId ns, BlobIdentifier id)
    {
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();
        DeleteResult result = await collection.DeleteOneAsync(m => m.Ns == ns.ToString() && m.BlobId == id.ToString());

        return result.IsAcknowledged;
    }

    public async Task RemoveBlobFromRegion(NamespaceId ns, BlobIdentifier id, string? region = null)
    {
        region ??= _jupiterSettings.CurrentValue.CurrentSite;

        BlobInfo? blobInfo = await GetBlobInfo(ns, id);
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();
        MongoBlobIndexModelV0 model = new MongoBlobIndexModelV0(blobInfo!);
        model.Regions.Remove(region);
        FilterDefinition<MongoBlobIndexModelV0> filter = Builders<MongoBlobIndexModelV0>.Filter.Where(m => m.Ns == ns.ToString() && m.BlobId == id.ToString());
        await collection.FindOneAndReplaceAsync(filter, model, new FindOneAndReplaceOptions<MongoBlobIndexModelV0, MongoBlobIndexModelV0>
        {
            IsUpsert = true
        });

    }

    public async Task<bool> BlobExistsInRegion(NamespaceId ns, BlobIdentifier blobIdentifier)
    {
        BlobInfo? blobInfo = await GetBlobInfo(ns, blobIdentifier);
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

    public async IAsyncEnumerable<BlobInfo> GetAllBlobs()
    {
        IMongoCollection<MongoBlobIndexModelV0> collection = GetCollection<MongoBlobIndexModelV0>();
        IAsyncCursor<MongoBlobIndexModelV0>? cursor = await collection.FindAsync(FilterDefinition<MongoBlobIndexModelV0>.Empty);

        while (await cursor.MoveNextAsync())
        {
            foreach (MongoBlobIndexModelV0 model in cursor.Current)
            {
                yield return model.ToBlobInfo();
            }
        }
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

    public MongoBlobIndexModelV0(BlobInfo blobInfo)
    {
        Ns = blobInfo.Namespace.ToString();
        BlobId = blobInfo.BlobIdentifier.ToString();
        Regions = blobInfo.Regions.ToList();
        References = blobInfo.References.Select(pair => new Dictionary<string, string>{ {"bucket", pair.Item1.ToString()}, {"key", pair.Item2.ToString()}}).ToList();
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

    public BlobInfo ToBlobInfo()
    {
        return new BlobInfo()
        {
            Namespace = new NamespaceId(Ns),
            BlobIdentifier = new BlobIdentifier(BlobId),
            Regions = Regions.ToHashSet(),
            References = References.Select(dictionary =>
                (new BucketId(dictionary["bucket"]), new IoHashKey(dictionary["key"]))).ToList(),
        };
    }
}
