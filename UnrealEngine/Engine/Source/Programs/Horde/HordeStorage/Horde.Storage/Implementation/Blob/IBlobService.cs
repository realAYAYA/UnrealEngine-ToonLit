// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Mime;
using System.Threading.Tasks;
using Dasync.Collections;
using Datadog.Trace;
using EpicGames.AspNet;
using EpicGames.Horde.Storage;
using Horde.Storage.Controllers;
using Horde.Storage.Implementation.Blob;
using Jupiter.Common;
using Jupiter.Common.Implementation;
using Jupiter.Implementation;
using Jupiter.Utils;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Serilog;
using ContentId = Jupiter.Implementation.ContentId;
using CustomMediaTypeNames = Jupiter.CustomMediaTypeNames;

namespace Horde.Storage.Implementation;

public interface IBlobService
{
    Task<ContentHash> VerifyContentMatchesHash(Stream content, ContentHash identifier);
    Task<BlobIdentifier> PutObjectKnownHash(NamespaceId ns, IBufferedPayload content, BlobIdentifier identifier);
    Task<BlobIdentifier> PutObject(NamespaceId ns, IBufferedPayload payload, BlobIdentifier identifier);
    Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] payload, BlobIdentifier identifier);

    Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob);

    Task<BlobContents> ReplicateObject(NamespaceId ns, BlobIdentifier blob, bool force = false);

    Task<bool> Exists(NamespaceId ns, BlobIdentifier blob);

    /// <summary>
    /// Checks that the blob exists in the root store, the store which is last in the list and thus is intended to have every blob in it
    /// </summary>
    /// <param name="ns">The namespace</param>
    /// <param name="blob">The identifier of the blob</param>
    /// <returns></returns>
    Task<bool> ExistsInRootStore(NamespaceId ns, BlobIdentifier blob);

    // Delete a object
    Task DeleteObject(NamespaceId ns, BlobIdentifier blob);

    // delete the whole namespace
    Task DeleteNamespace(NamespaceId ns);

    IAsyncEnumerable<(BlobIdentifier,DateTime)> ListObjects(NamespaceId ns);
    Task<BlobIdentifier[]> FilterOutKnownBlobs(NamespaceId ns, BlobIdentifier[] blobs);
    Task<BlobIdentifier[]> FilterOutKnownBlobs(NamespaceId ns, IAsyncEnumerable<BlobIdentifier> blobs);
    Task<BlobContents> GetObjects(NamespaceId ns, BlobIdentifier[] refRequestBlobReferences);

    bool ShouldFetchBlobOnDemand(NamespaceId ns);
}

public class BlobService : IBlobService
{
    private List<IBlobStore> _blobStores;
    private readonly IOptionsMonitor<HordeStorageSettings> _settings;
    private readonly IBlobIndex _blobIndex;
    private readonly IPeerStatusService _peerStatusService;
    private readonly IHttpClientFactory _httpClientFactory;
    private readonly IServiceCredentials _serviceCredentials;
    private readonly INamespacePolicyResolver _namespacePolicyResolver;
    private readonly IHttpContextAccessor _httpContextAccessor;
    private readonly ILogger _logger = Log.ForContext<BlobService>();

    internal IEnumerable<IBlobStore> BlobStore
    {
        get => _blobStores;
        set => _blobStores = value.ToList();
    }

    public BlobService(IServiceProvider provider, IOptionsMonitor<HordeStorageSettings> settings, IBlobIndex blobIndex, IPeerStatusService peerStatusService, IHttpClientFactory httpClientFactory, IServiceCredentials serviceCredentials, INamespacePolicyResolver namespacePolicyResolver, IHttpContextAccessor httpContextAccessor)
    {
        _blobStores = GetBlobStores(provider, settings).ToList();
        _settings = settings;
        _blobIndex = blobIndex;
        _peerStatusService = peerStatusService;
        _httpClientFactory = httpClientFactory;
        _serviceCredentials = serviceCredentials;
        _namespacePolicyResolver = namespacePolicyResolver;
        _httpContextAccessor = httpContextAccessor;
    }

    public static IEnumerable<IBlobStore> GetBlobStores(IServiceProvider provider, IOptionsMonitor<HordeStorageSettings> settings)
    {
        return settings.CurrentValue.GetStorageImplementations().Select(impl => ToStorageImplementation(provider, impl));
    }

    private static IBlobStore ToStorageImplementation(IServiceProvider provider, HordeStorageSettings.StorageBackendImplementations impl)
    {
        IBlobStore? store = impl switch
        {
            HordeStorageSettings.StorageBackendImplementations.S3 => provider.GetService<AmazonS3Store>(),
            HordeStorageSettings.StorageBackendImplementations.Azure => provider.GetService<AzureBlobStore>(),
            HordeStorageSettings.StorageBackendImplementations.FileSystem => provider.GetService<FileSystemStore>(),
            HordeStorageSettings.StorageBackendImplementations.Memory => provider.GetService<MemoryCacheBlobStore>(),
            HordeStorageSettings.StorageBackendImplementations.MemoryBlobStore => provider.GetService<MemoryBlobStore>(),
            HordeStorageSettings.StorageBackendImplementations.Relay => provider.GetService<RelayBlobStore>(),
            _ => throw new NotImplementedException("Unknown blob store {store")
        };
        if (store == null)
        {
            throw new ArgumentException($"Failed to find a provider service for type: {impl}");
        }

        return store;
    }

    public async Task<ContentHash> VerifyContentMatchesHash(Stream content, ContentHash identifier)
    {
        ContentHash blobHash;
        {
            using IScope _ = Tracer.Instance.StartActive("web.hash");
            blobHash = await BlobIdentifier.FromStream(content);
        }

        if (!identifier.Equals(blobHash))
        {
            throw new HashMismatchException(identifier, blobHash);
        }

        return identifier;
    }

    public async Task<BlobIdentifier> PutObject(NamespaceId ns, IBufferedPayload payload, BlobIdentifier identifier)
    {
        using IScope scope = Tracer.Instance.StartActive("put_blob");
        scope.Span.ResourceName = identifier.ToString();
        scope.Span.SetTag("Content-Length", payload.Length.ToString());

        await using Stream hashStream = payload.GetStream();
        BlobIdentifier id = BlobIdentifier.FromContentHash(await VerifyContentMatchesHash(hashStream, identifier));

        BlobIdentifier objectStoreIdentifier = await PutObjectToStores(ns, payload, id);
        await _blobIndex.AddBlobToIndex(ns, id);

        return objectStoreIdentifier;

    }

    public async Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] payload, BlobIdentifier identifier)
    {
        using IScope scope = Tracer.Instance.StartActive("put_blob");
        scope.Span.ResourceName = identifier.ToString();
        scope.Span.SetTag("Content-Length", payload.Length.ToString());

        await using Stream hashStream = new MemoryStream(payload);
        BlobIdentifier id = BlobIdentifier.FromContentHash(await VerifyContentMatchesHash(hashStream, identifier));

        BlobIdentifier objectStoreIdentifier = await PutObjectToStores(ns, payload, id);
        await _blobIndex.AddBlobToIndex(ns, id);

        return objectStoreIdentifier;
    }

    public async Task<BlobIdentifier> PutObjectKnownHash(NamespaceId ns, IBufferedPayload content, BlobIdentifier identifier)
    {
        using IScope scope = Tracer.Instance.StartActive("put_blob");
        scope.Span.ResourceName = identifier.ToString();
        scope.Span.SetTag("Content-Length", content.Length.ToString());

        BlobIdentifier objectStoreIdentifier = await PutObjectToStores(ns, content, identifier);
        await _blobIndex.AddBlobToIndex(ns, identifier);

        return objectStoreIdentifier;
    }

    private async Task<BlobIdentifier> PutObjectToStores(NamespaceId ns, IBufferedPayload bufferedPayload, BlobIdentifier identifier)
    {
        IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();

        foreach (IBlobStore store in _blobStores)
        {
            using IScope scope = Tracer.Instance.StartActive("put_blob_to_store");
            scope.Span.ResourceName = identifier.ToString();
            scope.Span.SetTag("store", store.ToString());
            string storeName = store.GetType().Name;

            using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope($"blob.put.{storeName}", $"PUT to store: '{storeName}'");

            await using Stream s = bufferedPayload.GetStream();
            await store.PutObject(ns, s, identifier);
        }

        return identifier;
    }

    private async Task<BlobIdentifier> PutObjectToStores(NamespaceId ns, byte[] payload, BlobIdentifier identifier)
    {
        foreach (IBlobStore store in _blobStores)
        {
            await store.PutObject(ns, payload, identifier);
        }

        return identifier;
    }

    public async Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob)
    {
        bool seenBlobNotFound = false;
        bool seenNamespaceNotFound = false;
        int numStoreMisses = 0;
        BlobContents? blobContents = null;
        IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();

        foreach (IBlobStore store in _blobStores)
        {
            using IScope scope = Tracer.Instance.StartActive("HierarchicalStore.GetObject");
            scope.Span.ResourceName = blob.ToString();
            scope.Span.SetTag("BlobStore", store.GetType().Name);
            scope.Span.SetTag("ObjectFound", false.ToString());
            string storeName = store.GetType().Name;
            using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope($"blob.get.{storeName}", $"Blob GET from: '{storeName}'");

            try
            {
                blobContents = await store.GetObject(ns, blob);
                scope.Span.SetTag("ObjectFound", true.ToString());
                break;
            }
            catch (BlobNotFoundException)
            {
                seenBlobNotFound = true;
                numStoreMisses++;
            }
            catch (NamespaceNotFoundException)
            {
                seenNamespaceNotFound = true;
            }
        }

        if (seenBlobNotFound && blobContents == null)
        {
            throw new BlobNotFoundException(ns, blob);
        }

        if (seenNamespaceNotFound && blobContents == null)
        {
            throw new NamespaceNotFoundException(ns);
        }
        
        if (blobContents == null)
        {
            // Should not happen but exists to safeguard against the null pointer
            throw new Exception("blobContents is null");
        }

        if (numStoreMisses >= 1)
        {
            using IScope _ = Tracer.Instance.StartActive("HierarchicalStore.Populate");
            using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope($"blob.populate", "Populating caches with blob contents");

            await using MemoryStream tempStream = new MemoryStream();
            await blobContents.Stream.CopyToAsync(tempStream);
            byte[] data = tempStream.ToArray();
            
            // Don't populate the last store, as that is where we got the hit
            for (int i = 0; i < numStoreMisses; i++)
            {
                IBlobStore blobStore = _blobStores[i];
                using IScope scope = Tracer.Instance.StartActive("HierarchicalStore.PopulateStore");
                scope.Span.SetTag("BlobStore", blobStore.GetType().Name);
                // Populate each store traversed that did not have the content found lower in the hierarchy
                await blobStore.PutObject(ns, data, blob);
            }

#pragma warning disable CA2000 // Dispose objects before losing scope , ownership is transfered to caller
            blobContents = new BlobContents(data);
#pragma warning restore CA2000 // Dispose objects before losing scope
        }
        
        return blobContents;
    }

    public async Task<BlobContents> ReplicateObject(NamespaceId ns, BlobIdentifier blob, bool force = false)
    {
        if (!force && !ShouldFetchBlobOnDemand(ns))
        {
            throw new NotSupportedException($"Replication is not allowed in namespace {ns}");
        }
        IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();
        using IScope scope = Tracer.Instance.StartActive("HierarchicalStore.Replicate");

        using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope("blob.replicate", "Replicating blob from remote instances");

        BlobInfo? blobInfo = await _blobIndex.GetBlobInfo(ns, blob);
        if (blobInfo == null)
        {
            throw new BlobNotFoundException(ns, blob);
        }

        if (!blobInfo.Regions.Any())
        {
            throw new BlobReplicationException(ns, blob, "Blob not found in any region");
        }

        _logger.Information("On-demand replicating blob {Blob} in Namespace {Namespace}", blob, ns);
        List<(int, string)> possiblePeers = new List<(int, string)>(_peerStatusService.GetPeersByLatency(blobInfo.Regions.ToList()));

        bool replicated = false;
        foreach ((int latency, string? region) in possiblePeers)
        {
            PeerStatus? peerStatus = _peerStatusService.GetPeerStatus(region);
            if (peerStatus == null)
            {
                throw new Exception($"Failed to find peer {region}");
            }

            _logger.Information("Attempting to replicate blob {Blob} in Namespace {Namespace} from {Region}", blob, ns, region);

            PeerEndpoints peerEndpoint = peerStatus.Endpoints.First();
            using HttpClient httpClient = _httpClientFactory.CreateClient();
            using HttpRequestMessage blobRequest = BuildHttpRequest(HttpMethod.Get, new Uri($"{peerEndpoint.Url}/api/v1/blobs/{ns}/{blob}"));
            HttpResponseMessage blobResponse = await httpClient.SendAsync(blobRequest, HttpCompletionOption.ResponseHeadersRead);

            if (blobResponse.StatusCode == HttpStatusCode.NotFound)
            {
                // try the next region
                continue;
            }

            if (blobResponse.StatusCode != HttpStatusCode.OK)
            {
                throw new BlobReplicationException(ns, blob, $"Failed to replicate {blob} in {ns} from region {region} due to bad http status code {blobResponse.StatusCode}.");
            }

            await using Stream s = await blobResponse.Content.ReadAsStreamAsync();
            using FilesystemBufferedPayload payload = await FilesystemBufferedPayload.Create(s);
            await PutObject(ns, payload, blob);
            replicated = true;
            break;
        }

        if (!replicated)
        {
            throw new BlobReplicationException(ns, blob, $"Failed to replicate {blob} in {ns} due to it not existing in any region");
        }

        return await GetObject(ns, blob);
    }

    public bool ShouldFetchBlobOnDemand(NamespaceId ns)
    {
        return _settings.CurrentValue.EnableOnDemandReplication && _namespacePolicyResolver.GetPoliciesForNs(ns).OnDemandReplication;
    }

    private HttpRequestMessage BuildHttpRequest(HttpMethod httpMethod, Uri uri)
    {
        string? token = _serviceCredentials.GetToken();
        HttpRequestMessage request = new HttpRequestMessage(httpMethod, uri);
        if (!string.IsNullOrEmpty(token))
        {
            request.Headers.Add("Authorization", $"{_serviceCredentials.GetAuthenticationScheme()} {token}");
        }

        return request;
    }

    public async Task<bool> Exists(NamespaceId ns, BlobIdentifier blob)
    {
        bool useBlobIndex = _namespacePolicyResolver.GetPoliciesForNs(ns).UseBlobIndexForExists;
        if (useBlobIndex)
        {
            using IScope scope = Tracer.Instance.StartActive("HierarchicalStore.ObjectExists");
            scope.Span.ResourceName = blob.ToString();
            scope.Span.SetTag("BlobStore", "BlobIndex");
            bool exists = await _blobIndex.BlobExistsInRegion(ns, blob);
            if (exists)
            {
                scope.Span.SetTag("ObjectFound", true.ToString());
                return true;
            }

            scope.Span.SetTag("ObjectFound", false.ToString());
            return false;
        }
        else
        {
            foreach (IBlobStore store in _blobStores)
            {
                using IScope scope = Tracer.Instance.StartActive("HierarchicalStore.ObjectExists");
                scope.Span.ResourceName = blob.ToString();
                scope.Span.SetTag("BlobStore", store.GetType().Name);
                if (await store.Exists(ns, blob))
                {
                    scope.Span.SetTag("ObjectFound", true.ToString());
                    return true;
                }
                scope.Span.SetTag("ObjectFound", false.ToString());
            }

            return false;
        }
    }

    public async Task<bool> ExistsInRootStore(NamespaceId ns, BlobIdentifier blob)
    {
        IBlobStore store = _blobStores.Last();

        using IScope scope = Tracer.Instance.StartActive("HierarchicalStore.ObjectExistsInRoot");
        scope.Span.ResourceName = blob.ToString();
        scope.Span.SetTag("BlobStore", store.GetType().Name);
        if (await store.Exists(ns, blob))
        {
            scope.Span.SetTag("ObjectFound", true.ToString());
            return true;
        }
        scope.Span.SetTag("ObjectFound", false.ToString());
        return false;
    }

    public async Task DeleteObject(NamespaceId ns, BlobIdentifier blob)
    {
        bool blobNotFound = false;
        bool deletedAtLeastOnce = false;

        foreach (IBlobStore store in _blobStores)
        {
            try
            {
                using IScope scope = Tracer.Instance.StartActive("HierarchicalStore.DeleteObject");
                scope.Span.ResourceName = blob.ToString();
                scope.Span.SetTag("BlobStore", store.GetType().Name);
                await store.DeleteObject(ns, blob);
                deletedAtLeastOnce = true;
            }
            catch (NamespaceNotFoundException)
            {
                // Ignore
            }
            catch (BlobNotFoundException)
            {
                blobNotFound = true;
            }
        }

        await _blobIndex.RemoveBlobFromRegion(ns, blob);

        if (deletedAtLeastOnce)
        {
            return;
        }

        if (blobNotFound)
        {
            throw new BlobNotFoundException(ns, blob);
        }

        throw new NamespaceNotFoundException(ns);
    }

    public async Task DeleteNamespace(NamespaceId ns)
    {
        bool deletedAtLeastOnce = false;
        foreach (IBlobStore store in _blobStores)
        {
            using IScope scope = Tracer.Instance.StartActive("HierarchicalStore.DeleteNamespace");
            scope.Span.SetTag("BlobStore", store.GetType().Name);
            try
            {
                await store.DeleteNamespace(ns);
                deletedAtLeastOnce = true;
            }
            catch (NamespaceNotFoundException)
            {
                // Ignore
            }
        }

        if (deletedAtLeastOnce)
        {
            return;
        }

        throw new NamespaceNotFoundException(ns);
    }

    public IAsyncEnumerable<(BlobIdentifier,DateTime)> ListObjects(NamespaceId ns)
    {
        // as this is a hierarchy of blob stores the last blob store should contain the superset of all stores
        return _blobStores.Last().ListObjects(ns);
    }

    public async Task<BlobIdentifier[]> FilterOutKnownBlobs(NamespaceId ns, BlobIdentifier[] blobs)
    {
        var tasks = blobs.Select(async blobIdentifier => new { BlobIdentifier = blobIdentifier, Exists = await Exists(ns, blobIdentifier) });
        var blobResults = await Task.WhenAll(tasks);
        IEnumerable<BlobIdentifier> filteredBlobs = blobResults.Where(ac => !ac.Exists).Select(ac => ac.BlobIdentifier);
        return filteredBlobs.ToArray();
    }

    public async Task<BlobIdentifier[]> FilterOutKnownBlobs(NamespaceId ns, IAsyncEnumerable<BlobIdentifier> blobs)
    {
        ConcurrentBag<BlobIdentifier> missingBlobs = new ConcurrentBag<BlobIdentifier>();

        try
        {
            await blobs.ParallelForEachAsync(async identifier =>
            {
                bool exists = await Exists(ns, identifier);

                if (!exists)
                {
                    missingBlobs.Add(identifier);
                }
            });
        }
        catch (ParallelForEachException e)
        {
            if (e.InnerException is PartialReferenceResolveException)
            {
                throw e.InnerException;
            }

            throw;
        }

        return missingBlobs.ToArray();
    }

    public async Task<BlobContents> GetObjects(NamespaceId ns, BlobIdentifier[] blobs)
    {
        using IScope _ = Tracer.Instance.StartActive("blob.combine");
        Task<BlobContents>[] tasks = new Task<BlobContents>[blobs.Length];
        for (int i = 0; i < blobs.Length; i++)
        {
            tasks[i] = GetObject(ns, blobs[i]);
        }

        MemoryStream ms = new MemoryStream();
        foreach (Task<BlobContents> task in tasks)
        {
            BlobContents blob = await task;
            await using Stream s = blob.Stream;
            await s.CopyToAsync(ms);
        }

        ms.Seek(0, SeekOrigin.Begin);

        return new BlobContents(ms, ms.Length);
    }
}

public static class BlobServiceExtensions
{
    public static async Task<ContentId> PutCompressedObject(this IBlobService blobService, NamespaceId ns, IBufferedPayload payload, ContentId? id, IContentIdStore contentIdStore, CompressedBufferUtils compressedBufferUtils)
    {
        // decompress the content and generate a identifier from it to verify the identifier we got
        await using Stream decompressStream = payload.GetStream();
        // TODO: we should add a overload for decompress content that can work on streams, otherwise we are still limited to 2GB compressed blobs
        byte[] decompressedContent = compressedBufferUtils.DecompressContent(await decompressStream.ToByteArray());

        await using MemoryStream decompressedStream = new MemoryStream(decompressedContent);
        ContentId identifierDecompressedPayload;
        if (id != null)
        {
            identifierDecompressedPayload = ContentId.FromContentHash(await blobService.VerifyContentMatchesHash(decompressedStream, id));
        }
        else
        {
            ContentHash blobHash;
            {
                using IScope _ = Tracer.Instance.StartActive("web.hash");
                blobHash = await BlobIdentifier.FromStream(decompressedStream);
            }

            identifierDecompressedPayload = ContentId.FromContentHash(blobHash);
        }

        BlobIdentifier identifierCompressedPayload;
        {
            using IScope _ = Tracer.Instance.StartActive("web.hash");
            await using Stream hashStream = payload.GetStream();
            identifierCompressedPayload = await BlobIdentifier.FromStream(hashStream);
        }

        // commit the mapping from the decompressed hash to the compressed hash, we run this in parallel with the blob store submit
        // TODO: let users specify weight of the blob compared to previously submitted content ids
        int contentIdWeight = (int)payload.Length;
        Task contentIdStoreTask = contentIdStore.Put(ns, identifierDecompressedPayload, identifierCompressedPayload, contentIdWeight);

        // we still commit the compressed buffer to the object store using the hash of the compressed content
        {
            await blobService.PutObjectKnownHash(ns, payload, identifierCompressedPayload);
        }

        await contentIdStoreTask;

        return identifierDecompressedPayload;
    }

    public static async Task<(BlobContents, string)> GetCompressedObject(this IBlobService blobService, NamespaceId ns, ContentId contentId, IContentIdStore contentIdStore)
    {
        BlobIdentifier[]? chunks = await contentIdStore.Resolve(ns, contentId, mustBeContentId: false);
        if (chunks == null || chunks.Length == 0)
        {
            throw new ContentIdResolveException(contentId);
        }

        // single chunk, we just return that chunk
        if (chunks.Length == 1)
        {
            BlobIdentifier blobToReturn = chunks[0];
            string mimeType = CustomMediaTypeNames.UnrealCompressedBuffer;
            if (contentId.Equals(blobToReturn))
            {
                // this was actually the unmapped blob, meaning its not a compressed buffer
                mimeType = MediaTypeNames.Application.Octet;
            }

            return (await blobService.GetObject(ns, blobToReturn), mimeType);
        }

        // chunked content, combine the chunks into a single stream
        using IScope _ = Tracer.Instance.StartActive("blob.combine");
        Task<BlobContents>[] tasks = new Task<BlobContents>[chunks.Length];
        for (int i = 0; i < chunks.Length; i++)
        {
            tasks[i] = blobService.GetObject(ns, chunks[i]);
        }

        MemoryStream ms = new MemoryStream();
        foreach (Task<BlobContents> task in tasks)
        {
            BlobContents blob = await task;
            await using Stream s = blob.Stream;
            await s.CopyToAsync(ms);
        }

        ms.Seek(0, SeekOrigin.Begin);

        // chunking could not have happened for a non compressed buffer so assume it is compressed
        return (new BlobContents(ms, ms.Length), CustomMediaTypeNames.UnrealCompressedBuffer);
    }
}