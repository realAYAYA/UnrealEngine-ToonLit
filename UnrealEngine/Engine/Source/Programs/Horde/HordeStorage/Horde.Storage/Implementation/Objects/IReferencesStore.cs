// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;

namespace Horde.Storage.Implementation
{
    public interface IReferencesStore
    {
        Task<ObjectRecord> Get(NamespaceId ns, BucketId bucket, IoHashKey key, FieldFlags flags);

        [Flags]
        public enum FieldFlags
        {
            None = 0,
            IncludePayload = 1,
            All = IncludePayload
        }

        Task Put(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash, byte[] blob, bool isFinalized);
        Task Finalize(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobIdentifier);

        Task UpdateLastAccessTime(NamespaceId ns, BucketId bucket, IoHashKey key, DateTime newLastAccessTime);
        IAsyncEnumerable<(BucketId, IoHashKey, DateTime)> GetRecords(NamespaceId ns);

        IAsyncEnumerable<NamespaceId> GetNamespaces();
        Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHashKey key);
        Task<long> DropNamespace(NamespaceId ns);
        Task<long> DeleteBucket(NamespaceId ns, BucketId bucket);
    }

    public class ObjectRecord
    {
        public ObjectRecord(NamespaceId ns, BucketId bucket, IoHashKey name, DateTime lastAccess, byte[]? inlinePayload, BlobIdentifier blobIdentifier, bool isFinalized)
        {
            Namespace = ns;
            Bucket = bucket;
            Name = name;
            LastAccess = lastAccess;
            InlinePayload = inlinePayload;
            BlobIdentifier = blobIdentifier;
            IsFinalized = isFinalized;
        }

        public NamespaceId Namespace { get; }
        public BucketId Bucket { get; }
        public IoHashKey Name { get; }
        public DateTime LastAccess { get; }
        public byte[]? InlinePayload { get; set; }
        public BlobIdentifier BlobIdentifier { get; set; }
        public bool IsFinalized {get;}
    }

    public class ObjectNotFoundException : Exception
    {
        public ObjectNotFoundException(NamespaceId ns, BucketId bucket, IoHashKey key) : base($"Object not found {key} in bucket {bucket} namespace {ns}")
        {
            Namespace = ns;
            Bucket = bucket;
            Key = key;
        }

        public NamespaceId Namespace { get; }
        public BucketId Bucket { get; }
        public IoHashKey Key { get; }
    }
}
