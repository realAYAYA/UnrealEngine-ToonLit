// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;

namespace Horde.Storage.Implementation
{
    public class RefRecord
    {
        public RefRecord(NamespaceId ns, BucketId bucket, KeyId refName, BlobIdentifier[] blobs, DateTime? lastAccessTime,
            ContentHash contentHash, Dictionary<string, object>? metadata)
        {
            Namespace = ns;
            Bucket = bucket;
            RefName = refName;
            Blobs = blobs;
            Metadata = metadata;
            LastAccessTime = lastAccessTime;
            ContentHash = contentHash;
        }
        public NamespaceId Namespace { get; }

        public BucketId Bucket { get; }
        public KeyId RefName { get; }

        public BlobIdentifier[] Blobs { get; }
        public Dictionary<string, object>? Metadata { get; }
        public DateTime? LastAccessTime { get; internal set; }
        public ContentHash ContentHash { get; }

        public TransactionEvent ToAddTransactionEvent()
        {
            return new AddTransactionEvent(RefName.ToString(), Bucket.ToString(), Blobs, Metadata);
        }
    }

    public class OldRecord
    {
        public OldRecord(NamespaceId ns, BucketId bucket, KeyId refName)
        {
            Namespace = ns;
            Bucket = bucket;
            RefName = refName;
        }

        public NamespaceId Namespace { get; }

        public BucketId Bucket { get; }
        public KeyId RefName { get; }
    }

    public interface IRefsStore
    {
        [Flags]
        enum ExtraFieldsFlag
        {
            None = 0,
            LastAccess = 1,
            Metadata = 2
        }

        Task<RefRecord?> Get(NamespaceId ns, BucketId bucket, KeyId key, ExtraFieldsFlag fields);
        Task Add(RefRecord record);
        Task<long> DeleteBucket(NamespaceId ns, BucketId bucket);
        Task<long> Delete(NamespaceId ns, BucketId bucket, KeyId key);
        Task UpdateLastAccessTime(RefRecord record, DateTime lastAccessTime);

        IAsyncEnumerable<NamespaceId> GetNamespaces();
        IAsyncEnumerable<OldRecord> GetOldRecords(NamespaceId ns, TimeSpan oldRecordCutoff);
        Task DropNamespace(NamespaceId ns);
    }
}
