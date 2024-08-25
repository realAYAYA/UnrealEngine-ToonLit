// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Http;
using System.Net.Http.Json;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Controllers;
using Microsoft.Extensions.Options;

namespace Jupiter.Implementation
{
	public class CacheContentIdStore : RelayStore, IContentIdStore
	{
		
		public CacheContentIdStore(IOptionsMonitor<UpstreamRelaySettings> settings, IHttpClientFactory httpClientFactory, IServiceCredentials serviceCredentials) : base(settings, httpClientFactory, serviceCredentials)
		{
		}

		public async Task<BlobId[]?> ResolveAsync(NamespaceId ns, ContentId contentId, bool mustBeContentId)
		{
			using HttpRequestMessage getContentIdRequest = await BuildHttpRequestAsync(HttpMethod.Get, new Uri($"api/v1/content-id/{ns}/{contentId}", UriKind.Relative));
			HttpResponseMessage response = await HttpClient.SendAsync(getContentIdRequest);

			if (response.StatusCode == HttpStatusCode.NotFound)
			{
				return null;
			}

			response.EnsureSuccessStatusCode();
			ResolvedContentIdResponse? resolvedContentId = await response.Content.ReadFromJsonAsync<ResolvedContentIdResponse>();

			if (resolvedContentId == null)
			{
				throw new Exception("Unable to deserialize resolved content id response");
			}
			return resolvedContentId.Blobs;
		}

		public async Task PutAsync(NamespaceId ns, ContentId contentId, BlobId blobIdentifier, int contentWeight)
		{
			using HttpRequestMessage putContentIdRequest = await BuildHttpRequestAsync(HttpMethod.Put, new Uri($"api/v1/content-id/{ns}/{contentId}/update/{blobIdentifier}/{contentWeight}", UriKind.Relative));
			HttpResponseMessage response = await HttpClient.SendAsync(putContentIdRequest);

			response.EnsureSuccessStatusCode();
		}
	}
}
