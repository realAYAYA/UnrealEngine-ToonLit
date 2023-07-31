// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Options;
using Microsoft.Extensions.Primitives;
using Serilog;
using Serilog.Context;

namespace Horde.Storage.Implementation
{
    public class CachedRefStore : IRefsStore, IDisposable
    {
        private readonly IRefsStore _backingStore;
        private readonly IOptionsMonitor<MemoryCacheRefSettings> _memoryOptions;
        private readonly ConcurrentDictionary<NamespaceId, BoolChangeToken> _namespaceChangeTokens;
        private readonly ConcurrentDictionary<string, BoolChangeToken> _bucketChangeTokens;
        private readonly MemoryCache _memoryCache;

        private readonly ILogger _logger = Log.ForContext<CachedRefStore>();

        // ReSharper disable once NotAccessedField.Local
        private readonly Timer _cacheInfoTimer;

        public CachedRefStore(IRefsStore backingStore, IOptionsMonitor<MemoryCacheRefSettings> memoryOptions)
        {
            _backingStore = backingStore;
            _memoryOptions = memoryOptions;
            _memoryCache = new MemoryCache(_memoryOptions.CurrentValue);
            _namespaceChangeTokens = new ConcurrentDictionary<NamespaceId, BoolChangeToken>();
            _bucketChangeTokens = new ConcurrentDictionary<string, BoolChangeToken>();

            _cacheInfoTimer = new Timer(LogCacheInfo, null, TimeSpan.Zero, TimeSpan.FromSeconds(60));
        }

        public IRefsStore BackingStore => _backingStore;

        private void LogCacheInfo(object? state)
        {
            LogContext.Reset();
            _logger.Information("Memory db-cache {Count}", _memoryCache.Count);
        }

        private static string BuildCacheKey(NamespaceId ns, BucketId bucket, KeyId key)
        {
            return $"{ns}.{bucket}.{key}";
        }

        public async Task<RefRecord?> Get(NamespaceId ns, BucketId bucket, KeyId key, IRefsStore.ExtraFieldsFlag fields)
        {
            // we do not cache last access as it is frequently updated, if this fields is requested this cache can not be used
            if (fields.HasFlag(IRefsStore.ExtraFieldsFlag.LastAccess))
            {
                return await _backingStore.Get(ns, bucket, key, fields);
            }

            string cacheKey = BuildCacheKey(ns, bucket, key);
            if (_memoryCache.TryGetValue(cacheKey, out RefRecord? cachedKey))
            {
                return cachedKey;
            }

            // override the fields used so that we get a full object for use by the cache
            IRefsStore.ExtraFieldsFlag localFields = IRefsStore.ExtraFieldsFlag.Metadata;
            RefRecord? backendRecord = await _backingStore.Get(ns, bucket, key, localFields);
            
            // do not refs a miss as it could get added at any time
            if (backendRecord == null)
            {
                return null;
            }

            CreateMemoryCacheEntry(cacheKey, backendRecord, ns, bucket);

            return backendRecord;
        }

        private void CreateMemoryCacheEntry(string cacheKey, object value, NamespaceId ns, BucketId bucket)
        {
            using ICacheEntry cacheEntry = _memoryCache.CreateEntry(cacheKey);

            if (_memoryOptions.CurrentValue.EnableSlidingExpiry)
            {
                cacheEntry.SlidingExpiration = TimeSpan.FromMinutes(_memoryOptions.CurrentValue.SlidingExpirationMinutes);
            }

            cacheEntry.RegisterPostEvictionCallback((key, value, reason, state) =>
            {
                _logger.Information("{Key} evicted from db cache because {Reason}", key, reason);
            });
            cacheEntry.Value = value;

            BoolChangeToken namespaceChangeToken = _namespaceChangeTokens.GetOrAdd(ns, new BoolChangeToken());
            cacheEntry.ExpirationTokens.Add(namespaceChangeToken);

            BoolChangeToken bucketChangeToken = _bucketChangeTokens.GetOrAdd($"{ns}.{bucket}", new BoolChangeToken());
            cacheEntry.ExpirationTokens.Add(bucketChangeToken);
        }

        public Task Add(RefRecord record)
        {
            string cacheKey = BuildCacheKey(record.Namespace, record.Bucket, record.RefName);
            CreateMemoryCacheEntry(cacheKey, record, record.Namespace, record.Bucket);

            return _backingStore.Add(record);
        }

        public Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
        {
            if (_bucketChangeTokens.TryGetValue($"{ns}.{bucket}", out BoolChangeToken? bucketToken))
            {
                // sets the objects as invalid and will be cleaned up when the next compact runs
                bucketToken.HasChanged = true;
            }

            return _backingStore.DeleteBucket(ns, bucket);
        }

        public Task<long> Delete(NamespaceId ns, BucketId bucket, KeyId key)
        {
            string cacheKey = BuildCacheKey(ns, bucket, key);
            _memoryCache.Remove(cacheKey);

            return _backingStore.Delete(ns, bucket, key);
        }

        public Task UpdateLastAccessTime(RefRecord record, DateTime lastAccessTime)
        {
            // we do not cache last access time so we just calling the backing store
            return _backingStore.UpdateLastAccessTime(record, lastAccessTime);
        }

        public IAsyncEnumerable<NamespaceId> GetNamespaces()
        {
            // we do not cache namespaces so we just call the backing store
            return _backingStore.GetNamespaces();
        }

        public IAsyncEnumerable<OldRecord> GetOldRecords(NamespaceId ns, TimeSpan oldRecordCutoff)
        {
            // we do not cache the last access time so we can not filter out old records, thus we call the backing store
            return _backingStore.GetOldRecords(ns, oldRecordCutoff);
        }

        public Task DropNamespace(NamespaceId ns)
        {
            if (_namespaceChangeTokens.TryGetValue(ns, out BoolChangeToken? namespaceToken))
            {
                // sets the objects as invalid and will be cleaned up when the next compact runs
                namespaceToken.HasChanged = true;
            }

            return _backingStore.DropNamespace(ns);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                _memoryCache.Dispose();
                _cacheInfoTimer.Dispose();
            }
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }
    }

    class BoolChangeToken : IChangeToken
    {
        public IDisposable RegisterChangeCallback(Action<object> callback, object state)
        {
            throw new NotImplementedException();
        }

        public bool HasChanged { get; set; }

        public bool ActiveChangeCallbacks => false;
    }
}
