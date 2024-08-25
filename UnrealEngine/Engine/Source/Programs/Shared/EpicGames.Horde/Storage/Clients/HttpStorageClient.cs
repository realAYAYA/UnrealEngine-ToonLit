// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Factory for constructing HttpStorageClient instances
	/// </summary>
	public sealed class HttpStorageClientFactory : IStorageClientFactory
	{
		readonly HttpStorageBackendFactory _backendFactory;
		readonly BundleCache _bundleCache;
		readonly IOptionsSnapshot<HordeOptions> _hordeOptions;
		readonly ILogger<BundleStorageClient> _clientLogger;

		/// <summary>
		/// Constructor
		/// </summary>
		public HttpStorageClientFactory(HttpStorageBackendFactory backendFactory, BundleCache bundleCache, IOptionsSnapshot<HordeOptions> hordeOptions, ILogger<BundleStorageClient> clientLogger)
		{
			_backendFactory = backendFactory;
			_bundleCache = bundleCache;
			_hordeOptions = hordeOptions;
			_clientLogger = clientLogger;
		}

		/// <summary>
		/// Creates a new HTTP storage client
		/// </summary>
		/// <param name="basePath">Base path for all requests</param>
		/// <param name="accessToken">Custom access token to use for requests</param>
		/// <param name="withBackendCache"></param>
		public IStorageClient CreateClientWithPath(string basePath, string? accessToken = null, bool withBackendCache = true)
		{
			IStorageBackend backend = _backendFactory.CreateBackend(basePath, accessToken, withBackendCache);
			return new BundleStorageClient(backend, _bundleCache, _hordeOptions.Value.Bundle, _clientLogger);
		}

		/// <summary>
		/// Creates a new HTTP storage client
		/// </summary>
		/// <param name="namespaceId">Namespace to create a client for</param>
		/// <param name="accessToken">Custom access token to use for requests</param>
		/// <param name="withBackendCache">Whether to enable the backend cache, which caches full bundles to disk</param>
		public IStorageClient CreateClient(NamespaceId namespaceId, string? accessToken = null, bool withBackendCache = true) => CreateClientWithPath($"api/v1/storage/{namespaceId}", accessToken, withBackendCache);

		/// <inheritdoc/>
		IStorageClient? IStorageClientFactory.TryCreateClient(NamespaceId namespaceId) => CreateClient(namespaceId);
	}
}
