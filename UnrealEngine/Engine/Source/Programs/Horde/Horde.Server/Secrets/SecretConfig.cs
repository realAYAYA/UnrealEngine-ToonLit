// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Security.Claims;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Secrets;
using Horde.Server.Acls;
using Horde.Server.Server;

namespace Horde.Server.Secrets
{
	/// <summary>
	/// Configuration for a secret value
	/// </summary>
	public class SecretConfig
	{
		/// <summary>
		/// Identifier for this secret
		/// </summary>
		public SecretId Id { get; set; }

		/// <summary>
		/// Key/value pairs associated with this secret
		/// </summary>
		public Dictionary<string, string> Data { get; set; } = new Dictionary<string, string>();

		/// <summary>
		/// Providers to source key/value pairs from
		/// </summary>
		public List<ExternalSecretConfig> Sources { get; set; } = new List<ExternalSecretConfig>();

		/// <summary>
		/// Defines access to this particular secret
		/// </summary>
		public AclConfig Acl { get; set; } = new AclConfig();

		/// <summary>
		/// Called after the config has been read
		/// </summary>
		/// <param name="globalConfig">Parent GlobalConfig object</param>
		public void PostLoad(GlobalConfig globalConfig)
		{
			Acl.PostLoad(globalConfig.Acl, $"secret:{Id}");
		}

		/// <summary>
		/// Authorizes a user to perform a given action
		/// </summary>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to validate</param>
		public bool Authorize(AclAction action, ClaimsPrincipal user)
			=> Acl.Authorize(action, user);
	}

	/// <summary>
	/// Configuration for an external secret provider
	/// </summary>
	public class ExternalSecretConfig
	{
		/// <summary>
		/// Name of the provider to use
		/// </summary>
		public string Provider { get; set; } = String.Empty;

		/// <summary>
		/// Optional key indicating the parameter to set in the resulting data array
		/// </summary>
		public string? Key { get; set; }

		/// <summary>
		/// Optional value indicating what to fetch from the provider
		/// </summary>
		public string? Path { get; set; }

		/// <summary>
		/// Additional provider-specific arguments
		/// </summary>
		public Dictionary<string, string>? Arguments { get; set; }
	}
}
