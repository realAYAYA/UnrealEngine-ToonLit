// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics.Metrics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Claims;
using System.Threading.Tasks;
using EpicGames.AspNet;
using EpicGames.Horde.Storage;
using Jupiter.Controllers;
using Jupiter.Implementation.Blob;
using Jupiter.Common;
using Jupiter.Common.Implementation;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Microsoft.Extensions.Logging;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation;

public class BlobService : IBlobService
{
	private List<IBlobStore> _blobStores;
	private readonly IOptionsMonitor<UnrealCloudDDCSettings> _settings;
	private readonly IBlobIndex _blobIndex;
	private readonly IPeerStatusService _peerStatusService;
	private readonly IHttpClientFactory _httpClientFactory;
	private readonly IServiceCredentials _serviceCredentials;
	private readonly INamespacePolicyResolver _namespacePolicyResolver;
	private readonly IHttpContextAccessor _httpContextAccessor;
	private readonly IRequestHelper? _requestHelper;
	private readonly Tracer _tracer;
	private readonly BufferedPayloadFactory _bufferedPayloadFactory;
	private readonly ILogger _logger;
	private readonly Counter<long>? _storeGetHitsCounter;
	private readonly Counter<long>? _storeGetAttemptsCounter;
	private readonly string _currentSite;

	internal IEnumerable<IBlobStore> BlobStore
	{
		get => _blobStores;
		set => _blobStores = value.ToList();
	}

	public BlobService(IServiceProvider provider, IOptionsMonitor<UnrealCloudDDCSettings> settings, IOptionsMonitor<JupiterSettings> jupiterSettings, IBlobIndex blobIndex, IPeerStatusService peerStatusService, IHttpClientFactory httpClientFactory, IServiceCredentials serviceCredentials, INamespacePolicyResolver namespacePolicyResolver, IHttpContextAccessor httpContextAccessor, IRequestHelper? requestHelper, Tracer tracer, BufferedPayloadFactory bufferedPayloadFactory, ILogger<BlobService> logger, Meter? meter)
	{
		_blobStores = GetBlobStores(provider, settings).ToList();
		_settings = settings;
		_blobIndex = blobIndex;
		_peerStatusService = peerStatusService;
		_httpClientFactory = httpClientFactory;
		_serviceCredentials = serviceCredentials;
		_namespacePolicyResolver = namespacePolicyResolver;
		_httpContextAccessor = httpContextAccessor;
		_requestHelper = requestHelper;
		_tracer = tracer;
		_bufferedPayloadFactory = bufferedPayloadFactory;
		_logger = logger;

		_currentSite = jupiterSettings.CurrentValue.CurrentSite;

		_storeGetHitsCounter = meter?.CreateCounter<long>("store.blob_get.found");
		_storeGetAttemptsCounter = meter?.CreateCounter<long>("store.blob_get.attempt");
	}

	public static IEnumerable<IBlobStore> GetBlobStores(IServiceProvider provider, IOptionsMonitor<UnrealCloudDDCSettings> settings)
	{
		return settings.CurrentValue.GetStorageImplementations().Select(impl => ToStorageImplementation(provider, impl));
	}

	private static IBlobStore ToStorageImplementation(IServiceProvider provider, UnrealCloudDDCSettings.StorageBackendImplementations impl)
	{
		IBlobStore? store = impl switch
		{
			UnrealCloudDDCSettings.StorageBackendImplementations.S3 => provider.GetService<AmazonS3Store>(),
			UnrealCloudDDCSettings.StorageBackendImplementations.Azure => provider.GetService<AzureBlobStore>(),
			UnrealCloudDDCSettings.StorageBackendImplementations.FileSystem => provider.GetService<FileSystemStore>(),
			UnrealCloudDDCSettings.StorageBackendImplementations.Memory => provider.GetService<MemoryBlobStore>(),
			UnrealCloudDDCSettings.StorageBackendImplementations.Relay => provider.GetService<RelayBlobStore>(),
			UnrealCloudDDCSettings.StorageBackendImplementations.Peer => provider.GetService<PeerBlobStore>(),
			_ => throw new NotImplementedException("Unknown blob store {store")
		};
		if (store == null)
		{
			throw new ArgumentException($"Failed to find a provider service for type: {impl}");
		}

		return store;
	}

	public async Task<ContentHash> VerifyContentMatchesHashAsync(Stream content, ContentHash identifier)
	{
		ContentHash blobHash;
		{
			using TelemetrySpan _ = _tracer.StartActiveSpan("web.hash").SetAttribute("operation.name", "web.hash");
			blobHash = await BlobId.FromStreamAsync(content);
		}

		if (!identifier.Equals(blobHash))
		{
			throw new HashMismatchException(identifier, blobHash);
		}

		return identifier;
	}

	public async Task<BlobId> PutObjectAsync(NamespaceId ns, IBufferedPayload payload, BlobId identifier)
	{
		bool useContentAddressedStorage = _namespacePolicyResolver.GetPoliciesForNs(ns).UseContentAddressedStorage;
		using TelemetrySpan scope = _tracer.StartActiveSpan("put_blob")
			.SetAttribute("operation.name", "put_blob")
			.SetAttribute("resource.name", identifier.ToString())
			.SetAttribute("Content-Length", payload.Length.ToString());

		await using Stream hashStream = payload.GetStream();
		BlobId id = useContentAddressedStorage ? BlobId.FromContentHash(await VerifyContentMatchesHashAsync(hashStream, identifier)) : identifier;

		BlobId objectStoreIdentifier = await PutObjectToStoresAsync(ns, payload, id);
		await _blobIndex.AddBlobToIndexAsync(ns, id);

		return objectStoreIdentifier;

	}

	public async Task<BlobId> PutObjectAsync(NamespaceId ns, byte[] payload, BlobId identifier)
	{
		bool useContentAddressedStorage = _namespacePolicyResolver.GetPoliciesForNs(ns).UseContentAddressedStorage;
		using TelemetrySpan scope = _tracer.StartActiveSpan("put_blob")
			.SetAttribute("operation.name", "put_blob")
			.SetAttribute("resource.name", identifier.ToString())
			.SetAttribute("Content-Length", payload.Length.ToString())
			;

		await using Stream hashStream = new MemoryStream(payload);
		BlobId id = useContentAddressedStorage ? BlobId.FromContentHash(await VerifyContentMatchesHashAsync(hashStream, identifier)) : identifier;

		BlobId objectStoreIdentifier = await PutObjectToStoresAsync(ns, payload, id);
		await _blobIndex.AddBlobToIndexAsync(ns, id);

		return objectStoreIdentifier;
	}

	public async Task<Uri?> MaybePutObjectWithRedirectAsync(NamespaceId ns, BlobId identifier)
	{
		bool allowRedirectUris = _namespacePolicyResolver.GetPoliciesForNs(ns).AllowRedirectUris;
		if (!allowRedirectUris)
		{
			return null;
		}

		IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();

		// we only attempt to upload to the last store
		// assuming that any other store will pull from it when they lack content once the upload has finished
		IBlobStore store = _blobStores.Last();
		{
			string storeName = store.GetType().Name;
			using TelemetrySpan scope = _tracer.StartActiveSpan("put_blob_with_redirect")
					.SetAttribute("operation.name", "put_blob_with_redirect")
					.SetAttribute("resource.name", identifier.ToString())
					.SetAttribute("store", storeName)
			;

			using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope($"blob.put-redirect.{storeName}", $"PUT(redirect) to store: '{storeName}'");

			Uri? redirectUri = await store.PutObjectWithRedirectAsync(ns, identifier);
			if (redirectUri != null)
			{
				return redirectUri;
			}
		}

		// no store found that supports redirect
		return null;
	}

	public async Task<BlobId> PutObjectKnownHashAsync(NamespaceId ns, IBufferedPayload content, BlobId identifier)
	{
		using TelemetrySpan scope = _tracer.StartActiveSpan("put_blob")
			.SetAttribute("operation.name", "put_blob")
			.SetAttribute("resource.name", identifier.ToString())
			.SetAttribute("Content-Length", content.Length.ToString())
			;

		BlobId objectStoreIdentifier = await PutObjectToStoresAsync(ns, content, identifier);
		await _blobIndex.AddBlobToIndexAsync(ns, identifier);

		return objectStoreIdentifier;
	}

	private async Task<BlobId> PutObjectToStoresAsync(NamespaceId ns, IBufferedPayload bufferedPayload, BlobId identifier)
	{
		IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();

		foreach (IBlobStore store in _blobStores)
		{
			using TelemetrySpan scope = _tracer.StartActiveSpan("put_blob_to_store")
				.SetAttribute("operation.name", "put_blob_to_store")
				.SetAttribute("resource.name", identifier.ToString())
				.SetAttribute("store", store.GetType().ToString())
				;
			string storeName = store.GetType().Name;

			using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope($"blob.put.{storeName}", $"PUT to store: '{storeName}'");

			await using Stream s = bufferedPayload.GetStream();
			await store.PutObjectAsync(ns, s, identifier);
		}
		NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);
		if (policy.PopulateFallbackNamespaceOnUpload && policy.FallbackNamespace.HasValue)
		{
			await PutObjectToStoresAsync(policy.FallbackNamespace.Value, bufferedPayload, identifier);
			await _blobIndex.AddBlobToIndexAsync(policy.FallbackNamespace.Value, identifier);
		}
		return identifier;
	}

	private async Task<BlobId> PutObjectToStoresAsync(NamespaceId ns, byte[] payload, BlobId identifier)
	{
		foreach (IBlobStore store in _blobStores)
		{
			await store.PutObjectAsync(ns, payload, identifier);
		}

		NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);
		if (policy.PopulateFallbackNamespaceOnUpload && policy.FallbackNamespace.HasValue)
		{
			await PutObjectToStoresAsync(policy.FallbackNamespace.Value, payload, identifier);
			await _blobIndex.AddBlobToIndexAsync(policy.FallbackNamespace.Value, identifier);
		}
		return identifier;
	}

	public async Task<BlobContents> GetObjectAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null, bool supportsRedirectUri = false, bool allowOndemandReplication = true)
	{
		try
		{
			return await GetObjectFromStoresAsync(ns, blob, storageLayers, supportsRedirectUri);
		}
		catch (BlobNotFoundException)
		{
			NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);

			if (policy.FallbackNamespace != null)
			{
				ClaimsPrincipal? user = _httpContextAccessor.HttpContext?.User;
				HttpRequest? request = _httpContextAccessor.HttpContext?.Request;

				if (user == null || request == null)
				{
					_logger.LogWarning("Unable to fallback to namespace {Namespace} due to not finding required http context values", policy.FallbackNamespace.Value);
					throw;
				}

				if (_requestHelper == null)
				{
					_logger.LogWarning("Unable to fallback to namespace {Namespace} due to request helper missing", policy.FallbackNamespace.Value);
					throw;
				}

				ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(user, request, policy.FallbackNamespace.Value, new [] { JupiterAclAction.ReadObject });
				if (result != null)
				{
					_logger.LogInformation("Authorization error when attempting to fallback to namespace {FallbackNamespace}. This may be confusing for users that as they had access to original namespace {Namespace}", policy.FallbackNamespace.Value, ns);
					throw new AuthorizationException(result, "Failed to authenticate for fallback namespace");
				}

				try
				{
					IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();

					// read the content from the fallback namespace
					BlobContents fallbackContent = await GetObjectFromStoresAsync(policy.FallbackNamespace.Value, blob, storageLayers);
					
					// populate the primary namespace with the content
					using TelemetrySpan _ = _tracer.StartActiveSpan("HierarchicalStore.Populate").SetAttribute("operation.name", "HierarchicalStore.Populate");
					using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope($"blob.populate", "Populating caches with blob contents");

					await using MemoryStream tempStream = new MemoryStream();
					await fallbackContent.Stream.CopyToAsync(tempStream);
					byte[] data = tempStream.ToArray();

					await PutObjectAsync(ns, data, blob);
					return await GetObjectAsync(ns, blob);
				}
				catch (BlobNotFoundException)
				{
					// if we fail to find the blob in the fallback namespace we can carry on as we will rethrow the blob not found exception later (if the replication also fails)
				}
			}

			if (ShouldFetchBlobOnDemand(ns) && allowOndemandReplication)
			{
				try
				{
					return await ReplicateObjectAsync(ns, blob);
				}
				catch (BlobNotFoundException)
				{
					// if the blob is not found we can ignore it as we will just rethrow it later anyway
				}
			}

			// if the primary namespace failed check to see if we should use a fallback policy which has replication enabled
			if (policy.FallbackNamespace != null && ShouldFetchBlobOnDemand(policy.FallbackNamespace.Value) && allowOndemandReplication)
			{
				try
				{
					return await ReplicateObjectAsync(policy.FallbackNamespace.Value, blob);
				}
				catch (BlobNotFoundException)
				{
					// if the blob is not found we can ignore it as we will just rethrow it later anyway
				}
			}

			// we might have attempted to fetch the object and failed, or had no fallback options, either way the blob can not be found so we should rethrow
			throw;
		}
	}

	private async Task<BlobContents> GetObjectFromStoresAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null, bool supportsRedirectUri = false)
	{
		bool seenBlobNotFound = false;
		bool seenNamespaceNotFound = false;
		int numStoreMisses = 0;
		BlobContents? blobContents = null;
		IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();

		foreach (IBlobStore store in _blobStores)
		{
			string blobStoreName = store.GetType().Name;

			// check which storage layers to skip if we have a explicit list of storage layers to use
			if (storageLayers != null && storageLayers.Count != 0)
			{
				bool found = false;
				foreach (string storageLayer in storageLayers)
				{
					if (string.Equals(storageLayer, blobStoreName, StringComparison.OrdinalIgnoreCase))
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					continue;
				}
			}

			using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.GetObject")
				.SetAttribute("operation.name", "HierarchicalStore.GetObject")
				.SetAttribute("resource.name",  blob.ToString())
				.SetAttribute("BlobStore", store.GetType().ToString())
				.SetAttribute("ObjectFound", false.ToString())
				;

			string storeName = store.GetType().Name;
			using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope($"blob.get.{storeName}", $"Blob GET from: '{storeName}'");
			_storeGetAttemptsCounter?.Add(1, new KeyValuePair<string, object?>("store", storeName));
			try
			{
				blobContents = await store.GetObjectAsync(ns, blob, supportsRedirectUri: supportsRedirectUri);
				scope.SetAttribute("ObjectFound", true.ToString());
				_storeGetHitsCounter?.Add(1, new KeyValuePair<string, object?>("store", storeName));
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

		// if we applied filters to the storage layers resulting in no blob found we consider it a miss
		if (storageLayers != null && storageLayers.Count != 0 && blobContents == null)
		{
			throw new BlobNotFoundException(ns, blob);
		}

		if (blobContents == null)
		{
			// Should not happen but exists to safeguard against the null pointer
			throw new Exception("blobContents is null");
		}

		// if we had a miss populate the earlier stores unless we are using a redirect uri, at which point we do not have the contents available to us
		if (numStoreMisses >= 1 && blobContents.RedirectUri == null)
		{
			using TelemetrySpan _ = _tracer.StartActiveSpan("HierarchicalStore.Populate").SetAttribute("operation.name", "HierarchicalStore.Populate");
			using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope($"blob.populate", "Populating caches with blob contents");

			// not using using as the blob contents will take ownership of this buffered payload and dispose it when the contents is disposed
			IBufferedPayload bufferedPayload = await _bufferedPayloadFactory.CreateFromStreamAsync(blobContents.Stream, blobContents.Length);

			// Don't populate the last store, as that is where we got the hit
			for (int i = 0; i < numStoreMisses; i++)
			{
				IBlobStore blobStore = _blobStores[i];
				using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.PopulateStore")
					.SetAttribute("operation.name", "HierarchicalStore.PopulateStore")
					.SetAttribute("BlobStore", blobStore.GetType().Name);
				await using Stream s = bufferedPayload.GetStream();
				// Populate each store traversed that did not have the content found lower in the hierarchy
				await blobStore.PutObjectAsync(ns, s, blob);
			}

#pragma warning disable CA2000 // Dispose objects before losing scope , ownership is transfered to caller
			blobContents = new BlobContents(bufferedPayload);
#pragma warning restore CA2000 // Dispose objects before losing scope
		}
		
		return blobContents;
	}

	public async Task<Uri?> GetObjectWithRedirectAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null)
	{
		bool seenBlobNotFound = false;
		bool seenNamespaceNotFound = false;
		int numStoreMisses = 0;
		IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();

		Uri? redirectUri = null;
		foreach (IBlobStore store in _blobStores)
		{
			string blobStoreName = store.GetType().Name;

			// check which storage layers to skip if we have a explicit list of storage layers to use
			if (storageLayers != null && storageLayers.Count != 0)
			{
				bool found = false;
				foreach (string storageLayer in storageLayers)
				{
					if (string.Equals(storageLayer, blobStoreName, StringComparison.OrdinalIgnoreCase))
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					continue;
				}
			}

			using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.GetObjectRedirect")
				.SetAttribute("operation.name", "HierarchicalStore.GetObject")
				.SetAttribute("resource.name",  blob.ToString())
				.SetAttribute("BlobStore", store.GetType().ToString())
				.SetAttribute("ObjectFound", false.ToString())
				;

			string storeName = store.GetType().Name;
			using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope($"blob.get-redirect.{storeName}", $"Blob GET from: '{storeName}'");

			try
			{
				redirectUri = await store.GetObjectByRedirectAsync(ns, blob);
				scope.SetAttribute("ObjectFound", true.ToString());
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

		if (seenBlobNotFound && redirectUri == null)
		{
			throw new BlobNotFoundException(ns, blob);
		}

		if (seenNamespaceNotFound && redirectUri == null)
		{
			throw new NamespaceNotFoundException(ns);
		}

		// if we applied filters to the storage layers resulting in no blob found we consider it a miss
		if (storageLayers != null && storageLayers.Count != 0 && redirectUri == null)
		{
			throw new BlobNotFoundException(ns, blob);
		}

		return redirectUri;
	}

	public async Task<BlobMetadata> GetObjectMetadataAsync(NamespaceId ns, BlobId blobId)
	{
		bool seenBlobNotFound = false;
		bool seenNamespaceNotFound = false;
		IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();

		foreach (IBlobStore store in _blobStores)
		{
			string storeName = store.GetType().Name;

			using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.GetObjectMetadata")
					.SetAttribute("operation.name", "HierarchicalStore.GetObjectMetadata")
					.SetAttribute("resource.name", blobId.ToString())
					.SetAttribute("BlobStore", storeName)
					.SetAttribute("ObjectFound", false.ToString())
				;

			using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope($"blob.get-metadata.{storeName}", $"Blob GET Metadata from: '{storeName}'");

			try
			{
				BlobMetadata metadata = await store.GetObjectMetadataAsync(ns, blobId);
				scope.SetAttribute("ObjectFound", true.ToString());
				return metadata;
			}
			catch (BlobNotFoundException)
			{
				seenBlobNotFound = true;
			}
			catch (NamespaceNotFoundException)
			{
				seenNamespaceNotFound = true;
			}
		}

		if (seenBlobNotFound)
		{
			throw new BlobNotFoundException(ns, blobId);
		}

		if (seenNamespaceNotFound)
		{
			throw new NamespaceNotFoundException(ns);
		}

		// the only way we get here is we can not find the blob in any store, thus it does not exist (should have triggered the blob not found above)
		throw new BlobNotFoundException(ns, blobId);
	}

	public async Task<BlobContents> ReplicateObjectAsync(NamespaceId ns, BlobId blob, bool force = false)
	{
		if (!force && !ShouldFetchBlobOnDemand(ns))
		{
			throw new NotSupportedException($"Replication is not allowed in namespace {ns}");
		}
		IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();
		using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.Replicate").SetAttribute("operation.name", "HierarchicalStore.Replicate");

		using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope("blob.replicate", "Replicating blob from remote instances");

		List<string> regions = await _blobIndex.GetBlobRegionsAsync(ns, blob);

		if (!regions.Any())
		{
			throw new BlobReplicationException(ns, blob, "Blob not found in any region");
		}

		_logger.LogInformation("On-demand replicating blob {Blob} in Namespace {Namespace}", blob, ns);
		List<(int, string)> possiblePeers = new List<(int, string)>(_peerStatusService.GetPeersByLatency(regions));

		bool replicated = false;
		foreach ((int latency, string? region) in possiblePeers)
		{
			PeerStatus? peerStatus = _peerStatusService.GetPeerStatus(region);
			if (peerStatus == null)
			{
				throw new Exception($"Failed to find peer {region}");
			}

			_logger.LogInformation("Attempting to replicate blob {Blob} in Namespace {Namespace} from {Region}", blob, ns, region);

			PeerEndpoints peerEndpoint = peerStatus.Endpoints.First();
			using HttpClient httpClient = _httpClientFactory.CreateClient();
			string url = peerEndpoint.Url.ToString();
			if (!url.EndsWith("/", StringComparison.InvariantCultureIgnoreCase))
			{
				url += "/";
			}
			using HttpRequestMessage blobRequest = await BuildHttpRequestAsync(HttpMethod.Get, new Uri($"{url}api/v1/blobs/{ns}/{blob}?allowOndemandReplication=false"));
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
			using IBufferedPayload payload = await _bufferedPayloadFactory.CreateFilesystemBufferedPayloadAsync(s);
			await PutObjectAsync(ns, payload, blob);
			replicated = true;
			break;
		}

		if (!replicated)
		{
			throw new BlobReplicationException(ns, blob, $"Failed to replicate {blob} in {ns} due to it not existing in any region");
		}

		return await GetObjectAsync(ns, blob);
	}

	public bool ShouldFetchBlobOnDemand(NamespaceId ns)
	{
		return _settings.CurrentValue.EnableOnDemandReplication && _namespacePolicyResolver.GetPoliciesForNs(ns).OnDemandReplication;
	}

	private async Task<HttpRequestMessage> BuildHttpRequestAsync(HttpMethod httpMethod, Uri uri)
	{
		string? token = await _serviceCredentials.GetTokenAsync();
		HttpRequestMessage request = new HttpRequestMessage(httpMethod, uri);
		if (!string.IsNullOrEmpty(token))
		{
			request.Headers.Add("Authorization", $"{_serviceCredentials.GetAuthenticationScheme()} {token}");
		}

		return request;
	}

	public async Task<bool> ExistsAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null)
	{
		bool exists = await ExistsInStoresAsync(ns, blob, storageLayers);
		if (exists)
		{
			return exists;
		}

		NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);
		if (policy.FallbackNamespace != null)
		{
			return await ExistsInStoresAsync(policy.FallbackNamespace.Value, blob, storageLayers);
		}

		if (ShouldFetchBlobOnDemand(ns))
		{
			return await ExistsInRemoteAsync(ns, blob);
		}

		return false;
	}

	private async Task<bool> ExistsInStoresAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null)
	{
		bool useBlobIndex = _namespacePolicyResolver.GetPoliciesForNs(ns).UseBlobIndexForExists;
		if (useBlobIndex)
		{
			using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.ObjectExists")
				.SetAttribute("operation.name", "HierarchicalStore.ObjectExists")
				.SetAttribute("resource.name", blob.ToString())
				.SetAttribute("BlobStore", "BlobIndex")
				;
			bool exists = await _blobIndex.BlobExistsInRegionAsync(ns, blob);
			if (exists)
			{
				scope.SetAttribute("ObjectFound", true.ToString());
				return true;
			}

			scope.SetAttribute("ObjectFound", false.ToString());
			return false;
		}
		else
		{
			foreach (IBlobStore store in _blobStores)
			{
				string blobStoreName = store.GetType().Name;
				// check which storage layers to skip if we have a explicit list of storage layers to use
				if (storageLayers != null && storageLayers.Count != 0)
				{
					bool found = false;
					foreach (string storageLayer in storageLayers)
					{
						if (string.Equals(storageLayer, blobStoreName, StringComparison.OrdinalIgnoreCase))
						{
							found = true;
							break;
						}
					}

					if (!found)
					{
						continue;
					}
				}

				using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.ObjectExists")
					.SetAttribute("operation.name", "HierarchicalStore.ObjectExists")
					.SetAttribute("resource.name", blob.ToString())
					.SetAttribute("BlobStore", blobStoreName)
					;
				if (await store.ExistsAsync(ns, blob))
				{
					scope.SetAttribute("ObjectFound", true.ToString());
					return true;
				}
				scope.SetAttribute("ObjectFound", false.ToString());
			}

			return false;
		}
	}

	public async Task<bool> ExistsInRemoteAsync(NamespaceId ns, BlobId blob)
	{
		IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();
		using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.ExistsRemote").SetAttribute("operation.name", "HierarchicalStore.ExistsRemote");

		using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope("blob.exists-remote", "Verify if blob exists in remotes");

		List<string> regions = await _blobIndex.GetBlobRegionsAsync(ns, blob);

		// we do not actually verify that the blob exists remotely as that would take a lot of time
		// instead we simply check if there are any regions were the blob exists that is not our current region

		// if it exists in more then one region, we are sure it exists somewhere that is not here
		if (regions.Count > 1)
		{
			return true;
		}

		if (regions.Any(region => !string.Equals(region, _currentSite, StringComparison.OrdinalIgnoreCase)))
		{
			return true;
		}

		return false;
	}

	public async Task<bool> ExistsInRootStoreAsync(NamespaceId ns, BlobId blob)
	{
		IBlobStore store = _blobStores.Last();

		using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.ObjectExistsInRoot")
			.SetAttribute("operation.name", "HierarchicalStore.ObjectExistsInRoot")
			.SetAttribute("resource.name", blob.ToString())
			.SetAttribute("BlobStore", store.GetType().Name)
			;
		if (await store.ExistsAsync(ns, blob))
		{
			scope.SetAttribute("ObjectFound", true.ToString());
			return true;
		}
		scope.SetAttribute("ObjectFound", false.ToString());
		return false;
	}

	public async Task DeleteObjectAsync(NamespaceId ns, BlobId blob)
	{
		bool blobNotFound = false;
		bool deletedAtLeastOnce = false;

		// remove the object from the tracking first, if this times out we do not want to end up with a inconsistent blob index
		// if the blob store delete fails on the other hand we will still run a delete again during GC (as the blob is still orphaned at that point)
		// this assumes that blob gc is based on scanning the root blob store
		await _blobIndex.RemoveBlobFromRegionAsync(ns, blob);
		await _blobIndex.RemoveReferencesAsync(ns, blob, null);

		foreach (IBlobStore store in _blobStores)
		{
			try
			{
				using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.DeleteObject")
					.SetAttribute("operation.name", "HierarchicalStore.DeleteObject")
					.SetAttribute("resource.name", blob.ToString())
					.SetAttribute("BlobStore", store.GetType().Name)
					;

				await store.DeleteObjectAsync(ns, blob);
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

	public async Task DeleteNamespaceAsync(NamespaceId ns)
	{
		bool deletedAtLeastOnce = false;
		foreach (IBlobStore store in _blobStores)
		{
			using TelemetrySpan scope = _tracer.StartActiveSpan("HierarchicalStore.DeleteNamespace")
				.SetAttribute("operation.name", "HierarchicalStore.DeleteNamespace")
				.SetAttribute("resource.name", ns.ToString())
				.SetAttribute("BlobStore", store.GetType().Name)
				;
			try
			{
				await store.DeleteNamespaceAsync(ns);
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

	public IAsyncEnumerable<(BlobId,DateTime)> ListObjectsAsync(NamespaceId ns)
	{
		// as this is a hierarchy of blob stores the last blob store should contain the superset of all stores
		return _blobStores.Last().ListObjectsAsync(ns);
	}

	public async Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IEnumerable<BlobId> blobs)
	{
		List<(BlobId, Task<bool>)> existTasks = new();
		foreach (BlobId blob in blobs)
		{
			existTasks.Add((blob, ExistsAsync(ns, blob)));
		}

		List<BlobId> missingBlobs = new();
		foreach ((BlobId blob, Task<bool> existsTask) in existTasks)
		{
			bool exists = await existsTask;

			if (!exists)
			{
				missingBlobs.Add(blob);
			}
		}

		return missingBlobs.ToArray();
	}

	public async Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IAsyncEnumerable<BlobId> blobs)
	{
		ConcurrentBag<BlobId> missingBlobs = new ConcurrentBag<BlobId>();

		try
		{
			await Parallel.ForEachAsync(blobs, async (identifier, ctx) =>
			{
				bool exists = await ExistsAsync(ns, identifier);

				if (!exists)
				{
					missingBlobs.Add(identifier);
				}
			});
		}
		catch (AggregateException e)
		{
			if (e.InnerException is PartialReferenceResolveException)
			{
				throw e.InnerException;
			}

			throw;
		}

		return missingBlobs.ToArray();
	}

	public async Task<BlobContents> GetObjectsAsync(NamespaceId ns, BlobId[] blobs)
	{
		using TelemetrySpan _ = _tracer.StartActiveSpan("blob.combine").SetAttribute("operation.name", "blob.combine");
		Task<BlobContents>[] tasks = new Task<BlobContents>[blobs.Length];
		for (int i = 0; i < blobs.Length; i++)
		{
			tasks[i] = GetObjectAsync(ns, blobs[i]);
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
