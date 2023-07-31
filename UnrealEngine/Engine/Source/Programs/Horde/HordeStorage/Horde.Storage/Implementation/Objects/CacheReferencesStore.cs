// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Mime;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Storage.Controllers;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Jupiter;
using ContentId = Jupiter.Implementation.ContentId;

namespace Horde.Storage.Implementation
{
    public class CacheReferencesStore : IReferencesStore
    {
        private readonly MongoReferencesStore _mongoReferenceStore;
        private readonly UpstreamReferenceStore _upstreamReferenceStore;

        public CacheReferencesStore(IServiceProvider provider)
        {
            _mongoReferenceStore = ActivatorUtilities.CreateInstance<MongoReferencesStore>(provider, "HordeStorage_Cache");
            _mongoReferenceStore.SetTTLDuration(TimeSpan.FromDays(7.0));

            _upstreamReferenceStore = ActivatorUtilities.CreateInstance<UpstreamReferenceStore>(provider);
        }

        public async Task<ObjectRecord> Get(NamespaceId ns, BucketId bucket, IoHashKey key, IReferencesStore.FieldFlags flags)
        {
            try
            {
                ObjectRecord cachedRecord = await _mongoReferenceStore.Get(ns, bucket, key, flags);
                return cachedRecord;
            }
            catch (ObjectNotFoundException)
            {
                // not cached, we check the upstream for it
            }
 
            ObjectRecord record = await _upstreamReferenceStore.Get(ns, bucket, key, flags);
            await _mongoReferenceStore.Put(record.Namespace, record.Bucket, record.Name, record.BlobIdentifier, record.InlinePayload, record.IsFinalized);
            return record;
        }

        public async Task Put(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash, byte[] blob, bool isFinalized)
        {
            Task cachePut = _mongoReferenceStore.Put(ns, bucket, key, blobHash, blob, isFinalized);
            Task upstreamPut = _upstreamReferenceStore.Put(ns, bucket, key, blobHash, blob, isFinalized);

            await Task.WhenAll(cachePut, upstreamPut);
        }

        public async Task Finalize(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobIdentifier)
        {
            Task cacheFinalize = _mongoReferenceStore.Finalize(ns, bucket, key, blobIdentifier);
            Task upstreamFinalize = _upstreamReferenceStore.Finalize(ns, bucket, key, blobIdentifier);

            await Task.WhenAll(cacheFinalize, upstreamFinalize);
        }

        public async Task UpdateLastAccessTime(NamespaceId ns, BucketId bucket, IoHashKey key, DateTime newLastAccessTime)
        {
            Task cacheUpdateLastAccess = _mongoReferenceStore.UpdateLastAccessTime(ns, bucket, key, newLastAccessTime);
            Task upstreamUpdateLastAccess = _upstreamReferenceStore.UpdateLastAccessTime(ns, bucket, key, newLastAccessTime);

            await Task.WhenAll(cacheUpdateLastAccess, upstreamUpdateLastAccess);
        }

        public IAsyncEnumerable<(BucketId, IoHashKey, DateTime)> GetRecords(NamespaceId ns)
        {
            throw new NotImplementedException("GetRecords not supported on a cached reference store");
        }

        public IAsyncEnumerable<NamespaceId> GetNamespaces()
        {
            throw new NotImplementedException("GetNamespaces not supported on a cached reference store");
        }

        public Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHashKey key)
        {
            throw new NotImplementedException("Deletes are not supported on a cached reference store");
        }

        public Task<long> DropNamespace(NamespaceId ns)
        {
            throw new NotImplementedException("DropNamespace is not supported on a cached reference store");

        }

        public Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
        {
            throw new NotImplementedException("DeleteBucket is not supported on a cached reference store");
        }
    }

    class UpstreamReferenceStore : RelayStore, IReferencesStore
    {
        public UpstreamReferenceStore(IOptionsMonitor<UpstreamRelaySettings> settings, IHttpClientFactory httpClientFactory, IServiceCredentials serviceCredentials) : base(settings, httpClientFactory, serviceCredentials)
        {
        }

        public async Task<ObjectRecord> Get(NamespaceId ns, BucketId bucket, IoHashKey key, IReferencesStore.FieldFlags flags)
        {
            using HttpRequestMessage getObjectRequest = BuildHttpRequest(HttpMethod.Get, new Uri($"api/v1/refs/{ns}/{bucket}/{key}/metadata", UriKind.Relative));
            getObjectRequest.Headers.Add("Accept", MediaTypeNames.Application.Json);
            HttpResponseMessage response = await HttpClient.SendAsync(getObjectRequest);
            if (response.StatusCode == HttpStatusCode.NotFound)
            {
                throw new ObjectNotFoundException(ns, bucket, key);
            }

            response.EnsureSuccessStatusCode();

            RefMetadataResponse metadataResponse = await response.Content.ReadAsAsync<RefMetadataResponse>();

            return new ObjectRecord(metadataResponse.Ns, metadataResponse.Bucket, metadataResponse.Name, metadataResponse.LastAccess, metadataResponse.InlinePayload, metadataResponse.PayloadIdentifier, metadataResponse.IsFinalized);
        }

        public async Task Put(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash, byte[] blob, bool isFinalized)
        {
            using HttpRequestMessage putObjectRequest = BuildHttpRequest(HttpMethod.Put, new Uri($"api/v1/refs/{ns}/{bucket}/{key}", UriKind.Relative));
            putObjectRequest.Headers.Add("Accept", MediaTypeNames.Application.Json);
            putObjectRequest.Headers.Add(CommonHeaders.HashHeaderName, blobHash.ToString());
            putObjectRequest.Content = new ByteArrayContent(blob);
            // a blob sent to the reference store is always by definition a compact binary, so we upload it as such to avoid any automatic conversions that we would get with octet-stream
            putObjectRequest.Content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

            HttpResponseMessage response = await HttpClient.SendAsync(putObjectRequest);
            if (response.StatusCode == HttpStatusCode.BadRequest)
            {
                ProblemDetails? problem = await response.Content.ReadAsAsync<ProblemDetails>();
                throw new Exception("Upstream returned 400 (BadRequest) with error: " + problem.Title);
            }
            response.EnsureSuccessStatusCode();

            // if this put returns needs, we cant really do anything about it. we should be calling put in the upstream blob store as the operation continues and we should be able to catch the missing blobs during the finalize call
        }

        public async Task Finalize(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobIdentifier)
        {
            using HttpRequestMessage putObjectRequest = BuildHttpRequest(HttpMethod.Post, new Uri($"api/v1/refs/{ns}/{bucket}/{key}/finalize/{blobIdentifier}", UriKind.Relative));
            putObjectRequest.Headers.Add("Accept", MediaTypeNames.Application.Json);

            HttpResponseMessage response = await HttpClient.SendAsync(putObjectRequest);

            if (response.StatusCode == HttpStatusCode.BadRequest)
            {
                ProblemDetails? problem = await response.Content.ReadAsAsync<ProblemDetails>();
                throw new Exception("Upstream returned 400 (BadRequest) with error: " + problem.Title);
            }
            response.EnsureSuccessStatusCode();

            PutObjectResponse putObjectResponse = await response.Content.ReadAsAsync<PutObjectResponse>();
            if (putObjectResponse.Needs.Length != 0)
            {
                // TODO: we assume here that all the objects that are missing are content ids, that may not be true, but as needs only contains a list with mixed hashes we can not detect the difference
                throw new PartialReferenceResolveException(putObjectResponse.Needs.Select(hash => new ContentId(hash.HashData)).ToList());
            }
        }

        public async Task UpdateLastAccessTime(NamespaceId ns, BucketId bucket, IoHashKey key, DateTime newLastAccessTime)
        {
            // there is no endpoint to update last access time externally, but fetching a object will in turn update its last access time, though to a different time then newLastAccessTime
            await Get(ns, bucket, key, IReferencesStore.FieldFlags.None);
        }

        public IAsyncEnumerable<(BucketId, IoHashKey, DateTime)> GetRecords(NamespaceId ns)
        {
            throw new NotImplementedException("GetRecords is not supported on a upstream reference store");
        }

        public IAsyncEnumerable<NamespaceId> GetNamespaces()
        {
            throw new NotImplementedException("GetNamespaces is not supported on a upstream reference store");
        }

        public Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHashKey key)
        {
            throw new NotImplementedException("Delete is not supported on a upstream reference store");
        }

        public Task<long> DropNamespace(NamespaceId ns)
        {
            throw new NotImplementedException("DropNamespace is not supported on a upstream reference store");
        }

        public Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
        {
            throw new NotImplementedException("DeleteBucket is not supported on a upstream reference store");
        }
    }
}
