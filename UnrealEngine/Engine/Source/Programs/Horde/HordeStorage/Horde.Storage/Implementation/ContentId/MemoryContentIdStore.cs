// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using ContentId = Jupiter.Implementation.ContentId;

namespace Horde.Storage.Implementation
{
    public class MemoryContentIdStore : IContentIdStore
    {
        private readonly IBlobService _blobStore;
        private readonly ConcurrentDictionary<NamespaceId, ConcurrentDictionary<ContentId, SortedList<int, BlobIdentifier[]>>> _contentIds = new ConcurrentDictionary<NamespaceId, ConcurrentDictionary<ContentId, SortedList<int, BlobIdentifier[]>>>();
        
        public MemoryContentIdStore(IBlobService blobStore)
        {
            _blobStore = blobStore;
        }

        public async Task<BlobIdentifier[]?> Resolve(NamespaceId ns, ContentId contentId, bool mustBeContentId)
        {
            if (_contentIds.TryGetValue(ns, out ConcurrentDictionary<ContentId, SortedList<int, BlobIdentifier[]>>? contentIdsForNamespace))
            {
                if (contentIdsForNamespace.TryGetValue(contentId, out SortedList<int, BlobIdentifier[]>? contentIdMappings))
                {
                    foreach ((int _, BlobIdentifier[] blobs) in contentIdMappings)
                    {
                        BlobIdentifier[] missingBlobs = await _blobStore.FilterOutKnownBlobs(ns, blobs);
                        if (missingBlobs.Length == 0)
                        {
                            return blobs;
                        }
                        // blobs are missing continue testing with the next content id in the weighted list as that might exist
                    }
                }
            }

            BlobIdentifier uncompressedBlobIdentifier = contentId.AsBlobIdentifier();
            // if no content id is found, but we have a blob that matches the content id (so a unchunked and uncompressed version of the data) we use that instead
            if (!mustBeContentId && await _blobStore.Exists(ns, uncompressedBlobIdentifier))
            {
                return new[] { uncompressedBlobIdentifier };
            }

            return null;
        }

        public Task Put(NamespaceId ns, ContentId contentId, BlobIdentifier blobIdentifier, int contentWeight)
        {
            _contentIds.AddOrUpdate(ns, (_) =>
            {
                ConcurrentDictionary<ContentId, SortedList<int, BlobIdentifier[]>> dict = new()
                {
                    [contentId] = new SortedList<int, BlobIdentifier[]>
                    {
                        {contentWeight, new BlobIdentifier[] { blobIdentifier } }
                    }
                };

                return dict;
            }, (_, dict) =>
            {
                dict.AddOrUpdate(contentId, (_) =>
                {
                    return new SortedList<int, BlobIdentifier[]>
                    {
                        { contentWeight, new BlobIdentifier[] { blobIdentifier } }
                    };
                }, (_, mappings) =>
                {
                    mappings[contentWeight] = new[] { blobIdentifier };
                    return mappings;
                });

                return dict;
            });

            return Task.CompletedTask;
        }
    }
}
