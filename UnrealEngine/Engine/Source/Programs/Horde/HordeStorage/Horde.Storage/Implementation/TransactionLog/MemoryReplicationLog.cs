// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Cassandra;
using Dasync.Collections;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;

namespace Horde.Storage.Implementation
{
    internal class MemoryReplicationLog : IReplicationLog
    {
        private readonly ConcurrentDictionary<NamespaceId, SortedList<string, SortedList<TimeUuid, ReplicationLogEvent>>> _replicationEvents = new();
        private readonly ConcurrentDictionary<NamespaceId, List<SnapshotInfo>>  _snapshots = new();

        private readonly ConcurrentDictionary<NamespaceId, ConcurrentDictionary<string, ReplicatorState>>  _replicatorState = new();

        public IAsyncEnumerable<NamespaceId> GetNamespaces()
        {
            return _replicationEvents.Keys.ToAsyncEnumerable();
        }

        public Task<(string, Guid)> InsertAddEvent(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier objectBlob, DateTime? timestamp)
        {
            return DoInsert(ns, bucket, key, objectBlob, ReplicationLogEvent.OpType.Added, timestamp);
        }

        private async Task<(string, Guid)> DoInsert(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier? hash, ReplicationLogEvent.OpType op, DateTime? lastTimestamp)
        {
            DateTime timestamp = lastTimestamp.GetValueOrDefault(DateTime.Now);

            return await Task.Run(() =>
            {
                DateTime hourlyBucket = timestamp.ToHourlyBucket();
                string bucketId = hourlyBucket.ToReplicationBucketIdentifier();
                TimeUuid eventId = TimeUuid.NewId(timestamp);
                ReplicationLogEvent logEvent = new ReplicationLogEvent(ns, bucket, key, hash, eventId, bucketId, hourlyBucket, op);

                _replicationEvents.AddOrUpdate(ns, _ =>
                {
                    SortedList<string, SortedList<TimeUuid, ReplicationLogEvent>> l = new() { { bucketId, new () { {eventId, logEvent} } } };
                    return l;
                }, (_, buckets) =>
                {
                    lock (buckets)
                    {
                        if (buckets.TryGetValue(bucketId, out SortedList<TimeUuid, ReplicationLogEvent>? events))
                        {
                            lock (events)
                            {
                                events.Add(eventId, logEvent);
                            }
                        }
                        else
                        {
                            buckets.Add(bucketId, new () {{eventId, logEvent} });
                        }
                    }

                    return buckets;
                });

                return (bucketId, eventId.ToGuid());
            });
        }

        public Task<(string, Guid)> InsertDeleteEvent(NamespaceId ns, BucketId bucket, IoHashKey key, DateTime? timestamp)
        {
            return DoInsert(ns, bucket, key, null, ReplicationLogEvent.OpType.Deleted, timestamp); 
        }

        public async IAsyncEnumerable<ReplicationLogEvent> Get(NamespaceId ns, string? lastBucket, Guid? lastEvent)
        {
            await Task.CompletedTask;
            if (!_replicationEvents.TryGetValue(ns, out SortedList<string, SortedList<TimeUuid, ReplicationLogEvent>>? buckets))
            {
                throw new NamespaceNotFoundException(ns);
            }

            // if no previous bucket was specified we started from the oldest one
            if (lastBucket == null)
            {
                // if we have no buckets we are done
                if (!buckets.Any())
                {
                    yield break;
                }

                lastBucket = buckets.First().Key;
            }

            int currentBucketIndex = buckets.IndexOfKey(lastBucket!);
            if (currentBucketIndex == -1)
            {
                // the bucket to resume from does not exist anymore, we should start from a snapshot
                throw new IncrementalLogNotAvailableException();
            }
            // loop over all the buckets after this including the previous one
            // we will then skip all the events we have previously processed
            for (int i = currentBucketIndex; i < buckets.Keys.Count; i++)
            {
                string bucket = buckets.Keys[i];

                if (!buckets.TryGetValue(bucket, out SortedList<TimeUuid, ReplicationLogEvent>? logEvents))
                {
                    yield break;
                }

                int skipCount = 0;
                if (lastEvent.HasValue)
                {
                    int eventIndex = logEvents.IndexOfKey(lastEvent.Value);
                    // if the event to resume from does not exist we need to use a snapshot instead
                    if (eventIndex == -1)
                    {
                        throw new IncrementalLogNotAvailableException();
                    }

                    skipCount = eventIndex + 1;
                }

                foreach ((TimeUuid _, ReplicationLogEvent value) in logEvents.Skip(skipCount))
                {
                    yield return value;
                }

                // once we have enumerated the bucket we were working on we should now return all events from the remaining ones
                lastEvent = null;
            }
        }

        public Task AddSnapshot(SnapshotInfo snapshotHeader)
        {
            _snapshots.AddOrUpdate(snapshotHeader.SnapshottedNamespace, _ => new List<SnapshotInfo> { snapshotHeader }, (_, list) =>
            {
                // we want the newest snapshots first
                list.Insert(0, snapshotHeader);
                const int maxCountOfSnapshots = 10;
                if (list.Count > maxCountOfSnapshots)
                {
                    // remove the last snapshots as they are the oldest
                    list.RemoveRange(maxCountOfSnapshots, list.Count - 10);
                }
                return list;
            });

            return Task.CompletedTask;
        }

        public Task<SnapshotInfo?> GetLatestSnapshot(NamespaceId ns)
        {
            if (!_snapshots.TryGetValue(ns, out List<SnapshotInfo>? snapshots))
            {
                return Task.FromResult<SnapshotInfo?>(null);
            }

            return Task.FromResult<SnapshotInfo?>(snapshots.Last());
        }

        public async IAsyncEnumerable<SnapshotInfo> GetSnapshots(NamespaceId ns)
        {
            await Task.CompletedTask;

            if (!_snapshots.TryGetValue(ns, out List<SnapshotInfo>? snapshots))
            {
                yield break;
            }

            foreach (SnapshotInfo snapshot in snapshots)
            {
                yield return snapshot;
            }
        }

        public Task UpdateReplicatorState(NamespaceId ns, string replicatorName,
            ReplicatorState newState)
        {
            _replicatorState.AddOrUpdate(ns,
                _ =>
                new ConcurrentDictionary<string, ReplicatorState>
                {
                    [replicatorName] = newState
                },
                (_, states) =>
                {
                    states[replicatorName] = newState;
                    return states;
                });

            return Task.CompletedTask;
        }

        public Task<ReplicatorState?> GetReplicatorState(NamespaceId ns, string replicatorName)
        {
            if (_replicatorState.TryGetValue(ns, out ConcurrentDictionary<string, ReplicatorState>? replicationState))
            {
                replicationState.TryGetValue(replicatorName, out ReplicatorState? state);
                return Task.FromResult(state);
            }

            return Task.FromResult<ReplicatorState?>(null);
        }
    }

    public static class DateTimeUtils
    {
        public static DateTime ToHourlyBucket(this DateTime timestamp)
        {
            return new DateTime(timestamp.Year, timestamp.Month, timestamp.Day, timestamp.Hour, 0, 0);
        }

        public static string ToReplicationBucketIdentifier(this DateTime timestamp)
        {
            return $"rep-{timestamp.ToFileTimeUtc()}";
        }
    }
}
