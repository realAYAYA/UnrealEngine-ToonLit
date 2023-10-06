// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;

namespace Horde.Server.Secrets
{
	/// <summary>
	/// Interface for a service that can provide secret data
	/// </summary>
	public interface ISecretProvider
	{
		/// <summary>
		/// Name of this provider
		/// </summary>
		string Name { get; }

		/// <summary>
		/// Gets values for a particular secret
		/// </summary>
		/// <param name="config">Configuration for the secret to fetch</param>
		/// <param name="user">User requesting the data</param>
		/// <param name="data">Dictionary to receive any enumerated properties</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task GetValuesAsync(ExternalSecretConfig config, ClaimsPrincipal user, Dictionary<string, string> data, CancellationToken cancellationToken);
	}
}
