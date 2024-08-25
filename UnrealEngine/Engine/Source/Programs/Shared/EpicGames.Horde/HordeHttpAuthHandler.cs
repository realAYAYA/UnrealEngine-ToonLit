// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Server;
using EpicGames.OIDC;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace EpicGames.Horde
{
	/// <summary>
	/// HTTP message handler which automatically refreshes access tokens as required
	/// </summary>
	public class HordeHttpAuthHandler : DelegatingHandler
	{
		readonly HordeHttpAuthHandlerState _authState;
		readonly IOptions<HordeOptions> _options;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeHttpAuthHandler(HordeHttpAuthHandlerState authState, IOptions<HordeOptions> options)
		{
			_authState = authState;
			_options = options;
		}

		/// <inheritdoc/>
		protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
		{
			// If the request already has a custom auth header, send the request as it is
			if (request.Headers.Authorization != null)
			{
				return await base.SendAsync(request, cancellationToken);
			}

			// Get the current access token and send the request with that
			string? accessToken = await _authState.GetAccessTokenAsync(_options.Value.AllowAuthPrompt, cancellationToken);
			for (int attempt = 0; ; attempt++)
			{
				// Attempt to perform the request with this access token
				request.Headers.Authorization = (accessToken == null)? null : new AuthenticationHeaderValue("Bearer", accessToken);
				HttpResponseMessage response = await base.SendAsync(request, cancellationToken);

				const int MaxAttempts = 3;
				if (response.StatusCode != HttpStatusCode.Unauthorized || attempt >= MaxAttempts)
				{
					return response;
				}

				// Mark this access token as invalid
				if (accessToken != null)
				{
					_authState.Invalidate(accessToken);
				}

				// Get the next token, and quit out if it's the same
				string? nextAccessToken = await _authState.GetAccessTokenAsync(_options.Value.AllowAuthPrompt, cancellationToken);
				if (String.Equals(accessToken, nextAccessToken, StringComparison.Ordinal))
				{
					return response;
				}

				// Otherwise update the token and try again
				accessToken = nextAccessToken;
			}
		}
	}

	/// <summary>
	/// Shared object used to track the latest access obtained token
	/// </summary>
	public sealed class HordeHttpAuthHandlerState : IAsyncDisposable
	{
		/// <summary>
		/// HTTP client name
		/// </summary>
		public const string HttpClientName = "HordeHttpAuthState";

		record class AuthState(AuthMethod Method, OidcTokenInfo? TokenInfo, bool Interactive)
		{
			public bool IsAuthorized()
				=> (Method == AuthMethod.Anonymous) || (TokenInfo != null && TokenInfo.IsValid);
		}

		readonly object _lockObject = new object();
		readonly CancellationTokenSource _cancellationTokenSource = new CancellationTokenSource();
		Task<AuthState>? _authStateTask = null;
		readonly IHttpClientFactory _httpClientFactory;
		readonly IOptions<HordeOptions> _options;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeHttpAuthHandlerState(IHttpClientFactory httpClientFactory, IOptions<HordeOptions> options, ILogger<HordeHttpAuthHandler> logger)
		{
			_httpClientFactory = httpClientFactory;
			_options = options;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			if (_authStateTask != null && !_authStateTask.IsCompleted)
			{
				_cancellationTokenSource.Cancel();
				try
				{
#pragma warning disable VSTHRD003
					await _authStateTask;
#pragma warning restore VSTHRD003
				}
				catch (OperationCanceledException)
				{
				}
			}

			_cancellationTokenSource.Dispose();
		}

		/// <summary>
		/// Checks if we have a valid auth header at the moment
		/// </summary>
		public bool IsLoggedIn()
		{
			if (GetAccessTokenFromConfig() != null)
			{
				return true;
			}
			if (_authStateTask != null && _authStateTask.IsCompletedSuccessfully && _authStateTask.TryGetResult(out AuthState? authState) && authState.IsAuthorized())
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Marks the given access token as invalid, having attempted to use it and got an unauthorized response
		/// </summary>
		/// <param name="accessToken">The access  header to invalidate</param>
		public void Invalidate(string? accessToken)
		{
			lock (_lockObject)
			{
#pragma warning disable VSTHRD002
				if (_authStateTask != null && _authStateTask.IsCompleted && Object.Equals(_authStateTask.Result?.TokenInfo?.AccessToken, accessToken))
				{
					_authStateTask = null;
				}
#pragma warning restore VSTHRD002
			}
		}

		/// <summary>
		/// Try to get a configured auth header
		/// </summary>
		public string? GetAccessTokenFromConfig()
		{
			// If an explicit access token is specified, just use that
			if (_options.Value.AccessToken != null)
			{
				return _options.Value.AccessToken;
			}

			// Check environment variables for an access token matching the current server
			string? hordeUrlEnvVar = Environment.GetEnvironmentVariable(HordeHttpClient.HordeUrlEnvVarName);
			if (!String.IsNullOrEmpty(hordeUrlEnvVar))
			{
				Uri hordeUrl = new Uri(hordeUrlEnvVar);
				if (_options.Value.ServerUrl == null || String.Equals(_options.Value.ServerUrl.Host, hordeUrl.Host, StringComparison.OrdinalIgnoreCase))
				{
					string? hordeToken = Environment.GetEnvironmentVariable(HordeHttpClient.HordeTokenEnvVarName);
					if (!String.IsNullOrEmpty(hordeToken))
					{
						return hordeToken;
					}
				}
			}

			// Otherwise we need to find the access token dynamically
			return null;
		}

		/// <summary>
		/// Refresh the auth state
		/// </summary>
		/// <param name="interactive">Whether to allow logging in interactively</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task<bool> LoginAsync(bool interactive, CancellationToken cancellationToken)
		{
			if (GetAccessTokenFromConfig() != null)
			{
				return true;
			}

			AuthState? state = await GetAuthStateAsync(interactive, cancellationToken);
			return state?.IsAuthorized() ?? false;
		}

		/// <summary>
		/// Gets the current access token
		/// </summary>
		public async Task<string?> GetAccessTokenAsync(bool allowAuthPrompt, CancellationToken cancellationToken)
		{
			string? accessToken = GetAccessTokenFromConfig();
			if (accessToken != null)
			{
				return accessToken;
			}

			AuthState? authState = await GetAuthStateAsync(allowAuthPrompt, cancellationToken);
			return authState?.TokenInfo?.AccessToken;
		}

		async Task<AuthState?> GetAuthStateAsync(bool interactive, CancellationToken cancellationToken)
		{
			if (GetAccessTokenFromConfig() != null)
			{
				return null;
			}

			Task<AuthState>? authStateTask = null;
			for (; ; )
			{
				lock (_lockObject)
				{
					if (_authStateTask == null || _authStateTask == authStateTask)
					{
						_authStateTask = Task.Run(() => GetAuthStateInternalAsync(interactive, _cancellationTokenSource.Token), _cancellationTokenSource.Token);
					}
					authStateTask = _authStateTask;
				}

				AuthState authState = await authStateTask.WaitAsync(cancellationToken);
				if (authState.IsAuthorized() || !interactive || authState.Interactive)
				{
					return authState;
				}
			}
		}

		async Task<AuthState> GetAuthStateInternalAsync(bool interactive, CancellationToken cancellationToken)
		{
			Uri serverUrl;

			GetAuthConfigResponse? authConfig;
			using (HttpClient httpClient = _httpClientFactory.CreateClient(HttpClientName))
			{
				if (httpClient.BaseAddress == null)
				{
					throw new Exception("No http client is configured for Horde. Call IServiceCollection.AddHordeHttpClient().");
				}

				serverUrl = httpClient.BaseAddress;
				_logger.LogDebug("Retrieving auth configuration for {Server}", serverUrl);

				JsonSerializerOptions jsonOptions = new JsonSerializerOptions();
				HordeHttpClient.ConfigureJsonSerializer(jsonOptions);

				authConfig = await httpClient.GetFromJsonAsync<GetAuthConfigResponse>("api/v1/server/auth", jsonOptions, cancellationToken);
				if (authConfig == null)
				{
					throw new Exception($"Invalid response from server");
				}
			}

			if (authConfig.Method == AuthMethod.Anonymous)
			{
				return new AuthState(authConfig.Method, null, true);
			}

			string? localRedirectUrl = authConfig.LocalRedirectUrls?.FirstOrDefault();
			if (String.IsNullOrEmpty(authConfig.ServerUrl) || String.IsNullOrEmpty(localRedirectUrl))
			{
				throw new Exception("No auth server configuration found");
			}

			string oidcProvider = authConfig.ProfileName ?? "Horde";

			Dictionary<string, string?> values = new Dictionary<string, string?>();
			values[$"Providers:{oidcProvider}:DisplayName"] = oidcProvider;
			values[$"Providers:{oidcProvider}:ServerUri"] = authConfig.ServerUrl;
			values[$"Providers:{oidcProvider}:ClientId"] = authConfig.ClientId;
			values[$"Providers:{oidcProvider}:RedirectUri"] = localRedirectUrl;

			ConfigurationBuilder builder = new ConfigurationBuilder();
			builder.AddInMemoryCollection(values);

			IConfiguration configuration = builder.Build();

			using ITokenStore tokenStore = TokenStoreFactory.CreateTokenStore();
			OidcTokenManager oidcTokenManager = OidcTokenManager.CreateTokenManager(configuration, tokenStore, new List<string>() { oidcProvider });

			OidcTokenInfo? result = null;
			if (oidcTokenManager.GetStatusForProvider(oidcProvider) != OidcStatus.NotLoggedIn)
			{
				try
				{
					result = await oidcTokenManager.TryGetAccessToken(oidcProvider, cancellationToken);
				}
				catch (Exception ex)
				{
					_logger.LogTrace(ex, "Unable to get access token; attempting login: {Message}", ex.Message);
				}
			}
			if (result == null && interactive)
			{
				_logger.LogInformation("Logging in to {Server}...", serverUrl);
				result = await oidcTokenManager.Login(oidcProvider, cancellationToken);
			}

			return new AuthState(authConfig.Method, result, interactive);
		}
	}
}
