// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace EpicGames.Horde
{
	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	class HordeClient : IHordeClient
	{
		readonly IHttpClientFactory _httpClientFactory;
		readonly HordeHttpAuthHandlerState _authHandlerState;
		readonly BundleCache _bundleCache;
		readonly HordeOptions _hordeOptions;
		readonly ILoggerFactory _loggerFactory;

		Uri? _serverUrl;

		/// <inheritdoc/>
		public Uri ServerUrl => GetServerUrl();

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeClient(IHttpClientFactory httpClientFactory, HordeHttpAuthHandlerState authHandlerState, BundleCache bundleCache, IOptionsSnapshot<HordeOptions> hordeOptions, ILoggerFactory loggerFactory)
		{
			_httpClientFactory = httpClientFactory;
			_authHandlerState = authHandlerState;
			_bundleCache = bundleCache;
			_hordeOptions = hordeOptions.Value;
			_loggerFactory = loggerFactory;
		}

		Uri GetServerUrl()
		{
			if (_serverUrl == null)
			{
				using HttpClient httpClient = _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
				_serverUrl = httpClient.BaseAddress ?? new Uri("http://horde-server");
			}
			return _serverUrl;
		}

		/// <inheritdoc/>
		public async Task<bool> ConnectAsync(bool allowLogin, CancellationToken cancellationToken)
		{
			return await _authHandlerState.LoginAsync(allowLogin, cancellationToken);
		}

		/// <inheritdoc/>
		public bool IsConnected()
		{
			try
			{
				return _authHandlerState.IsLoggedIn();
			}
			catch
			{
				return false;
			}
		}

		/// <inheritdoc/>
		public HordeHttpClient CreateHttpClient()
			=> _httpClientFactory.CreateHordeHttpClient();

		/// <inheritdoc/>
		public IStorageClient CreateStorageClient(string basePath)
		{
			Func<HttpClient> createClient = () => _httpClientFactory.CreateClient(HordeHttpClient.HttpClientName);
			Func<HttpClient> createUploadRedirectClient = () => _httpClientFactory.CreateClient(HordeHttpClient.UploadRedirectHttpClientName);
			HttpStorageBackend httpStorageBackend = new HttpStorageBackend(basePath, createClient, createUploadRedirectClient, _loggerFactory.CreateLogger<HttpStorageBackend>());
			return new BundleStorageClient(httpStorageBackend, _bundleCache, _hordeOptions.Bundle, _loggerFactory.CreateLogger<BundleStorageClient>());
		}
	}
}
