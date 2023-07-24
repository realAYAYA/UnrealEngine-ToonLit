// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using Microsoft.Extensions.Caching.Memory;

namespace Jupiter
{
    
    public class UnrealCloudDDCSettings
    {
        public enum ReplicationLogWriterImplementations
        {
            Memory,
            Scylla,
            Mongo
        }

        public enum StorageBackendImplementations
        {
            S3,
            Azure,
            FileSystem,
            Memory,
            Relay,
            Peer
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

        public enum ServiceDiscoveryImplementations
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
            foreach (string s in StorageImplementations ?? new [] {UnrealCloudDDCSettings.StorageBackendImplementations.Memory.ToString()})
            {
                UnrealCloudDDCSettings.StorageBackendImplementations impl = (UnrealCloudDDCSettings.StorageBackendImplementations)Enum.Parse(typeof(UnrealCloudDDCSettings.StorageBackendImplementations), s, ignoreCase: true);

                yield return impl;
            }
        }

        [Required] public ReplicationLogWriterImplementations ReplicationLogWriterImplementation { get; set; } = ReplicationLogWriterImplementations.Memory;

        [Required]
        public ReferencesDbImplementations ReferencesDbImplementation { get; set; } = ReferencesDbImplementations.Memory;

        public LeaderElectionImplementations LeaderElectionImplementation { get; set; } = LeaderElectionImplementations.Static;
        public ContentIdStoreImplementations ContentIdStoreImplementation { get; set; } = ContentIdStoreImplementations.Memory;
        public BlobIndexImplementations BlobIndexImplementation { get; set; } = BlobIndexImplementations.Memory;
        public ServiceDiscoveryImplementations ServiceDiscoveryImplementation { get; set; } = ServiceDiscoveryImplementations.Static;

        public int? MaxSingleBlobSize { get; set; } = null; // disable blob partitioning

        public int LastAccessRollupFrequencySeconds { get; set; } = 900; // 15 minutes
        public bool EnableLastAccessTracking { get; set; } = true;
        public bool EnableOnDemandReplication { get; set; } = false;
    }

    public class MongoSettings
    {
        [Required] public string ConnectionString { get; set; } = "";

        public bool RequireTls12 { get; set; } = true;
        public bool CreateDatabaseIfMissing { get; set; } = true;
    }
    
    public class MemoryCacheContentIdSettings : MemoryCacheOptions
    {
        public bool Enabled { get; set; } = true;

        public bool EnableSlidingExpiry { get; set; } = true;
        public int SlidingExpirationMinutes { get; set; } = 120;
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
        public bool RunFilesystemCleanup { get; set; } = false;

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
