// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Horde.Secrets;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Server.Secrets
{
	/// <summary>
	/// Controller for the /api/v1/credentials endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class SecretsController : HordeControllerBase
	{
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public SecretsController(IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Query all the secrets available for the current user
		/// </summary>
		[HttpGet]
		[Route("/api/v1/secrets")]
		public ActionResult<GetSecretsResponse> GetSecrets()
		{
			List<SecretId> secretIds = new List<SecretId>();
			foreach (SecretConfig secret in _globalConfig.Value.Secrets)
			{
				if (secret.Authorize(SecretAclAction.ViewSecret, User))
				{
					secretIds.Add(secret.Id);
				}
			}
			return new GetSecretsResponse(secretIds);
		}

		/// <summary>
		/// Retrieve information about a specific secret
		/// </summary>
		/// <param name="secretId">Id of the secret to retrieve</param>
		/// <param name="filter">Filter for properties to return</param>
		/// <returns>Information about the requested secret</returns>
		[HttpGet]
		[Route("/api/v1/secrets/{secretId}")]
		[ProducesResponseType(typeof(GetSecretResponse), 200)]
		public ActionResult<object> GetSecret(SecretId secretId, [FromQuery] PropertyFilter? filter = null)
		{
			if (!_globalConfig.Value.TryGetSecret(secretId, out SecretConfig? secretConfig))
			{
				return NotFound(secretId);
			}
			if (!secretConfig.Authorize(SecretAclAction.ViewSecret, User))
			{
				return Forbid(SecretAclAction.ViewSecret, secretId);
			}

			return new GetSecretResponse(secretConfig.Id, secretConfig.Data).ApplyFilter(filter);
		}
	}
}
