// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Common;
using Jupiter.Utils;

namespace Jupiter.Implementation
{
    internal class MemoryBlobStore : IBlobStore
    {
        private readonly ConcurrentDictionary<NamespaceId, MemoryStorageBackend> _backends = new ConcurrentDictionary<NamespaceId, MemoryStorageBackend>();

        /// <summary>
        /// Throw an exception when putting an object that already exists
        ///
        /// Primarily used for testing.
        /// </summary>
        private readonly bool _throwOnOverwrite;

        public MemoryBlobStore(bool throwOnOverwrite = false)
        {
            _throwOnOverwrite = throwOnOverwrite;
        }

        internal MemoryStorageBackend GetBackend(NamespaceId ns)
        {
            return _backends.GetOrAdd(ns, x => new MemoryStorageBackend(_throwOnOverwrite));
        }

        private static string GetPath(BlobIdentifier blob) => blob.ToString();

        public Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] blob, BlobIdentifier identifier)
        {
            using MemoryStream stream = new MemoryStream(blob);
            return PutObject(ns, stream, identifier);
        }
        public Task<Uri?> GetObjectByRedirect(NamespaceId ns, BlobIdentifier identifier)
        {
            // not supported
            return Task.FromResult<Uri?>(null);
        }

        public Task<Uri?> PutObjectWithRedirect(NamespaceId ns, BlobIdentifier identifier)
        {
            // not supported
            return Task.FromResult<Uri?>(null);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> blob, BlobIdentifier identifier)
        {
            return await PutObject(ns, blob: blob.ToArray(), identifier);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, Stream blob, BlobIdentifier identifier)
        {
            IStorageBackend backend = GetBackend(ns);
            await backend.WriteAsync(GetPath(identifier), blob);
            return identifier;
        }

        public async Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking, bool supportsRedirectUri = false)
        {
            BlobContents? contents = await GetBackend(ns).TryReadAsync(GetPath(blob), flags);
            if(contents == null)
            {
                throw new BlobNotFoundException(ns, blob);
            }
            return contents;
        }

        public Task DeleteObject(NamespaceId ns, BlobIdentifier blob) => GetBackend(ns).DeleteAsync(GetPath(blob));

        public Task<bool> Exists(NamespaceId ns, BlobIdentifier blob, bool forceCheck = false) => GetBackend(ns).ExistsAsync(GetPath(blob));

        public Task DeleteNamespace(NamespaceId ns)
        {
            if (!_backends.TryRemove(ns, out _))
            {
                throw new NamespaceNotFoundException(ns);
            }
            return Task.CompletedTask;
        }

        public async IAsyncEnumerable<(BlobIdentifier,DateTime)> ListObjects(NamespaceId ns)
        {
            IStorageBackend backend = GetBackend(ns);
            await foreach ((string path, DateTime time) in backend.ListAsync())
            {
                yield return (new BlobIdentifier(path), time);
            }
        }

        internal IEnumerable<BlobIdentifier> GetIdentifiers(NamespaceId ns)
        {
            return GetBackend(ns).GetIdentifiers().Select(x => new BlobIdentifier(x));
        }

        // only for unit tests to update the last modified time
        internal void SetLastModifiedTime(NamespaceId ns, BlobIdentifier blob, DateTime modifiedTime)
        {
            GetBackend(ns).SetLastModifiedTime(GetPath(blob), modifiedTime);
        }
    }

    internal class MemoryStorageBackend : IStorageBackend
    {
        private class BlobContainer
        {
            public string Path { get; }
            public byte[] Contents { get; }
            public DateTime LastModified { get; set; }

            public BlobContainer(string path, byte[] contents)
            {
                Path = path;
                Contents = contents;
                LastModified = DateTime.Now;
            }
        }

        private readonly ConcurrentDictionary<string, BlobContainer> _blobs = new ConcurrentDictionary<string, BlobContainer>(StringComparer.Ordinal);

        /// <summary>
        /// Throw an exception when putting an object that already exists
        ///
        /// Primarily used for testing.
        /// </summary>
        private readonly bool throwOnOverwrite;

        public MemoryStorageBackend(bool throwOnOverwrite = false)
        {
            this.throwOnOverwrite = throwOnOverwrite;
        }

        public async Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken)
        {
            byte[] blob = await stream.ToByteArray();

            // we do not split the blob into smaller parts when storing in memory, this is only for test purposes
            // so there is no need to add that complexity

            if (throwOnOverwrite && _blobs.ContainsKey(path))
            {
                throw new Exception($"Blob {path} already exists");
            }
            _blobs.TryAdd(path, new BlobContainer(path, blob.ToArray()));
        }

        public Task<BlobContents?> TryReadAsync(string path, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking, CancellationToken cancellationToken = default)
        {
            if (!_blobs.TryGetValue(path, value: out BlobContainer? blobContainer))
            {
                return Task.FromResult<BlobContents?>(null);
            }

            byte[] content = blobContainer.Contents;
            return Task.FromResult<BlobContents?>(new BlobContents(new MemoryStream(content), content.LongLength));
        }

        public Task DeleteAsync(string path, CancellationToken cancellationToken = default)
        {
            _blobs.TryRemove(path, out _);
            return Task.CompletedTask;
        }

        public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default)
        {
            return Task.FromResult(_blobs.ContainsKey(path));
        }

        public async IAsyncEnumerable<(string, DateTime)> ListAsync([EnumeratorCancellation] CancellationToken cancellationToken)
        {
            await Task.Yield();
            foreach (BlobContainer blobContainer in _blobs.Values)
            {
                yield return (blobContainer.Path, blobContainer.LastModified);
            }
        }

        public IEnumerable<string> GetIdentifiers() => _blobs.Keys;

        // only for unit tests to update the last modified time
        public void SetLastModifiedTime(string path, DateTime modifiedTime)
        {
            if (_blobs.TryGetValue(path, value: out BlobContainer? blobContainer))
            {
                blobContainer.LastModified = modifiedTime;
            }
        }
    }
}
