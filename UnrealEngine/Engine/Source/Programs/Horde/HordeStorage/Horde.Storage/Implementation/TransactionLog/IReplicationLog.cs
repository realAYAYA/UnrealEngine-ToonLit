// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Newtonsoft.Json;

namespace Horde.Storage.Implementation
{
    public interface IReplicationLog
    {
        IAsyncEnumerable<NamespaceId> GetNamespaces();

        Task<(string, Guid)> InsertAddEvent(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier objectBlob, DateTime? timeBucket = null);
        Task<(string, Guid)> InsertDeleteEvent(NamespaceId ns, BucketId bucket, IoHashKey key, DateTime? timeBucket = null);
        IAsyncEnumerable<ReplicationLogEvent> Get(NamespaceId ns, string? lastBucket, Guid? lastEvent);

        Task AddSnapshot(SnapshotInfo snapshotHeader);
        Task<SnapshotInfo?> GetLatestSnapshot(NamespaceId ns);
        IAsyncEnumerable<SnapshotInfo> GetSnapshots(NamespaceId ns);

        Task UpdateReplicatorState(NamespaceId ns, string replicatorName, ReplicatorState newState);
        Task<ReplicatorState?> GetReplicatorState(NamespaceId ns, string replicatorName);
    }

    public class SnapshotInfo
    {
        [JsonConstructor]
        public SnapshotInfo(NamespaceId snapshottedNamespace, NamespaceId blobNamespace, BlobIdentifier snapshotBlob, DateTime timestamp)
        {
            SnapshottedNamespace = snapshottedNamespace;
            BlobNamespace = blobNamespace;
            SnapshotBlob = snapshotBlob;
            Timestamp = timestamp;
        }

        public NamespaceId SnapshottedNamespace { get; set; }
        public NamespaceId BlobNamespace { get; set; }
        public BlobIdentifier SnapshotBlob { get; set; }
        public DateTime Timestamp { get; set; }
    }

    public class ReplicationLogEvent
    {
        public ReplicationLogEvent(NamespaceId @namespace, BucketId bucket, IoHashKey key, BlobIdentifier? blob, Guid eventId, string timeBucket, DateTime timestamp, OpType op)
        {
            Namespace = @namespace;
            Bucket = bucket;
            Key = key;
            Blob = blob;
            EventId = eventId;
            TimeBucket = timeBucket;
            Timestamp = timestamp;
            Op = op;
        }

        // these are serialized as ints so make sure to keep the value intact
        public enum OpType
        {
            Added = 0,
            Deleted = 1
        };

        public NamespaceId Namespace { get; }
        public BucketId Bucket { get; }
        public IoHashKey Key { get; }
        public OpType Op { get; }
        public DateTime Timestamp { get; }
        public string TimeBucket { get; }
        public Guid EventId { get; }
        public BlobIdentifier? Blob { get; }
    }

    public class IncrementalLogNotAvailableException : Exception
    {
    }
}
