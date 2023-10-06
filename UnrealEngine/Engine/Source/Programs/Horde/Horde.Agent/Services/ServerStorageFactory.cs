// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Net.Http.Headers;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Services
{
	/// <summary>
	/// Interface for creating <see cref="IStorageClient"/> instances
	/// </summary>
	interface IServerStorageFactory
	{
		/// <summary>
		/// Creates a storage client which authenticates using tokens for the current session
		/// </summary>
		/// <param name="baseAddress">Base URL for writing blobs on the server</param>
		/// <param name="token">Token to use for authentication</param>
		/// <returns>New logger instance</returns>
		IStorageClient CreateStorageClient(Uri baseAddress, string token);
	}

	/// <summary>
	/// Extension methods for <see cref="IServerStorageFactory"/>
	/// </summary>
	static class ServerStorageExtensions
	{
		/// <summary>
		/// Creates a storage client which authenticates using tokens for the current session
		/// </summary>
		/// <param name="factory">Factory to create the client</param>
		/// <param name="session">The current session</param>
		/// <param name="baseUrl">Base URL for writing blobs on the server</param>
		/// <returns>New logger instance</returns>
		public static IStorageClient CreateStorageClient(this IServerStorageFactory factory, ISession session, string baseUrl) => CreateStorageClient(factory, session, baseUrl, session.Token);

		/// <summary>
		/// Creates a storage client which authenticates using tokens for the current session
		/// </summary>
		/// <param name="factory">Factory to create the client</param>
		/// <param name="session">The current session</param>
		/// <param name="baseUrl">Base URL for writing blobs on the server</param>
		/// <param name="token">Bearer token for the server</param>
		/// <returns>New logger instance</returns>
		public static IStorageClient CreateStorageClient(this IServerStorageFactory factory, ISession session, string baseUrl, string token)
		{
			if (!baseUrl.EndsWith("/", StringComparison.Ordinal))
			{
				baseUrl += "/";
			}
			return factory.CreateStorageClient(new Uri(session.ServerUrl, baseUrl), token);
		}

		/// <summary>
		/// Creates a storage client which authenticates using tokens for the current session
		/// </summary>
		/// <param name="factory">Factory to create the client</param>
		/// <param name="session">The current session</param>
		/// <param name="namespaceId">Namespace to write to</param>
		/// <param name="token">Bearer token for the server</param>
		/// <returns>New logger instance</returns>
		public static IStorageClient CreateStorageClient(this IServerStorageFactory factory, ISession session, NamespaceId namespaceId, string token)
		{
			return factory.CreateStorageClient(new Uri(session.ServerUrl, $"/api/v1/storage/{namespaceId}/"), token);
		}
	}

	/// <summary>
	/// Implementation of <see cref="IServerStorageFactory"/> which constructs objects that communicate via HTTP
	/// </summary>
	class HttpServerStorageFactory : IServerStorageFactory
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly IMemoryCache _memoryCache;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public HttpServerStorageFactory(IHttpClientFactory httpClientFactory, IMemoryCache memoryCache, ILogger<HttpStorageClient> logger)
		{
			_httpClientFactory = httpClientFactory;
			_memoryCache = memoryCache;
			_logger = logger;
		}

		/// <inheritdoc/>
		public IStorageClient CreateStorageClient(Uri baseAddress, string token)
		{
			return new HttpStorageClient(() => CreateHttpClient(baseAddress, token), CreateHttpRedirectClient, _memoryCache, _logger);
		}

		HttpClient CreateHttpClient(Uri baseAddress, string token)
		{
			HttpClient httpClient = _httpClientFactory.CreateClient(HttpStorageClient.HttpClientName);
			httpClient.BaseAddress = baseAddress;
			httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", token);
			return httpClient;
		}

		HttpClient CreateHttpRedirectClient()
		{
			return _httpClientFactory.CreateClient(HttpStorageClient.HttpClientName);
		}
	}
}
