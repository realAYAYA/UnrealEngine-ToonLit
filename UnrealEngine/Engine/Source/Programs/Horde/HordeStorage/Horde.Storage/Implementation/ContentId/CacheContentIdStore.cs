// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Http;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Storage.Controllers;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using ContentId = Jupiter.Implementation.ContentId;

namespace Horde.Storage.Implementation
{
    public class CacheContentIdStore : RelayStore, IContentIdStore
    {
        
        public CacheContentIdStore(IOptionsMonitor<UpstreamRelaySettings> settings, IHttpClientFactory httpClientFactory, IServiceCredentials serviceCredentials) : base(settings, httpClientFactory, serviceCredentials)
        {
        }

        public async Task<BlobIdentifier[]?> Resolve(NamespaceId ns, ContentId contentId, bool mustBeContentId)
        {
            using HttpRequestMessage getContentIdRequest = BuildHttpRequest(HttpMethod.Get, new Uri($"api/v1/content-id/{ns}/{contentId}", UriKind.Relative));
            HttpResponseMessage response = await HttpClient.SendAsync(getContentIdRequest);

            if (response.StatusCode == HttpStatusCode.NotFound)
            {
                return null;
            }

            response.EnsureSuccessStatusCode();
            ResolvedContentIdResponse resolvedContentId = await response.Content.ReadAsAsync<ResolvedContentIdResponse>();

            return resolvedContentId.Blobs;
        }

        public async Task Put(NamespaceId ns, ContentId contentId, BlobIdentifier blobIdentifier, int contentWeight)
        {
            using HttpRequestMessage putContentIdRequest = BuildHttpRequest(HttpMethod.Put, new Uri($"api/v1/content-id/{ns}/{contentId}/update/{blobIdentifier}/{contentWeight}", UriKind.Relative));
            HttpResponseMessage response = await HttpClient.SendAsync(putContentIdRequest);

            response.EnsureSuccessStatusCode();
        }
    }
}
