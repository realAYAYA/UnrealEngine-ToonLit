// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Mime;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Options;

namespace Jupiter.Implementation
{
    public class RelayBlobStore : RelayStore, IBlobStore
    {
        public RelayBlobStore(IOptionsMonitor<UpstreamRelaySettings> settings, IHttpClientFactory httpClientFactory, IServiceCredentials serviceCredentials) : base(settings, httpClientFactory, serviceCredentials)
        {
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] blob, BlobIdentifier identifier)
        {
            using HttpRequestMessage putObjectRequest = await BuildHttpRequest(HttpMethod.Put, new Uri($"api/v1/blobs/{ns}/{identifier}", UriKind.Relative));

            putObjectRequest.Content = new ByteArrayContent(blob);
            putObjectRequest.Content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            putObjectRequest.Content.Headers.Add(CommonHeaders.HashHeaderName, identifier.ToString());

            HttpResponseMessage response = await HttpClient.SendAsync(putObjectRequest);

            response.EnsureSuccessStatusCode();

            return identifier;
        }

        public Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> blob, BlobIdentifier identifier)
        {
            return PutObject(ns, blob.ToArray(), identifier);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, Stream content, BlobIdentifier identifier)
        {
            using HttpRequestMessage putObjectRequest = await BuildHttpRequest(HttpMethod.Put, new Uri($"api/v1/blobs/{ns}/{identifier}", UriKind.Relative));
            putObjectRequest.Content = new StreamContent(content);
            putObjectRequest.Content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
            putObjectRequest.Content.Headers.Add(CommonHeaders.HashHeaderName, identifier.ToString());
            HttpResponseMessage response = await HttpClient.SendAsync(putObjectRequest);

            response.EnsureSuccessStatusCode();

            return identifier;
        }

        public async Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, LastAccessTrackingFlags flags)
        {
            using HttpRequestMessage getObjectRequest = await BuildHttpRequest(HttpMethod.Get, new Uri($"api/v1/blobs/{ns}/{blob}", UriKind.Relative));
            getObjectRequest.Headers.Add("Accept", MediaTypeNames.Application.Octet);
            HttpResponseMessage response = await HttpClient.SendAsync(getObjectRequest);
            if (response.StatusCode == HttpStatusCode.NotFound)
            {
                throw new BlobNotFoundException(ns, blob);
            }

            response.EnsureSuccessStatusCode();

            long? contentLength = response.Content.Headers.ContentLength;
            if (contentLength == null)
            {
                throw new Exception($"Content length missing in response from upstream blob store. This is not supported");
            }

            return new BlobContents(await response.Content.ReadAsStreamAsync(), contentLength.Value);
        }

        public async Task<bool> Exists(NamespaceId ns, BlobIdentifier blob, bool forceCheck)
        {
            using HttpRequestMessage headObjectRequest = await BuildHttpRequest(HttpMethod.Head, new Uri($"api/v1/blobs/{ns}/{blob}", UriKind.Relative));
            HttpResponseMessage response = await HttpClient.SendAsync(headObjectRequest);
            if (response.StatusCode == HttpStatusCode.NotFound)
            {
                return false;
            }

            response.EnsureSuccessStatusCode();

            return true;
        }

        public Task DeleteObject(NamespaceId ns, BlobIdentifier blob)
        {
            throw new NotImplementedException("DeleteObjects is not supported on the relay blob store");
        }

        public Task DeleteNamespace(NamespaceId ns)
        {
            throw new NotImplementedException("DeleteNamespace is not supported on the relay blob store");
        }

        public IAsyncEnumerable<(BlobIdentifier, DateTime)> ListObjects(NamespaceId ns)
        {
            throw new NotImplementedException("ListObjects is not supported on the relay blob store");
        }
    }
}
