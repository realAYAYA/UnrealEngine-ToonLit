// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.DataModel;
using Amazon.DynamoDBv2.DocumentModel;
using Amazon.DynamoDBv2.Model;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Serilog;

namespace Horde.Storage.Implementation
{
    internal class DynamoDbRefsStore : DynamoStore, IRefsStore
    {
        private readonly ILogger _logger = Log.ForContext<DynamoDbRefsStore>();
        private readonly IOptionsMonitor<DynamoDbSettings> _settings;
        private readonly Random _lastAccessIndexGenerator = new Random();

        private const int LastAccessIndexMax = 100;

        public const string MainTableName = "Europa_Cache";

        private readonly DynamoNamespaceStore _namespaceStore;

        public DynamoDbRefsStore(IOptionsMonitor<DynamoDbSettings> settings, IAmazonDynamoDB dynamoDb, DynamoNamespaceStore namespaceStore) : base(settings, dynamoDb)
        {
            _settings = settings;
            _namespaceStore = namespaceStore;
        }

        protected override async Task CreateTables()
        {
            // TODO: Add a Bucket Table to be able query which buckets exist within a ns

            Task createMainTable = CreateTable(MainTableName, new[]
            {
                new KeySchemaElement("Id", KeyType.HASH),
            }, new[]
            {
                new AttributeDefinition("Id", ScalarAttributeType.S),
                // these two are used by the GSI for LastAccess
                new AttributeDefinition("LastAccessTime", ScalarAttributeType.S),
                new AttributeDefinition("LastAccessIndex", ScalarAttributeType.N)
            }, new ProvisionedThroughput
            {
                ReadCapacityUnits = _settings.CurrentValue.ReadCapacityUnits,
                WriteCapacityUnits = _settings.CurrentValue.WriteCapacityUnits
            }, 
            // LastAccessIndex is sorted on last access time to allow us to quickly find out of date records
            new [] {
                new GlobalSecondaryIndex
                {
                    IndexName = "LastAccessIndex",
                    KeySchema = new[]
                    {
                        new KeySchemaElement("LastAccessIndex", KeyType.HASH),
                        new KeySchemaElement("LastAccessTime", KeyType.RANGE)
                    }.ToList(),
                    Projection = new Projection
                    {
                        ProjectionType = ProjectionType.KEYS_ONLY,
                    }
                }}
            );

            await Task.WhenAll(_namespaceStore.Initialize(), createMainTable);
        }

        public async Task<RefRecord?> Get(NamespaceId ns, BucketId bucket, KeyId key, IRefsStore.ExtraFieldsFlag fields)
        {
            await Initialize();

            using IScope scope = Tracer.Instance.StartActive("dynamo.get");
            ISpan span = scope.Span;
            span.ResourceName = $"{ns}.{bucket}.{key}";

            Task<DynamoBaseRefRecord> baseRefRecordTask = Context.LoadAsync<DynamoBaseRefRecord>(DynamoBaseRefRecord.BuildCacheKey(ns, bucket, key));
            Task<DynamoMetadataRefRecord>? metadataRefRecordTask = fields.HasFlag(IRefsStore.ExtraFieldsFlag.Metadata) ? Context.LoadAsync<DynamoMetadataRefRecord>(DynamoMetadataRefRecord.BuildCacheKey(ns, bucket, key)) : null;
            Task<DynamoLastAccessRefRecord>? lastAccessRefRecordTask = fields.HasFlag(IRefsStore.ExtraFieldsFlag.LastAccess) ? Context.LoadAsync<DynamoLastAccessRefRecord>(DynamoLastAccessRefRecord.BuildCacheKey(ns, bucket, key)) : null;

            DynamoBaseRefRecord? baseRefRecord = await baseRefRecordTask;
            DynamoLastAccessRefRecord? lastAccessRefRecord = lastAccessRefRecordTask != null ? await lastAccessRefRecordTask : null;
            DynamoMetadataRefRecord? metadataRefRecord = metadataRefRecordTask != null ? await metadataRefRecordTask : null;

            // LoadAsync can return null records even if its not annotated for it
            // ReSharper disable once ConditionIsAlwaysTrueOrFalse
            if (baseRefRecord == null)
            {
                return null; // this key is not valid
            }

            if (!baseRefRecord.IsValid())
            {
                throw new InvalidOperationException("Refs record was not fully populated, version mismatch?");
            }

            if (!lastAccessRefRecord?.IsValid() ?? false)
            {
                throw new InvalidOperationException("LastAccess Refs record was not fully populated, version mismatch?");
            }

            if (!metadataRefRecord?.IsValid() ?? false)
            {
                throw new InvalidOperationException("Metadata refs record was not fully populated, version mismatch?");
            }

            return new DynamoRefRecord(new NamespaceId(baseRefRecord.Namespace!), new BucketId(baseRefRecord.Bucket!), new KeyId(baseRefRecord.Name!), 
                baseRefRecord.Blobs!.Select(s => new BlobIdentifier(s)).ToArray(), 
                lastAccessRefRecord?.LastAccessTime ?? null, lastAccessRefRecord?.LastAccessIndex ?? null, 
                new ContentHash(baseRefRecord.ContentHash!), metadataRefRecord?.Metadata ?? null);
        }

        public async Task Add(RefRecord record)
        {
            await Initialize();

            using IScope scope = Tracer.Instance.StartActive("dynamo.add");
            ISpan span = scope.Span;
            span.ResourceName = $"{record.Namespace}.{record.Bucket}.{record.RefName}";

            Task namespaceTableTask = _namespaceStore.AddToNamespaceTable(record.Namespace, DynamoNamespaceStore.NamespaceUsage.Cache);
            // TODO: write to a bucket table

            DynamoBaseRefRecord dynamoRefRecord = new DynamoBaseRefRecord(record);
            Task baseInsertTask = Context.SaveAsync(dynamoRefRecord);

            int lastAccessIndex = _lastAccessIndexGenerator.Next(LastAccessIndexMax);
            DynamoLastAccessRefRecord dynamoLastAccessRefRecord = new DynamoLastAccessRefRecord(record, lastAccessIndex);
            Task lastAccessInsertTask = Context.SaveAsync(dynamoLastAccessRefRecord);

            DynamoMetadataRefRecord dynamoMetadataRefRecord = new DynamoMetadataRefRecord(record);
            Task metadataInsertTask = Context.SaveAsync(dynamoMetadataRefRecord);

            await Task.WhenAll(
                baseInsertTask, 
                lastAccessInsertTask, 
                metadataInsertTask,
                namespaceTableTask
            );
        }

        public async Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
        {
            await Initialize();

            AsyncSearch<DynamoBaseRefRecord> search = Context.ScanAsync<DynamoBaseRefRecord>(new[]
            {
                new ScanCondition("Namespace", ScanOperator.Equal, ns.ToString()),
                new ScanCondition("Bucket", ScanOperator.Equal, bucket.ToString())
            });

            int countOfDeletedItems = 0;
            do
            {
                List<DynamoBaseRefRecord> newSet = await search.GetNextSetAsync();
                foreach (DynamoBaseRefRecord document in newSet)
                {
                    if (!document.IsValid())
                    {
                        _logger.Warning("Invalid refs record with id {Id}, missing one or more member, unable to delete", document.Id);
                        continue;
                    }

                    Task deleteBase = Context.DeleteAsync(document);

                    NamespaceId documentNs = new NamespaceId(document.Namespace!);
                    BucketId documentBucket = new BucketId(document.Bucket!);
                    KeyId documentKey = new KeyId(document.Name!);

                    Task deleteLastAccess = Context.DeleteAsync<DynamoLastAccessRefRecord>(DynamoLastAccessRefRecord.BuildCacheKey(documentNs, documentBucket, documentKey));
                    Task deleteMetadata = Context.DeleteAsync<DynamoMetadataRefRecord>(DynamoMetadataRefRecord.BuildCacheKey(documentNs, documentBucket, documentKey));
                    countOfDeletedItems++;

                    await Task.WhenAll(deleteBase, deleteLastAccess, deleteMetadata);
                }
            } while (!search.IsDone);

            return countOfDeletedItems;
        }

        public async Task<long> Delete(NamespaceId ns, BucketId bucket, KeyId key)
        {
            await Initialize();

            DeleteItemRequest deleteBaseTask = new(MainTableName, new Dictionary<string, AttributeValue> {{"Id", new AttributeValue(DynamoBaseRefRecord.BuildCacheKey(ns, bucket, key)) }})
            {
                ConditionExpression = "attribute_exists(Id)"
            };
            DeleteItemRequest deleteLastAccessTask = new(MainTableName, new Dictionary<string, AttributeValue> {{"Id", new AttributeValue(DynamoLastAccessRefRecord.BuildCacheKey(ns, bucket, key)) }});
            DeleteItemRequest deleteMetadataTask = new(MainTableName, new Dictionary<string, AttributeValue> {{"Id", new AttributeValue(DynamoMetadataRefRecord.BuildCacheKey(ns, bucket, key)) }});

            try
            {
                await Task.WhenAll(Client.DeleteItemAsync(deleteBaseTask), Client.DeleteItemAsync(deleteLastAccessTask), Client.DeleteItemAsync(deleteMetadataTask));
            }
            catch (ConditionalCheckFailedException)
            {
                // our condition is that it exists, so if it fails it means we had nothing to delete
                return 0;
            }

            return 1;
        }

        public async Task UpdateLastAccessTime(RefRecord record, DateTime lastAccessTime)
        {
            await Initialize();

            using IScope scope = Tracer.Instance.StartActive("dynamo.update_last_access");
            ISpan span = scope.Span;
            span.ResourceName = $"{record.Namespace}.{record.Bucket}.{record.RefName}";

            Table table = Table.LoadTable(Client, MainTableName);
            // we use the document access here so that we can update just the last access time field
            Document? item = await table.GetItemAsync(DynamoLastAccessRefRecord.BuildCacheKey(record.Namespace, record.Bucket, record.RefName));
            // if the item has been deleted after it was scheduled to be updated it will be null
            if (item == null)
            {
                return;
            }

            item["LastAccessTime"] = lastAccessTime;
            await table.UpdateItemAsync(item);
        }

        public async IAsyncEnumerable<NamespaceId> GetNamespaces()
        {
            await Initialize();

            await foreach (NamespaceId ns in _namespaceStore.GetNamespaces(DynamoNamespaceStore.NamespaceUsage.Cache))
            {
                yield return ns;
            }
        }

        public async IAsyncEnumerable<OldRecord> GetOldRecords(NamespaceId ns, TimeSpan oldRecordCutoff)
        {
            await Initialize();

            DateTime cutOffDate = DateTime.Now - oldRecordCutoff;
            using DynamoDBContext context = new DynamoDBContext(Client, new DynamoDBContextConfig { Conversion = DynamoDBEntryConversion.V2 });
            AsyncSearch<DynamoLastAccessRefRecord> search = context.ScanAsync<DynamoLastAccessRefRecord>(new[]
            {
                new ScanCondition("LastAccessTime", ScanOperator.Between, DateTime.MinValue, cutOffDate)
            });

            do
            {
                List<DynamoLastAccessRefRecord> newSet = await search.GetNextSetAsync();
                foreach (DynamoLastAccessRefRecord document in newSet)
                {
                    if (document == null)
                    {
                        continue;
                    }

                    string parentKey = DynamoLastAccessRefRecord.ToBaseRefRecordsKey(document.Id!);

                    DynamoBaseRefRecord? baseRecord = await context.LoadAsync<DynamoBaseRefRecord>(parentKey);
                    if (baseRecord == null || baseRecord.Namespace == null || baseRecord.Bucket == null || baseRecord.Name == null)
                    {
                        continue;
                    }

                    if (baseRecord.Namespace != ns.ToString())
                    {
                        continue;
                    }

                    yield return new OldRecord(new NamespaceId(baseRecord.Namespace), new BucketId(baseRecord.Bucket), new KeyId(baseRecord.Name));
                }
            } while (!search.IsDone);
        }

        public async Task DropNamespace(NamespaceId ns)
        {
            using DynamoDBContext context = new DynamoDBContext(Client, new DynamoDBContextConfig { Conversion = DynamoDBEntryConversion.V2 });
            try
            {
                AsyncSearch<DynamoBaseRefRecord> search = context.ScanAsync<DynamoBaseRefRecord>(new[]
                {
                    new ScanCondition("Namespace", ScanOperator.Equal, ns.ToString())
                });

                int countOfDeletedItems = 0;
                do
                {
                    List<DynamoBaseRefRecord> newSet = await search.GetNextSetAsync();
                    foreach (DynamoBaseRefRecord document in newSet)
                    {
                        if (!document.IsValid())
                        {
                            _logger.Warning("Invalid refs record with id {Id}, missing one or more member, unable to delete", document.Id);
                            continue;
                        }

                        Task deleteBase = Context.DeleteAsync(document);

                        NamespaceId documentNs = new NamespaceId(document.Namespace!);
                        BucketId documentBucket = new BucketId(document.Bucket!);
                        KeyId documentKey = new KeyId(document.Name!);

                        Task deleteLastAccess = Context.DeleteAsync<DynamoLastAccessRefRecord>(DynamoLastAccessRefRecord.BuildCacheKey(documentNs, documentBucket, documentKey));
                        Task deleteMetadata = Context.DeleteAsync<DynamoMetadataRefRecord>(DynamoMetadataRefRecord.BuildCacheKey(documentNs, documentBucket, documentKey));
                        countOfDeletedItems++;

                        await Task.WhenAll(deleteBase, deleteLastAccess, deleteMetadata);
                    }
                } while (!search.IsDone);
            }
            catch (ResourceNotFoundException)
            {
                // if the db doesn't exist we consider the drop a success
                return;
            }

            await _namespaceStore.RemoveNamespace(ns);

        }
    }

    [DynamoDBTable(DynamoDbRefsStore.MainTableName)] 
    public class DynamoBaseRefRecord
    {
        // Required by DynamoDb Serialization
        // ReSharper disable once UnusedMember.Global
        public DynamoBaseRefRecord()
        {

        }

        public DynamoBaseRefRecord(RefRecord record)
        {
            Namespace = record.Namespace.ToString();
            Bucket = record.Bucket.ToString();
            Name = record.RefName.ToString();
            Blobs = record.Blobs.Select(identifier => identifier.ToString()).ToList();
            ContentHash = record.ContentHash.ToString();

            Id = BuildCacheKey(record.Namespace, record.Bucket, record.RefName);
        }

        [DynamoDBHashKey] 
        public string? Id { get; set; }

        public string? Namespace { get; set; }

        public string? Bucket { get; set; }

        public string? Name { get; set; }

        [DynamoDBProperty]
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Required by serialization")]
        public List<string>? Blobs { get; set; }

        public string? ContentHash { get; set; }

        public static string BuildCacheKey(NamespaceId ns, BucketId bucket, KeyId key)
        {
            return $"{ns}.{bucket}.{key}";
        }

        public bool IsValid()
        {
            if (Id == null)
            {
                return false;
            }

            if (Namespace == null)
            {
                return false;
            }

            if (Bucket == null)
            {
                return false;
            }

            if (Name == null)
            {
                return false;
            }

            if (Blobs == null)
            {
                return false;
            }

            if (ContentHash == null)
            {
                return false;
            }

            return true;
        }
    }

    [DynamoDBTable(DynamoDbRefsStore.MainTableName)]
    public class DynamoLastAccessRefRecord
    {
        // Required by DynamoDb Serialization
        // ReSharper disable once UnusedMember.Global
        public DynamoLastAccessRefRecord()
        {
        }

        public DynamoLastAccessRefRecord(RefRecord record, int lastAccessIndex)
        {
            LastAccessTime = record.LastAccessTime;
            LastAccessIndex = lastAccessIndex;

            Id = BuildCacheKey(record.Namespace, record.Bucket, record.RefName);
        }

        public DynamoLastAccessRefRecord(DynamoRefRecord record)
        {
            LastAccessTime = record.LastAccessTime;
            LastAccessIndex = record.LastAccessIndex;

            Id = BuildCacheKey(record.Namespace, record.Bucket, record.RefName);
        }

        [DynamoDBHashKey]
        public string? Id { get; set; }

        public DateTime? LastAccessTime { get; set; }

        // a random index used to distribute requests over a bucket of records
        public int? LastAccessIndex { get; set; }

        public static string BuildCacheKey(NamespaceId ns, BucketId bucket, KeyId key)
        {
            return $"{DynamoBaseRefRecord.BuildCacheKey(ns, bucket, key)}#last-access";
        }

        public static string ToBaseRefRecordsKey(string cacheKey)
        {
            return cacheKey.Replace("#last-access", "", StringComparison.OrdinalIgnoreCase);
        }

        public bool IsValid()
        {
            if (Id == null)
            {
                return false;
            }

            if (LastAccessTime == null)
            {
                return false;
            }

            if (LastAccessIndex == null)
            {
                return false;
            }

            return true;
        }
    }

    [DynamoDBTable(DynamoDbRefsStore.MainTableName)]
    public class DynamoMetadataRefRecord
    {
        // Required by DynamoDb Serialization
        // ReSharper disable once UnusedMember.Global
        public DynamoMetadataRefRecord()
        {
        }

        public DynamoMetadataRefRecord(RefRecord record)
        {
            Id = BuildCacheKey(record.Namespace, record.Bucket, record.RefName);
            Metadata = record.Metadata;
        }

        [DynamoDBHashKey]
        public string? Id { get; set; }

        [DynamoDBProperty(typeof(DynamoJsonConverter))]
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
        public Dictionary<string, object>? Metadata { get; set; }

        public static string BuildCacheKey(NamespaceId ns, BucketId bucket, KeyId key)
        {
            return $"{DynamoBaseRefRecord.BuildCacheKey(ns, bucket, key)}#metadata";
        }

        public bool IsValid()
        {
            if (Id == null)
            {
                return false;
            }

            return true;
        }
    }

    public class DynamoRefRecord : RefRecord
    {
        public DynamoRefRecord(NamespaceId ns, BucketId bucket, KeyId refName, BlobIdentifier[] blobs, DateTime? lastAccessTime, int? lastAccessIndex, ContentHash contentHash, Dictionary<string, object>? metadata) : base(ns, bucket, refName, blobs, lastAccessTime, contentHash, metadata)
        {
            LastAccessIndex = lastAccessIndex;
        }

        public int? LastAccessIndex { get; set; }
    }
}
