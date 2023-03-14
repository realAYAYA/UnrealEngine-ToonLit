// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Storage.Implementation
{
    public class MongoReferencesStore : MongoStore, IReferencesStore
    {
        public MongoReferencesStore(IOptionsMonitor<MongoSettings> settings, string? overrideDatabaseName = null) : base(settings, overrideDatabaseName)
        {
            CreateCollectionIfNotExists<MongoReferencesModelV0>().Wait();
            CreateCollectionIfNotExists<MongoNamespacesModelV0>().Wait();

            IndexKeysDefinitionBuilder<MongoReferencesModelV0> indexKeysDefinitionBuilder = Builders<MongoReferencesModelV0>.IndexKeys;
            CreateIndexModel<MongoReferencesModelV0> indexModelClusteredKey = new CreateIndexModel<MongoReferencesModelV0>(
                indexKeysDefinitionBuilder.Combine(
                    indexKeysDefinitionBuilder.Ascending(m => m.Ns),
                    indexKeysDefinitionBuilder.Ascending(m => m.Bucket),
                    indexKeysDefinitionBuilder.Ascending(m => m.Key)
                ), new CreateIndexOptions {Name = "CompoundIndex"}
            );

            CreateIndexModel<MongoReferencesModelV0> indexModelNamespace = new CreateIndexModel<MongoReferencesModelV0>(
                indexKeysDefinitionBuilder.Ascending(m => m.Ns),
                new CreateIndexOptions {Name = "NamespaceIndex"}
            );

            AddIndexFor<MongoReferencesModelV0>().CreateMany(new[] { 
                indexModelClusteredKey, 
                indexModelNamespace
            });

        }

        public async Task<ObjectRecord> Get(NamespaceId ns, BucketId bucket, IoHashKey key, IReferencesStore.FieldFlags flags)
        {
            bool includePayload = (flags & IReferencesStore.FieldFlags.IncludePayload) != 0;
            IMongoCollection<MongoReferencesModelV0> collection = GetCollection<MongoReferencesModelV0>();
            IAsyncCursor<MongoReferencesModelV0>? cursor = await collection.FindAsync(m => m.Ns == ns.ToString() && m.Bucket == bucket.ToString() && m.Key == key.ToString());
            MongoReferencesModelV0? model = await cursor.FirstOrDefaultAsync();
            if (model == null)
            {
                throw new ObjectNotFoundException(ns, bucket, key);
            }

            if (!includePayload)
            {
                // TODO: We should actually use this information to use a projection and not fetch the actual blob
                model.InlineBlob = null;
            }

            return model.ToObjectRecord();
        }

        public async Task Put(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash, byte[]? blob, bool isFinalized)
        {
            IMongoCollection<MongoReferencesModelV0> collection = GetCollection<MongoReferencesModelV0>();

            //if (blob.LongLength > _settings.CurrentValue.InlineBlobMaxSize)
            if (blob != null && blob.LongLength > 64_000) // TODO: add a setting to control this
            {
                // do not inline large blobs
                blob = Array.Empty<byte>();
            }

            Task addNamespaceTask = AddNamespaceIfNotExist(ns);
            MongoReferencesModelV0 model = new MongoReferencesModelV0(ns, bucket, key, blobHash, blob, isFinalized, DateTime.Now);
            
            FilterDefinition<MongoReferencesModelV0> filter = Builders<MongoReferencesModelV0>.Filter.Where(m => m.Ns == ns.ToString() && m.Bucket == bucket.ToString() && m.Key == key.ToString());
            FindOneAndReplaceOptions<MongoReferencesModelV0, MongoReferencesModelV0> options = new FindOneAndReplaceOptions<MongoReferencesModelV0, MongoReferencesModelV0>
            {
                IsUpsert = true
            };
            await collection.FindOneAndReplaceAsync(filter, model, options);

            await addNamespaceTask;
        }

        public async Task Finalize(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobIdentifier)
        {
            IMongoCollection<MongoReferencesModelV0> collection = GetCollection<MongoReferencesModelV0>();

            UpdateResult _ = await collection.UpdateOneAsync(
                model => model.Ns == ns.ToString() && model.Bucket == bucket.ToString() && model.Key == key.ToString(),
                Builders<MongoReferencesModelV0>.Update.Set(model => model.IsFinalized, true)
            );
        }

        public async Task UpdateLastAccessTime(NamespaceId ns, BucketId bucket, IoHashKey key, DateTime newLastAccessTime)
        {
            IMongoCollection<MongoReferencesModelV0> collection = GetCollection<MongoReferencesModelV0>();

            UpdateResult _ = await collection.UpdateOneAsync(
                model => model.Ns == ns.ToString() && model.Bucket == bucket.ToString() && model.Key == key.ToString(),
                Builders<MongoReferencesModelV0>.Update.Set(model => model.LastAccessTime, newLastAccessTime)
            );
        }

        public async IAsyncEnumerable<(BucketId, IoHashKey, DateTime)> GetRecords(NamespaceId ns)
        {
            IMongoCollection<MongoReferencesModelV0> collection = GetCollection<MongoReferencesModelV0>();
            IAsyncCursor<MongoReferencesModelV0>? cursor = await collection.FindAsync(model => model.Ns == ns.ToString());

            while (await cursor.MoveNextAsync())
            {
                foreach (MongoReferencesModelV0 model in cursor.Current)
                {
                    yield return (new BucketId(model.Bucket), new IoHashKey(model.Key), model.LastAccessTime);
                }
            }
        }

        public async Task AddNamespaceIfNotExist(NamespaceId ns)
        {
            FilterDefinition<MongoNamespacesModelV0> filter = Builders<MongoNamespacesModelV0>.Filter.Where(m => m.Ns == ns.ToString());
            FindOneAndReplaceOptions<MongoNamespacesModelV0, MongoNamespacesModelV0> options = new FindOneAndReplaceOptions<MongoNamespacesModelV0, MongoNamespacesModelV0>
            {
                IsUpsert = true
            };

            IMongoCollection<MongoNamespacesModelV0> collection = GetCollection<MongoNamespacesModelV0>();
            await collection.FindOneAndReplaceAsync(filter, new MongoNamespacesModelV0(ns), options);
        }

        public async IAsyncEnumerable<NamespaceId> GetNamespaces()
        {
            IMongoCollection<MongoNamespacesModelV0> collection = GetCollection<MongoNamespacesModelV0>();

            IAsyncCursor<MongoNamespacesModelV0> cursor = await collection.FindAsync(FilterDefinition<MongoNamespacesModelV0>.Empty);
            while (await cursor.MoveNextAsync())
            {
                foreach (MongoNamespacesModelV0? document in cursor.Current)
                {
                    yield return new NamespaceId(document.Ns);
                }
            }
        }

        public async Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHashKey key)
        {
            IMongoCollection<MongoReferencesModelV0> collection = GetCollection<MongoReferencesModelV0>();

            DeleteResult result = await collection.DeleteOneAsync(model =>
                model.Ns == ns.ToString() && model.Bucket == bucket.ToString() && model.Key == key.ToString());

            return result.DeletedCount != 0;
        }

        public async Task<long> DropNamespace(NamespaceId ns)
        {
            IMongoCollection<MongoReferencesModelV0> collection = GetCollection<MongoReferencesModelV0>();

            DeleteResult result = await collection.DeleteManyAsync(model =>
                model.Ns == ns.ToString());

            if (result.IsAcknowledged)
            {
                return result.DeletedCount;
            }

            // failed to delete
            return 0L;

        }

        public async Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
        {
            IMongoCollection<MongoReferencesModelV0> collection = GetCollection<MongoReferencesModelV0>();

            DeleteResult result = await collection.DeleteManyAsync(model =>
                model.Ns == ns.ToString() && model.Bucket == bucket.ToString());

            if (result.IsAcknowledged)
            {
                return result.DeletedCount;
            }

            // failed to delete
            return 0L;
        }

        public void SetTTLDuration(TimeSpan duration)
        {
            IndexKeysDefinitionBuilder<MongoReferencesModelV0> indexKeysDefinitionBuilder = Builders<MongoReferencesModelV0>.IndexKeys;
            CreateIndexModel<MongoReferencesModelV0> indexTTL = new CreateIndexModel<MongoReferencesModelV0>(
                indexKeysDefinitionBuilder.Ascending(m => m.LastAccessTime), new CreateIndexOptions {Name = "LastAccessTTL", ExpireAfter = duration}
            );

            AddIndexFor<MongoReferencesModelV0>().CreateOne(indexTTL);
        }
    }

    
    // we do versioning by writing a discriminator into object
    [BsonDiscriminator("ref.v0")]
    [BsonIgnoreExtraElements]
    [MongoCollectionName("References")]
    public class MongoReferencesModelV0
    {
        [BsonConstructor]
        public MongoReferencesModelV0(string ns, string bucket, string key, string blobIdentifier, byte[] inlineBlob, bool isFinalized, DateTime lastAccessTime)
        {
            Ns = ns;
            Bucket = bucket;
            Key = key;
            BlobIdentifier = blobIdentifier;
            InlineBlob = inlineBlob;
            IsFinalized = isFinalized;
            LastAccessTime = lastAccessTime;
        }

        public MongoReferencesModelV0(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobIdentifier, byte[]? blob, bool isFinalized, DateTime lastAccessTime)
        {
            Ns = ns.ToString();
            Bucket = bucket.ToString();
            Key = key.ToString();
            InlineBlob = blob;
            BlobIdentifier = blobIdentifier.ToString();
            IsFinalized = isFinalized;
            LastAccessTime = lastAccessTime;
        }

        [BsonRequired]
        public string Ns { get; set; }

        [BsonRequired]
        public string Bucket { get; set; }

        [BsonRequired]
        public string Key { get; set; }

        public string BlobIdentifier { get; set; }

        public bool IsFinalized { get; set; }

        [BsonRequired]
        public DateTime LastAccessTime { get; set; }

        public byte[]? InlineBlob { get; set; }

        public ObjectRecord ToObjectRecord()
        {
            return new ObjectRecord(new NamespaceId(Ns), new BucketId(Bucket), new IoHashKey(Key), 
                LastAccessTime,
                InlineBlob, new BlobIdentifier(BlobIdentifier), IsFinalized);
        }
    }

    [MongoCollectionName("Namespaces")]
    [BsonDiscriminator("namespace.v0")]
    [BsonIgnoreExtraElements]
    public class MongoNamespacesModelV0
    {
        [BsonRequired]
        public string Ns { get; set; }

        [BsonConstructor]
        public MongoNamespacesModelV0(string ns)
        {
            Ns = ns;
        }

        public MongoNamespacesModelV0(NamespaceId ns)
        {
            Ns = ns.ToString();
        }
    }
}
