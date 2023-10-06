// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net.Http.Headers;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using Microsoft.Extensions.Logging;

namespace Horde.Commands
{
	/// <summary>
	/// Base class for commands that require a configured storage client
	/// </summary>
	abstract class StorageCommandBase : Command
	{
		/// <summary>
		/// Namespace to use
		/// </summary>
		[CommandLine("-Namespace=", Description = "Namespace for data to manipulate")]
		public string Namespace { get; set; } = "default";

		/// <summary>
		/// Base URI to upload to
		/// </summary>
		[CommandLine("-Path=", Description = "Relative path on the server for the store to write to/from (eg. api/v1/storage/default)")]
		public string? Path { get; set; }

		/// <summary>
		/// Creates a new client instance
		/// </summary>
		/// <param name="logger">Logger for output messages</param>
		/// <param name="cancellationToken"></param>
		public async Task<IStorageClient> CreateStorageClientAsync(ILogger logger, CancellationToken cancellationToken = default)
		{
			Uri? server = await Settings.GetServerAsync(cancellationToken);
			if (server == null)
			{
				throw new Exception("No server is configured. Run 'horde login -server=...' to set up.");
			}

			string? token = await Settings.GetAccessTokenAsync(logger, cancellationToken);
			if (token == null)
			{
				throw new Exception("Unable to log in to server.");
			}

			if (String.IsNullOrEmpty(Path))
			{
				Path = $"api/v1/storage/{Namespace}/";
			}
			else if(!Path.EndsWith("/", StringComparison.Ordinal))
			{
				Path += "/";
			}

			server = new Uri(server, Path);
			return new HttpStorageClient(() => CreateDefaultHttpClient(server, token), () => new HttpClient(), null, logger);
		}

		static HttpClient CreateDefaultHttpClient(Uri server, string token)
		{
			HttpClient client = new HttpClient();
			client.BaseAddress = server;
			client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", token);
			return client;
		}
	}
}
