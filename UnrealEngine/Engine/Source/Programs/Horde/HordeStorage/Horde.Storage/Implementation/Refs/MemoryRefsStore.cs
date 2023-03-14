// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Dasync.Collections;
using Jupiter.Implementation;
using RefContainer = System.Collections.Concurrent.ConcurrentDictionary<Jupiter.Implementation.KeyId, Horde.Storage.Implementation.RefRecord>;
using BucketContainer = System.Collections.Concurrent.ConcurrentDictionary<EpicGames.Horde.Storage.BucketId, System.Collections.Concurrent.ConcurrentDictionary<Jupiter.Implementation.KeyId, Horde.Storage.Implementation.RefRecord>>;
using NamespaceContainer = System.Collections.Concurrent.ConcurrentDictionary<EpicGames.Horde.Storage.NamespaceId, System.Collections.Concurrent.ConcurrentDictionary<EpicGames.Horde.Storage.BucketId, System.Collections.Concurrent.ConcurrentDictionary<Jupiter.Implementation.KeyId, Horde.Storage.Implementation.RefRecord>>>;
using EpicGames.Horde.Storage;

namespace Horde.Storage.Implementation
{
    internal class MemoryRefsStore : IRefsStore
    {
        private readonly NamespaceContainer _refs = new NamespaceContainer();

        public Task<RefRecord?> Get(NamespaceId ns, BucketId bucket, KeyId key, IRefsStore.ExtraFieldsFlag fields)
        {
            BucketContainer bucketContainer = _refs.GetOrAdd(ns, new BucketContainer());
            RefContainer refContainer = bucketContainer.GetOrAdd(bucket, new RefContainer());
            if (refContainer.TryGetValue(key, out RefRecord? refObject))
            {
                return Task.FromResult<RefRecord?>(refObject);
            }

            return Task.FromResult<RefRecord?>(null);
        }

        public Task Add(RefRecord record)
        {
            BucketContainer bucketContainer = _refs.GetOrAdd(record.Namespace, new BucketContainer());
            RefContainer refContainer = bucketContainer.GetOrAdd(record.Bucket, new RefContainer());
            refContainer.AddOrUpdate(record.RefName, id => record, (id, o) => record);

            return Task.CompletedTask;
        }

        public Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
        {
            BucketContainer bucketContainer = _refs.GetOrAdd(ns, new BucketContainer());
            if (bucketContainer.TryRemove(bucket, out RefContainer? refContainer))
            {
                return Task.FromResult<long>(refContainer.Keys.Count);
            }

            return Task.FromResult(0L);
        }

        public Task<long> Delete(NamespaceId ns, BucketId bucket, KeyId key)
        {
            BucketContainer bucketContainer = _refs.GetOrAdd(ns, new BucketContainer());
            RefContainer refContainer = bucketContainer.GetOrAdd(bucket, new RefContainer());

            if (refContainer.TryRemove(key, out RefRecord? _))
            {
                return Task.FromResult(1L);
            }

            return Task.FromResult(0L);
        }

        public Task UpdateLastAccessTime(RefRecord record, DateTime lastAccessTime)
        {
            BucketContainer bucketContainer = _refs.GetOrAdd(record.Namespace, new BucketContainer());
            RefContainer refContainer = bucketContainer.GetOrAdd(record.Bucket, new RefContainer());
            if (refContainer.TryGetValue(record.RefName, out RefRecord? refRecord))
            {
                refRecord.LastAccessTime = lastAccessTime;
            }

            return Task.CompletedTask;
        }

        public IAsyncEnumerable<NamespaceId> GetNamespaces()
        {
            return _refs.Keys.ToAsyncEnumerable();
        }

        public async IAsyncEnumerable<OldRecord> GetOldRecords(NamespaceId ns, TimeSpan oldRecordCutoff)
        {
            DateTime cutoff = DateTime.Now.AddSeconds(-1 * oldRecordCutoff.TotalSeconds);
            BucketContainer bucketContainer = _refs.GetOrAdd(ns, new BucketContainer());

            await Task.CompletedTask;

            foreach (BucketId bucket in bucketContainer.Keys)
            {
                RefContainer refContainer = bucketContainer[bucket];
                foreach (KeyId keyId in refContainer.Keys)
                {
                    RefRecord refRecord = refContainer[keyId];
                    if (refRecord.LastAccessTime < cutoff)
                    {
                        yield return new OldRecord(ns, bucket, keyId);
                    }
                }
            }
        }

        public Task DropNamespace(NamespaceId ns)
        {
            _refs.TryRemove(ns, out BucketContainer? _);

            return Task.CompletedTask;
        }
    }

    class RefObject
    {
    }
}
