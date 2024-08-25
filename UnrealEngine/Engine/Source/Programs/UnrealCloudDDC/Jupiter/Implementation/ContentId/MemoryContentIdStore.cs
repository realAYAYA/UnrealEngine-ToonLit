// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;

namespace Jupiter.Implementation
{
	public class MemoryContentIdStore : IContentIdStore
	{
		private readonly IBlobService _blobStore;
		private readonly ConcurrentDictionary<NamespaceId, ConcurrentDictionary<ContentId, SortedList<int, BlobId[]>>> _contentIds = new ConcurrentDictionary<NamespaceId, ConcurrentDictionary<ContentId, SortedList<int, BlobId[]>>>();
		
		public MemoryContentIdStore(IBlobService blobStore)
		{
			_blobStore = blobStore;
		}

		public async Task<BlobId[]?> ResolveAsync(NamespaceId ns, ContentId contentId, bool mustBeContentId)
		{
			if (_contentIds.TryGetValue(ns, out ConcurrentDictionary<ContentId, SortedList<int, BlobId[]>>? contentIdsForNamespace))
			{
				if (contentIdsForNamespace.TryGetValue(contentId, out SortedList<int, BlobId[]>? contentIdMappings))
				{
					foreach ((int _, BlobId[] blobs) in contentIdMappings)
					{
						BlobId[] missingBlobs = await _blobStore.FilterOutKnownBlobsAsync(ns, blobs);
						if (missingBlobs.Length == 0)
						{
							return blobs;
						}
						// blobs are missing continue testing with the next content id in the weighted list as that might exist
					}
				}
			}

			BlobId uncompressedBlobIdentifier = contentId.AsBlobIdentifier();
			// if no content id is found, but we have a blob that matches the content id (so a unchunked and uncompressed version of the data) we use that instead
			if (!mustBeContentId && await _blobStore.ExistsAsync(ns, uncompressedBlobIdentifier))
			{
				return new[] { uncompressedBlobIdentifier };
			}

			return null;
		}

		public Task PutAsync(NamespaceId ns, ContentId contentId, BlobId blobIdentifier, int contentWeight)
		{
			_contentIds.AddOrUpdate(ns, (_) =>
			{
				ConcurrentDictionary<ContentId, SortedList<int, BlobId[]>> dict = new()
				{
					[contentId] = new SortedList<int, BlobId[]>
					{
						{contentWeight, new BlobId[] { blobIdentifier } }
					}
				};

				return dict;
			}, (_, dict) =>
			{
				dict.AddOrUpdate(contentId, (_) =>
				{
					return new SortedList<int, BlobId[]>
					{
						{ contentWeight, new BlobId[] { blobIdentifier } }
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
