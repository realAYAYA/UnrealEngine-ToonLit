// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Redis;
using Google.Protobuf.WellKnownTypes;
using Horde.Server.Auditing;
using Horde.Server.Server;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using StackExchange.Redis;

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
			public bool Enabled { get; set; } = true;

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

			[BsonIgnoreIfNull]
			public int? UpgradeAttemptCount { get; set; }

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

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool RequestForceRestart { get; set; }

			[BsonIgnoreIfNull]
			public string? LastShutdownReason { get; set; }

			public List<AgentWorkspaceInfo> Workspaces { get; set; } = new List<AgentWorkspaceInfo>();
			public DateTime LastConformTime { get; set; }

			[BsonIgnoreIfNull]
			public int? ConformAttemptCount { get; set; }

			public AgentCapabilities Capabilities { get; set; } = new AgentCapabilities();
			public List<AgentLease>? Leases { get; set; }
			public DateTime UpdateTime { get; set; }
			public uint UpdateIndex { get; set; }
			public string EnrollmentKey { get; set; } = String.Empty;
			public string? Comment { get; set; }

			IReadOnlyList<PoolId> IAgent.DynamicPools => DynamicPools;
			IReadOnlyList<PoolId> IAgent.ExplicitPools => Pools;
			IReadOnlyList<AgentWorkspaceInfo> IAgent.Workspaces => Workspaces;
			IReadOnlyList<AgentLease> IAgent.Leases => Leases ?? s_emptyLeases;
			IReadOnlyList<string> IAgent.Properties => Properties ?? Capabilities.Devices.FirstOrDefault()?.Properties?.ToList() ?? new List<string>();
			IReadOnlyDictionary<string, int> IAgent.Resources => Resources ?? Capabilities.Devices.FirstOrDefault()?.Resources ?? new Dictionary<string, int>();

			[BsonConstructor]
			private AgentDocument()
			{
			}

			public AgentDocument(AgentId id, bool ephemeral, string enrollmentKey)
			{
				Id = id;
				Ephemeral = ephemeral;
				EnrollmentKey = enrollmentKey;
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
			_updateEventChannel = new RedisChannel<AgentId>(RedisChannel.Literal("agents/notify"));
			_auditLog = auditLog;
		}

		/// <inheritdoc/>
		public async Task<IAgent> AddAsync(AgentId id, bool ephemeral, string enrollmentKey, CancellationToken cancellationToken)
		{
			AgentDocument agent = new AgentDocument(id, ephemeral, enrollmentKey);
			await _agents.InsertOneAsync(agent, null, cancellationToken);
			return agent;
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryResetAsync(IAgent agent, bool ephemeral, string enrollmentKey, CancellationToken cancellationToken = default)
		{
			AgentDocument agentDocument = (AgentDocument)agent;

			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update
				.Set(x => x.Ephemeral, ephemeral)
				.Set(x => x.EnrollmentKey, enrollmentKey)
				.Unset(x => x.Deleted)
				.Unset(x => x.SessionId);

			IAgent? newAgent = await TryUpdateAsync(agentDocument, update, cancellationToken);
			if (newAgent != null)
			{
				await PublishUpdateEventAsync(agent.Id);
			}
			return newAgent;
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryDeleteAsync(IAgent agentInterface, CancellationToken cancellationToken)
		{
			AgentDocument agent = (AgentDocument)agentInterface;

			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update
				.Set(x => x.Deleted, true)
				.Set(x => x.EnrollmentKey, "")
				.Unset(x => x.SessionId);

			return await TryUpdateAsync(agent, update, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task ForceDeleteAsync(AgentId agentId, CancellationToken cancellationToken)
		{
			await _agents.DeleteOneAsync(x => x.Id == agentId, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IAgent?> GetAsync(AgentId agentId, CancellationToken cancellationToken)
		{
			return await _agents.Find<AgentDocument>(x => x.Id == agentId).FirstOrDefaultAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IAgent>> GetManyAsync(List<AgentId> agentIds, CancellationToken cancellationToken)
		{
			return await _agents.Find(p => agentIds.Contains(p.Id)).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IAgent>> FindAsync(PoolId? poolId, DateTime? modifiedAfter, string? property, AgentStatus? status, bool? enabled, bool includeDeleted, int? index, int? count, CancellationToken cancellationToken)
		{
			FilterDefinitionBuilder<AgentDocument> filterBuilder = new FilterDefinitionBuilder<AgentDocument>();

			FilterDefinition<AgentDocument> filter = filterBuilder.Empty;
			if (!includeDeleted)
			{
				filter &= filterBuilder.Ne(x => x.Deleted, true);
			}

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

			return await search.ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IAgent>> FindExpiredAsync(DateTime utcNow, int maxAgents, CancellationToken cancellationToken)
		{
			return await _agents.Find(x => x.SessionId.HasValue && !(x.SessionExpiresAt > utcNow)).Limit(maxAgents).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IAgent>> FindDeletedAsync(CancellationToken cancellationToken)
		{
			return await _agents.Find(x => x.Deleted).ToListAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<List<LeaseId>> FindActiveLeaseIdsAsync(CancellationToken cancellationToken)
		{
			RedisValue[] activeLeaseIds = await _redisService.GetDatabase().SetMembersAsync(RedisKeyActiveLeaseIds());
			return activeLeaseIds.Select(x => LeaseId.Parse(x.ToString())).ToList();
		}

		/// <inheritdoc/>
		public async Task<List<LeaseId>> GetChildLeaseIdsAsync(LeaseId id, CancellationToken cancellationToken)
		{
			RedisValue[] childIds = await _redisService.GetDatabase().SetMembersAsync(RedisKeyLeaseChildren(id));
			return childIds.Select(x => LeaseId.Parse(x.ToString())).ToList();
		}

		/// <summary>
		/// Update a single document
		/// </summary>
		/// <param name="current">The document to update</param>
		/// <param name="update">The update definition</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The updated agent document or null if update failed</returns>
		private async Task<AgentDocument?> TryUpdateAsync(AgentDocument current, UpdateDefinition<AgentDocument> update, CancellationToken cancellationToken)
		{
			uint prevUpdateIndex = current.UpdateIndex++;
			current.UpdateTime = DateTime.UtcNow;

			Expression<Func<AgentDocument, bool>> filter = x => x.Id == current.Id && x.UpdateIndex == prevUpdateIndex;
			UpdateDefinition<AgentDocument> updateWithIndex = update.Set(x => x.UpdateIndex, current.UpdateIndex).Set(x => x.UpdateTime, current.UpdateTime);

			return await _agents.FindOneAndUpdateAsync<AgentDocument>(filter, updateWithIndex, new FindOneAndUpdateOptions<AgentDocument, AgentDocument> { ReturnDocument = ReturnDocument.After }, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryUpdateSettingsAsync(IAgent agentInterface, bool? enabled = null, bool? requestConform = null, bool? requestFullConform = null, bool? requestRestart = null, bool? requestShutdown = null, bool? requestForceRestart = null, string? shutdownReason = null, List<PoolId>? pools = null, string? comment = null, CancellationToken cancellationToken = default)
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
			if (requestForceRestart != null)
			{
				if (requestForceRestart.Value)
				{
					updates.Add(updateBuilder.Set(x => x.RequestForceRestart, true));
				}
				else
				{
					updates.Add(updateBuilder.Unset(x => x.RequestForceRestart));
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
			IAgent? newAgent = await TryUpdateAsync(agent, updateBuilder.Combine(updates), cancellationToken);
			if (newAgent != null)
			{
				if (newAgent.RequestRestart != agent.RequestRestart || newAgent.RequestConform != agent.RequestConform || newAgent.RequestShutdown != agent.RequestShutdown || newAgent.RequestForceRestart != agent.RequestForceRestart)
				{
					await PublishUpdateEventAsync(agent.Id);
				}
			}
			return newAgent;
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryUpdateSessionAsync(IAgent agentInterface, AgentStatus? status, DateTime? sessionExpiresAt, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, IReadOnlyList<PoolId>? dynamicPools, List<AgentLease>? leases, CancellationToken cancellationToken)
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
						GetNewLeaseUpdates(agent, lease, updates);
					}
				}

				List<AgentLease> currentLeases = agent.Leases ?? new List<AgentLease>();
				List<AgentLease> newLeases = leases;
				List<AgentLease> leasesToAdd = newLeases.Where(nl => currentLeases.All(cl => cl.Id != nl.Id)).ToList();
				List<AgentLease> leasesToRemove = currentLeases.Where(cl => newLeases.All(nl => nl.Id != cl.Id)).ToList();

				foreach (AgentLease lease in leasesToAdd)
				{
					await AddActiveLeaseAsync(lease);
				}

				foreach (AgentLease lease in leasesToRemove)
				{
					await RemoveActiveLeaseAsync(lease);
				}

				updates.Add(updateBuilder.Set(x => x.Leases, leases));
			}

			// If there are no new updates, return immediately. This is important for preventing UpdateSession calls from returning immediately.
			if (updates.Count == 0)
			{
				return agent;
			}

			// Update the agent, and try to create new lease documents if we succeed
			return await TryUpdateAsync(agent, updateBuilder.Combine(updates), cancellationToken);
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
		public async Task<IAgent?> TryUpdateWorkspacesAsync(IAgent agentInterface, List<AgentWorkspaceInfo> workspaces, bool requestConform, CancellationToken cancellationToken)
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
			return await TryUpdateAsync(agent, update, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryStartSessionAsync(IAgent agentInterface, SessionId sessionId, DateTime sessionExpiresAt, AgentStatus status, IReadOnlyList<string> properties, IReadOnlyDictionary<string, int> resources, IReadOnlyList<PoolId> pools, IReadOnlyList<PoolId> dynamicPools, DateTime lastStatusChange, string? version, CancellationToken cancellationToken)
		{
			AgentDocument agent = (AgentDocument)agentInterface;
			List<string> newProperties = properties.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
			Dictionary<string, int> newResources = new(resources);
			List<PoolId> newPools = new(pools);
			List<PoolId> newDynamicPools = new(dynamicPools);

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
			updates.Add(updateBuilder.Unset(x => x.RequestForceRestart));
			updates.Add(updateBuilder.Set(x => x.LastShutdownReason, "Unexpected"));

			if (String.Equals(version, agent.LastUpgradeVersion, StringComparison.Ordinal))
			{
				updates.Add(updateBuilder.Unset(x => x.UpgradeAttemptCount));
			}

			if (agent.Status != status)
			{
				updates.Add(updateBuilder.Set(x => x.LastStatusChange, lastStatusChange));
			}

			foreach (AgentLease agentLease in agent.Leases ?? Enumerable.Empty<AgentLease>())
			{
				await RemoveActiveLeaseAsync(agentLease);
			}

			// Apply the update
			return await TryUpdateAsync(agent, updateBuilder.Combine(updates), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryTerminateSessionAsync(IAgent agentInterface, CancellationToken cancellationToken)
		{
			AgentDocument agent = (AgentDocument)agentInterface;
			UpdateDefinition<AgentDocument> update = new BsonDocument();

			update = update.Unset(x => x.SessionId);
			update = update.Unset(x => x.SessionExpiresAt);
			update = update.Unset(x => x.Leases);
			update = update.Set(x => x.Status, AgentStatus.Stopped);
			update = update.Set(x => x.LastStatusChange, _clock.UtcNow);

			if (agent.Ephemeral)
			{
				update = update.Set(x => x.Deleted, true);
			}

			foreach (AgentLease agentLease in agent.Leases ?? Enumerable.Empty<AgentLease>())
			{
				await RemoveActiveLeaseAsync(agentLease);
			}

			return await TryUpdateAsync(agent, update, cancellationToken);
		}

		private static string RedisKeyActiveLeaseIds() => $"agent/active-lease-id";
		private static string RedisKeyLeaseChildren(LeaseId parentId) => $"agent/lease-children/{parentId.ToString()}";

		/// <inheritdoc/>
		public async Task<IAgent?> TryAddLeaseAsync(IAgent agentInterface, AgentLease newLease, CancellationToken cancellationToken)
		{
			AgentDocument agent = (AgentDocument)agentInterface;

			List<AgentLease> leases = new List<AgentLease>();
			if (agent.Leases != null)
			{
				leases.AddRange(agent.Leases);
			}
			leases.Add(newLease);

			List<UpdateDefinition<AgentDocument>> updates = new List<UpdateDefinition<AgentDocument>>();
			updates.Add(Builders<AgentDocument>.Update.Set(x => x.Leases, leases));
			GetNewLeaseUpdates(agent, newLease, updates);

			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Combine(updates);
			AgentDocument? updatedDoc = await TryUpdateAsync(agent, update, cancellationToken);

			if (updatedDoc != null)
			{
				await AddActiveLeaseAsync(newLease);
			}

			return updatedDoc;
		}

		private async Task AddActiveLeaseAsync(AgentLease lease)
		{
			IDatabase redis = _redisService.GetDatabase();

			await redis.SetAddAsync(RedisKeyActiveLeaseIds(), lease.Id.ToString());
			await redis.KeyExpireAsync(RedisKeyActiveLeaseIds(), TimeSpan.FromHours(36));

			if (lease.ParentId != null)
			{
				await redis.SetAddAsync(RedisKeyLeaseChildren(lease.ParentId.Value), lease.Id.ToString());
				await redis.KeyExpireAsync(RedisKeyLeaseChildren(lease.ParentId.Value), TimeSpan.FromHours(36));
			}
		}

		private async Task RemoveActiveLeaseAsync(AgentLease lease)
		{
			IDatabase redis = _redisService.GetDatabase();
			await redis.SetRemoveAsync(RedisKeyActiveLeaseIds(), lease.Id.ToString());

			if (lease.ParentId != null)
			{
				await redis.SetRemoveAsync(RedisKeyLeaseChildren(lease.ParentId.Value), lease.Id.ToString());
			}
		}

		static void GetNewLeaseUpdates(IAgent agent, AgentLease lease, List<UpdateDefinition<AgentDocument>> updates)
		{
			if (lease.Payload != null)
			{
				Any payload = Any.Parser.ParseFrom(lease.Payload.ToArray());
				if (payload.TryUnpack(out ConformTask conformTask))
				{
					int newConformAttemptCount = (agent.ConformAttemptCount ?? 0) + 1;
					updates.Add(Builders<AgentDocument>.Update.Set(x => x.ConformAttemptCount, newConformAttemptCount));
					updates.Add(Builders<AgentDocument>.Update.Set(x => x.LastConformTime, DateTime.UtcNow));
				}
				else if (payload.TryUnpack(out UpgradeTask upgradeTask))
				{
					string newVersion = upgradeTask.SoftwareId;

					int versionIdx = newVersion.IndexOf(':', StringComparison.Ordinal);
					if (versionIdx != -1)
					{
						newVersion = newVersion.Substring(versionIdx + 1);
					}

					int newUpgradeAttemptCount = (agent.UpgradeAttemptCount ?? 0) + 1;
					updates.Add(Builders<AgentDocument>.Update.Set(x => x.LastUpgradeVersion, newVersion));
					updates.Add(Builders<AgentDocument>.Update.Set(x => x.UpgradeAttemptCount, newUpgradeAttemptCount));
					updates.Add(Builders<AgentDocument>.Update.Set(x => x.LastUpgradeTime, DateTime.UtcNow));
				}
			}
		}

		/// <inheritdoc/>
		public async Task<IAgent?> TryCancelLeaseAsync(IAgent agentInterface, int leaseIdx, CancellationToken cancellationToken)
		{
			AgentDocument agent = (AgentDocument)agentInterface;

			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Set(x => x.Leases![leaseIdx].State, LeaseState.Cancelled);
			IAgent? newAgent = await TryUpdateAsync(agent, update, cancellationToken);
			if (newAgent != null)
			{
				await PublishUpdateEventAsync(agent.Id);
				if (agent.Leases != null && leaseIdx < agent.Leases.Count)
				{
					await RemoveActiveLeaseAsync(agent.Leases[leaseIdx]);
				}
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
