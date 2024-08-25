// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Horde.Storage.ObjectStores;
using EpicGames.Horde.Tools;
using EpicGames.Serialization;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Tools
{
	/// <summary>
	/// Collection of tool documents
	/// </summary>
	public class ToolCollection : IToolCollection
	{
		class Tool : ITool
		{
			public ToolId Id { get; set; }

			[BsonIgnore]
			public ToolConfig Config { get; set; } = null!;

			[BsonElement("dep")]
			public List<ToolDeployment> Deployments { get; set; } = new List<ToolDeployment>();

			// Last time that the document was updated. This field is checked and updated as part of updates to ensure atomicity.
			[BsonElement("_u")]
			public DateTime LastUpdateTime { get; set; }

			// ITool interface
			IReadOnlyList<IToolDeployment> ITool.Deployments => Deployments;

			[BsonConstructor]
			public Tool(ToolId id)
			{
				Id = id;
				Config = null!;
			}

			public Tool(ToolConfig config)
			{
				Id = config.Id;
				Config = config;
			}

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

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; } = Namespace.Tools;

			[BsonElement("ref")]
			public RefName RefName { get; set; }

			[BsonConstructor]
			public ToolDeployment(ToolDeploymentId id)
			{
				Id = id;
				Version = String.Empty;
			}

			public ToolDeployment(ToolDeploymentId id, ToolDeploymentConfig options, NamespaceId namespaceId, RefName refName)
			{
				Id = id;
				Version = options.Version;
				Duration = options.Duration;
				NamespaceId = namespaceId;
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

		private readonly IMongoCollection<Tool> _tools;
		private readonly StorageService _storageService;
		private readonly FileObjectStoreFactory _fileObjectStoreFactory;
		private readonly IClock _clock;
		private readonly BundleCache _cache;
		private readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ToolCollection(MongoService mongoService, StorageService storageService, BundleCache cache, FileObjectStoreFactory fileObjectStoreFactory, IClock clock, ILogger<ToolCollection> logger)
		{
			_tools = mongoService.GetCollection<Tool>("Tools");
			_storageService = storageService;
			_fileObjectStoreFactory = fileObjectStoreFactory;
			_clock = clock;
			_cache = cache;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task<ITool?> GetAsync(ToolId id, GlobalConfig globalConfig, CancellationToken cancellationToken)
			=> await GetInternalAsync(id, globalConfig, cancellationToken);

		/// <summary>
		/// Gets a tool with the given identifier
		/// </summary>
		/// <param name="toolId">The tool identifier</param>
		/// <param name="globalConfig">The current global configuration</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		async Task<Tool?> GetInternalAsync(ToolId toolId, GlobalConfig globalConfig, CancellationToken cancellationToken)
		{
			ToolConfig? toolConfig;
			if (globalConfig.TryGetTool(toolId, out toolConfig))
			{
				Tool? tool;
				for (; ; )
				{
					tool = await _tools.Find(x => x.Id == toolId).FirstOrDefaultAsync(cancellationToken);
					if (tool != null)
					{
						break;
					}

					tool = new Tool(toolId);
					if (await _tools.InsertOneIgnoreDuplicatesAsync(tool, cancellationToken))
					{
						break;
					}
				}

				tool.Config = toolConfig;
				tool.UpdateTemporalState(_clock.UtcNow);
				return tool;
			}

			BundledToolConfig? bundledToolConfig;
			if (globalConfig.ServerSettings.TryGetBundledTool(toolId, out bundledToolConfig))
			{
				Tool tool = new Tool(bundledToolConfig);

				ToolDeploymentId deploymentId = GetDeploymentId(bundledToolConfig);

				ToolDeployment deployment = new ToolDeployment(deploymentId);
				deployment.Version = bundledToolConfig.Version;
				deployment.State = ToolDeploymentState.Complete;
				deployment.RefName = bundledToolConfig.RefName;
				tool.Deployments.Add(deployment);

				return tool;
			}

			return null;
		}

		static ToolDeploymentId GetDeploymentId(BundledToolConfig bundledToolConfig)
		{
			// Create a pseudo-random deployment id from the tool id and version
			IoHash hash = IoHash.Compute(Encoding.UTF8.GetBytes($"{bundledToolConfig.Id}/{bundledToolConfig.Version}"));

			Span<byte> bytes = stackalloc byte[IoHash.NumBytes];
			hash.CopyTo(bytes);

			// Set a valid timestamp to allow it to appear as an ObjectId
			BinaryPrimitives.WriteInt32BigEndian(bytes, 1446492960);
			return new ToolDeploymentId(new BinaryId(bytes));
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
			ToolDeploymentId deploymentId = new ToolDeploymentId(BinaryIdUtils.CreateNew());

			using IStorageClient client = _storageService.CreateClient(tool.Config.NamespaceId);

			IBlobRef<DirectoryNode> nodeRef;
			await using (IBlobWriter writer = client.CreateBlobWriter($"{tool.Id}/{deploymentId}"))
			{
				DirectoryNode directoryNode = new DirectoryNode();
				await directoryNode.CopyFromZipStreamAsync(stream, writer, new ChunkingOptions(), cancellationToken: cancellationToken);
				nodeRef = await writer.WriteBlobAsync(directoryNode, cancellationToken: cancellationToken);
			}

			return await CreateDeploymentInternalAsync(tool, deploymentId, options, client, nodeRef, globalConfig, cancellationToken);
		}

		/// <summary>
		/// Adds a new deployment to the given tool. The new deployment will replace the current active deployment.
		/// </summary>
		/// <param name="tool">The tool to update</param>
		/// <param name="options">Options for the new deployment</param>
		/// <param name="target">Path to the tool data</param>
		/// <param name="globalConfig">The current configuration</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Updated tool document, or null if it does not exist</returns>
		public async Task<ITool?> CreateDeploymentAsync(ITool tool, ToolDeploymentConfig options, BlobRefValue target, GlobalConfig globalConfig, CancellationToken cancellationToken)
		{
			ToolDeploymentId deploymentId = new ToolDeploymentId(BinaryIdUtils.CreateNew());

			using IStorageClient client = _storageService.CreateClient(tool.Config.NamespaceId);
			return await CreateDeploymentInternalAsync(tool, deploymentId, options, client, client.CreateBlobRef(target), globalConfig, cancellationToken);
		}

		async Task<ITool?> CreateDeploymentInternalAsync(ITool tool, ToolDeploymentId deploymentId, ToolDeploymentConfig options, IStorageClient storageClient, IBlobRef content, GlobalConfig globalConfig, CancellationToken cancellationToken)
		{
			if (tool.Config is BundledToolConfig)
			{
				throw new InvalidOperationException("Cannot update the state of bundled tools.");
			}

			// Write a ref for the deployment so the blobs aren't GC'd
			RefName refName = new RefName($"{tool.Id}/{deploymentId}");
			await storageClient.WriteRefAsync(refName, content, cancellationToken: cancellationToken);

			// Create the new deployment object
			ToolDeployment deployment = new ToolDeployment(deploymentId, options, tool.Config.NamespaceId, refName);

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

				newTool = await GetInternalAsync(tool.Id, globalConfig, cancellationToken);
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
				newTool = await UpdateAsync(newTool, Builders<Tool>.Update.PopFirst(x => x.Deployments), cancellationToken);
				if (newTool == null)
				{
					return null;
				}

				ToolDeployment removeDeployment = tool.Deployments[0];
				using IStorageClient client = _storageService.CreateClient(removeDeployment.NamespaceId);
				await client.DeleteRefAsync(removeDeployment.RefName, cancellationToken);
			}

			// Add the new deployment
			return await UpdateAsync(newTool, Builders<Tool>.Update.Push(x => x.Deployments, deployment), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<ITool?> UpdateDeploymentAsync(ITool tool, ToolDeploymentId deploymentId, ToolDeploymentState action, CancellationToken cancellationToken)
		{
			if (tool.Config is BundledToolConfig)
			{
				throw new InvalidOperationException("Cannot update the state of bundled tools.");
			}

			return await UpdateDeploymentInternalAsync((Tool)tool, deploymentId, action, cancellationToken);
		}

		async Task<Tool?> UpdateDeploymentInternalAsync(Tool tool, ToolDeploymentId deploymentId, ToolDeploymentState action, CancellationToken cancellationToken)
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
					return await UpdateAsync(tool, Builders<Tool>.Update.Set(x => x.Deployments[idx].BaseProgress, 1.0).Unset(x => x.Deployments[idx].StartedAt), cancellationToken);

				case ToolDeploymentState.Cancelled:
					List<ToolDeployment> newDeployments = tool.Deployments.Where(x => x != deployment).ToList();
					return await UpdateAsync(tool, Builders<Tool>.Update.Set(x => x.Deployments, newDeployments), cancellationToken);

				case ToolDeploymentState.Paused:
					if (deployment.StartedAt == null)
					{
						return tool;
					}
					else
					{
						return await UpdateAsync(tool, Builders<Tool>.Update.Set(x => x.Deployments[idx].BaseProgress, deployment.GetProgressValue(_clock.UtcNow)).Set(x => x.Deployments[idx].StartedAt, null), cancellationToken);
					}

				case ToolDeploymentState.Active:
					if (deployment.StartedAt != null)
					{
						return tool;
					}
					else
					{
						return await UpdateAsync(tool, Builders<Tool>.Update.Set(x => x.Deployments[idx].StartedAt, _clock.UtcNow), cancellationToken);
					}

				default:
					throw new ArgumentException("Invalid action for deployment", nameof(action));
			}
		}

		/// <summary>
		/// Gets the storage client containing data for a particular tool
		/// </summary>
		/// <param name="tool">Identifier for the tool</param>
		/// <returns>Storage client for the data</returns>
		public IStorageClient CreateStorageClient(ITool tool)
		{
			if (tool.Config is BundledToolConfig bundledConfig)
			{
				return BundleStorageClient.CreateFromDirectory(DirectoryReference.Combine(ServerApp.AppDir, bundledConfig.DataDir ?? $"Tools"), _cache, _logger);
			}
			else
			{
				return _storageService.CreateClient(Namespace.Tools);
			}
		}

		/// <summary>
		/// Gets the storage client containing data for a particular tool
		/// </summary>
		/// <param name="tool">Identifier for the tool</param>
		/// <returns>Storage client for the data</returns>
		public IStorageBackend CreateStorageBackend(ITool tool)
		{
			if (tool.Config is BundledToolConfig bundledConfig)
			{
				return new FileStorageBackend(_fileObjectStoreFactory.CreateStore(DirectoryReference.Combine(ServerApp.AppDir, bundledConfig.DataDir ?? $"tools/{tool.Id}")), _logger);
			}
			else
			{
				return _storageService.CreateBackend(Namespace.Tools);
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
#pragma warning disable CA2000
			IStorageClient client = CreateStorageClient(tool);
			try
			{
				IBlobRef<DirectoryNode> nodeRef = await client.ReadRefAsync<DirectoryNode>(deployment.RefName, DateTime.UtcNow - TimeSpan.FromDays(2.0), cancellationToken: cancellationToken);
				return nodeRef.AsZipStream().WrapOwnership(client);
			}
			catch
			{
				client.Dispose();
				throw;
			}
#pragma warning restore CA2000
		}

		async Task<Tool> UpdateAsync(Tool tool, UpdateDefinition<Tool> update, CancellationToken cancellationToken)
		{
			update = update.Set(x => x.LastUpdateTime, new DateTime(Math.Max(tool.LastUpdateTime.Ticks + 1, DateTime.UtcNow.Ticks)));

			FilterDefinition<Tool> filter = Builders<Tool>.Filter.Eq(x => x.Id, tool.Id) & Builders<Tool>.Filter.Eq(x => x.LastUpdateTime, tool.LastUpdateTime);
			return await _tools.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<Tool> { ReturnDocument = ReturnDocument.After }, cancellationToken);
		}
	}
}
