// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Web;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Storage backend which communicates with the Horde server over HTTP for content
	/// </summary>
	public sealed class HttpStorageBackend : IStorageBackend
	{
		class WriteBlobResponse
		{
			public string Blob { get; set; } = String.Empty;
			public Uri? UploadUrl { get; set; }
			public bool? SupportsRedirects { get; set; }
		}

		readonly string _basePath;
		readonly Func<HttpClient> _createClient;
		readonly Func<HttpClient> _createUploadRedirectClient;
		readonly ILogger _logger;
		bool _supportsUploadRedirects = true;

		long _numBytes;
		int _numActive;
		TimeSpan _sequentialReadTime;
		readonly Stopwatch _readTime = new Stopwatch();

		/// <inheritdoc/>
		public bool SupportsRedirects => _supportsUploadRedirects;

		/// <summary>
		/// Constructor
		/// </summary>
		public HttpStorageBackend(string basePath, Func<HttpClient> createClient, Func<HttpClient> createUploadRedirectClient, ILogger logger)
		{
			_basePath = basePath.TrimEnd('/');
			_createClient = createClient;
			_createUploadRedirectClient = createUploadRedirectClient;
			_logger = logger;
		}

		#region Blobs

		/// <inheritdoc/>
		public async Task<Stream> OpenBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default)
		{
			Stopwatch blobTimer = Stopwatch.StartNew();
			try
			{
				lock (_readTime)
				{
					if (++_numActive == 1)
					{
						_readTime.Start();
					}
				}

				if (offset == 0 && length == null)
				{
					_logger.LogDebug("Reading {Locator}", locator);
				}
				else if (length == null)
				{
					_logger.LogDebug("Reading {Locator} ({Offset}..)", locator, offset);
				}
				else
				{
					_logger.LogDebug("Reading {Locator} ({Offset}+{Length})", locator, offset, length);
				}

				if (length.HasValue && length.Value == 0)
				{
					return new MemoryStream(Array.Empty<byte>());
				}

				using (HttpClient httpClient = _createClient())
				{
					using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, $"{_basePath}/blobs/{locator}"))
					{
						if (offset != 0 || length != null)
						{
							request.Headers.Range = new RangeHeaderValue(offset, (length == null) ? null : (offset + (length - 1)));
						}

						HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken);
						response.EnsureSuccessStatusCode();

						Stream stream = await response.Content.ReadAsStreamAsync(cancellationToken);

						try
						{
							Interlocked.Add(ref _numBytes, response.Content.Headers.ContentLength ?? stream.Length);
						}
						catch
						{
						}

						return stream;
					}
				}
			}
			finally
			{
				lock (_readTime)
				{
					_sequentialReadTime += blobTimer.Elapsed;
					if (--_numActive == 0)
					{
						_readTime.Stop();
					}
				}
			}
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyMemoryOwner<byte>> ReadBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default)
		{
			using (Stream stream = await OpenBlobAsync(locator, offset, length, cancellationToken))
			{
				byte[] data = await stream.ReadAllBytesAsync(cancellationToken);
				return ReadOnlyMemoryOwner.Create(data);
			}
		}

		/// <inheritdoc/>
		public async Task<BlobLocator> WriteBlobAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default)
		{
			using StreamContent streamContent = new StreamContent(stream);

			if (_supportsUploadRedirects)
			{
				WriteBlobResponse redirectResponse = await SendWriteRequestAsync(null, prefix, cancellationToken);
				if (redirectResponse.UploadUrl != null)
				{
					using HttpClient uploadRedirectClient = _createUploadRedirectClient();
					using HttpResponseMessage uploadResponse = await uploadRedirectClient.PutAsync(redirectResponse.UploadUrl, streamContent, cancellationToken);
					if (!uploadResponse.IsSuccessStatusCode)
					{
						string body = await uploadResponse.Content.ReadAsStringAsync(cancellationToken);
						throw new StorageException($"Unable to upload data to redirected URL: {body}");
					}

					_logger.LogDebug("Written {Locator} (using redirect)", redirectResponse.Blob);
					return new BlobLocator(redirectResponse.Blob);
				}
			}

			WriteBlobResponse response = await SendWriteRequestAsync(streamContent, prefix, cancellationToken);
			_supportsUploadRedirects = response.SupportsRedirects ?? false;
			_logger.LogDebug("Written {Locator} (direct)", response.Blob);
			return new BlobLocator(response.Blob);
		}

		async Task<WriteBlobResponse> SendWriteRequestAsync(StreamContent? streamContent, string? prefix = null, CancellationToken cancellationToken = default)
		{
			using (HttpClient httpClient = _createClient())
			{
				using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, $"{_basePath}/blobs"))
				{
					using StringContent stringContent = new StringContent(prefix ?? String.Empty);

					MultipartFormDataContent form = new MultipartFormDataContent();
					if (streamContent != null)
					{
						form.Add(streamContent, "file", "filename");
					}
					form.Add(stringContent, "prefix");

					request.Content = form;
					using (HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken))
					{
						if (!response.IsSuccessStatusCode)
						{
							string responseText = await response.Content.ReadAsStringAsync(cancellationToken);
							throw new StorageException($"Upload to {request.RequestUri} failed ({response.StatusCode}). Response: {responseText}");
						}

						WriteBlobResponse? data = await response.Content.ReadFromJsonAsync<WriteBlobResponse>(cancellationToken: cancellationToken);
						return data!;
					}
				}
			}
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetBlobReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			// We don't currently have any need for getting a read redirect explicitly, though ReadAsync() calls may redirect us automatically.
			return default;
		}

		/// <inheritdoc/>
		public async ValueTask<(BlobLocator, Uri)?> TryGetBlobWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default)
		{
			if (_supportsUploadRedirects)
			{
				WriteBlobResponse redirectResponse = await SendWriteRequestAsync(null, prefix, cancellationToken);
				if (redirectResponse.UploadUrl != null)
				{
					return (new BlobLocator(redirectResponse.Blob), redirectResponse.UploadUrl);
				}
			}
			return null;
		}

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public Task AddAliasAsync(string name, BlobLocator target, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("Http storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public Task RemoveAliasAsync(string name, BlobLocator target, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("Http storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public async Task<BlobAliasLocator[]> FindAliasesAsync(string alias, int? maxResults = null, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Finding nodes with alias {Alias}", alias);
			using (HttpClient httpClient = _createClient())
			{
				string queryPath = $"{_basePath}/nodes?alias={HttpUtility.UrlEncode(alias.ToString())}";
				if (maxResults != null)
				{
					queryPath += $"&maxResults={maxResults.Value}";
				}

				using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, queryPath))
				{
					using (HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken))
					{
						response.EnsureSuccessStatusCode();

						FindNodesResponse? message = await response.Content.ReadFromJsonAsync<FindNodesResponse>(cancellationToken: cancellationToken);

						BlobAliasLocator[] aliases = new BlobAliasLocator[message!.Nodes.Count];
						for (int idx = 0; idx < message.Nodes.Count; idx++)
						{
							FindNodeResponse node = message.Nodes[idx];
							aliases[idx] = new BlobAliasLocator(node.Blob, node.Rank, node.Data);
						}

						return aliases;
					}
				}
			}
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public async Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken)
		{
			_logger.LogDebug("Deleting ref {RefName}", name);
			using (HttpClient httpClient = _createClient())
			{
				using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Delete, $"{_basePath}/refs/{name}"))
				{
					using (HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken))
					{
						if (response.IsSuccessStatusCode)
						{
							return true;
						}
						if (response.StatusCode == HttpStatusCode.NotFound)
						{
							return false;
						}

						response.EnsureSuccessStatusCode();
						return false;
					}
				}
			}
		}

		/// <inheritdoc/>
		public async Task<BlobRefValue?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			using (HttpClient httpClient = _createClient())
			{
				using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, $"{_basePath}/refs/{name}"))
				{
					if (cacheTime.IsSet())
					{
						request.Headers.CacheControl = new CacheControlHeaderValue { MaxAge = cacheTime.MaxAge };
					}

					using (HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken))
					{
						if (response.StatusCode == HttpStatusCode.NotFound)
						{
							_logger.LogDebug("Read ref {RefName} -> None", name);
							return null;
						}
						else if (!response.IsSuccessStatusCode)
						{
							_logger.LogError("Unable to read ref {RefName} (status: {StatusCode}, body: {Body})", name, response.StatusCode, await response.Content.ReadAsStringAsync(cancellationToken));
							throw new StorageException($"Unable to read ref '{name}'");
						}
						else
						{
							response.EnsureSuccessStatusCode();
							ReadRefResponse? data = await response.Content.ReadFromJsonAsync<ReadRefResponse>(cancellationToken: cancellationToken);
							_logger.LogDebug("Read ref {RefName} -> {Hash} / {Locator}", name, data!.Hash, data!.Target);

							return new BlobRefValue(data.Hash, data.Target);
						}
					}
				}
			}
		}

		/// <inheritdoc/>
		public async Task WriteRefAsync(RefName name, BlobRefValue value, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			_logger.LogDebug("Writing ref {RefName} -> {Hash} / {Locator}", name, value.Hash, value.Locator);
			using (HttpClient httpClient = _createClient())
			{
				WriteRefRequest request = new WriteRefRequest();
				request.Hash = value.Hash;
				request.Target = value.Locator;
				request.Options = options;

				using (HttpResponseMessage response = await httpClient.PutAsync($"{_basePath}/refs/{name}", request, cancellationToken))
				{
					response.EnsureSuccessStatusCode();
				}
			}
		}

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
		{
			stats.Add("backend.http.wall_time_secs", (long)_readTime.Elapsed.TotalSeconds);
			stats.Add("backend.http.num_bytes", _numBytes);
			if (_readTime.Elapsed > TimeSpan.Zero)
			{
				stats.Add("backend.http.speed_mb_sec", (long)(_numBytes / (1024.0 * 1024.0 * _readTime.Elapsed.TotalSeconds)));
				stats.Add("backend.http.concurrency_ratio", (long)(_sequentialReadTime.TotalSeconds * 100.0 / _readTime.Elapsed.TotalSeconds));
			}
		}
	}

	/// <summary>
	/// Factory for constructing HttpStorageBackend instances
	/// </summary>
	public sealed class HttpStorageBackendFactory
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly StorageBackendCache _backendCache;
		readonly ILogger<HttpStorageBackend> _backendLogger;

		/// <summary>
		/// Constructor
		/// </summary>
		public HttpStorageBackendFactory(IHttpClientFactory httpClientFactory, StorageBackendCache backendCache, ILogger<HttpStorageBackend> backendLogger)
		{
			_httpClientFactory = httpClientFactory;
			_backendCache = backendCache;
			_backendLogger = backendLogger;
		}

		/// <summary>
		/// Creates a new HTTP storage client
		/// </summary>
		/// <param name="basePath">Base path for all requests</param>
		/// <param name="accessToken">Custom access token to use for requests</param>
		/// <param name="withBackendCache"></param>
		public IStorageBackend CreateBackend(string basePath, string? accessToken = null, bool withBackendCache = true)
		{
			HttpClient CreateClient()
			{
				HttpClient httpClient = _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
				if (accessToken != null)
				{
					httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", accessToken);
				}
				return httpClient;
			}

			HttpClient CreateUploadRedirectClient() => _httpClientFactory.CreateClient(HordeHttpClient.UploadRedirectHttpClientName);

			IStorageBackend backend = new HttpStorageBackend(basePath, CreateClient, CreateUploadRedirectClient, _backendLogger);
			if (_backendCache != null && withBackendCache)
			{
				backend = _backendCache.CreateWrapper(basePath, backend);
			}

			return backend;
		}

		/// <summary>
		/// Creates a new HTTP storage client
		/// </summary>
		/// <param name="namespaceId">Namespace to create a client for</param>
		/// <param name="accessToken">Custom access token to use for requests</param>
		/// <param name="withBackendCache">Whether to enable the backend cache, which caches full bundles to disk</param>
		public IStorageBackend CreateBackend(NamespaceId namespaceId, string? accessToken = null, bool withBackendCache = true) => CreateBackend($"api/v1/storage/{namespaceId}", accessToken, withBackendCache);
	}
}
