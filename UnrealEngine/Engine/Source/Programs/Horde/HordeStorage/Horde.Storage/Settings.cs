// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using Microsoft.Extensions.Caching.Memory;

namespace Horde.Storage
{
    
    public class HordeStorageSettings
    {
        public enum RefDbImplementations
        {
            Memory,
            Mongo,
            Cosmos,
            DynamoDb
        }

        public enum TransactionLogWriterImplementations
        {
            Callisto,
            Memory
        }

        public enum ReplicationLogWriterImplementations
        {
            Memory,
            Scylla,
            Mongo
        }

        public enum TreeStoreImplementations
        {
            Memory,
            DynamoDb
        }
        public enum TreeRootStoreImplementations
        {
            Memory,
            DynamoDb
        }

        public enum StorageBackendImplementations
        {
            S3,
            Azure,
            FileSystem,
            Memory,
            MemoryBlobStore,
            Relay
        }

        public enum ReferencesDbImplementations
        {
            Memory,
            Scylla,
            Mongo,
            Cache
        }

        public enum ContentIdStoreImplementations
        {
            Memory,
            Scylla,
            Mongo,
            Cache
        }

        public enum BlobIndexImplementations
        {
            Memory,
            Scylla,
            Mongo,
            Cache
        }

        public enum LeaderElectionImplementations
        {
            Static, 
            Kubernetes
        }

        private sealed class ValidStorageBackend : ValidationAttribute
        {
            public override string FormatErrorMessage(string name)
            {
                return "Need to specify at least one storage backend. Valid ones are: " +
                       string.Join(", ", Enum.GetNames(typeof(StorageBackendImplementations)));
            }

            public override bool IsValid(object? value)
            {
                if (value == null)
                {
                    return true;
                }

                return value is IEnumerable<string> backends && backends.All(x => Enum.TryParse(typeof(StorageBackendImplementations), x, true, out _));
            }
        }

        [ValidStorageBackend]
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Naming", "CA1721:Property names should not match get methods", Justification = "This pattern is used to work around limitations in dotnet configurations support for enums in arrays")]
        public string[]? StorageImplementations { get; set; }

        public IEnumerable<StorageBackendImplementations> GetStorageImplementations()
        {
            foreach (string s in StorageImplementations ?? new [] {HordeStorageSettings.StorageBackendImplementations.Memory.ToString()})
            {
                HordeStorageSettings.StorageBackendImplementations impl = (HordeStorageSettings.StorageBackendImplementations)Enum.Parse(typeof(HordeStorageSettings.StorageBackendImplementations), s, ignoreCase: true);

                yield return impl;
            }
        }

        [Required]
        public TransactionLogWriterImplementations TransactionLogWriterImplementation { get; set; } = TransactionLogWriterImplementations.Memory;

        [Required] public ReplicationLogWriterImplementations ReplicationLogWriterImplementation { get; set; } = ReplicationLogWriterImplementations.Memory;

        [Required] public RefDbImplementations RefDbImplementation { get; set; } = RefDbImplementations.Memory;

        [Required]
        public TreeStoreImplementations TreeStoreImplementation { get; set; } = TreeStoreImplementations.Memory;

        [Required]
        public TreeRootStoreImplementations TreeRootStoreImplementation { get; set; } = TreeRootStoreImplementations.Memory;

        [Required]
        public ReferencesDbImplementations ReferencesDbImplementation { get; set; } = ReferencesDbImplementations.Memory;

        public LeaderElectionImplementations LeaderElectionImplementation { get; set; } = LeaderElectionImplementations.Static;
        public ContentIdStoreImplementations ContentIdStoreImplementation { get; set; } = ContentIdStoreImplementations.Memory;
        public BlobIndexImplementations BlobIndexImplementation { get; set; } = BlobIndexImplementations.Memory;

        public int? MaxSingleBlobSize { get; set; } = null; // disable blob partitioning

        public int LastAccessRollupFrequencySeconds { get; set; } = 900; // 15 minutes
        public bool EnableLastAccessTracking { get; set; } = true;
        public bool EnableOnDemandReplication { get; set; } = false;

        // disable the legacy api
        public bool DisableLegacyApi { get; set; } = false;
    }

    public class MongoSettings
    {
        [Required] public string ConnectionString { get; set; } = "";

        public bool RequireTls12 { get; set; } = true;
        public bool CreateDatabaseIfMissing { get; set; } = true;
    }

    public class DynamoDbSettings
    {
        [Required] public string ConnectionString { get; set; } = "";

        public long ReadCapacityUnits { get; set; } = 100;
        public long WriteCapacityUnits { get; set; } = 20;

        public bool UseOndemandCapacityProvisioning { get; set; } = true;
        
        /// <summary>
        /// Endpoint name for DynamoDB Accelerator (DAX). Acts as a cache in front of DynamoDB to speed up requests
        /// Disabled when set to null.
        /// </summary>
        public string? DaxEndpoint { get; set; } = null;

        /// <summary>
        /// Enabling this will make Horde.Storage create missing tables on demand, works great for local tables but for global tables its easier to have terraform manage it
        /// </summary>
        public bool CreateTablesOnDemand { get; set; } = true;

        public static (string, int) ParseDaxEndpointAsHostPort(string endpoint)
        {
            if (!endpoint.Contains(':', StringComparison.InvariantCultureIgnoreCase))
            {
                return (endpoint, 8111);
            }

            string host = endpoint.Split(":")[0];
            int port = Convert.ToInt32(endpoint.Split(":")[1]);
            return (host, port);
        }
    }

    public class CosmosSettings
    {
        [Range(400, 10_000)] public int DefaultRU { get; set; } = 400;
    }

    public class CallistoTransactionLogSettings
    {
        [Required] public string ConnectionString { get; set; } = "";
    }

    public class MemoryCacheContentIdSettings : MemoryCacheOptions
    {
        public bool Enabled { get; set; } = true;

        public bool EnableSlidingExpiry { get; set; } = true;
        public int SlidingExpirationMinutes { get; set; } = 120;
    }

	public class MemoryCacheBlobSettings : MemoryCacheOptions
    {
        public bool Enabled { get; set; } = true;

        public bool EnableSlidingExpiry { get; set; } = true;
        public int SlidingExpirationMinutes { get; set; } = 60;
    }

    public class MemoryCacheTreeSettings : MemoryCacheOptions
    {
        public bool Enabled { get; set; } = true;
        public bool EnableSlidingExpiry { get; set; } = true;
        public int SlidingExpirationMinutes { get; set; } = 60;
    }

    public class MemoryCacheRefSettings : MemoryCacheOptions
    {
        public bool Enabled { get; set; } = true;
        public bool EnableSlidingExpiry { get; set; } = true;
        public int SlidingExpirationMinutes { get; set; } = 60;
    }

    public class AzureSettings
    {
        [Required] public string ConnectionString { get; set; } = string.Empty;
    }

    public class FilesystemSettings
    {
        [Required] public string RootDir { get; set; } = "";
        public ulong MaxSizeBytes { get; set; } = 500 * 1024 * 1024;
        public double TriggerThresholdPercentage { get; set; } = 0.95;
        public double TargetThresholdPercentage { get; set; } = 0.85;
    }

    public class S3Settings
    {
        [Required] public string ConnectionString  { get; set; } = "";

        [Required] public string BucketName { get; set; } = "";

        public bool ForceAWSPathStyle { get; set; }
        public bool CreateBucketIfMissing { get;set; } = true;

        // Options to disable setting of bucket access policies, useful for local testing as minio does not support them.
        public bool SetBucketPolicies { get; set; } = true;

        public bool UseBlobIndexForExistsCheck { get; set; } = false;
    }

    public class GCSettings
    {
        public bool BlobCleanupServiceEnabled { get; set; } = true;
        
        public bool CleanOldRefRecords { get; set; } = false;
        public bool CleanOldBlobs { get; set; } = true;
        public bool CleanOldBlobsLegacy { get; set; } = false;

        public TimeSpan LastAccessCutoff { get; set; } = TimeSpan.FromDays(14);

        public TimeSpan BlobCleanupPollFrequency { get; set; } = TimeSpan.FromMinutes(60);
        public TimeSpan RefCleanupPollFrequency { get; set; } = TimeSpan.FromMinutes(60);
        public int OrphanGCMaxParallelOperations { get; set; } = 8;
        public int OrphanRefMaxParallelOperations { get; set; } = 8;
    }

    public class UpstreamRelaySettings
    {
        [Required] public string ConnectionString { get; set; } = null!;
    }

    public class ClusterSettings
    {
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
        public List<PeerSettings> Peers { get; set; } = new List<PeerSettings>();
    }

    public class PeerSettings
    {
        [Required] public string Name { get; set; } = null!;

        [Required] public string FullName { get; set; } = null!;

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
        public List<PeerEndpoints> Endpoints { get; set; } = new List<PeerEndpoints>();
    }

    public class PeerEndpoints
    { 
        [Required] public Uri Url { get; set; } = null!;

        public bool IsInternal { get; set; } = false;
    }

    public class ConsistencyCheckSettings
    {
        public bool EnableBlobStoreChecks { get; set; } = false;
        public bool EnableBlobIndexChecks { get; set; } = false;
        public double ConsistencyCheckPollFrequencySeconds { get; set; } = TimeSpan.FromHours(2).TotalSeconds;
        public int BlobIndexMaxParallelOperations { get; set; } = 4;
        public bool AllowDeletesInBlobIndex { get; set; } = false;
        public bool RunBlobStoreConsistencyCheckOnRootStore { get; set; } = false;
    }
}
