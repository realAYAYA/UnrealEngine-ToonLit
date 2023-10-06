// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Cassandra;
using Cassandra.Mapping;
using EpicGames.Horde.Storage;
using Jupiter.Common;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation
{
    public class ScyllaReferencesStore : IReferencesStore
    {
        private readonly ISession _session;
        private readonly IMapper _mapper;
        private readonly IOptionsMonitor<ScyllaSettings> _settings;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly Tracer _tracer;
        private readonly ILogger _logger;
        private readonly PreparedStatement _getObjectsStatement;
        private readonly PreparedStatement _getObjectsLastAccessStatement;
        private readonly PreparedStatement _getNamespacesStatement;
        private readonly PreparedStatement _getNamespacesOldStatement;
        private readonly PreparedStatement _getObjectsForPartitionRangeStatement;
        private readonly PreparedStatement _getObjectsLastAccessForPartitionRangeStatement;
        private readonly ConcurrentDictionary<NamespaceId, ConcurrentBag<BucketId>> _addedBuckets = new ConcurrentDictionary<NamespaceId, ConcurrentBag<BucketId>>();

        public ScyllaReferencesStore(IScyllaSessionManager scyllaSessionManager, IOptionsMonitor<ScyllaSettings> settings, INamespacePolicyResolver namespacePolicyResolver, Tracer tracer, ILogger<ScyllaReferencesStore> logger)
        {
            _session = scyllaSessionManager.GetSessionForReplicatedKeyspace();
            _settings = settings;
            _namespacePolicyResolver = namespacePolicyResolver;
            _tracer = tracer;
            _logger = logger;

            _mapper = new Mapper(_session);

            _session.Execute(new SimpleStatement(@"CREATE TABLE IF NOT EXISTS objects (
                namespace text, 
                bucket text, 
                name text, 
                payload_hash blob_identifier, 
                inline_payload blob, 
                is_finalized boolean,
                last_access_time timestamp,
                PRIMARY KEY ((namespace, bucket, name))
            );"
            ));

            _session.Execute(new SimpleStatement(@"CREATE TABLE IF NOT EXISTS object_last_access_v2 (
                namespace text, 
                bucket text, 
                name text, 
                last_access_time timestamp,
                PRIMARY KEY ((namespace, bucket, name))
            );"
            ));
            
            _session.Execute(new SimpleStatement(@"CREATE TABLE IF NOT EXISTS buckets_v2 (
                namespace text, 
                bucket text, 
                PRIMARY KEY ((namespace), bucket)
            );"
            ));

            _session.Execute(new SimpleStatement(@"CREATE TABLE IF NOT EXISTS buckets (
                namespace text, 
                bucket set<text>, 
                PRIMARY KEY (namespace)
            );"
            ));

            // BYPASS CACHE is a scylla specific extension to disable populating the cache, should be ignored by other cassandra dbs
            string cqlOptions = scyllaSessionManager.IsScylla ? "BYPASS CACHE" : "";
            _getObjectsStatement = _session.Prepare($"SELECT namespace, bucket, name, last_access_time FROM objects ALLOW FILTERING {cqlOptions}");
            _getObjectsLastAccessStatement = _session.Prepare($"SELECT namespace, bucket, name, last_access_time FROM object_last_access_v2 ALLOW FILTERING {cqlOptions}");
            _getNamespacesStatement = _session.Prepare("SELECT DISTINCT namespace FROM buckets_v2");
            _getNamespacesOldStatement = _session.Prepare("SELECT DISTINCT namespace FROM buckets");

            _getObjectsForPartitionRangeStatement = _session.Prepare($"SELECT namespace, bucket, name, last_access_time FROM objects WHERE token(namespace, bucket, name) >= ? AND token(namespace, bucket, name) <= ? {cqlOptions}");
            _getObjectsLastAccessForPartitionRangeStatement = _session.Prepare($"SELECT namespace, bucket, name, last_access_time FROM object_last_access_v2 WHERE token(namespace, bucket, name) >= ? AND token(namespace, bucket, name) <= ? {cqlOptions}");
        }

        public async Task<ObjectRecord> Get(NamespaceId ns, BucketId bucket, IoHashKey name, IReferencesStore.FieldFlags flags)
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.get").SetAttribute("resource.name", $"{ns}.{bucket}.{name}");

            ScyllaObject? o;
            bool includePayload = (flags & IReferencesStore.FieldFlags.IncludePayload) != 0;
            if (includePayload)
            {
                o = await _mapper.SingleOrDefaultAsync<ScyllaObject>("WHERE namespace = ? AND bucket = ? AND name = ?", ns.ToString(), bucket.ToString(), name.ToString());
            }
            else
            {
                // fetch everything except for the inline blob which is quite large
                o = await _mapper.SingleOrDefaultAsync<ScyllaObject>("SELECT namespace, bucket, name , payload_hash, is_finalized, last_access_time FROM objects WHERE namespace = ? AND bucket = ? AND name = ?", ns.ToString(), bucket.ToString(), name.ToString());
            }

            if (o == null)
            {
                throw new ObjectNotFoundException(ns, bucket, name);
            }

            try
            {
                o.ThrowIfRequiredFieldIsMissing();
            }
            catch (Exception e)
            {
                _logger.LogWarning(e, "Partial object found {Namespace} {Bucket} {Name} ignoring object", ns, bucket, name);
                throw new ObjectNotFoundException(ns, bucket, name);
            }

            return new ObjectRecord(new NamespaceId(o.Namespace!), new BucketId(o.Bucket!), new IoHashKey(o.Name!), o.LastAccessTime, o.InlinePayload, o.PayloadHash!.AsBlobIdentifier(), o.IsFinalized!.Value);
        }

        public async Task Put(NamespaceId ns, BucketId bucket, IoHashKey name, BlobIdentifier blobHash, byte[] blob, bool isFinalized)
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.put").SetAttribute("resource.name", $"{ns}.{bucket}.{name}");

            if (blob.LongLength > _settings.CurrentValue.InlineBlobMaxSize)
            {
                // do not inline large blobs
                blob = Array.Empty<byte>();
            }

            // add the bucket in parallel with inserting the actual object
            Task addBucketTask = AddBucket(ns, bucket);

            int? ttl = null;
            NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);
            if (policy.GcMethod == NamespacePolicy.StoragePoolGCMethod.TTL)
            {
                ttl = (int)policy.DefaultTTL.TotalSeconds;
            }

            Task? insertLastAccess = policy.GcMethod == NamespacePolicy.StoragePoolGCMethod.LastAccess ? _mapper.InsertAsync<ScyllaObjectLastAccess>(new ScyllaObjectLastAccess(ns, bucket, name, DateTime.Now)) : null;

            await _mapper.InsertAsync<ScyllaObject>(new ScyllaObject(ns, bucket, name, blob, blobHash, isFinalized), ttl: ttl, insertNulls: false);

            if (insertLastAccess != null)
            {
                await insertLastAccess;
            }

            await addBucketTask;
        }

        public async Task Finalize(NamespaceId ns, BucketId bucket, IoHashKey name, BlobIdentifier blobIdentifier)
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.finalize").SetAttribute("resource.name", $"{ns}.{bucket}.{name}");

            await _mapper.UpdateAsync<ScyllaObject>("SET is_finalized=true WHERE namespace=? AND bucket=? AND name=?", ns.ToString(), bucket.ToString(), name.ToString());
        }

        public async Task UpdateLastAccessTime(NamespaceId ns, BucketId bucket, IoHashKey name, DateTime lastAccessTime)
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.update_last_access_time");

            Task? updateObjectLastAccessTask = null;
            updateObjectLastAccessTask = _mapper.InsertAsync<ScyllaObjectLastAccess>(new ScyllaObjectLastAccess(ns, bucket, name, lastAccessTime));

            if (_settings.CurrentValue.UpdateLegacyLastAccessField)
            {
                await _mapper.UpdateAsync<ScyllaObject>("SET last_access_time = ? WHERE namespace = ? AND bucket = ? AND name = ?", lastAccessTime, ns.ToString(), bucket.ToString(), name.ToString());
            }

            await updateObjectLastAccessTask;
        }

        public async IAsyncEnumerable<(NamespaceId, BucketId, IoHashKey, DateTime)> GetRecords()
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.get_records");

            if (_settings.CurrentValue.UsePerShardScanning)
            {
                IAsyncEnumerable<(NamespaceId, BucketId, IoHashKey, DateTime)> enumerable = GetRecordsPerShard();

                await foreach ((NamespaceId, BucketId, IoHashKey, DateTime) record in enumerable)
                {
                    yield return (record.Item1, record.Item2, record.Item3, record.Item4);
                }
            }
            else
            {
                PreparedStatement getObjectStatement = _settings.CurrentValue.ListObjectsFromLastAccessTable
                    ? _getObjectsLastAccessStatement
                    : _getObjectsStatement;
                const int MaxRetryAttempts = 3;
                RowSet rowSet = await _session.ExecuteAsync(getObjectStatement.Bind());

                do
                {
                    int countOfRows = rowSet.GetAvailableWithoutFetching();
                    IEnumerable<Row> localRows = rowSet.Take(countOfRows);
                    Task prefetchTask = rowSet.FetchMoreResultsAsync();

                    foreach (Row row in localRows)
                    {
                        string ns = row.GetValue<string>("namespace");
                        string bucket = row.GetValue<string>("bucket");
                        string name = row.GetValue<string>("name");
                        DateTime? lastAccessTime = row.GetValue<DateTime?>("last_access_time");

                        // skip any names that are not conformant to io hash
                        if (name.Length != 40)
                        {
                            continue;
                        }

                        // if last access time is missing we treat it as being very old
                        lastAccessTime ??= DateTime.MinValue;
                        yield return (new NamespaceId(ns), new BucketId(bucket), new IoHashKey(name), lastAccessTime.Value);
                    }

                    int retryAttempts = 0;
                    Exception? timeoutException = null;
                    while (retryAttempts < MaxRetryAttempts)
                    {
                        try
                        {
                            await prefetchTask;
                            timeoutException = null;
                            break;
                        }
                        catch (ReadTimeoutException e)
                        {
                            retryAttempts += 1;
                            _logger.LogWarning(
                                "Cassandra read timeouts, waiting a while and then retrying. Attempt {Attempts} .",
                                retryAttempts);
                            // wait 10 seconds and try again as the Db is under heavy load right now
                            await Task.Delay(TimeSpan.FromSeconds(10));
                            timeoutException = e;
                        }
                    }

                    if (timeoutException != null)
                    {
                        _logger.LogWarning("Cassandra read timeouts, attempted {Attempts} attempts now we give up.",
                            retryAttempts);
                        // we have failed to many times, rethrow the exception and abort to avoid stalling here for ever
                        throw timeoutException;
                    }
                } while (!rowSet.IsFullyFetched);
            }
        }

        /// <summary>
        /// This implements a more efficient scanning where we fetch objects based on which shard it is in. It scans the entire database and thus returns all namespaces.
        /// See https://www.scylladb.com/2017/03/28/parallel-efficient-full-table-scan-scylla/
        /// </summary>
        /// <returns></returns>
        private async IAsyncEnumerable<(NamespaceId, BucketId, IoHashKey, DateTime)> GetRecordsPerShard()
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.get_records_per_shard");
            PreparedStatement getObjectStatement = _settings.CurrentValue.ListObjectsFromLastAccessTable
                ? _getObjectsLastAccessForPartitionRangeStatement
                : _getObjectsForPartitionRangeStatement;

            foreach ((long, long) range in ScyllaUtils.GetTableRanges(_settings.CurrentValue.CountOfNodes, _settings.CurrentValue.CountOfCoresPerNode, 3))
            {
                RowSet rowSet = await _session.ExecuteAsync(getObjectStatement.Bind(range.Item1, range.Item2));
                foreach (Row row in rowSet)
                {
                    string ns = row.GetValue<string>("namespace");
                    string bucket = row.GetValue<string>("bucket");
                    string name = row.GetValue<string>("name");
                    DateTime? lastAccessTime = row.GetValue<DateTime?>("last_access_time");

                    // skip any names that are not conformant to io hash
                    if (name.Length != 40)
                    {
                        continue;
                    }

                    // if last access time is missing we treat it as being very old
                    lastAccessTime ??= DateTime.MinValue;
                    yield return (new NamespaceId(ns), new BucketId(bucket), new IoHashKey(name), lastAccessTime.Value);
                }
            }
        }

        public async IAsyncEnumerable<NamespaceId> GetNamespaces()
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.get_namespaces");

            {
                RowSet rowSet = await _session.ExecuteAsync(_getNamespacesStatement.Bind());

                foreach (Row row in rowSet)
                {
                    if (rowSet.GetAvailableWithoutFetching() == 0)
                    {
                        await rowSet.FetchMoreResultsAsync();
                    }

                    yield return new NamespaceId(row.GetValue<string>(0));
                }
            }

            if (_settings.CurrentValue.ListObjectsFromOldNamespaceTable)
            {
                // this will likely generate duplicates from the statements above but that is not a huge issue
                using TelemetrySpan _ = _tracer.BuildScyllaSpan("scylla.get_old_namespaces");

                RowSet rowSet = await _session.ExecuteAsync(_getNamespacesOldStatement.Bind());

                foreach (Row row in rowSet)
                {
                    if (rowSet.GetAvailableWithoutFetching() == 0)
                    {
                        await rowSet.FetchMoreResultsAsync();
                    }

                    yield return new NamespaceId(row.GetValue<string>(0));
                }
            }
        }

        public async Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHashKey key)
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.delete_record").SetAttribute("resource.name", $"{ns}.{bucket}.{key}");

            AppliedInfo<ScyllaObject> info = await _mapper.DeleteIfAsync<ScyllaObject>("WHERE namespace=? AND bucket=? AND name=? IF EXISTS", ns.ToString(), bucket.ToString(), key.ToString());

            if (info.Applied)
            {
                return true;
            }

            return false;
        }

        public async Task<long> DropNamespace(NamespaceId ns)
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.delete_namespace");
            RowSet rowSet = await _session.ExecuteAsync(new SimpleStatement("SELECT bucket, name FROM objects WHERE namespace = ? ALLOW FILTERING;", ns.ToString()));
            long deletedCount = 0;
            foreach (Row row in rowSet)
            {
                string bucket = row.GetValue<string>("bucket");
                string name = row.GetValue<string>("name");

                await Delete(ns, new BucketId(bucket), new IoHashKey(name));

                deletedCount++;
            }

            // remove the tracking in the buckets table as well
            await _session.ExecuteAsync(new SimpleStatement("DELETE FROM buckets WHERE namespace = ?", ns.ToString()));

            return deletedCount;
        }

        public async Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.delete_bucket");

            RowSet rowSet = await _session.ExecuteAsync(new SimpleStatement("SELECT name FROM objects WHERE namespace = ? AND bucket = ? ALLOW FILTERING;", ns.ToString(), bucket.ToString()));
            long deletedCount = 0;
            foreach (Row row in rowSet)
            {
                string name = row.GetValue<string>("name");

                await Delete(ns, bucket, new IoHashKey(name));
                deletedCount++;
            }

            // remove the tracking in the buckets table as well
            await _mapper.DeleteAsync<ScyllaBucket>(new ScyllaBucket(ns, bucket));

            return deletedCount;
        }

        private async Task AddBucket(NamespaceId ns, BucketId bucket)
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("scylla.add_bucket");

            ConcurrentBag<BucketId> addedBuckets = _addedBuckets.GetOrAdd(ns, id => new ConcurrentBag<BucketId>());

            bool alreadyAdded = addedBuckets.Contains(bucket);
            if (!alreadyAdded)
            {
                Task addTask = _mapper.InsertAsync<ScyllaBucket>(new ScyllaBucket(ns, bucket));
                addedBuckets.Add(bucket);
                await addTask;
            }
        }
    }

    public class ObjectHashMismatchException : Exception
    {
        public ObjectHashMismatchException(NamespaceId ns, BucketId bucket, IoHashKey name, BlobIdentifier suppliedHash, BlobIdentifier actualHash) : base($"Object {name} in bucket {bucket} and namespace {ns} did not reference hash {suppliedHash} was referencing {actualHash}")
        {
        }
    }

    public class ScyllaBlobIdentifier
    {
        public ScyllaBlobIdentifier()
        {
            Hash = null;
        }

        public ScyllaBlobIdentifier(ContentHash hash)
        {
            Hash = hash.HashData;
        }

        public byte[]? Hash { get;set; }

        public BlobIdentifier AsBlobIdentifier()
        {
            return new BlobIdentifier(Hash!); 
        }
    }

    public class ScyllaObjectReference
    {
        // used by the cassandra mapper
        public ScyllaObjectReference()
        {
            Bucket = null!;
            Key = null!;
        }

        public ScyllaObjectReference(BucketId bucket, IoHashKey key)
        {
            Bucket = bucket.ToString();
            Key = key.ToString();
        }

        public string Bucket { get;set; }
        public string Key { get; set; }

        public (BucketId, IoHashKey) AsTuple()
        {
            return (new BucketId(Bucket), new IoHashKey(Key));
        }
    }

    [Cassandra.Mapping.Attributes.Table("objects")]
    public class ScyllaObject
    {
        public ScyllaObject()
        {

        }

        public ScyllaObject(NamespaceId ns, BucketId bucket, IoHashKey name, byte[] payload, BlobIdentifier payloadHash, bool isFinalized)
        {
            Namespace = ns.ToString();
            Bucket = bucket.ToString();
            Name = name.ToString();
            InlinePayload = payload;
            PayloadHash = new ScyllaBlobIdentifier(payloadHash);

            IsFinalized = isFinalized;

            LastAccessTime = DateTime.Now;
        }

        [Cassandra.Mapping.Attributes.PartitionKey(0)]
        public string? Namespace { get; set; }

        [Cassandra.Mapping.Attributes.PartitionKey(1)]
        public string? Bucket { get; set; }

        [Cassandra.Mapping.Attributes.PartitionKey(2)]
        public string? Name { get; set; }

        [Cassandra.Mapping.Attributes.Column("payload_hash")]
        public ScyllaBlobIdentifier? PayloadHash { get; set; }

        [Cassandra.Mapping.Attributes.Column("inline_payload")]
        public byte[]? InlinePayload {get; set; }

        [Cassandra.Mapping.Attributes.Column("is_finalized")]
        public bool? IsFinalized { get;set; }
        [Cassandra.Mapping.Attributes.Column("last_access_time")]
        public DateTime LastAccessTime { get; set; }

        public void ThrowIfRequiredFieldIsMissing()
        {
            if (string.IsNullOrEmpty(Namespace))
            {
                throw new InvalidOperationException("Namespace was not valid");
            }

            if (string.IsNullOrEmpty(Bucket))
            {
                throw new InvalidOperationException("Bucket was not valid");
            }

            if (string.IsNullOrEmpty(Name))
            {
                throw new InvalidOperationException("Name was not valid");
            }

            if (PayloadHash == null)
            {
                throw new ArgumentException("PayloadHash was not valid");
            }

            if (!IsFinalized.HasValue)
            {
                throw new ArgumentException("IsFinalized was not valid");
            }
        }
    }

    [Cassandra.Mapping.Attributes.Table("buckets_v2")]
    public class ScyllaBucket
    {
        public ScyllaBucket(NamespaceId ns, BucketId bucket)
        {
            Namespace = ns.ToString();
            Bucket = bucket.ToString();
        }

        [Cassandra.Mapping.Attributes.PartitionKey]
        public string? Namespace { get; set; }

        [Cassandra.Mapping.Attributes.ClusteringKey]
        public string Bucket { get; set; }
    }

    
    [Cassandra.Mapping.Attributes.Table("object_last_access_v2")]
    public class ScyllaObjectLastAccess
    {
        public ScyllaObjectLastAccess()
        {

        }

        public ScyllaObjectLastAccess(NamespaceId ns, BucketId bucket, IoHashKey name, DateTime lastAccessTime)
        {
            Namespace = ns.ToString();
            Bucket = bucket.ToString();
            Name = name.ToString();

            LastAccessTime = lastAccessTime;
        }

        [Cassandra.Mapping.Attributes.PartitionKey(0)]
        public string? Namespace { get; set; }

        [Cassandra.Mapping.Attributes.PartitionKey(1)]
        public string? Bucket { get; set; }

        [Cassandra.Mapping.Attributes.PartitionKey(2)]
        public string? Name { get; set; }

        [Cassandra.Mapping.Attributes.Column("last_access_time")]
        public DateTime LastAccessTime { get; set; }
    }
}
