// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Build.Acls;
using Horde.Build.Server;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Driver;
using StackExchange.Redis;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Horde.Build.Tools
{
	using ToolId = StringId<Tool>;
	using ToolDeploymentId = ObjectId<ToolDeployment>;

	/// <summary>
	/// Collection of tool documents
	/// </summary>
	public class ToolCollection
	{
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

		private readonly RedisService _redisService;
		private readonly VersionedCollection<ToolId, Tool> _tools;
		private readonly IStorageClient _storage;
		private readonly IClock _clock;
		private readonly NamespaceId _namespaceId;
		private readonly ILogger _logger;

		private static readonly RedisKey s_baseKey = "tools/v1/";
		private static readonly RedisKey s_indexKey = s_baseKey.Append("index");

		private static readonly IReadOnlyDictionary<int, Type> s_types = RegisterTypes();

		/// <summary>
		/// Constructor
		/// </summary>
		public ToolCollection(MongoService mongoService, RedisService redisService, IStorageClient storage, IClock clock, IOptions<ServerSettings> options, ILogger<ToolCollection> logger)
		{
			_redisService = redisService;
			_tools = new VersionedCollection<ToolId, Tool>(mongoService, "Tools", redisService, s_baseKey, s_types);
			_storage = storage;
			_clock = clock;
			_namespaceId = options.Value.ToolNamespaceId;
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
				cm.MapMember(x => x.Acl).SetIgnoreIfDefault(true).SetDefaultValue(new AclV2());
			});

			return versionToType;
		}

		/// <summary>
		/// Configures all the available tools
		/// </summary>
		/// <param name="tools">The list of configured tools</param>
		public async Task ConfigureAsync(List<ToolOptions> tools)
		{
			List<ToolId> ids = await FindAllIdsAsync();

			List<ToolId> removeIds = ids.Except(tools.Select(x => x.Id)).ToList();
			foreach (ToolId removeId in removeIds)
			{
				_logger.LogInformation("Removing tool {ToolId}", removeId);
				await _tools.DeleteAsync(removeId);
			}

			foreach (ToolOptions tool in tools)
			{
				if (!ids.Contains(tool.Id))
				{
					_logger.LogInformation("Creating tool {ToolId}", tool.Id);
				}
				await AddOrUpdateAsync(tool);
			}
		}

		/// <summary>
		/// Finds or adds a tool with the given identifier
		/// </summary>
		/// <param name="options">Options for the tool</param>
		/// <returns>Document describing the tool</returns>
		private async Task<Tool> AddOrUpdateAsync(ToolOptions options)
		{
			for (; ; )
			{
				// Check for the common case where the tool already exists and is set to the right values already
				Tool tool = await _tools.FindOrAddAsync(options.Id, () => new Tool(options));
				if (tool.Name.Equals(options.Name, StringComparison.Ordinal) && tool.Description.Equals(options.Description, StringComparison.Ordinal) && tool.Public == options.Public && AclV2.Equals(tool.Acl, options.Acl))
				{
					return tool;
				}

				// Otherwise attempt to do an in-place upgrade
				UpdateDefinition<Tool> update = Builders<Tool>.Update.Set(x => x.Name, options.Name).Set(x => x.Description, options.Description).Set(x => x.Public, options.Public);
				if (AclV2.IsNullOrDefault(options.Acl))
				{
					update = update.Unset(x => x.Acl);
				}
				else
				{
					update = update.Set(x => x.Acl, options.Acl);
				}

				// Update the existing tool
				Tool? updatedTool = await _tools.UpdateAsync(tool, update);
				if (updatedTool != null)
				{
					await ClearCachedIndexAsync();
					return updatedTool;
				}
			}
		}

		/// <summary>
		/// Removes a tool with the given identifier
		/// </summary>
		/// <param name="id">The tool to remove</param>
		/// <returns>True if a tool with the given identifier was deleted</returns>
		private async Task<bool> DeleteAsync(ToolId id)
		{
			bool deleted = await _tools.DeleteAsync(id);
			if (deleted)
			{
				await ClearCachedIndexAsync();
			}
			return deleted;
		}

		private async Task ClearCachedIndexAsync()
		{
			CachedIndex newIndexData = new CachedIndex { Rev = ObjectId.GenerateNewId().ToString() };
			await _redisService.GetDatabase().StringSetAsync(s_indexKey, CbSerializer.SerializeToByteArray(newIndexData));
		}

		/// <summary>
		/// Finds all tools matching a set of criteria
		/// </summary>
		/// <returns>Sequence of tool documents</returns>
		async Task<List<ToolId>> FindAllIdsAsync()
		{
			IDatabase redis = _redisService.GetDatabase();
			CachedIndex? index;
			for (; ; )
			{
				// Get the cached index, and check if it has valid data
				RedisValue value = await redis.StringGetAsync(s_indexKey);
				if (!value.IsNullOrEmpty)
				{
					index = CbSerializer.Deserialize<CachedIndex>((byte[])value!);
					if (index.Empty || index.Ids.Count > 0)
					{
						break;
					}
				}

				// Create a new index object
				index = new CachedIndex();
				index.Rev = ObjectId.GenerateNewId().ToString();
				index.Ids = await _tools.BaseCollection.Find(FilterDefinition<VersionedDocument<ToolId, Tool>>.Empty).Project(x => x.Id).ToListAsync();
				index.Empty = index.Ids.Count == 0;

				// Try to replace the existing one
				RedisValue newValue = CbSerializer.SerializeToByteArray(index);
				if (value.IsNull)
				{
					if (await redis.StringSetAsync(s_indexKey, newValue, when: When.NotExists))
					{
						break;
					}
				}
				else
				{
					ITransaction transaction = redis.CreateTransaction();
					transaction.AddCondition(Condition.StringEqual(s_indexKey, value));
					_ = transaction.StringSetAsync(s_indexKey, newValue);

					if (await transaction.ExecuteAsync())
					{
						break;
					}
				}
			}
			return index.Ids;
		}

		/// <summary>
		/// Finds all tools matching a set of criteria
		/// </summary>
		/// <returns>Sequence of tool documents</returns>
		public async Task<List<Tool>> FindAllAsync()
		{
			// Fetch all the tools in parallel
			IEnumerable<ToolId> toolIds =await FindAllIdsAsync();
			List<Task<Tool?>> tasks = toolIds.Select(x => Task.Run(() => GetAsync(x))).ToList();

			// Return all the successful results
			List<Tool> results = new List<Tool>();
			foreach (Task<Tool?> task in tasks)
			{
				Tool? tool = await task;
				if (tool != null)
				{
					results.Add(tool);
				}
			}
			return results;
		}

		/// <summary>
		/// Gets a tool with the given identifier
		/// </summary>
		/// <param name="id">The tool identifier</param>
		/// <returns></returns>
		public async Task<Tool?> GetAsync(ToolId id)
		{
			Tool? tool = await _tools.GetAsync(id);
			if (tool != null)
			{
				tool.UpdateTemporalState(_clock.UtcNow);
			}
			return tool;
		}

		/// <summary>
		/// Adds a new deployment to the given tool. The new deployment will replace the current active deployment.
		/// </summary>
		/// <param name="tool">The tool to update</param>
		/// <param name="options">Options for the new deployment</param>
		/// <param name="stream">Stream containing the tool data</param>
		/// <returns>Updated tool document, or null if it does not exist</returns>
		public async Task<Tool?> CreateDeploymentAsync(Tool tool, ToolDeploymentOptions options, Stream stream)
		{
			// Upload the tool data first
			IoHash hash = await _storage.WriteBlobAsync(_namespaceId, stream);

			ToolDeploymentData deploymentData = new ToolDeploymentData();
			deploymentData.Id = ToolDeploymentId.GenerateNewId();
			deploymentData.Version = options.Version;
			deploymentData.Data = new CbBinaryAttachment(hash);

			BucketId bucketId = GetBucket(tool.Id);
			RefId refId = new RefId(deploymentData.Id.ToString());
			await _storage.SetRefAsync(_namespaceId, bucketId, refId, deploymentData);

			// Create the new deployment object
			ToolDeployment deployment = new ToolDeployment(deploymentData.Id, options, refId);

			// Start the deployment
			DateTime utcNow = _clock.UtcNow;
			if (!options.CreatePaused)
			{
				deployment.StartedAt = utcNow;
			}

			// Create the deployment
			Tool? newTool = tool;
			for (; ; )
			{
				newTool = await TryAddDeploymentAsync(newTool, deployment);
				if (newTool != null)
				{
					break;
				}

				newTool = await GetAsync(tool.Id);
				if (newTool == null)
				{
					return null;
				}
			}

			// Return the new tool with updated deployment states
			newTool.UpdateTemporalState(utcNow);
			return newTool;
		}

		async ValueTask<Tool?> TryAddDeploymentAsync(Tool tool, ToolDeployment deployment)
		{
			Tool? newTool = tool;

			// If there are already a maximum number of deployments, remove the oldest one
			const int MaxDeploymentCount = 5;
			while (tool.Deployments.Count >= MaxDeploymentCount)
			{
				newTool = await _tools.UpdateAsync(newTool, Builders<Tool>.Update.PopFirst(x => x.Deployments));
				if (newTool == null)
				{
					return null;
				}
				await _storage.DeleteRefAsync(_namespaceId, GetBucket(newTool.Id), tool.Deployments[0].RefId);
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
		public async Task<Tool?> UpdateDeploymentAsync(Tool tool, ToolDeploymentId deploymentId, ToolDeploymentState action)
		{
			int idx = tool.Deployments.FindIndex(x => x.Id == deploymentId);
			if (idx == -1)
			{
				return null;
			}

			BucketId bucketId = GetBucket(tool.Id);

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
		/// Opens a stream to the data for a particular deployment
		/// </summary>
		/// <param name="tool">Identifier for the tool</param>
		/// <param name="deployment">The deployment</param>
		/// <returns>Stream for the data</returns>
		public async Task<Stream> GetDeploymentPayloadAsync(Tool tool, ToolDeployment deployment)
		{
			ToolDeploymentData data = await _storage.GetRefAsync<ToolDeploymentData>(_namespaceId, GetBucket(tool.Id), deployment.RefId);
			return await _storage.ReadBlobAsync(_namespaceId, data.Data);
		}

		private static BucketId GetBucket(ToolId toolId) => new BucketId(toolId.ToString());
	}
}
