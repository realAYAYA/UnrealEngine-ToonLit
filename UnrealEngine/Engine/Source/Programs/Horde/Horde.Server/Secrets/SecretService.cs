// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Secrets
{
	/// <summary>
	/// Service for retrieving secrets
	/// </summary>
	public class SecretService
	{
		readonly Dictionary<string, ISecretProvider> _providers;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public SecretService(IEnumerable<ISecretProvider> providers, ILogger<SecretService> logger)
		{
			_providers = providers.ToDictionary(x => x.Name, x => x);
			_logger = logger;
		}

		/// <summary>
		/// Resolve a secret to concrete values
		/// </summary>
		/// <param name="config">Configuration for the secret</param>
		/// <param name="user">User requesting the secret</param>
		/// <param name="data">The resolved secret values</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task<bool> ResolveAsync(SecretConfig config, ClaimsPrincipal user, Dictionary<string, string> data, CancellationToken cancellationToken)
		{
			// Add the hard-coded secrets
			foreach ((string key, string value) in config.Data)
			{
				data.Add(key, value);
			}

			// Fetch all the secrets from external providers
			foreach (ExternalSecretConfig source in config.Sources)
			{
				ISecretProvider? provider;
				if (!_providers.TryGetValue(source.Provider, out provider))
				{
					_logger.LogError("Unknown secret provider provider {Provider} for secret {SecretId}", source.Provider, config.Id);
					return false;
				}

				try
				{
					await provider.GetValuesAsync(source, user, data, cancellationToken);
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while fetching secret {SecretId} from {Provider}: {Message}", config.Id, source.Provider, ex.Message);
					return false;
				}
			}

			return true;
		}
	}
}
