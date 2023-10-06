// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Serialization;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using StackExchange.Redis;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Horde.Server.Tools
{
	/// <summary>
	/// Collection of tool documents
	/// </summary>
	public class ToolCollection : IToolCollection
	{
		class Tool : VersionedDocument<ToolId, Tool>, ITool
		{
			[BsonIgnore]
			public ToolConfig Config { get; set; } = null!;

			[BsonElement("dep")]
			public List<ToolDeployment> Deployments { get; set; } = new List<ToolDeployment>();

			// ITool interface
			IReadOnlyList<IToolDeployment> ITool.Deployments => Deployments;

			[BsonConstructor]
			public Tool(ToolId id)
				: base(id)
			{
				Config = null!;
			}

			public Tool(ToolConfig config)
				: base(config.Id)
			{
				Config = config;
			}

			/// <inheritdoc/>
			public override Tool UpgradeToLatest() => this;

			public void UpdateTemporalState(DateTime utcNow)
			{
				foreach (ToolDeployment deployment in Deployments)
				{
					deployment.UpdateTemporalState(utcNow);
				}
			}
		}

		class ToolDeployment : IToolDeployment
		{
			public ToolDeploymentId Id { get; set; }

			[BsonElement("ver")]
			public string Version { get; set; }

			[BsonIgnore]
			public ToolDeploymentState State { get; set; }

			[BsonIgnore]
			public double Progress { get; set; }

			[BsonElement("bpr")]
			public double BaseProgress { get; set; }

			[BsonElement("stm")]
			public DateTime? StartedAt { get; set; }

			[BsonElement("dur")]
			public TimeSpan Duration { get; set; }

			[BsonElement("ref")]
			public RefName RefName { get; set; }

			public ToolDeployment(ToolDeploymentId id)
			{
				Id = id;
				Version = String.Empty;
			}

			public ToolDeployment(ToolDeploymentId id, ToolDeploymentConfig options, RefName refName)
			{
				Id = id;
				Version = options.Version;
				Duration = options.Duration;
				RefName = refName;
			}

			public void UpdateTemporalState(DateTime utcNow)
			{
				if (BaseProgress >= 1.0)
				{
					State = ToolDeploymentState.Complete;
					Progress = 1.0;
				}
				else if (StartedAt == null)
				{
					State = ToolDeploymentState.Paused;
					Progress = BaseProgress;
				}
				else if (Duration > TimeSpan.Zero)
				{
					State = ToolDeploymentState.Active;
					Progress = Math.Clamp((utcNow - StartedAt.Value) / Duration, 0.0, 1.0);
				}
				else
				{
					State = ToolDeploymentState.Complete;
					Progress = 1.0;
				}
			}
		}

		private class ToolDeploymentData
		{
			[CbField]
			public ToolDeploymentId Id { get; set; }

			[CbField("version")]
			public string Version { get; set; } = String.Empty;

			[CbField("data")]
			public CbBinaryAttachment Data { get; set; }
		}

		private class CachedIndex
		{
			[CbField("rev")]
			public string Rev { get; set; } = String.Empty;

			[CbField("empty")]
			public bool Empty { get; set; }

			[CbField("ids")]
			public List<ToolId> Ids { get; set; } = new List<ToolId>();
		}

		private readonly VersionedCollection<ToolId, Tool> _tools;
		private readonly StorageService _storageService;
		private readonly IClock _clock;
		private readonly IMemoryCache _cache;
		private readonly ILogger _logger;

		private static readonly RedisKey s_baseKey = "tools/v1/";

		private static readonly IReadOnlyDictionary<int, Type> s_types = RegisterTypes();

		/// <summary>
		/// Constructor
		/// </summary>
		public ToolCollection(MongoService mongoService, RedisService redisService, StorageService storageService, IMemoryCache cache, IClock clock, ILogger<ToolCollection> logger)
		{
			_tools = new VersionedCollection<ToolId, Tool>(mongoService, "Tools", redisService, s_baseKey, s_types);
			_storageService = storageService;
			_clock = clock;
			_cache = cache;
			_logger = logger;
		}

		/// <summary>
		/// Registers types required for this collection
		/// </summary>
		/// <returns></returns>
		static IReadOnlyDictionary<int, Type> RegisterTypes()
		{
			Dictionary<int, Type> versionToType = new Dictionary<int, Type>();

			versionToType[1] = typeof(Tool);
			BsonClassMap.RegisterClassMap<Tool>(cm =>
			{
				cm.AutoMap();
				cm.MapCreator(t => new Tool(t.Id));
			});

			return versionToType;
		}

		/// <summary>
		/// Gets a tool with the given identifier
		/// </summary>
		/// <param name="id">The tool identifier</param>
		/// <param name="globalConfig">The current global configuration</param>
		/// <returns></returns>
		public async Task<ITool?> GetAsync(ToolId id, GlobalConfig globalConfig) => await GetInternalAsync(id, globalConfig);

		/// <summary>
		/// Gets a tool with the given identifier
		/// </summary>
		/// <param name="toolId">The tool identifier</param>
		/// <param name="globalConfig">The current global configuration</param>
		/// <returns></returns>
		async Task<Tool?> GetInternalAsync(ToolId toolId, GlobalConfig globalConfig)
		{
			ToolConfig? toolConfig;
			if (globalConfig.TryGetTool(toolId, out toolConfig))
			{
				Tool tool = await _tools.FindOrAddAsync(toolId, () => new Tool(toolId));
				tool.Config = toolConfig;
				tool.UpdateTemporalState(_clock.UtcNow);
				return tool;
			}

			BundledToolConfig? bundledToolConfig;
			if (globalConfig.ServerSettings.TryGetBundledTool(toolId, out bundledToolConfig))
			{
				Tool tool = new Tool(bundledToolConfig);

				ToolDeployment deployment = new ToolDeployment(default);
				deployment.Version = bundledToolConfig.Version;
				deployment.State = ToolDeploymentState.Complete;
				deployment.RefName = bundledToolConfig.RefName;
				tool.Deployments.Add(deployment);

				return tool;
			}

			return null;
		}

		/// <summary>
		/// Adds a new deployment to the given tool. The new deployment will replace the current active deployment.
		/// </summary>
		/// <param name="tool">The tool to update</param>
		/// <param name="options">Options for the new deployment</param>
		/// <param name="stream">Stream containing the tool data</param>
		/// <param name="globalConfig">The current configuration</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Updated tool document, or null if it does not exist</returns>
		public async Task<ITool?> CreateDeploymentAsync(ITool tool, ToolDeploymentConfig options, Stream stream, GlobalConfig globalConfig, CancellationToken cancellationToken)
		{
			ToolDeploymentId deploymentId = ToolDeploymentId.GenerateNewId();
			RefName refName = new RefName($"{tool.Id}/{deploymentId}");

			IStorageClient client = await _storageService.GetClientAsync(Namespace.Tools, cancellationToken);

			NodeRef<DirectoryNode> nodeRef;
			await using (IStorageWriter writer = client.CreateWriter(refName))
			{
				DirectoryNode directoryNode = new DirectoryNode();
				await directoryNode.CopyFromZipStreamAsync(stream, writer, new ChunkingOptions(), cancellationToken);
				nodeRef = await writer.WriteNodeAsync(directoryNode, cancellationToken);
			}

			BlobHandle handle = nodeRef.Handle;
			await client.WriteRefTargetAsync(refName, handle, cancellationToken: cancellationToken);

			return await CreateDeploymentAsync(tool, options, handle.GetLocator(), globalConfig, cancellationToken);
		}

		/// <summary>
		/// Adds a new deployment to the given tool. The new deployment will replace the current active deployment.
		/// </summary>
		/// <param name="tool">The tool to update</param>
		/// <param name="options">Options for the new deployment</param>
		/// <param name="locator">Locator for the tool data</param>
		/// <param name="globalConfig">The current configuration</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Updated tool document, or null if it does not exist</returns>
		public async Task<ITool?> CreateDeploymentAsync(ITool tool, ToolDeploymentConfig options, NodeLocator locator, GlobalConfig globalConfig, CancellationToken cancellationToken)
		{
			ToolDeploymentId deploymentId = ToolDeploymentId.GenerateNewId();
			RefName refName = new RefName($"{tool.Id}/{deploymentId}");

			IStorageClientImpl client = await _storageService.GetClientAsync(Namespace.Tools, cancellationToken);
			await client.WriteRefTargetAsync(refName, locator, cancellationToken: cancellationToken);

			return await CreateDeploymentInternalAsync(tool, deploymentId, options, refName, globalConfig, cancellationToken);
		}

		async Task<ITool?> CreateDeploymentInternalAsync(ITool tool, ToolDeploymentId deploymentId, ToolDeploymentConfig options, RefName refName, GlobalConfig globalConfig, CancellationToken cancellationToken)
		{
			if (tool.Config is BundledToolConfig)
			{
				throw new InvalidOperationException("Cannot update the state of bundled tools.");
			}

			// Create the new deployment object
			ToolDeployment deployment = new ToolDeployment(deploymentId, options, refName);

			// Start the deployment
			DateTime utcNow = _clock.UtcNow;
			if (!options.CreatePaused)
			{
				deployment.StartedAt = utcNow;
			}

			// Create the deployment
			Tool? newTool = (Tool)tool;
			for (; ; )
			{
				newTool = await TryAddDeploymentAsync(newTool, deployment, cancellationToken);
				if (newTool != null)
				{
					break;
				}

				newTool = await GetInternalAsync(tool.Id, globalConfig);
				if (newTool == null)
				{
					return null;
				}
			}

			// Return the new tool with updated deployment states
			newTool.UpdateTemporalState(utcNow);
			return newTool;
		}

		async ValueTask<Tool?> TryAddDeploymentAsync(Tool tool, ToolDeployment deployment, CancellationToken cancellationToken)
		{
			Tool? newTool = tool;

			// If there are already a maximum number of deployments, remove the oldest one
			const int MaxDeploymentCount = 5;
			while (newTool.Deployments.Count >= MaxDeploymentCount)
			{
				newTool = await _tools.UpdateAsync(newTool, Builders<Tool>.Update.PopFirst(x => x.Deployments));
				if (newTool == null)
				{
					return null;
				}

				IStorageClient client = await _storageService.GetClientAsync(Namespace.Tools, cancellationToken);
				await client.DeleteRefAsync(tool.Deployments[0].RefName, cancellationToken);
			}

			// Add the new deployment
			return await _tools.UpdateAsync(newTool, Builders<Tool>.Update.Push(x => x.Deployments, deployment));
		}

		/// <summary>
		/// Updates the state of the current deployment
		/// </summary>
		/// <param name="tool">Tool to be updated</param>
		/// <param name="deploymentId">Identifier for the deployment to modify</param>
		/// <param name="action">New state of the deployment</param>
		/// <returns></returns>
		public async Task<ITool?> UpdateDeploymentAsync(ITool tool, ToolDeploymentId deploymentId, ToolDeploymentState action)
		{
			if (tool.Config is BundledToolConfig)
			{
				throw new InvalidOperationException("Cannot update the state of bundled tools.");
			}

			return await UpdateDeploymentInternalAsync((Tool)tool, deploymentId, action);
		}

		async Task<Tool?> UpdateDeploymentInternalAsync(Tool tool, ToolDeploymentId deploymentId, ToolDeploymentState action)
		{
			int idx = tool.Deployments.FindIndex(x => x.Id == deploymentId);
			if (idx == -1)
			{
				return null;
			}

			ToolDeployment deployment = tool.Deployments[idx];
			switch (action)
			{
				case ToolDeploymentState.Complete:
					return await _tools.UpdateAsync(tool, Builders<Tool>.Update.Set(x => x.Deployments[idx].BaseProgress, 1.0).Unset(x => x.Deployments[idx].StartedAt));

				case ToolDeploymentState.Cancelled:
					List<ToolDeployment> newDeployments = tool.Deployments.Where(x => x != deployment).ToList();
					return await _tools.UpdateAsync(tool, Builders<Tool>.Update.Set(x => x.Deployments, newDeployments));

				case ToolDeploymentState.Paused:
					if (deployment.StartedAt == null)
					{
						return tool;
					}
					else
					{
						return await _tools.UpdateAsync(tool, Builders<Tool>.Update.Set(x => x.Deployments[idx].BaseProgress, deployment.GetProgressValue(_clock.UtcNow)).Set(x => x.Deployments[idx].StartedAt, null));
					}

				case ToolDeploymentState.Active:
					if (deployment.StartedAt != null)
					{
						return tool;
					}
					else
					{
						return await _tools.UpdateAsync(tool, Builders<Tool>.Update.Set(x => x.Deployments[idx].StartedAt, _clock.UtcNow));
					}

				default:
					throw new ArgumentException("Invalid action for deployment", nameof(action));
			}
		}

		/// <summary>
		/// Gets the storage client containing data for a particular tool
		/// </summary>
		/// <param name="tool">Identifier for the tool</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Storage client for the data</returns>
		public async Task<IStorageClient> GetStorageClientAsync(ITool tool, CancellationToken cancellationToken)
		{
			if (tool.Config is BundledToolConfig bundledConfig)
			{
				return new FileStorageClient(DirectoryReference.Combine(Program.AppDir, bundledConfig.DataDir ?? $"tools/{tool.Id}"), _logger);
			}
			else
			{
				return await _storageService.GetClientAsync(Namespace.Tools, cancellationToken);
			}
		}

		/// <summary>
		/// Opens a stream to the data for a particular deployment
		/// </summary>
		/// <param name="tool">Identifier for the tool</param>
		/// <param name="deployment">The deployment</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream for the data</returns>
		public async Task<Stream> GetDeploymentZipAsync(ITool tool, IToolDeployment deployment, CancellationToken cancellationToken)
		{
			IStorageClient client = await GetStorageClientAsync(tool, cancellationToken);

			DirectoryNode node = await client.ReadNodeAsync<DirectoryNode>(deployment.RefName, DateTime.UtcNow - TimeSpan.FromDays(2.0), cancellationToken);

			return node.AsZipStream();
		}
	}
}
