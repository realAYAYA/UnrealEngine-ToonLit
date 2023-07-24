// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Security.Claims;
using System.Text.Json.Serialization;
using EpicGames.Horde.Storage;
using Horde.Build.Acls;
using Horde.Build.Server;
using Horde.Build.Storage.Backends;
using Horde.Build.Utilities;

namespace Horde.Build.Storage
{
	using BackendId = StringId<IStorageBackend>;

	/// <summary>
	/// Well-known namespace identifiers
	/// </summary>
	public static class Namespace
	{
		/// <summary>
		/// Storage of artifacts 
		/// </summary>
		public static NamespaceId Artifacts { get; } = new NamespaceId("horde-artifacts");

		/// <summary>
		/// Replicated Perforce data
		/// </summary>
		public static NamespaceId Perforce { get; } = new NamespaceId("horde-perforce");

		/// <summary>
		/// Log data
		/// </summary>
		public static NamespaceId Logs { get; } = new NamespaceId("horde-logs");

		/// <summary>
		/// Storage of tool data
		/// </summary>
		public static NamespaceId Tools { get; } = new NamespaceId("horde-tools");
	}

	/// <summary>
	/// Configuration for storage
	/// </summary>
	public class StorageConfig
	{
		/// <summary>
		/// List of storage backends
		/// </summary>
		public List<BackendConfig> Backends { get; set; } = new List<BackendConfig>();

		/// <summary>
		/// List of namespaces for storage
		/// </summary>
		public List<NamespaceConfig> Namespaces { get; set; } = new List<NamespaceConfig>();

		private readonly Dictionary<BackendId, BackendConfig> _backendLookup = new Dictionary<BackendId, BackendConfig>();
		private readonly Dictionary<NamespaceId, NamespaceConfig> _namespaceLookup = new Dictionary<NamespaceId, NamespaceConfig>();

		/// <summary>
		/// Called after the config has been read from disk
		/// </summary>
		internal void PostLoad(GlobalConfig globalConfig)
		{
			foreach (BackendConfig backendConfig in Backends)
			{
				_backendLookup.Add(backendConfig.Id, backendConfig);
			}
			foreach (NamespaceConfig namespaceConfig in Namespaces)
			{
				namespaceConfig.PostLoad(globalConfig);
				_namespaceLookup.Add(namespaceConfig.Id, namespaceConfig);
			}
		}

		/// <summary>
		/// Gets a backend with the given id
		/// </summary>
		/// <param name="backendId">Identifier for the backend</param>
		/// <param name="backendConfig">Receives the backend config on success</param>
		/// <returns>True on success</returns>
		public bool TryGetBackend(BackendId backendId, [NotNullWhen(true)] out BackendConfig? backendConfig) => _backendLookup.TryGetValue(backendId, out backendConfig);

		/// <summary>
		/// Gets a namespace with the given id
		/// </summary>
		/// <param name="namespaceId">Identifier for the backend</param>
		/// <param name="namespaceConfig">Receives the backend config on success</param>
		/// <returns>True on success</returns>
		public bool TryGetNamespace(NamespaceId namespaceId, [NotNullWhen(true)] out NamespaceConfig? namespaceConfig) => _namespaceLookup.TryGetValue(namespaceId, out namespaceConfig);
	}

	/// <summary>
	/// Common settings object for different providers
	/// </summary>
	[DebuggerDisplay("{Id}")]
	public class BackendConfig : IStorageBackendOptions
	{
		/// <summary>
		/// The storage backend ID
		/// </summary>
		[Required]
		public BackendId Id { get; set; }

		/// <summary>
		/// Base backend to copy default settings from
		/// </summary>
		public BackendId Base { get; set; }

		/// <inheritdoc/>
		public StorageBackendType? Type { get; set; }

		/// <inheritdoc/>
		public string? BaseDir { get; set; }

		/// <inheritdoc/>
		public string? AwsBucketName { get; set; }

		/// <inheritdoc/>
		public string? AwsBucketPath { get; set; }

		/// <inheritdoc/>
		public AwsCredentialsType AwsCredentials { get; set; }

		/// <inheritdoc/>
		public string? AwsRole { get; set; }

		/// <inheritdoc/>
		public string? AwsProfile { get; set; }

		/// <inheritdoc/>
		public string? AwsRegion { get; set; }

		/// <inheritdoc/>
		public string? RelayServer { get; set; }

		/// <inheritdoc/>
		public string? RelayToken { get; set; }
	}

	/// <summary>
	/// Configuration of a particular namespace
	/// </summary>
	[DebuggerDisplay("{Id}")]
	public class NamespaceConfig
	{
		/// <summary>
		/// Owner of this config object
		/// </summary>
		[JsonIgnore]
		public GlobalConfig GlobalConfig { get; private set; } = null!;

		/// <summary>
		/// Identifier for this namespace
		/// </summary>
		[Required]
		public NamespaceId Id { get; set; }

		/// <summary>
		/// Backend to use for this namespace
		/// </summary>
		[Required]
		public BackendId Backend { get; set; }

		/// <summary>
		/// Prefix for items within this namespace
		/// </summary>
		public string Prefix { get; set; } = String.Empty;

		/// <summary>
		/// How frequently to run garbage collection, in hours.
		/// </summary>
		public double GcFrequencyHrs { get; set; } = 2.0;

		/// <summary>
		/// How long to keep newly uploaded orphanned objects before allowing them to be deleted, in hours.
		/// </summary>
		public double GcDelayHrs { get; set; } = 6.0;

		/// <summary>
		/// Support querying exports by their aliases
		/// </summary>
		public bool EnableAliases { get; set; }

		/// <summary>
		/// Access list for this namespace
		/// </summary>
		public AclConfig Acl { get; set; } = new AclConfig();

		/// <summary>
		/// Callback once the configuration has been read from disk
		/// </summary>
		/// <param name="globalConfig"></param>
		public void PostLoad(GlobalConfig globalConfig)
		{
			GlobalConfig = globalConfig;
		}

		/// <summary>
		/// Authorizes a user to perform a given action
		/// </summary>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to validate</param>
		public bool Authorize(AclAction action, ClaimsPrincipal user)
		{
			return Acl?.Authorize(action, user) ?? GlobalConfig.Authorize(action, user);
		}
	}
}
