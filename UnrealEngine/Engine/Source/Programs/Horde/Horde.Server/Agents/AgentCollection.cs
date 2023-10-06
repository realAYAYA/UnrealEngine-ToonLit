// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Threading.Tasks;
using EpicGames.Redis;
using Google.Protobuf.WellKnownTypes;
using Horde.Server.Agents.Pools;
using Horde.Server.Agents.Sessions;
using Horde.Server.Auditing;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Agents
{
	/// <summary>
	/// Collection of agent documents
	/// </summary>
	public class AgentCollection : IAgentCollection
	{
		/// <summary>
		/// Legacy information about a device attached to an agent
		/// </summary>
		class DeviceCapabilities
		{
			[BsonIgnoreIfNull]
			public HashSet<string>? Properties { get; set; }

			[BsonIgnoreIfNull]
			public Dictionary<string, int>? Resources { get; set; }
		}

		/// <summary>
		/// Legacy capabilities of an agent
		/// </summary>
		class AgentCapabilities
		{
			public List<DeviceCapabilities> Devices { get; set; } = new List<DeviceCapabilities>();

			[BsonIgnoreIfNull]
			public HashSet<string>? Properties { get; set; }
		}

		/// <summary>
		/// Concrete implementation of an agent document
		/// </summary>
		class AgentDocument : IAgent
		{
			static readonly IReadOnlyList<AgentLease> s_emptyLeases = new List<AgentLease>();

			[BsonRequired, BsonId]
			public AgentId Id { get; set; }

			public SessionId? SessionId { get; set; }
			public DateTime? SessionExpiresAt { get; set; }

			public AgentStatus Status { get; set; }
			public DateTime? LastStatusChange { get; set; }

			[BsonRequired]
			public bool Enabled { get; set; }

			public bool Ephemeral { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool Deleted { get; set; }

			[BsonElement("Version2")]
			public string? Version { get; set; }

			public List<string>? Properties { get; set; }
			public Dictionary<string, int>? Resources { get; set; }

			[BsonIgnoreIfNull]
			public string? LastUpgradeVersion { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? LastUpgradeTime { get; set; }

			public List<PoolId> DynamicPools { get; set; } = new List<PoolId>();
			public List<PoolId> Pools { get; set; } = new List<PoolId>();

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool RequestConform { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool RequestFullConform { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool RequestRestart { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool RequestShutdown { get; set; }

			[BsonIgnoreIfNull]
			public string? LastShutdownReason { get; set; }

			public List<AgentWorkspace> Workspaces { get; set; } = new List<AgentWorkspace>();
			public DateTime LastConformTime { get; set; }

			[BsonIgnoreIfNull]
			public int? ConformAttemptCount { get; set; }

			public AgentCapabilities Capabilities { get; set; } = new AgentCapabilities();
			public List<AgentLease>? Leases { get; set; }
			public DateTime UpdateTime { get; set; }
			public uint UpdateIndex { get; set; }
			public string? Comment { get; set; }

			IReadOnlyList<PoolId> IAgent.DynamicPools => DynamicPools;
			IReadOnlyList<PoolId> IAgent.ExplicitPools => Pools;
			IReadOnlyList<AgentWorkspace> IAgent.Workspaces => Workspaces;
			IReadOnlyList<AgentLease> IAgent.Leases => Leases ?? s_emptyLeases;
			IReadOnlyList<string> IAgent.Properties => Properties ?? Capabilities.Devices.FirstOrDefault()?.Properties?.ToList() ?? new List<string>();
			IReadOnlyDictionary<string, int> IAgent.Resources => Resources ?? Capabilities.Devices.FirstOrDefault()?.Resources ?? new Dictionary<string, int>();

			[BsonConstructor]
			private AgentDocument()
			{
			}

			public AgentDocument(AgentId id, bool enabled, List<PoolId> pools)
			{
				Id = id;
				Enabled = enabled;
				Pools = pools;
			}
		}

		readonly IMongoCollection<AgentDocument> _agents;
		readonly IAuditLog<AgentId> _auditLog;
		readonly RedisService _redisService;
		readonly IClock _clock;
		readonly RedisChannel<AgentId> _updateEventChannel;

		/// <summary>
		/// Constructor
		/// </summary>
		public AgentCollection(MongoService mongoService, RedisService redisService, IClock clock, IAuditLog<AgentId> auditLog)
		{
			List<MongoIndex<AgentDocument>> indexes = new List<MongoIndex<AgentDocument>>();
			indexes.Add(keys => keys.Ascending(x => x.Deleted).Ascending(x => x.Id).Ascending(x => x.Pools));

			_agents = mongoService.GetCollection<AgentDocument>("Agents", indexes);
			_redisService = redisService;
			_clock = clock;
			_updateEventChannel = new RedisChannel<AgentId>("agents/notify");
			_auditLog = auditLog;
		}

		/// <inheritdoc/>
		public async Task<IAgent> AddAsync(AgentId id, bool enabled, List<PoolId>? pools)
		{
			AgentDocument agent = new AgentDocument(id, enabled, pools ?? new List<PoolId>());
			await _agents.InsertOneAsync(agent);
			return agent;
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryDeleteAsync(IAgent agentInterface)
		{
			AgentDocument agent = (AgentDocument)agentInterface;

			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Set(x => x.Deleted, true);
			return await TryUpdateAsync(agent, update);
		}

		/// <inheritdoc/>
		public async Task ForceDeleteAsync(AgentId agentId)
		{
			await _agents.DeleteOneAsync(x => x.Id == agentId);
		}

		/// <inheritdoc/>
		public async Task<IAgent?> GetAsync(AgentId agentId)
		{
			return await _agents.Find<AgentDocument>(x => x.Id == agentId).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<List<IAgent>> GetManyAsync(List<AgentId> agentIds)
		{
			List<AgentDocument> agentDocuments = await _agents.Find(p => agentIds.Contains(p.Id)).ToListAsync();
			return new List<IAgent>(agentDocuments);
		}

		/// <inheritdoc/>
		public async Task<List<IAgent>> FindAsync(PoolId? poolId, DateTime? modifiedAfter, string? property, AgentStatus? status, bool? enabled, int? index, int? count)
		{
			FilterDefinitionBuilder<AgentDocument> filterBuilder = new FilterDefinitionBuilder<AgentDocument>();

			FilterDefinition<AgentDocument> filter = filterBuilder.Ne(x => x.Deleted, true);
			
			if (poolId != null)
			{
				filter &= filterBuilder.Eq(nameof(AgentDocument.Pools), poolId);
			}
			
			if (modifiedAfter != null)
			{
				filter &= filterBuilder.Gt(x => x.UpdateTime, modifiedAfter.Value);
			}
			
			if (property != null)
			{
				filter &= filterBuilder.AnyEq(x => x.Properties, property);
			}
			
			if (status != null)
			{
				filter &= filterBuilder.Eq(x => x.Status, status.Value);
			}
			
			if (enabled != null)
			{
				filter &= filterBuilder.Eq(x => x.Enabled, enabled.Value);
			}

			IFindFluent<AgentDocument, AgentDocument> search = _agents.Find(filter);
			if (index != null)
			{
				search = search.Skip(index.Value);
			}
			if (count != null)
			{
				search = search.Limit(count.Value);
			}

			List<AgentDocument> results = await search.ToListAsync();
			return results.ConvertAll<IAgent>(x => x);
		}

		/// <inheritdoc/>
		public async Task<List<IAgent>> FindExpiredAsync(DateTime utcNow, int maxAgents)
		{
			List<AgentDocument> results = await _agents.Find(x => x.SessionId.HasValue && !(x.SessionExpiresAt > utcNow)).Limit(maxAgents).ToListAsync();
			return results.ConvertAll<IAgent>(x => x);
		}

		/// <summary>
		/// Update a single document
		/// </summary>
		/// <param name="current">The document to update</param>
		/// <param name="update">The update definition</param>
		/// <returns>The updated agent document or null if update failed</returns>
		private async Task<AgentDocument?> TryUpdateAsync(AgentDocument current, UpdateDefinition<AgentDocument> update)
		{
			uint prevUpdateIndex = current.UpdateIndex++;
			current.UpdateTime = DateTime.UtcNow;

			Expression<Func<AgentDocument, bool>> filter = x => x.Id == current.Id && x.UpdateIndex == prevUpdateIndex;
			UpdateDefinition<AgentDocument> updateWithIndex = update.Set(x => x.UpdateIndex, current.UpdateIndex).Set(x => x.UpdateTime, current.UpdateTime);

			return await _agents.FindOneAndUpdateAsync<AgentDocument>(filter, updateWithIndex, new FindOneAndUpdateOptions<AgentDocument, AgentDocument> { ReturnDocument = ReturnDocument.After });
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryUpdateSettingsAsync(IAgent agentInterface, bool? enabled = null, bool? requestConform = null, bool? requestFullConform = null, bool? requestRestart = null, bool? requestShutdown = null, string? shutdownReason = null, List<PoolId>? pools = null, string? comment = null)
		{
			AgentDocument agent = (AgentDocument)agentInterface;

			// Update the database
			UpdateDefinitionBuilder<AgentDocument> updateBuilder = new UpdateDefinitionBuilder<AgentDocument>();

			List<UpdateDefinition<AgentDocument>> updates = new List<UpdateDefinition<AgentDocument>>();
			if (pools != null)
			{
				updates.Add(updateBuilder.Set(x => x.Pools, pools));
			}
			if (enabled != null)
			{
				updates.Add(updateBuilder.Set(x => x.Enabled, enabled.Value));
			}
			if (requestConform != null)
			{
				updates.Add(updateBuilder.Set(x => x.RequestConform, requestConform.Value));
				updates.Add(updateBuilder.Unset(x => x.ConformAttemptCount));
			}
			if (requestFullConform != null)
			{
				updates.Add(updateBuilder.Set(x => x.RequestFullConform, requestFullConform.Value));
				updates.Add(updateBuilder.Unset(x => x.ConformAttemptCount));
			}
			if (requestRestart != null)
			{
				if (requestRestart.Value)
				{
					updates.Add(updateBuilder.Set(x => x.RequestRestart, true));
				}
				else
				{
					updates.Add(updateBuilder.Unset(x => x.RequestRestart));
				}
			}
			if (requestShutdown != null)
			{
				if (requestShutdown.Value)
				{
					updates.Add(updateBuilder.Set(x => x.RequestShutdown, true));
				}
				else
				{
					updates.Add(updateBuilder.Unset(x => x.RequestShutdown));
				}
			}

			if (shutdownReason != null)
			{
				updates.Add(updateBuilder.Set(x => x.LastShutdownReason, shutdownReason));
			}

			if (comment != null)
			{
				updates.Add(updateBuilder.Set(x => x.Comment, comment));
			}

			// Apply the update
			IAgent? newAgent = await TryUpdateAsync(agent, updateBuilder.Combine(updates));
			if (newAgent != null)
			{
				if (newAgent.RequestRestart != agent.RequestRestart || newAgent.RequestConform != agent.RequestConform || newAgent.RequestShutdown != agent.RequestShutdown)
				{
					await PublishUpdateEventAsync(agent.Id);
				}
			}
			return newAgent;
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryUpdateSessionAsync(IAgent agentInterface, AgentStatus? status, DateTime? sessionExpiresAt, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, IReadOnlyList<PoolId>? dynamicPools, List<AgentLease>? leases)
		{
			AgentDocument agent = (AgentDocument)agentInterface;

			// Create an update definition for the agent
			UpdateDefinitionBuilder<AgentDocument> updateBuilder = Builders<AgentDocument>.Update;
			List<UpdateDefinition<AgentDocument>> updates = new List<UpdateDefinition<AgentDocument>>();

			if (status != null && agent.Status != status.Value)
			{
				updates.Add(updateBuilder.Set(x => x.Status, status.Value));
				updates.Add(updateBuilder.Set(x => x.LastStatusChange, _clock.UtcNow));
			}
			if (sessionExpiresAt != null)
			{
				updates.Add(updateBuilder.Set(x => x.SessionExpiresAt, sessionExpiresAt.Value));
			}
			if (properties != null)
			{
				List<string> newProperties = properties.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
				if (!agentInterface.Properties.SequenceEqual(newProperties, StringComparer.Ordinal))
				{
					updates.Add(updateBuilder.Set(x => x.Properties, newProperties));
				}
			}
			if (resources != null && !ResourcesEqual(resources, agentInterface.Resources))
			{
				updates.Add(updateBuilder.Set(x => x.Resources, new Dictionary<string, int>(resources)));
			}
			if (dynamicPools != null && !dynamicPools.SequenceEqual(agent.DynamicPools))
			{
				updates.Add(updateBuilder.Set(x => x.DynamicPools, new List<PoolId>(dynamicPools)));
			}
			if (leases != null)
			{
				foreach (AgentLease lease in leases)
				{
					if (lease.Payload != null && (agent.Leases == null || !agent.Leases.Any(x => x.Id == lease.Id)))
					{
						Any payload = Any.Parser.ParseFrom(lease.Payload.ToArray());
						if (payload.TryUnpack(out ConformTask conformTask))
						{
							int newConformAttemptCount = (agent.ConformAttemptCount ?? 0) + 1;
							updates.Add(updateBuilder.Set(x => x.ConformAttemptCount, newConformAttemptCount));
							updates.Add(updateBuilder.Set(x => x.LastConformTime, DateTime.UtcNow));
						}
						else if (payload.TryUnpack(out UpgradeTask upgradeTask))
						{
							updates.Add(updateBuilder.Set(x => x.LastUpgradeVersion, upgradeTask.SoftwareId));
							updates.Add(updateBuilder.Set(x => x.LastUpgradeTime, DateTime.UtcNow));
						}
					}
				}

				updates.Add(updateBuilder.Set(x => x.Leases, leases));
			}

			// If there are no new updates, return immediately. This is important for preventing UpdateSession calls from returning immediately.
			if (updates.Count == 0)
			{
				return agent;
			}

			// Update the agent, and try to create new lease documents if we succeed
			return await TryUpdateAsync(agent, updateBuilder.Combine(updates));
		}

		static bool ResourcesEqual(IReadOnlyDictionary<string, int> dictA, IReadOnlyDictionary<string, int> dictB)
		{
			if (dictA.Count != dictB.Count)
			{
				return false;
			}

			foreach (KeyValuePair<string, int> pair in dictA)
			{
				int value;
				if (!dictB.TryGetValue(pair.Key, out value) || value != pair.Value)
				{
					return false;
				}
			}

			return true;
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryUpdateWorkspacesAsync(IAgent agentInterface, List<AgentWorkspace> workspaces, bool requestConform)
		{
			AgentDocument agent = (AgentDocument)agentInterface;
			DateTime lastConformTime = DateTime.UtcNow;

			// Set the new workspaces
			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Set(x => x.Workspaces, workspaces);
			update = update.Set(x => x.LastConformTime, lastConformTime);
			update = update.Unset(x => x.ConformAttemptCount);
			if (!requestConform)
			{
				update = update.Unset(x => x.RequestConform);
				update = update.Unset(x => x.RequestFullConform);
			}

			// Update the agent
			return await TryUpdateAsync(agent, update);
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryStartSessionAsync(IAgent agentInterface, SessionId sessionId, DateTime sessionExpiresAt, AgentStatus status, IReadOnlyList<string> properties, IReadOnlyDictionary<string, int> resources, IReadOnlyList<PoolId> pools, IReadOnlyList<PoolId> dynamicPools, DateTime lastStatusChange, string? version)
		{
			AgentDocument agent = (AgentDocument)agentInterface;
			List<string> newProperties = properties.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
			Dictionary<string, int> newResources = new (resources);
			List<PoolId> newPools = new (pools);
			List<PoolId> newDynamicPools = new (dynamicPools);

			// Reset the agent to use the new session
			UpdateDefinitionBuilder<AgentDocument> updateBuilder = Builders<AgentDocument>.Update;

			List<UpdateDefinition<AgentDocument>> updates = new List<UpdateDefinition<AgentDocument>>();
			updates.Add(updateBuilder.Set(x => x.SessionId, sessionId));
			updates.Add(updateBuilder.Set(x => x.SessionExpiresAt, sessionExpiresAt));
			updates.Add(updateBuilder.Set(x => x.Status, status));
			updates.Add(updateBuilder.Unset(x => x.Leases));
			updates.Add(updateBuilder.Unset(x => x.Deleted));
			updates.Add(updateBuilder.Set(x => x.Properties, newProperties));
			updates.Add(updateBuilder.Set(x => x.Resources, newResources));
			updates.Add(updateBuilder.Set(x => x.Pools, newPools));
			updates.Add(updateBuilder.Set(x => x.DynamicPools, newDynamicPools));
			updates.Add(updateBuilder.Set(x => x.Version, version));
			updates.Add(updateBuilder.Unset(x => x.RequestRestart));
			updates.Add(updateBuilder.Unset(x => x.RequestShutdown));
			updates.Add(updateBuilder.Set(x => x.LastShutdownReason, "Unexpected"));

			if (agent.Status != status)
			{
				updates.Add(updateBuilder.Set(x => x.LastStatusChange, lastStatusChange));
			}

			// Apply the update
			return await TryUpdateAsync(agent, updateBuilder.Combine(updates));
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryTerminateSessionAsync(IAgent agentInterface)
		{
			AgentDocument agent = (AgentDocument)agentInterface;
			UpdateDefinition<AgentDocument> update = new BsonDocument();

			update = update.Unset(x => x.SessionId);
			update = update.Unset(x => x.SessionExpiresAt);
			update = update.Unset(x => x.Leases);
			update = update.Set(x => x.Status, AgentStatus.Stopped);
			update = update.Set(x => x.LastStatusChange, _clock.UtcNow);

			bool deleted = agent.Deleted || agent.Ephemeral;
			if (deleted != agent.Deleted)
			{
				update = update.Set(x => x.Deleted, agent.Deleted);
			}

			return await TryUpdateAsync(agent, update);
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryAddLeaseAsync(IAgent agentInterface, AgentLease newLease)
		{
			AgentDocument agent = (AgentDocument)agentInterface;

			List<AgentLease> leases = new List<AgentLease>();
			if (agent.Leases != null)
			{
				leases.AddRange(agent.Leases);
			}
			leases.Add(newLease);

			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Set(x => x.Leases, leases);
			return await TryUpdateAsync(agent, update);
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryCancelLeaseAsync(IAgent agentInterface, int leaseIdx)
		{
			AgentDocument agent = (AgentDocument)agentInterface;

			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Set(x => x.Leases![leaseIdx].State, LeaseState.Cancelled);
			IAgent? newAgent = await TryUpdateAsync(agent, update);
			if (newAgent != null)
			{
				await PublishUpdateEventAsync(agent.Id);
			}
			return newAgent;
		}

		/// <inheritdoc/>
		public IAuditLogChannel<AgentId> GetLogger(AgentId agentId)
		{
			return _auditLog[agentId];
		}

		/// <inheritdoc/>
		public Task PublishUpdateEventAsync(AgentId agentId)
		{
			return _redisService.GetDatabase().PublishAsync(_updateEventChannel, agentId);
		}

		/// <inheritdoc/>
		public async Task<IAsyncDisposable> SubscribeToUpdateEventsAsync(Action<AgentId> onUpdate)
		{
			return await _redisService.GetDatabase().Multiplexer.SubscribeAsync(_updateEventChannel, onUpdate);
		}
	}
}
