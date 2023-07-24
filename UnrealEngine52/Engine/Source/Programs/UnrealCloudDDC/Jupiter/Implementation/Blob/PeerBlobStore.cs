// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Mime;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation.LeaderElection;
using k8s;
using k8s.Models;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Jupiter.Implementation
{
    public class PeerBlobStore : IBlobStore
    {
        private readonly IPeerServiceDiscovery _serviceDiscovery;
        private readonly IHttpClientFactory _httpClientFactory;
        private readonly IServiceCredentials _serviceCredentials;
        private readonly ILogger _logger;
        private readonly ConcurrentDictionary<string, HttpClient> _httpClients = new ConcurrentDictionary<string, HttpClient>();

        public PeerBlobStore(IPeerServiceDiscovery serviceDiscovery, IHttpClientFactory httpClientFactory, IServiceCredentials serviceCredentials, ILogger<PeerBlobStore> logger)
        {
            _serviceDiscovery = serviceDiscovery;
            _httpClientFactory = httpClientFactory;
            _serviceCredentials = serviceCredentials;
            _logger = logger;
        }

        private async Task<HttpRequestMessage> BuildHttpRequest(HttpMethod method, Uri uri)
        {
            string? token = await _serviceCredentials.GetTokenAsync();
            HttpRequestMessage request = new HttpRequestMessage(method, uri);
            if (!string.IsNullOrEmpty(token))
            {
                request.Headers.Add("Authorization", $"{_serviceCredentials.GetAuthenticationScheme()} {token}");
            }

            return request;
        }

        private async Task<BlobContents?> DoGetObject(string instance, NamespaceId ns, BlobIdentifier blob)
        {
            try
            {
                string filesystemLayerName = nameof(FileSystemStore);
                using HttpRequestMessage getObjectRequest = await BuildHttpRequest(HttpMethod.Get, new Uri($"api/v1/blobs/{ns}/{blob}?storageLayers={filesystemLayerName}", UriKind.Relative));
                getObjectRequest.Headers.Add("Accept", MediaTypeNames.Application.Octet);
                HttpResponseMessage response = await GetHttpClient(instance).SendAsync(getObjectRequest);
                if (response.StatusCode == HttpStatusCode.NotFound)
                {
                    return null;
                }

                response.EnsureSuccessStatusCode();

                long? contentLength = response.Content.Headers.ContentLength;
                if (contentLength == null)
                {
                    _logger.LogWarning("Content length missing in response from peer blob store. This is not supported, ignoring response");
                    return null;
                }
                    
                return new BlobContents(await response.Content.ReadAsStreamAsync(), contentLength.Value);
            }
            catch (Exception e)
            {
                _logger.LogWarning(e,
                    "Exception when attempting to fetch blob {Blob} in namespace {Namespace} from instance {Instance}",
                    blob, ns, instance);
            }

            return null;
        }

        public async Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking)
        {
            List<Task<BlobContents?>> tasks = new();

            await foreach (string instance in _serviceDiscovery.FindOtherInstances())
            {
                Task<BlobContents?> task = DoGetObject(instance, ns, blob);
                tasks.Add(task);
            }

            while (tasks.Count != 0)
            {
                Task<BlobContents?> finishedTask = await Task.WhenAny(tasks);
                BlobContents? result = await finishedTask;
                if (result != null)
                {
                    return result;
                }

                tasks.Remove(finishedTask);
            }

            throw new BlobNotFoundException(ns, blob);
        }

        private async Task<bool?> DoExists(string instance, NamespaceId ns, BlobIdentifier blob)
        {
            try
            {
                string filesystemLayerName = nameof(FileSystemStore);
                using HttpRequestMessage headObjectRequest = await BuildHttpRequest(HttpMethod.Head, new Uri($"api/v1/blobs/{ns}/{blob}?storageLayers={filesystemLayerName}", UriKind.Relative));
                HttpResponseMessage response = await GetHttpClient(instance).SendAsync(headObjectRequest, CancellationToken.None);
                if (response.StatusCode == HttpStatusCode.NotFound)
                {
                    return false;
                }

                response.EnsureSuccessStatusCode();

                return true;
            }
            catch (Exception e)
            {
                _logger.LogWarning(e,
                    "Exception when attempting to fetch blob {Blob} in namespace {Namespace} from instance {Instance}",
                    blob, ns, instance);
            }

            return null;
        }

        public async Task<bool> Exists(NamespaceId ns, BlobIdentifier blob, bool forceCheck = false)
        {
            List<Task<bool?>> tasks = new();

            await foreach (string instance in _serviceDiscovery.FindOtherInstances())
            {
                Task<bool?> task = DoExists(instance, ns, blob);
                tasks.Add(task);
            }

            while (tasks.Count != 0)
            {
                Task<bool?> finishedTask = await Task.WhenAny(tasks);
                bool? result = await finishedTask;
                if (result != null)
                {
                    return result.Value;
                }

                tasks.Remove(finishedTask);
            }

            return false;
        }

        private HttpClient GetHttpClient(string instance)
        {
            return _httpClients.GetOrAdd(instance, s =>
            {
                HttpClient httpClient = _httpClientFactory.CreateClient(instance);
                httpClient.BaseAddress = new Uri($"http://{instance}");

                // for these connections to be useful they need to be fast - so timeout quickly if we can not establish the connection
                httpClient.Timeout = TimeSpan.FromSeconds(1.0);
                return httpClient;
            });
        }

        public Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] blob, BlobIdentifier identifier)
        {
            // not applicable
            return Task.FromResult(identifier);
        }

        public Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> blob, BlobIdentifier identifier)
        {
            // not applicable
            return Task.FromResult(identifier);
        }

        public Task<BlobIdentifier> PutObject(NamespaceId ns, Stream content, BlobIdentifier identifier)
        {
            // not applicable
            return Task.FromResult(identifier);
        }

        public Task DeleteObject(NamespaceId ns, BlobIdentifier blob)
        {
            // not applicable
            return Task.CompletedTask;
        }

        public Task DeleteNamespace(NamespaceId ns)
        {
            // not applicable
            return Task.CompletedTask;
        }

        public IAsyncEnumerable<(BlobIdentifier, DateTime)> ListObjects(NamespaceId ns)
        {
            // not applicable
            return AsyncEnumerable.Empty<(BlobIdentifier, DateTime)>();
        }
    }

    public interface IPeerServiceDiscovery
    {
        public IAsyncEnumerable<string> FindOtherInstances();
    }

    public class StaticPeerServiceDiscoverySettings
    {
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
        public List<string> Peers { get; set; } = new List<string>();
    }

    public sealed class StaticPeerServiceDiscovery : IPeerServiceDiscovery
    {
        private readonly IOptionsMonitor<StaticPeerServiceDiscoverySettings> _settings;

        public StaticPeerServiceDiscovery(IOptionsMonitor<StaticPeerServiceDiscoverySettings> settings)
        {
            _settings = settings;
        }

        public async IAsyncEnumerable<string> FindOtherInstances()
        {
            await Task.CompletedTask;
            foreach (string peer in _settings.CurrentValue.Peers)
            {
                yield return peer;
            }
        }
    }

    public sealed class KubernetesPeerServiceDiscovery: IPeerServiceDiscovery, IDisposable
    {
        private readonly IOptionsMonitor<KubernetesLeaderElectionSettings> _leaderSettings;
        private readonly Kubernetes _client;
        private DateTime _podEnumerationValidUntil = DateTime.Now;
        private List<string>? _lastPodEnumeration;

        public KubernetesPeerServiceDiscovery(IOptionsMonitor<KubernetesLeaderElectionSettings> leaderSettings)
        {
            _leaderSettings = leaderSettings;
            KubernetesClientConfiguration config = KubernetesClientConfiguration.InClusterConfig();
            _client = new Kubernetes(config);
        }

        public async IAsyncEnumerable<string> FindOtherInstances()
        {
            if (_lastPodEnumeration != null && _podEnumerationValidUntil > DateTime.Now)
            {
                foreach (string pod in _lastPodEnumeration)
                {
                    yield return pod;
                }
                yield break;
            }

            _lastPodEnumeration = await EnumeratePods().ToListAsync();
            _podEnumerationValidUntil = DateTime.Now.AddMinutes(5);

            foreach (string pod in _lastPodEnumeration)
            {
                yield return pod;
            }
        }

        private async IAsyncEnumerable<string> EnumeratePods()
        {
            V1PodList podList = await _client.ListNamespacedPodAsync(_leaderSettings.CurrentValue.Namespace, labelSelector: _leaderSettings.CurrentValue.PeerPodLabelSelector);

            foreach (V1Pod pod in podList.Items)
            {
                if (pod.Status.Phase != "Running")
                {
                    continue;
                }
                string ip = pod.Status.PodIP;
                if (string.IsNullOrEmpty(ip))
                {
                    continue;
                }

                // we typically expose the container on port 80
                yield return $"{ip}:80";
            }
        }

        private void Dispose(bool disposing)
        {
            if (disposing)
            {
                _client.Dispose();
            }
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }
    }
}
