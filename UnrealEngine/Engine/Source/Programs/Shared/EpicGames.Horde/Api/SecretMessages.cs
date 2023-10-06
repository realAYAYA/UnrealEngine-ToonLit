// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;

#pragma warning disable CA2227 // Collection properties should be read only

namespace EpicGames.Horde.Api
{
	/// <summary>
	/// Response listing all the secrets available to the current user
	/// </summary>
	public class GetSecretsResponse
	{
		/// <summary>
		/// List of secret ids
		/// </summary>
		public List<SecretId> Ids { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetSecretsResponse(List<SecretId> ids) => Ids = ids;
	}

	/// <summary>
	/// Gets data for a particular secret
	/// </summary>
	[DebuggerDisplay("{Id}")]
	public class GetSecretResponse
	{
		/// <summary>
		/// Id of the secret
		/// </summary>
		public SecretId Id { get; }

		/// <summary>
		/// Key value pairs for the secret
		/// </summary>
		public Dictionary<string, string> Data { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetSecretResponse(SecretId id, Dictionary<string, string> data)
		{
			Id = id;
			Data = data;
		}
	}

	/// <summary>
	/// Extension methods for the secrets endpoint
	/// </summary>
	public static class SecretExtensions
	{
		/// <summary>
		/// Query all the secrets available to the current user
		/// </summary>
		/// <param name="horde">The horde client instance</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the projects</returns>
		public static Task<GetSecretsResponse> GetSecretsAsync(this HordeHttpClient horde, CancellationToken cancellationToken = default)
		{
			return horde.GetAsync<GetSecretsResponse>($"api/v1/secrets", cancellationToken);
		}

		/// <summary>
		/// Retrieve information about a specific project
		/// </summary>
		/// <param name="horde">The horde client instance</param>
		/// <param name="secretId">Id of the secret to retrieve</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about the requested project</returns>
		public static Task<GetSecretResponse> GetSecretAsync(this HordeHttpClient horde, SecretId secretId, CancellationToken cancellationToken = default)
		{
			return horde.GetAsync<GetSecretResponse>($"api/v1/secrets/{secretId}", cancellationToken);
		}
	}
}
