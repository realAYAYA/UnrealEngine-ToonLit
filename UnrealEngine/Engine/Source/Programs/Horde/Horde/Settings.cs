// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.Json;
using EpicGames.Core;
using EpicGames.OIDC;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;

namespace Horde
{
	/// <summary>
	/// Utility methods for CLI settings
	/// </summary>
	static class Settings
	{
		class SettingsData
		{
			public Uri Server { get; set; } = new Uri("http://localhost:5000/");
		}

		class GetAuthConfigResponse
		{
			public string? Method { get; set; }
			public string? ServerUrl { get; set; }
			public string? ClientId { get; set; }
			public string[]? LocalRedirectUrls { get; set; }
		}

		static FileReference GetSettingsFile()
		{
			DirectoryReference? baseDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile);
			baseDir ??= DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData);
			baseDir ??= DirectoryReference.GetCurrentDirectory();
			return FileReference.Combine(baseDir, ".horde.json");
		}

		static SettingsData? s_settings;

		static async Task<SettingsData> ReadSettingsAsync(CancellationToken cancellationToken)
		{
			if (s_settings == null)
			{
				FileReference file = GetSettingsFile();
				if (FileReference.Exists(file))
				{
					byte[] data = await FileReference.ReadAllBytesAsync(GetSettingsFile(), cancellationToken);
					s_settings = JsonSerializer.Deserialize<SettingsData>(data, new JsonSerializerOptions { AllowTrailingCommas = true, PropertyNameCaseInsensitive = true, ReadCommentHandling = JsonCommentHandling.Skip });
				}
				s_settings ??= new SettingsData();
			}
			return s_settings;
		}

		static async Task WriteSettingsAsync(CancellationToken cancellationToken)
		{
			if (s_settings != null)
			{
				byte[] data = JsonSerializer.SerializeToUtf8Bytes(s_settings, new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase, WriteIndented = true });

				FileReference file = GetSettingsFile();
				DirectoryReference.CreateDirectory(file.Directory);

				await FileReference.WriteAllBytesAsync(file, data, cancellationToken);
			}
		}

		public static async Task SetServerAsync(string server, CancellationToken cancellationToken = default)
		{
			if (!server.EndsWith("/", StringComparison.Ordinal))
			{
				server += "/";
			}

			SettingsData settings = await ReadSettingsAsync(cancellationToken);
			settings.Server = new Uri(server);
			await WriteSettingsAsync(cancellationToken);
		}

		public static async Task<Uri?> GetServerAsync(CancellationToken cancellationToken = default)
		{
			SettingsData settings = await ReadSettingsAsync(cancellationToken);
			return settings.Server;
		}

		public static async Task<string?> GetAccessTokenAsync(ILogger logger, CancellationToken cancellationToken = default)
		{
			SettingsData settings = await ReadSettingsAsync(cancellationToken);
			if (settings.Server == null)
			{
				throw new InvalidOperationException("No Horde server is configured. Run 'horde login -server=...' to configure.");
			}

			logger.LogInformation("Getting access token for {Server}", settings.Server);

			GetAuthConfigResponse authConfig;
			using (HttpClient httpClient = new HttpClient())
			{
				Uri uri = new Uri(settings.Server, "api/v1/server/auth");
				authConfig = await httpClient.GetAsync<GetAuthConfigResponse>(uri, cancellationToken);
			}

			if (String.Equals(authConfig.Method, "Anonymous", StringComparison.OrdinalIgnoreCase))
			{
				return null;
			}
				
			const string OidcProvider = "Horde";

			Dictionary<string, string?> values = new Dictionary<string, string?>();
			values[$"Providers:{OidcProvider}:DisplayName"] = "Horde";
			values[$"Providers:{OidcProvider}:ServerUri"] = authConfig.ServerUrl;
			values[$"Providers:{OidcProvider}:ClientId"] = authConfig.ClientId;
			values[$"Providers:{OidcProvider}:RedirectUri"] = authConfig.LocalRedirectUrls?.FirstOrDefault();

			ConfigurationBuilder builder = new ConfigurationBuilder();
			builder.AddInMemoryCollection(values);

			IConfiguration configuration = builder.Build();

			using ITokenStore tokenStore = TokenStoreFactory.CreateTokenStore();
			OidcTokenManager oidcTokenManager = OidcTokenManager.CreateTokenManager(configuration, tokenStore, new List<string>() { OidcProvider });

			OidcTokenInfo result;
			try
			{
				result = await oidcTokenManager.GetAccessToken(OidcProvider, cancellationToken);
			}
			catch (NotLoggedInException)
			{
				result = await oidcTokenManager.Login(OidcProvider, cancellationToken);
			}

			if (result.AccessToken == null)
			{
				throw new Exception($"Unable to get access token for {settings.Server}");
			}

			logger.LogInformation("Received bearer token for {Server}", settings.Server);
			return result.AccessToken;
		}
	}
}
