// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Authentication;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using Serilog;

namespace Horde.Storage.Implementation
{
    [BsonIgnoreExtraElements]
    public abstract class MongoRefModelBase
    {
        protected MongoRefModelBase(string ns, string bucket, string name)
        {
            Ns = ns;
            Bucket = bucket;
            Name = name;
        }

        [BsonElement("Namespace")] public string Ns { get; set; }
        public string Bucket { get; set; }
        public string Name { get; set; }

        public abstract RefRecord ToRefRecord();
    }

    // we do versioning by writing a discriminator into object
    [BsonDiscriminator("cache.v0")]
    [BsonIgnoreExtraElements]
    public class MongoRefModelV0 : MongoRefModelBase
    {
        [BsonConstructor]
        public MongoRefModelV0(string ns, string bucket, string name, string[] blobIdentifiers,
            string contentHash, DateTime lastAccessTime) : base(ns, bucket, name)
        {
            BlobIdentifiers = blobIdentifiers;
            LastAccessTime = lastAccessTime;
            ContentHash = contentHash;
        }

        [BsonConstructor]
        public MongoRefModelV0(string ns, string bucket, string name, string[] blobIdentifiers,
            DateTime lastAccessTime, string contentHash, BsonDocument metadata) : base(ns, bucket, name)
        {
            BlobIdentifiers = blobIdentifiers;
            Metadata = metadata;
            LastAccessTime = lastAccessTime;
            ContentHash = contentHash;
        }

        public BsonDocument? Metadata { get; set; }
        public string[] BlobIdentifiers { get; set; }
        public DateTime LastAccessTime { get; set; }
        public string ContentHash { get; set; }

        public override RefRecord ToRefRecord()
        {
            return new RefRecord(new NamespaceId(Ns), new BucketId(Bucket), new KeyId(Name), BlobIdentifiers.Select(s => new BlobIdentifier(s)).ToArray(),
                LastAccessTime, new ContentHash(ContentHash), metadata: Metadata?.ToDictionary());
        }
    }

    internal static class RefRecordExtensions
    {
        public static MongoRefModelBase ToLatestRecord(this RefRecord record)
        {
            MongoRefModelV0 newModel = new MongoRefModelV0(
                record.Namespace.ToString(),
                record.Bucket.ToString(),
                record.RefName.ToString(),
                record.Blobs.Select(identifier => identifier.ToString()).ToArray(),
                record.LastAccessTime.GetValueOrDefault(DateTime.Now),
                contentHash: record.ContentHash.ToString(), 
                metadata: record.Metadata.ToBsonDocument()
            );

            return newModel;
        }
    }

    public class MongoRefsStore : IRefsStore
    {
        protected MongoClient Client { get; }
        protected const string ModelName = "Cache";
        private const string MetadataModel = "Metadata";
        private readonly ILogger _logger = Log.ForContext<MongoRefsStore>();

        public MongoRefsStore(IOptionsMonitor<MongoSettings> mongoSettings)
        {
            MongoSettings settings = mongoSettings.CurrentValue;
            MongoClientSettings mongoClientSettings = MongoClientSettings.FromUrl(
                new MongoUrl(settings.ConnectionString)
            );
            if (settings.RequireTls12)
            {
                mongoClientSettings.SslSettings = new SslSettings {EnabledSslProtocols = SslProtocols.None};
            }

            Client = new MongoClient(mongoClientSettings);

            SetupClassMaps();
        }

        private static bool s_classMapsCreated = false;

        private static void SetupClassMaps()
        {
            if (s_classMapsCreated)
            {
                return;
            }

            BsonClassMap.RegisterClassMap<MongoRefModelV0>();
            s_classMapsCreated = true;
        }

        protected IMongoCollection<MongoRefModelBase> GetCollection(NamespaceId ns)
        {
            return GetCollection<MongoRefModelBase>(ns, ModelName);
        }

        protected IMongoCollection<T> GetCollection<T>(NamespaceId ns, string collectionName)
        {
            string dbName = GetDatabaseName(ns);
            return Client.GetDatabase(dbName).GetCollection<T>(collectionName);
        }

        protected string GetDatabaseName(NamespaceId ns)
        {
            string cleanedNs = ns.ToString().Replace(".", "_", StringComparison.OrdinalIgnoreCase);
            string dbName = $"Europa_{cleanedNs}";
            return dbName;
        }

        public async Task<RefRecord?> Get(NamespaceId ns, BucketId bucket, KeyId key, IRefsStore.ExtraFieldsFlag fields)
        {
            MongoRefModelBase model = await ExecuteWithRetries(() =>
            {
                return GetCollection(ns).Find(m => m.Name == key.ToString() && m.Bucket == bucket.ToString()).FirstOrDefaultAsync();
            });

            return model?.ToRefRecord();
        }

        public virtual async Task Add(RefRecord record)
        {
            Task addMetadataRecord = AddMetadataRecord(record.Namespace);
            MongoRefModelBase model = record.ToLatestRecord();

            ReplaceOneResult result = await ExecuteWithRetries(() =>
            {
                return GetCollection(record.Namespace)
                    .ReplaceOneAsync(m => m.Name == model.Name && m.Bucket == model.Bucket, model,
                        options: new ReplaceOptions {IsUpsert = true});
            });

            if (!result.IsAcknowledged)
            {
                _logger.Error("Failed to create {Record} using mongo model {Model}. Got Mongo response {Mongo}", record,
                    model, result);
                throw new Exception(
                    $"Failed to create refs record {record.Bucket} {record.RefName} in namespace {record.Namespace}");
            }

            await addMetadataRecord;
        }

        private async Task AddMetadataRecord(NamespaceId ns)
        {
            // makes sure we have a metadata collection with information about this database
            // since this metadata record always have the same id we can just always insert as if it exists this will not do anything
            try
            {
                await GetCollection<MongoMetadata>(ns, MetadataModel).InsertOneAsync(new MongoMetadata(ns.ToString()));
            }
            catch (MongoWriteException e)
            {
                if (e.WriteError.Code == 11000)
                {
                    // the object already existed so lets ignore this
                }
                else
                {
                    throw;
                }
            }
        }

        public async Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
        {
            DeleteResult actionResult = await GetCollection(ns).DeleteManyAsync(b => b.Bucket == bucket.ToString());

            return actionResult.DeletedCount;
        }

        public async Task<long> Delete(NamespaceId ns, BucketId bucket, KeyId key)
        {
            DeleteResult actionResult =
                await GetCollection(ns).DeleteOneAsync(b => b.Bucket == bucket.ToString() && b.Name == key.ToString());
            return actionResult.DeletedCount;
        }

        public async Task UpdateLastAccessTime(RefRecord record, DateTime lastAccessTime)
        {
            NamespaceId ns = record.Namespace;

            UpdateDefinition<MongoRefModelV0> update =
                Builders<MongoRefModelV0>.Update.Set(b => b.LastAccessTime, lastAccessTime);
            UpdateResult result = await GetCollection<MongoRefModelV0>(ns, ModelName)
                .UpdateOneAsync(b => b.Bucket == record.Bucket.ToString() && b.Name == record.RefName.ToString(), update);

            if (!result.IsAcknowledged || result.MatchedCount != 1 || result.ModifiedCount != 1)
            {
                _logger.Error("Failed to update {Record} to {Time}. Got Mongo response {Mongo}", record, lastAccessTime,
                    result);
                throw new Exception(
                    $"Failed to update refs record {record.Bucket} {record.RefName} in namespace {ns}");
            }
        }

        public async IAsyncEnumerable<NamespaceId> GetNamespaces()
        {
            using IAsyncCursor<BsonDocument> cursor = await Client.ListDatabasesAsync();
            while (await cursor.MoveNextAsync())
            {
                foreach (BsonDocument doc in cursor.Current)
                {
                    string dbName = doc["name"].AsString;
                    MongoMetadata? result = await Client.GetDatabase(dbName).GetCollection<MongoMetadata>("Metadata")
                        .Find(FilterDefinition<MongoMetadata>.Empty).FirstOrDefaultAsync();
                    if (result == null)
                    {
                        continue;
                    }

                    NamespaceId ns;
                    try
                    {
                        ns = new NamespaceId(result.Namespace);
                    }
                    catch (Exception)
                    {
                        continue;
                    }
                    yield return ns;
                }
            }
        }

        public async IAsyncEnumerable<OldRecord> GetOldRecords(NamespaceId ns, TimeSpan oldRecordCutoff)
        {
            IAsyncCursor<MongoRefModelV0> cursor = await GetCollection<MongoRefModelV0>(ns, ModelName).FindAsync(b => b.LastAccessTime < DateTime.Now - oldRecordCutoff);
            while (await cursor.MoveNextAsync())
            {
                foreach (MongoRefModelV0 doc in cursor.Current)
                {
                    yield return new OldRecord(new NamespaceId(doc.Ns), new BucketId(doc.Bucket), new KeyId(doc.Name));
                }
            }
        }

        public async Task DropNamespace(NamespaceId ns)
        {
            string dbName = GetDatabaseName(ns);
            await Client.GetDatabase(dbName).DropCollectionAsync(ModelName);
        }

        private async Task<V> ExecuteWithRetries<V>(Func<Task<V>> function)
        {
            while (true)
            {
                TimeSpan HandleMongoException(MongoCommandException de)
                {
                    // to many requests (http 429 is 16500 from mongo)
                    if (de.Code != 16500)
                    {
                        throw de;
                    }

                    _logger.Warning(de, "Mongo retry exception encountered, sleeping and retrying. ");
                    //sleepTime = de.RetryAfter;
                    //sleepTime = (TimeSpan?)de.Data["RetryAfter"] ?? TimeSpan.Zero;
                    // the mongo api in cosmos does not return back when we should retry
                    // https://feedback.azure.com/forums/263030-azure-cosmos-db/suggestions/32956231-better-handling-of-request-limit-too-large
                    // so lets just guess that we should retry after a while
                    return TimeSpan.FromMilliseconds(250);
                }

                TimeSpan sleepTime;
                try
                {
                    return await function();
                }
                catch (MongoCommandException de)
                {
                    sleepTime = HandleMongoException(de);
                }
                catch (AggregateException ae)
                {
                    if (!(ae.InnerException is MongoCommandException))
                    {
                        throw;
                    }

                    MongoCommandException de = (MongoCommandException)ae.InnerException;
                    sleepTime = HandleMongoException(de);
                }

                await Task.Delay(sleepTime);
            }
        }
    }

    public class MongoMetadata
    {
        [BsonConstructor]
        public MongoMetadata(string @namespace)
        {
            Namespace = @namespace;
        }

        // we define the object id as a hardcoded string to make sure there is only ever one record of this in each database
        [BsonId] public string ObjectId { get; } = "CollectionMetadata";

        public string Namespace { get; set; }
    }
}
