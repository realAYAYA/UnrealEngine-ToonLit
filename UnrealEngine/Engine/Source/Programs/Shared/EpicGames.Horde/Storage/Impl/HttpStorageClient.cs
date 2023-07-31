// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage.Impl
{
	/// <summary>
	/// Response from checking for missing blobs
	/// </summary>
	public class BlobExistsResponse
	{
		/// <summary>
		/// Set of missing hashes
		/// </summary>
		public HashSet<IoHash> Needs { get; } = new HashSet<IoHash>();
	}

	/// <summary>
	/// Response from adding a new object
	/// </summary>
	public class RefPutResponse
	{
		/// <summary>
		/// List of missing hashes
		/// </summary>
		public List<IoHash> Needs { get; } = new List<IoHash>();
	}

	/// <summary>
	/// Response from posting to the /exists endpoint
	/// </summary>
	public class RefExistsResponse
	{
		/// <summary>
		/// List of hashes that the blob store needs.
		/// </summary>
		public List<RefId> Needs { get; } = new List<RefId>();
	}

	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> using REST requests.
	/// </summary>
	public sealed class HttpStorageClient : IStorageClient
	{
		class RefImpl : IRef
		{
			public NamespaceId NamespaceId { get; set; }

			public BucketId BucketId { get; set; }

			public RefId RefId { get; set; }

			public CbObject Value { get; }

			public RefImpl(NamespaceId namespaceId, BucketId bucketId, RefId refId, CbObject value)
			{
				NamespaceId = namespaceId;
				BucketId = bucketId;
				RefId = refId;
				Value = value;
			}
		}

		const string HashHeaderName = "X-Jupiter-IoHash";
//		const string LastAccessHeaderName = "X-Jupiter-LastAccess";

		const string CompactBinaryMimeType = "application/x-ue-cb";

		readonly HttpClient _httpClient;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="httpClient">Http client for the blob store. Should be pre-configured with a base address and appropriate authentication headers.</param>
		public HttpStorageClient(HttpClient httpClient)
		{
			_httpClient = httpClient;
		}

		#region Blobs

		/// <inheritdoc/>
		public Task<Stream> ReadBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken)
		{
			return ReadBlobFromUrlAsync($"api/v1/blobs/{namespaceId}/{hash}", namespaceId, hash, cancellationToken);
		}
		
		/// <inheritdoc/>
		public Task<Stream> ReadCompressedBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default)
		{
			return ReadBlobFromUrlAsync($"api/v1/compressed-blobs/{namespaceId}/{hash}", namespaceId, hash, cancellationToken);
		}
		
		private async Task<Stream> ReadBlobFromUrlAsync(string url, NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken)
		{
			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, url);

			HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			try
			{
				if (response.StatusCode == System.Net.HttpStatusCode.NotFound)
				{
					throw new BlobNotFoundException(namespaceId, hash);
				}

				response.EnsureSuccessStatusCode();
				return await response.Content.ReadAsStreamAsync(cancellationToken);
			}
			catch
			{
				response.Dispose();
				throw;
			}
		}

		/// <inheritdoc/>
		public async Task WriteBlobAsync(NamespaceId namespaceId, IoHash hash, Stream stream, CancellationToken cancellationToken)
		{
			StreamContent content = new StreamContent(stream);
			content.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
			content.Headers.ContentLength = stream.Length;

			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Put, $"api/v1/blobs/{namespaceId}/{hash}");
			request.Content = content;

			HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			response.EnsureSuccessStatusCode();
		}

		class WriteBlobResponse
		{
			[JsonPropertyName("identifier")]
			public IoHash Identifier { get; set; }
		}

		/// <inheritdoc/>
		public async Task<IoHash> WriteBlobAsync(NamespaceId namespaceId, Stream stream, CancellationToken cancellationToken)
		{
			StreamContent content = new StreamContent(stream);
			content.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
			content.Headers.ContentLength = stream.Length;

			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, $"api/v1/blobs/{namespaceId}");
			request.Content = content;

			HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			response.EnsureSuccessStatusCode();

			using (Stream responseStream = await response.Content.ReadAsStreamAsync(cancellationToken))
			{
				WriteBlobResponse? responseMessage = await JsonSerializer.DeserializeAsync<WriteBlobResponse>(responseStream, cancellationToken: cancellationToken);
				return responseMessage!.Identifier;
			}
		}

		/// <inheritdoc/>
		public async Task WriteCompressedBlobAsync(NamespaceId namespaceId, IoHash uncompressedHash, Stream stream, CancellationToken cancellationToken = default)
		{
			StreamContent content = new StreamContent(stream);
			content.Headers.ContentType = new MediaTypeHeaderValue("application/x-ue-comp");
			content.Headers.ContentLength = stream.Length;

			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Put, $"api/v1/compressed-blobs/{namespaceId}/{uncompressedHash}");
			request.Content = content;

			HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			response.EnsureSuccessStatusCode();
		}

		/// <inheritdoc/>
		public async Task<IoHash> WriteCompressedBlobAsync(NamespaceId namespaceId, Stream compressedStream, CancellationToken cancellationToken = default)
		{
			StreamContent content = new (compressedStream);
			content.Headers.ContentType = new MediaTypeHeaderValue("application/x-ue-comp");
			content.Headers.ContentLength = compressedStream.Length;

			using HttpRequestMessage request = new (HttpMethod.Post, $"api/v1/compressed-blobs/{namespaceId}");
			request.Content = content;

			HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			response.EnsureSuccessStatusCode();
			
			using (Stream responseStream = await response.Content.ReadAsStreamAsync(cancellationToken))
			{
				WriteBlobResponse? responseMessage = await JsonSerializer.DeserializeAsync<WriteBlobResponse>(responseStream, cancellationToken: cancellationToken);
				return responseMessage!.Identifier;
			}
		}

		/// <inheritdoc/>
		public async Task<bool> HasBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default)
		{
			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Head, $"api/v1/blobs/{namespaceId}/{hash}");

			HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			try
			{
				if (response.StatusCode != System.Net.HttpStatusCode.NotFound)
				{
					response.EnsureSuccessStatusCode();
				}
				return response.StatusCode == System.Net.HttpStatusCode.OK;
			}
			catch
			{
				response.Dispose();
				throw;
			}
		}

		/// <inheritdoc/>
		public async Task<HashSet<IoHash>> FindMissingBlobsAsync(NamespaceId namespaceId, HashSet<IoHash> hashes, CancellationToken cancellationToken)
		{
			using StringContent content = new StringContent(JsonSerializer.Serialize(hashes));

			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, $"api/v1/blobs/{namespaceId}/exist");

			request.Headers.Accept.Clear();
			request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
			content.Headers.ContentType = new MediaTypeHeaderValue("application/json");
			request.Content = content;

			HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			response.EnsureSuccessStatusCode();

			BlobExistsResponse? responseBody = await ReadJsonResponse<BlobExistsResponse>(response.Content);
			if (responseBody == null)
			{
				throw new StorageException("Unable to parse response body", null);
			}
			return responseBody.Needs;
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public async Task<IRef> GetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken)
		{
			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, $"api/v1/refs/{namespaceId}/{bucketId}/{refId}");

			request.Headers.Accept.Clear();
			request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue(CompactBinaryMimeType));

			using HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			if (!response.IsSuccessStatusCode)
			{
				if (response.StatusCode == System.Net.HttpStatusCode.NotFound)
				{
					throw new RefNotFoundException(namespaceId, bucketId, refId);
				}
				else
				{
					throw new RefException(namespaceId, bucketId, refId, await GetMessageFromResponse(response));
				}
			}

			byte[] data = await response.Content.ReadAsByteArrayAsync(cancellationToken);
			return new RefImpl(namespaceId, bucketId, refId, new CbObject(data));
		}

		/// <inheritdoc/>
		public async Task<List<IoHash>> TrySetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CbObject value, CancellationToken cancellationToken)
		{
			using ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(value.GetView());

			StreamContent content = new StreamContent(stream);
			content.Headers.ContentType = new MediaTypeHeaderValue(CompactBinaryMimeType);
			content.Headers.Add(HashHeaderName, IoHash.Compute(value.GetView().Span).ToString());

			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Put, $"api/v1/refs/{namespaceId}/{bucketId}/{refId}");

			request.Headers.Accept.Clear();
			request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
			request.Content = content;

			using HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			if (!response.IsSuccessStatusCode)
			{
				throw new RefException(namespaceId, bucketId, refId, await GetMessageFromResponse(response));
			}

			RefPutResponse? responseBody = await ReadJsonResponse<RefPutResponse>(response.Content);
			if (responseBody == null)
			{
				throw new RefException(namespaceId, bucketId, refId, "Unable to parse response body");
			}

			return responseBody.Needs;
		}

		/// <inheritdoc/>
		public async Task<List<IoHash>> TryFinalizeRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, IoHash hash, CancellationToken cancellationToken)
		{
			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, $"api/v1/refs/{namespaceId}/{bucketId}/{refId}/finalize/{hash}");

			request.Headers.Accept.Clear();
			request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));

			using HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			if (!response.IsSuccessStatusCode)
			{
				throw new RefException(namespaceId, bucketId, refId, await GetMessageFromResponse(response));
			}

			RefPutResponse? responseBody = await ReadJsonResponse<RefPutResponse>(response.Content);
			if (responseBody == null)
			{
				throw new RefException(namespaceId, bucketId, refId, "Unable to parse response body");
			}

			return responseBody.Needs;
		}

		/// <inheritdoc/>
		public async Task<bool> DeleteRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken)
		{
			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Delete, $"api/v1/refs/{namespaceId}/{bucketId}/{refId}");

			using HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			return response.IsSuccessStatusCode;
		}

		/// <inheritdoc/>
		public async Task<bool> HasRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken)
		{
			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Head, $"api/v1/refs/{namespaceId}/{bucketId}/{refId}");

			using HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			if (response.IsSuccessStatusCode)
			{
				return true;
			}
			else if (response.StatusCode == System.Net.HttpStatusCode.NotFound)
			{
				return false;
			}
			else
			{
				throw new RefException(namespaceId, bucketId, refId, $"Unexpected response for {nameof(HasRefAsync)} call: {response.StatusCode}");
			}
		}

		/// <inheritdoc/>
		public async Task<List<RefId>> FindMissingRefsAsync(NamespaceId namespaceId, BucketId bucketId, List<RefId> refIds, CancellationToken cancellationToken)
		{
			string refList = String.Join("&", refIds.Select(x => $"id={x}"));

			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, $"/api/v1/refs/{namespaceId}/{bucketId}/exists?{refList}");

			using HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			if (!response.IsSuccessStatusCode)
			{
				throw new RefException(namespaceId, bucketId, new RefId(IoHash.Zero), await GetMessageFromResponse(response));
			}

			RefExistsResponse? responseContent = await ReadJsonResponse<RefExistsResponse>(response.Content);
			if (responseContent == null)
			{
				throw new RefException(namespaceId, bucketId, new RefId(IoHash.Zero), "Unable to parse response body");
			}

			return responseContent.Needs;
		}

		#endregion

		class ProblemDetails
		{
			public string? Title { get; set; }
		}

		static async Task<string> GetMessageFromResponse(HttpResponseMessage response)
		{
			StringBuilder message = new StringBuilder($"HTTP {response.StatusCode}");
			try
			{
				byte[] data = await response.Content.ReadAsByteArrayAsync();
				ProblemDetails? details = JsonSerializer.Deserialize<ProblemDetails>(data);
				message.Append($": {details?.Title ?? "No description available"}");
			}
			catch
			{
				message.Append(" (Unable to parse response data)");
			}
			return message.ToString();
		}

		static async Task<T> ReadJsonResponse<T>(HttpContent content)
		{
			byte[] data = await content.ReadAsByteArrayAsync();
			return JsonSerializer.Deserialize<T>(data, new JsonSerializerOptions { PropertyNameCaseInsensitive = true })!;
		}
	}
}
