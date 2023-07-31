// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using Dasync.Collections;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Jupiter.Utils;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Options;
using Serilog;
using Serilog.Context;
using ILogger = Serilog.ILogger;

namespace Horde.Storage.Implementation
{
    /// <summary>
    /// In-memory only store implementing the IBlobStore interface using Microsoft's MemoryCache
    /// Primarily intended for use together with HierarchicalBlobStore to speed up reads from other stores, such as Amazon S3 and disk.
    /// </summary>
    // ReSharper disable once ClassNeverInstantiated.Global
    public class MemoryCacheBlobStore : IBlobStore, IDisposable
    {
        private readonly IOptionsMonitor<MemoryCacheBlobSettings> _memoryOptions;

        private readonly MemoryCache _memoryCache;

        private readonly ILogger _logger = Log.ForContext<MemoryCacheBlobStore>();
        private readonly FieldInfo _sizeField;
        private readonly Timer _cacheInfoTimer;

        public MemoryCacheBlobStore(IOptionsMonitor<MemoryCacheBlobSettings> memoryOptions)
        {
            _memoryOptions = memoryOptions;
            _memoryCache = new MemoryCache(memoryOptions.CurrentValue);
            _sizeField = typeof(MemoryCache).GetField("_cacheSize", BindingFlags.NonPublic | BindingFlags.Instance) ?? throw new Exception("Unable to find internal cache size field, did the memory cache get refactored?");
            _cacheInfoTimer = new Timer(LogCacheInfo, null, TimeSpan.Zero, TimeSpan.FromMinutes(5));
        }

        private void LogCacheInfo(object? state)
        {
            LogContext.Reset();
            _logger.Information("Memory Cache Blobs {Size} {Count}", GetMemoryCacheSize(), _memoryCache.Count);
        }

        public Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] blob, BlobIdentifier identifier)
        {
            using ICacheEntry entry = _memoryCache.CreateEntry(BuildKey(ns, identifier));
            entry.Value = blob;
            entry.Size = blob.Length;
            
            if (_memoryOptions.CurrentValue.EnableSlidingExpiry)
            {
                entry.SlidingExpiration = TimeSpan.FromMinutes(_memoryOptions.CurrentValue.SlidingExpirationMinutes);
            }

            return Task.FromResult(identifier);
        }

        public Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> blob, BlobIdentifier identifier)
        {
            return PutObject(ns, blob: blob.ToArray(), identifier);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, Stream blob, BlobIdentifier identifier)
        {
            if (blob.Length > int.MaxValue)
            {
                throw new BlobToLargeException(identifier);
            }

            return await PutObject(ns, blob: await blob.ToByteArray(), identifier);
        }

        public Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier identifier, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking)
        {
            bool exists = _memoryCache.TryGetValue(BuildKey(ns, identifier), out object? dataObj);
            if (!exists)
            {
                throw new BlobNotFoundException(ns, identifier);
            }

            byte[] data = (dataObj as byte[])!;
            return Task.FromResult(new BlobContents(new MemoryStream(data), data.LongLength));
        }

        public Task DeleteObject(NamespaceId ns, BlobIdentifier blobIdentifier)
        {
            _memoryCache.Remove(BuildKey(ns, blobIdentifier));
            return Task.CompletedTask;
        }

        public Task<bool> Exists(NamespaceId ns, BlobIdentifier blobIdentifier, bool forceCheck = false)
        {
            return Task.FromResult(_memoryCache.TryGetValue(BuildKey(ns, blobIdentifier), out _));
        }

        public Task DeleteNamespace(NamespaceId ns)
        {
            // Silently ignore this request as we cannot implement this using MemoryCache
            return Task.CompletedTask;
        }

        public IAsyncEnumerable<(BlobIdentifier,DateTime)> ListObjects(NamespaceId ns)
        {
            // Silently ignore this request as we cannot implement this using MemoryCache
            return Array.Empty<(BlobIdentifier, DateTime)>().ToAsyncEnumerable();
        }
        
        private static string BuildKey(NamespaceId ns, BlobIdentifier blob)
        {
            return $"{ns}.{blob}";
        }

        internal long? GetMemoryCacheSize()
        {
            return (long?)_sizeField.GetValue(_memoryCache);
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
}
