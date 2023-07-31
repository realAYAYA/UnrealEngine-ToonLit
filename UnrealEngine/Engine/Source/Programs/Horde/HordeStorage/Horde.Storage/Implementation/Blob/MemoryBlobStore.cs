// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Jupiter.Utils;

namespace Horde.Storage.Implementation
{
    internal class MemoryBlobStore : IBlobStore
    {
        private class BlobContainer
        {
            public BlobIdentifier BlobIdentifier { get; }
            public byte[] Contents { get; }
            public DateTime LastModified { get; set; }

            public BlobContainer(BlobIdentifier blobIdentifier, byte[] contents)
            {
                BlobIdentifier = blobIdentifier;
                Contents = contents;
                LastModified = DateTime.Now;
            }
        }

        private readonly ConcurrentDictionary<NamespaceId, ConcurrentDictionary<BlobIdentifier, BlobContainer>> _blobs = new ConcurrentDictionary<NamespaceId, ConcurrentDictionary<BlobIdentifier, BlobContainer>>();
        
        /// <summary>
        /// Throw an exception when putting an object that already exists
        ///
        /// Primarily used for testing.
        /// </summary>
        private readonly bool throwOnOverwrite;

        public MemoryBlobStore(bool throwOnOverwrite = false)
        {
            this.throwOnOverwrite = throwOnOverwrite;
        }

        public Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] blob, BlobIdentifier? identifier = null)
        {
            // we do not split the blob into smaller parts when storing in memory, this is only for test purposes
            // so there is no need to add that complexity

            if (identifier == null)
            {
                identifier = BlobIdentifier.FromBlob(blob);
            }

            ConcurrentDictionary<BlobIdentifier, BlobContainer> namespaceContainer = _blobs.GetOrAdd(ns, new ConcurrentDictionary<BlobIdentifier, BlobContainer>());
            if (throwOnOverwrite && namespaceContainer.ContainsKey(identifier))
            {
                throw new Exception($"Blob {identifier} already exists in {ns}");
            }
            namespaceContainer.TryAdd(identifier, new BlobContainer(identifier, blob.ToArray()));

            return Task.FromResult(identifier);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> blob, BlobIdentifier? identifier = null)
        {
            return await PutObject(ns, blob: blob.ToArray(), identifier);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, Stream blob, BlobIdentifier? identifier = null)
        {
            return await PutObject(ns, blob: await blob.ToByteArray(), identifier);
        }

        public Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking)
        {
            if (!_blobs.TryGetValue(ns, value: out ConcurrentDictionary<BlobIdentifier, BlobContainer>? namespaceContainer))
            {
                throw new NamespaceNotFoundException(ns);
            }

            if (!namespaceContainer.TryGetValue(blob, value: out BlobContainer? blobContainer))
            {
                throw new BlobNotFoundException(ns, blob);
            }

            byte[] content = blobContainer.Contents;
            return Task.FromResult(new BlobContents(new MemoryStream(content), content.LongLength));
        }

        public Task DeleteObject(NamespaceId ns, BlobIdentifier blob)
        {
            if (!_blobs.TryGetValue(ns, value: out ConcurrentDictionary<BlobIdentifier, BlobContainer>? namespaceContainer))
            {
                throw new NamespaceNotFoundException(ns);
            }

            if (!namespaceContainer.TryRemove(blob, out _))
            {
                throw new BlobNotFoundException(ns, blob);
            }

            return Task.CompletedTask;
        }

        public Task<bool> Exists(NamespaceId ns, BlobIdentifier blob, bool forceCheck = false)
        {
            if (!_blobs.TryGetValue(ns, value: out ConcurrentDictionary<BlobIdentifier, BlobContainer>? namespaceContainer))
            {
                throw new NamespaceNotFoundException(ns);
            }

            return Task.FromResult(namespaceContainer.ContainsKey(blob));
        }

        public Task DeleteNamespace(NamespaceId ns)
        {
            _blobs.Remove(ns, out ConcurrentDictionary<BlobIdentifier, BlobContainer>? nsBlobs);
            if (nsBlobs == null)
            {
                throw new NamespaceNotFoundException(ns);
            }
            
            return Task.CompletedTask;
        }

        public async IAsyncEnumerable<(BlobIdentifier,DateTime)> ListObjects(NamespaceId ns)
        {
            if (!_blobs.TryGetValue(ns, value: out ConcurrentDictionary<BlobIdentifier, BlobContainer>? namespaceContainer))
            {
                throw new NamespaceNotFoundException(ns);
            }

            await Task.CompletedTask;
            foreach (BlobContainer blobContainer in namespaceContainer.Values)
            {
                yield return (blobContainer.BlobIdentifier, blobContainer.LastModified);
            }
        }

        internal IEnumerable<BlobIdentifier> GetIdentifiers(NamespaceId ns)
        {
            if (!_blobs.TryGetValue(ns, value: out ConcurrentDictionary<BlobIdentifier, BlobContainer>? namespaceContainer))
            {
                throw new NamespaceNotFoundException(ns);
            }

            return namespaceContainer.Values.Select(container => container.BlobIdentifier);
        }

        // only for unit tests to update the last modified time
        internal void SetLastModifiedTime(NamespaceId ns, BlobIdentifier blob, DateTime modifiedTime)
        {
            if (!_blobs.TryGetValue(ns, value: out ConcurrentDictionary<BlobIdentifier, BlobContainer>? namespaceContainer))
            {
                throw new Exception($"Namespace {ns} not found");
            }

            if (namespaceContainer.TryGetValue(blob, value: out BlobContainer? blobContainer))
            {
                blobContainer.LastModified = modifiedTime;
            }
        }
    }
}
