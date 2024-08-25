// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;

namespace Horde.Server.Ddc
{
	class ContentIdStore : IContentIdStore
	{
		readonly IStorageClientFactory _storageClientFactory;

		public ContentIdStore(IStorageClientFactory storageService)
		{
			_storageClientFactory = storageService;
		}

		static string GetAlias(BlobId blobId) => BlobService.GetAlias(blobId);
		static string GetAlias(ContentId contentId) => $"cid:{contentId}";

		public async Task<BlobId[]?> ResolveAsync(NamespaceId ns, ContentId contentId, bool mustBeContentId = false, CancellationToken cancellationToken = default)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);

			BlobAlias? blobAlias = await storageClient.FindAliasAsync(GetAlias(contentId), cancellationToken);
			if (blobAlias == null && !mustBeContentId)
			{
				blobAlias = await storageClient.FindAliasAsync(GetAlias(contentId.AsBlobIdentifier()), cancellationToken);
			}
			if (blobAlias == null)
			{
				return null;
			}

			IoHash hash = new IoHash(blobAlias.Data.Span);
			return new[] { BlobId.FromIoHash(hash) };
		}

		public async Task PutAsync(NamespaceId ns, ContentId contentId, BlobId blobId, int contentWeight, CancellationToken cancellationToken)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);

			BlobAlias? blobAlias = await storageClient.FindAliasAsync(GetAlias(blobId), cancellationToken);
			if (blobAlias == null)
			{
				throw new BlobNotFoundException(ns, blobId);
			}

			await storageClient.AddAliasAsync(GetAlias(contentId), blobAlias.Target, -contentWeight, blobAlias.Data, cancellationToken: cancellationToken);
		}
	}
}
