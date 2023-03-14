// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Redis;
using EpicGames.Serialization;
using Google.Protobuf.WellKnownTypes;
using Horde.Build.Acls;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Sessions;
using Horde.Build.Agents.Software;
using Horde.Build.Auditing;
using Horde.Build.Server;
using Horde.Build.Tasks;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using MongoDB.Driver;
using OpenTracing;
using OpenTracing.Util;
using StackExchange.Redis;
using StatsdClient;

namespace Horde.Build.Agents
{
	using AgentSoftwareChannelName = StringId<AgentSoftwareChannels>;
	using LeaseId = ObjectId<ILease>;
	using PoolId = StringId<IPool>;
	using SessionId = ObjectId<ISession>;

	/// <summary>
	/// Singleton used to store agent costs
	/// </summary>
	[RedisConverter(typeof(RedisCbConverter<>))]
	public class AgentRateTable
	{
		/// <summary>
		/// List of costs for different agent types
		/// </summary>
		[CbField]
		public List<AgentRateConfig> Entries { get; set; } = new List<AgentRateConfig>();
	}

	/// <summary>
	/// Wraps funtionality for manipulating agents
	/// </summary>
	public sealed class AgentService : IHostedService, IDisposable
	{
		/// <summary>
		/// Maximum time between updates for an agent to be considered online
		/// </summary>
		public static readonly TimeSpan SessionExpiryTime = TimeSpan.FromMinutes(5);

		/// <summary>
		/// Time before a session expires that we will poll until
		/// </summary>
		public static readonly TimeSpan SessionLongPollTime = TimeSpan.FromSeconds(55);

		/// <summary>
		/// Time after which a session will be renewed
		/// </summary>
		public static readonly TimeSpan SessionRenewTime = TimeSpan.FromSeconds(50);
		readonly AclService _aclService;
		readonly IDowntimeService _downtimeService;

		/// <summary>
		/// Collection of agent documents
		/// </summary>
		public IAgentCollection Agents { get; }

		readonly ILeaseCollection _leases;
		readonly ISessionCollection _sessions;
		readonly IDogStatsd _dogStatsd;
		readonly ITaskSource[] _taskSources;
		readonly IHostApplicationLifetime _applicationLifetime;
		readonly IClock _clock;
		readonly ILogger _logger;
		readonly ITicker _ticker;
		
		readonly RedisString<AgentRateTable> _agentRateTableData;
		readonly RedisService _redisService;

		// Lazily updated costs for different agent types
		readonly AsyncCachedValue<AgentRateTable?> _agentRateTable;

		// Lazily updated list of current pools
		readonly AsyncCachedValue<List<IPool>> _poolsList;

		// All the agents currently performing a long poll for work on this server
		readonly Dictionary<AgentId, CancellationTokenSource> _waitingAgents = new Dictionary<AgentId, CancellationTokenSource>();

		// Subscription for update events
		readonly IDisposable _subscription;

		/// <summary>
		/// Constructor
		/// </summary>
		public AgentService(IAgentCollection agents, ILeaseCollection leases, ISessionCollection sessions, AclService aclService, IDowntimeService downtimeService, IPoolCollection poolCollection, IDogStatsd dogStatsd, IEnumerable<ITaskSource> taskSources, RedisService redisService, IHostApplicationLifetime applicationLifetime, IClock clock, ILogger<AgentService> logger)
		{
			Agents = agents;
			_leases = leases;
			_sessions = sessions;
			_aclService = aclService;
			_downtimeService = downtimeService;
			_agentRateTableData = new RedisString<AgentRateTable>(redisService.ConnectionPool, "agent-rates");
			_agentRateTable = new AsyncCachedValue<AgentRateTable?>(() => _agentRateTableData.GetAsync(), TimeSpan.FromSeconds(2.0));//.FromMinutes(5.0));
			_poolsList = new AsyncCachedValue<List<IPool>>(() => poolCollection.GetAsync(), TimeSpan.FromSeconds(30.0));
			_dogStatsd = dogStatsd;
			_taskSources = taskSources.ToArray();
			_applicationLifetime = applicationLifetime;
			_redisService = redisService;
			_clock = clock;
			_ticker = clock.AddTicker<AgentService>(TimeSpan.FromSeconds(30.0), TickAsync, logger);
			_logger = logger;

			_subscription = agents.SubscribeToUpdateEventsAsync(OnAgentUpdate).Result;
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public void Dispose()
		{
			_subscription.Dispose();
			_ticker.Dispose();
		}

		/// <summary>
		/// Gets user-readable payload information
		/// </summary>
		/// <param name="payload">The payload data</param>
		/// <returns>Dictionary of key/value pairs for the payload</returns>
		public Dictionary<string, string>? GetPayloadDetails(ReadOnlyMemory<byte>? payload)
		{
			Dictionary<string, string>? details = null;
			if (payload != null)
			{
				Any basePayload = Any.Parser.ParseFrom(payload.Value.ToArray());
				foreach (ITaskSource taskSource in _taskSources)
				{
					if (basePayload.Is(taskSource.Descriptor))
					{
						details = new Dictionary<string, string>();
						taskSource.GetLeaseDetails(basePayload, details);
						break;
					}
				}
			}
			return details;
		}

		/// <summary>
		/// Issues a bearer token for the given session id
		/// </summary>
		/// <param name="agentId">The agent id</param>
		/// <param name="sessionId">The session id</param>
		/// <returns>Bearer token for the agent</returns>
		public string IssueSessionToken(AgentId agentId, SessionId sessionId)
		{
			List<AclClaim> claims = new List<AclClaim>();
			claims.Add(AclService.AgentRoleClaim);
			claims.Add(AclService.GetAgentClaim(agentId));
			claims.Add(AclService.GetSessionClaim(sessionId));
			return _aclService.IssueBearerToken(claims, null);
		}

		/// <summary>
		/// Register a new agent
		/// </summary>
		/// <param name="name">Name of the agent</param>
		/// <param name="bEnabled">Whether the agent is currently enabled</param>
		/// <param name="channel">Override for the desired software version</param>
		/// <param name="pools">Pools for this agent</param>
		/// <returns>Unique id for the agent</returns>
		public Task<IAgent> CreateAgentAsync(string name, bool bEnabled, AgentSoftwareChannelName? channel, List<PoolId>? pools)
		{
			return Agents.AddAsync(new AgentId(name), bEnabled, channel, pools);
		}

		/// <summary>
		/// Gets an agent by ID
		/// </summary>
		/// <param name="agentId">Unique id of the agent</param>
		/// <returns>The agent document</returns>
		public Task<IAgent?> GetAgentAsync(AgentId agentId)
		{
			return Agents.GetAsync(agentId);
		}

		/// <summary>
		/// Finds all agents matching certain criteria
		/// </summary>
		/// <param name="poolId">The pool containing the agent</param>
		/// <param name="modifiedAfter">If set, only returns agents modified after this time</param>
		/// <param name="index">Index within the list of results</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of agents matching the given criteria</returns>
		public Task<List<IAgent>> FindAgentsAsync(PoolId? poolId, DateTime? modifiedAfter, int? index, int? count)
		{
			return Agents.FindAsync(poolId, modifiedAfter, null, null, index, count);
		}

		/// <summary>
		/// Update the current workspaces for an agent.
		/// </summary>
		/// <param name="agent">The agent to update</param>
		/// <param name="workspaces">Current list of workspaces</param>
		/// <param name="bPendingConform">Whether the agent still needs to run another conform</param>
		/// <returns>New agent state</returns>
		public async Task<bool> TryUpdateWorkspacesAsync(IAgent agent, List<AgentWorkspace> workspaces, bool bPendingConform)
		{
			IAgent? newAgent = await Agents.TryUpdateWorkspacesAsync(agent, workspaces, bPendingConform);
			return newAgent != null;
		}

		/// <summary>
		/// Marks the agent as deleted
		/// </summary>
		/// <param name="agent">The agent to delete</param>
		/// <returns>Async task</returns>
		public async Task DeleteAgentAsync(IAgent? agent)
		{
			while (agent != null && !agent.Deleted)
			{
				IAgent? newAgent = await Agents.TryDeleteAsync(agent);
				if(newAgent != null)
				{
					break;
				}
				agent = await GetAgentAsync(agent.Id);
			}
		}

		async ValueTask<List<PoolId>> GetDynamicPoolsAsync(IAgent agent)
		{
			List<PoolId> newDynamicPools = new List<PoolId>();

			List<IPool> pools = await _poolsList.GetAsync();
			foreach (IPool pool in pools)
			{
				if (pool.Condition != null && agent.SatisfiesCondition(pool.Condition))
				{
					newDynamicPools.Add(pool.Id);
				}
			}

			return newDynamicPools;
		}
		
		private static List<StringId<IPool>> GetRequestedPoolsFromProperties(IReadOnlyList<string> properties)
		{
			List<StringId<IPool>> poolIds = new();
			foreach (string property in properties)
			{
				const string Key = KnownPropertyNames.RequestedPools + "=";
				if (property.StartsWith(Key, StringComparison.InvariantCulture))
				{
					poolIds.AddRange(property[Key.Length..].Split(",").Select(x => new StringId<IPool>(x)));
				}
			}

			return poolIds;
		}
		
		private static List<StringId<IPool>> CombineCurrentAndRequestedPools(IReadOnlyList<StringId<IPool>> pools, IReadOnlyList<string> properties)
		{
			HashSet<StringId<IPool>> uniquePools = new(pools);
			uniquePools.UnionWith(GetRequestedPoolsFromProperties(properties));
			return new List<PoolId>(uniquePools);
		}

		/// <summary>
		/// Callback for an agents 
		/// </summary>
		/// <param name="agentId"></param>
		void OnAgentUpdate(AgentId agentId)
		{
			lock (_waitingAgents)
			{
				if (_waitingAgents.TryGetValue(agentId, out CancellationTokenSource? cancellationSource))
				{
					cancellationSource.Cancel();
				}
			}
		}

		/// <summary>
		/// Creates a new agent session
		/// </summary>
		/// <param name="agent">The agent to create a session for</param>
		/// <param name="status">Current status of the agent</param>
		/// <param name="properties">Properties for the agent</param>
		/// <param name="resources">Resources which the agent has</param>
		/// <param name="version">Version of the software that's running</param>
		/// <returns>New agent state</returns>
		public async Task<IAgent> CreateSessionAsync(IAgent agent, AgentStatus status, IReadOnlyList<string> properties, IReadOnlyDictionary<string, int> resources, string? version)
		{
			for (; ; )
			{
				IAuditLogChannel<AgentId> agentLogger = Agents.GetLogger(agent.Id);

				// Check if there's already a session running for this agent.
				IAgent? newAgent;
				if (agent.SessionId != null)
				{
					// Try to terminate the current session
					await TryTerminateSessionAsync(agent);
				}
				else
				{
					DateTime utcNow = _clock.UtcNow;

					// Remove any outstanding leases
					foreach (AgentLease lease in agent.Leases)
					{
						agentLogger.LogInformation("Removing outstanding lease {LeaseId}", lease.Id);
						await RemoveLeaseAsync(agent, lease, utcNow, LeaseOutcome.Failed, null);
					}

					// Create a new session document
					ISession newSession = await _sessions.AddAsync(SessionId.GenerateNewId(), agent.Id, _clock.UtcNow, properties, resources, version);
					DateTime sessionExpiresAt = utcNow + SessionExpiryTime;

					// Get the new pools for the agent
					List<PoolId> dynamicPools = await GetDynamicPoolsAsync(agent);
					List<PoolId> pools = CombineCurrentAndRequestedPools(agent.ExplicitPools, properties);

					// Reset the agent to use the new session
					newAgent = await Agents.TryStartSessionAsync(agent, newSession.Id, sessionExpiresAt, status, properties, resources, pools, dynamicPools, version);
					if(newAgent != null)
					{
						agent = newAgent;
						agentLogger.LogInformation("Session {SessionId} started", newSession.Id);
						break;
					}

					// Remove the session we didn't use
					await _sessions.DeleteAsync(newSession.Id);
				}

				// Get the current agent state
				newAgent = await GetAgentAsync(agent.Id);
				if (newAgent == null)
				{
					throw new InvalidOperationException($"Invalid agent id '{agent.Id}'");
				}
				agent = newAgent;
			}
			return agent;
		}

		/// <summary>
		/// Determines whether a task source can currently issue tasks
		/// </summary>
		bool CanUseTaskSource(IAgent agent, ITaskSource taskSource)
		{
			TaskSourceFlags flags = taskSource.Flags;
			if ((flags & TaskSourceFlags.AllowWhenDisabled) == 0 && !agent.Enabled)
			{
				return false;
			}
			if ((flags & TaskSourceFlags.AllowDuringDowntime) == 0 && _downtimeService.IsDowntimeActive)
			{
				return false;
			}
			return true;
		}

		/// <summary>
		/// Waits for a lease to be assigned to an agent
		/// </summary>
		/// <param name="agent">The agent to assign a lease to</param>
		/// <param name="cancellationToken"></param>
		/// <returns>True if a lease was assigned, false otherwise</returns>
		public async Task<IAgent?> WaitForLeaseAsync(IAgent? agent, CancellationToken cancellationToken)
		{
			while (agent != null)
			{
				if (!agent.SessionExpiresAt.HasValue)
				{
					break;
				}

				// Check we have some time to wait
				DateTime utcNow = _clock.UtcNow;
				TimeSpan maxWaitTime = (agent.SessionExpiresAt.Value - SessionExpiryTime + SessionLongPollTime) - utcNow;
				if (maxWaitTime <= TimeSpan.Zero)
				{
					break;
				}

				// Create a cancellation token that will expire with the session
				using CancellationTokenSource cancellationSource = CancellationTokenSource.CreateLinkedTokenSource(_applicationLifetime.ApplicationStopping, cancellationToken);
				cancellationSource.CancelAfter(maxWaitTime);

				// Assign a new lease
				(ITaskSource, AgentLease)? result = null;
				try
				{
					// Add the cancellation source to the set of waiting agents
					lock (_waitingAgents)
					{
						_waitingAgents[agent.Id] = cancellationSource;
					}

					// Create all the tasks to wait for
					List<Task<(ITaskSource, AgentLease)?>> tasks = new List<Task<(ITaskSource, AgentLease)?>>();
					foreach (ITaskSource taskSource in _taskSources)
					{
						if (CanUseTaskSource(agent, taskSource) && !cancellationSource.IsCancellationRequested)
						{
							Task<(ITaskSource, AgentLease)?> task = await GuardedAssignLeaseAsync(taskSource, agent, cancellationSource);
							tasks.Add(task);
						}
					}

					// If no task source is valid, just add a delay
					if (tasks.Count == 0)
					{
						await AsyncUtils.DelayNoThrow(maxWaitTime, cancellationToken);
						break;
					}

					// Wait for all the tasks to complete. Once the first task completes it will set the cancellation source, triggering the 
					// others to terminate.
					await Task.WhenAll(tasks);

					// Find the first result
					foreach (Task<(ITaskSource, AgentLease)?> task in tasks)
					{
						(ITaskSource, AgentLease)? taskResult;
						if (task.TryGetResult(out taskResult) && taskResult != null)
						{
							(ITaskSource taskSource, AgentLease taskLease) = taskResult.Value;
							if (result == null)
							{
								result = (taskSource, taskLease);
							}
							else
							{
								await taskSource.CancelLeaseAsync(agent, taskLease.Id, Any.Parser.ParseFrom(taskLease.Payload));
							}
						}
					}
				}
				finally
				{
					lock (_waitingAgents)
					{
						_waitingAgents.Remove(agent.Id);
					}
				}

				// Exit if we didn't find any work to do. It may be that all the task sources returned null, in which case wait for the time period to expire.
				if (result == null)
				{
					if (!cancellationSource.IsCancellationRequested)
					{
						await cancellationSource.Token.AsTask();
					}
					break;
				}

				// Get the resulting lease
				(ITaskSource source, AgentLease lease) = result.Value;

				// Add the new lease to the agent
				IAgent? newAgent = await Agents.TryAddLeaseAsync(agent, lease);
				if (newAgent != null)
				{
					await source.OnLeaseStartedAsync(newAgent, lease.Id, Any.Parser.ParseFrom(lease.Payload), Agents.GetLogger(agent.Id));
					await CreateLeaseAsync(agent, lease);
					return newAgent;
				}

				// Update the agent
				agent = await GetAgentAsync(agent.Id);
			}
			return agent;
		}

		async Task<Task<(ITaskSource, AgentLease)?>> GuardedAssignLeaseAsync(ITaskSource source, IAgent agent, CancellationTokenSource cancellationSource)
		{
			CancellationToken cancellationToken = cancellationSource.Token;
			try
			{
				Task<AgentLease?> task = await source.AssignLeaseAsync(agent, cancellationToken);
				return task.ContinueWith(x => WrapAssignedLease(source, x, cancellationSource), TaskScheduler.Default);
			}
			catch (TaskCanceledException) when (cancellationToken.IsCancellationRequested)
			{
				return Task.FromResult<(ITaskSource, AgentLease)?>(null);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Exception while trying to assign lease");
				return Task.FromResult<(ITaskSource, AgentLease)?>(null);
			}
		}

		(ITaskSource, AgentLease)? WrapAssignedLease(ITaskSource source, Task<AgentLease?> task, CancellationTokenSource cancellationSource)
		{
			if (task.IsCanceled)
			{
				return null;
			}
			else if (task.TryGetResult(out AgentLease? lease))
			{
				if (lease == null)
				{
					return null;
				}
				else
				{
					cancellationSource.Cancel();
					return (source, lease);
				}
			}
			else if (task.IsFaulted)
			{
				_logger.LogError(task.Exception, "Exception while trying to assign lease");
				return null;
			}
			else
			{
				_logger.LogWarning("Unhandled task state: {Status}", task.Status);
				return null;
			}
		}

		/// <summary>
		/// Cancels the specified agent lease
		/// </summary>
		/// <param name="agent">Agent to cancel the lease on</param>
		/// <param name="leaseId">The lease id to cancel</param>
		/// <returns></returns>
		public async Task<bool> CancelLeaseAsync(IAgent agent, LeaseId leaseId)
		{		
			int index = 0;
			while (index < agent.Leases.Count && agent.Leases[index].Id != leaseId)
			{
				index++;
			}

			if (index == agent.Leases.Count)
			{
				return false;
			}

			if (agent.Leases[index].State == LeaseState.Cancelled)
			{
				return false;
			}

			await Agents.TryCancelLeaseAsync(agent, index);
			return true;
		}

		/// <summary>
		/// 
		/// </summary>
		public async Task<IAgent?> UpdateSessionWithWaitAsync(IAgent inAgent, SessionId sessionId, AgentStatus status, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, IList<HordeCommon.Rpc.Messages.Lease> newLeases, CancellationToken cancellationToken)
		{
			IAgent? agent = inAgent;

			// Capture the current agent update index. This allows us to detect if anything has changed.
			uint updateIndex = agent.UpdateIndex;

			// Update the agent session and return to the caller if anything changes
			agent = await UpdateSessionAsync(agent, sessionId, status, properties, resources, newLeases);
			if (agent != null && agent.UpdateIndex == updateIndex && (agent.Leases.Count > 0 || agent.Status != AgentStatus.Stopping))
			{
				agent = await WaitForLeaseAsync(agent, cancellationToken);
			}
			return agent;
		}

		/// <summary>
		/// Updates the state of the current agent session
		/// </summary>
		/// <param name="inAgent">The current agent state</param>
		/// <param name="sessionId">Id of the session</param>
		/// <param name="status">New status for the agent</param>
		/// <param name="properties">New agent properties</param>
		/// <param name="resources">New agent resources</param>
		/// <param name="newLeases">New list of leases for this session</param>
		/// <returns>Updated agent state</returns>
		public async Task<IAgent?> UpdateSessionAsync(IAgent inAgent, SessionId sessionId, AgentStatus status, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, IList<HordeCommon.Rpc.Messages.Lease> newLeases)
		{
			DateTime utcNow = _clock.UtcNow;

			Stopwatch timer = Stopwatch.StartNew();

			IAgent? agent = inAgent;
			while (agent != null)
			{
				// If the agent is stopping and doesn't have any leases, we can terminate the current session.
				if (status == AgentStatus.Stopping && newLeases.Count == 0)
				{
					// If we've already decided to terminate the session, this update is redundant but harmless
					if (agent.SessionId != sessionId)
					{
						break;
					}

					// If the session is valid, we can terminate once the agent leases are also empty
					if (agent.Leases.Count == 0)
					{
						if (!await TryTerminateSessionAsync(agent))
						{
							agent = await GetAgentAsync(agent.Id);
							continue;
						}
						break;
					}
				}

				// Check the session id is correct.
				if (agent.SessionId != sessionId)
				{
					throw new InvalidOperationException($"Invalid agent session {sessionId}");
				}

				// Check the session hasn't expired
				if (!agent.IsSessionValid(utcNow))
				{
					throw new InvalidOperationException("Session has already expired");
				}

				// Extend the current session time if we're within a time period of the current time expiring. This reduces
				// unnecessary DB writes a little, but it also allows us to skip the update and jump into a long poll state
				// if there's still time on the session left.
				DateTime? sessionExpiresAt = null;
				if (!agent.SessionExpiresAt.HasValue || utcNow > (agent.SessionExpiresAt - SessionExpiryTime) + SessionRenewTime)
				{
					sessionExpiresAt = utcNow + SessionExpiryTime;
				}

				// Flag for whether the leases array should be updated
				bool bUpdateLeases = false;
				List<AgentLease> leases = new List<AgentLease>(agent.Leases);

				// Remove any completed leases from the agent
				Dictionary<LeaseId, HordeCommon.Rpc.Messages.Lease> leaseIdToNewState = newLeases.ToDictionary(x => new LeaseId(x.Id), x => x);
				for (int idx = 0; idx < leases.Count; idx++)
				{
					AgentLease lease = leases[idx];
					if (lease.State == LeaseState.Cancelled)
					{
						HordeCommon.Rpc.Messages.Lease? newLease;
						if (!leaseIdToNewState.TryGetValue(lease.Id, out newLease) || newLease.State == LeaseState.Cancelled || newLease.State == LeaseState.Completed)
						{
							await RemoveLeaseAsync(agent, lease, utcNow, LeaseOutcome.Cancelled, null);
							leases.RemoveAt(idx--);
							bUpdateLeases = true;
						}
					}
					else
					{
						HordeCommon.Rpc.Messages.Lease? newLease;
						if (leaseIdToNewState.TryGetValue(lease.Id, out newLease) && newLease.State != lease.State)
						{
							if (newLease.State == LeaseState.Cancelled || newLease.State == LeaseState.Completed)
							{
								await RemoveLeaseAsync(agent, lease, utcNow, newLease.Outcome, newLease.Output.ToByteArray());
								leases.RemoveAt(idx--);
							}
							else if (newLease.State == LeaseState.Active && lease.State == LeaseState.Pending)
							{
								lease.State = LeaseState.Active;
							}
							bUpdateLeases = true;
						}
					}
				}

				// If the agent is stopping, cancel all the leases. Clear out the current session once it's complete.
				if (status == AgentStatus.Stopping)
				{
					foreach (AgentLease lease in leases)
					{
						if (lease.State != LeaseState.Cancelled)
						{
							lease.State = LeaseState.Cancelled;
							bUpdateLeases = true;
						}
					}
				}

				// Get the new dynamic pools for the agent
				List<PoolId> dynamicPools = await GetDynamicPoolsAsync(agent);

				// Update the agent, and try to create new lease documents if we succeed
				IAgent? newAgent = await Agents.TryUpdateSessionAsync(agent, status, sessionExpiresAt, properties, resources, dynamicPools, bUpdateLeases ? leases : null);
				if (newAgent != null)
				{
					agent = newAgent;
					break;
				}

				// Fetch the agent again
				agent = await GetAgentAsync(agent.Id);
			}
			return agent;
		}

		/// <summary>
		/// Terminates an existing session. Does not update the agent itself, if it's currently 
		/// </summary>
		/// <param name="agent">The agent whose current session should be terminated</param>
		/// <returns>True if the session was terminated</returns>
		private async Task<bool> TryTerminateSessionAsync(IAgent agent)
		{
			// Make sure the agent has a valid session id
			if (agent.SessionId == null)
			{
				return true;
			}

			// Get the time that the session finishes at
			DateTime finishTime = _clock.UtcNow;
			if (agent.SessionExpiresAt.HasValue && agent.SessionExpiresAt.Value < finishTime)
			{
				finishTime = agent.SessionExpiresAt.Value;
			}

			// Save off the session id and current leases
			SessionId sessionId = agent.SessionId.Value;
			List<AgentLease> leases = new List<AgentLease>(agent.Leases);

			// Clear the current session
			IAgent? newAgent = await Agents.TryTerminateSessionAsync(agent);
			if (newAgent != null)
			{
				agent = newAgent;

				// Remove any outstanding leases
				foreach(AgentLease lease in leases)
				{
					Agents.GetLogger(agent.Id).LogInformation("Removing lease {LeaseId} during session terminate...", lease.Id);
					await RemoveLeaseAsync(agent, lease, finishTime, LeaseOutcome.Failed, null);
				}

				// Update the session document
				Agents.GetLogger(agent.Id).LogInformation("Terminated session {SessionId}", sessionId);
				await _sessions.UpdateAsync(sessionId, finishTime, agent.Properties, agent.Resources);
				return true;
			}
			return false;
		}

		/// <summary>
		/// Creates a new lease document
		/// </summary>
		/// <param name="agent">Agent that will be executing the lease</param>
		/// <param name="agentLease">The new agent lease</param>
		/// <returns>New lease document</returns>
		private Task<ILease> CreateLeaseAsync(IAgent agent, AgentLease agentLease)
		{
			try
			{
				return _leases.AddAsync(agentLease.Id, agentLease.Name, agent.Id, agent.SessionId!.Value, agentLease.StreamId, agentLease.PoolId, agentLease.LogId, agentLease.StartTime, agentLease.Payload!);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Unable to create lease {LeaseId} for agent {AgentId}; lease already exists?", agentLease.Id, agent.Id);
				throw;
			}
		}

		/// <summary>
		/// Finds all leases matching a set of criteria
		/// </summary>
		/// <param name="agentId">Unqiue id of the agent executing this lease</param>
		/// <param name="sessionId">Unique id of the agent session</param>
		/// <param name="startTime">Start of the search window to return results for</param>
		/// <param name="finishTime">End of the search window to return results for</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of leases matching the given criteria</returns>
		public Task<List<ILease>> FindLeasesAsync(AgentId? agentId, SessionId? sessionId, DateTime? startTime, DateTime? finishTime, int index, int count)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan($"{nameof(AgentService)}.{nameof(FindLeasesAsync)}").StartActive();
			scope.Span.SetTag("AgentId", agentId?.ToString());
			scope.Span.SetTag("SessionId", sessionId?.ToString());
			scope.Span.SetTag("StartTime", startTime?.ToString());
			scope.Span.SetTag("FinishTime", finishTime?.ToString());
			scope.Span.SetTag("Index", index);
			scope.Span.SetTag("Count", count);
			return _leases.FindLeasesAsync(agentId, sessionId, startTime, finishTime, index, count);
		}

		/// <summary>
		/// Finds all leases by finish time
		/// </summary>
		/// <param name="minFinishTime">Start of the search window to return results for</param>
		/// <param name="maxFinishTime">End of the search window to return results for</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of leases matching the given criteria</returns>
		public Task<List<ILease>> FindLeasesByFinishTimeAsync(DateTime? minFinishTime, DateTime? maxFinishTime, int? index, int? count)
		{
			return _leases.FindLeasesByFinishTimeAsync(minFinishTime, maxFinishTime, index, count, null, false);
		}

		/// <summary>
		/// Gets a specific lease
		/// </summary>
		/// <param name="leaseId">Unique id of the lease</param>
		/// <returns>The lease that was found, or null if it does not exist</returns>
		public Task<ILease?> GetLeaseAsync(LeaseId leaseId)
		{
			return _leases.GetAsync(leaseId);
		}

		/// <summary>
		/// Removes a lease with the given id. Updates the lease state in the database, and removes the item from the agent's leases array.
		/// </summary>
		/// <param name="agent">The agent to remove a lease from</param>
		/// <param name="lease">The lease to cancel</param>
		/// <param name="utcNow">The current time</param>
		/// <param name="outcome">Final status of the lease</param>
		/// <param name="output">Output from executing the task</param>
		/// <returns>Async task</returns>
		private async Task RemoveLeaseAsync(IAgent agent, AgentLease lease, DateTime utcNow, LeaseOutcome outcome, byte[]? output)
		{
			// Make sure the lease is terminated correctly
			if (lease.Payload == null)
			{
				_logger.LogWarning("Removing lease {LeaseId} (no payload)", lease.Id);
			}
			else
			{
				Any any = Any.Parser.ParseFrom(lease.Payload);
				_logger.LogInformation("Removing lease {LeaseId} ({LeaseType})", lease.Id, any.TypeUrl);

				foreach (ITaskSource taskSource in _taskSources)
				{
					if (any.Is(taskSource.Descriptor))
					{
						await taskSource.OnLeaseFinishedAsync(agent, lease.Id, any, outcome, output, Agents.GetLogger(agent.Id));
						break;
					}
				}
			}

			// Figure out what time the lease finished
			DateTime finishTime = utcNow;
			if (agent.SessionExpiresAt.HasValue && agent.SessionExpiresAt.Value < finishTime)
			{
				finishTime = agent.SessionExpiresAt.Value;
			}
			if (lease.ExpiryTime.HasValue && lease.ExpiryTime.Value < finishTime)
			{
				finishTime = lease.ExpiryTime.Value;
			}

			// Update the lease
			await _leases.TrySetOutcomeAsync(lease.Id, finishTime, outcome, output);
		}

		/// <summary>
		/// Gets information about a particular session
		/// </summary>
		/// <param name="sessionId">The unique session id</param>
		/// <returns>The session information</returns>
		public Task<ISession?> GetSessionAsync(SessionId sessionId)
		{
			return _sessions.GetAsync(sessionId);
		}

		/// <summary>
		/// Find sessions for the given agent
		/// </summary>
		/// <param name="agentId">The unique agent id</param>
		/// <param name="startTime">Start time to include in the search</param>
		/// <param name="finishTime">Finish time to include in the search</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <returns>List of sessions matching the given criteria</returns>
		public Task<List<ISession>> FindSessionsAsync(AgentId agentId, DateTime? startTime, DateTime? finishTime, int index, int count)
		{
			return _sessions.FindAsync(agentId, startTime, finishTime, index, count);
		}

		/// <summary>
		/// Update the rate table for different agent types
		/// </summary>
		/// <param name="entries">New entries for the rate table</param>
		/// <returns></returns>
		public async Task UpdateRateTableAsync(List<AgentRateConfig> entries)
		{
			await _agentRateTableData.SetAsync(new AgentRateTable { Entries = entries });
		}

		/// <summary>
		/// Gets the rate for the given agent
		/// </summary>
		/// <param name="agentId">Agent id to query</param>
		/// <returns>Hourly rate of running the given agent</returns>
		public async ValueTask<double?> GetRateAsync(AgentId agentId)
		{
			RedisKey key = $"agent-rate/{agentId}";

			// Try to get the current value
			RedisValue value = await _redisService.GetDatabase().StringGetAsync(key);
			if (!value.IsNull)
			{
				double rate = (double)value;
				if (rate == 0.0)
				{
					return null;
				}
				else
				{
					return rate;
				}
			}
			else
			{
				double rate = 0.0;

				// Get the rate table
				AgentRateTable? rateTable = await _agentRateTable.GetAsync();
				if (rateTable != null && rateTable.Entries.Count > 0)
				{
					IAgent? agent = await GetAgentAsync(agentId);
					if (agent != null)
					{
						foreach (AgentRateConfig config in rateTable.Entries)
						{
							if (config.Condition != null && config.Condition.Evaluate(x => agent.GetPropertyValues(x)))
							{
								rate = config.Rate;
								break;
							}
						}
					}
				}

				// Cache it for future reference
				await _redisService.GetDatabase().StringSetAsync(key, rate, TimeSpan.FromMinutes(5.0), flags: CommandFlags.FireAndForget);
				return rate;
			}
		}

		/// <summary>
		/// Terminate any sessions for agents that are offline
		/// </summary>
		/// <param name="stoppingToken">Token indicating the service is shutting down</param>
		/// <returns>Async task</returns>
		async ValueTask TickAsync(CancellationToken stoppingToken)
		{
			while (!stoppingToken.IsCancellationRequested)
			{
				// Find all the agents which are ready to be expired
				const int MaxAgents = 100;
				DateTime utcNow = _clock.UtcNow;
				List<IAgent> expiredAgents = await Agents.FindExpiredAsync(utcNow, MaxAgents);

				// Transition each agent to being offline
				foreach (IAgent expiredAgent in expiredAgents)
				{
					stoppingToken.ThrowIfCancellationRequested();
					_logger.LogDebug("Terminating session {SessionId} for agent {Agent}", expiredAgent.SessionId, expiredAgent.Id);
					await TryTerminateSessionAsync(expiredAgent);
				}

				// Try again if we didn't fetch everything
				if(expiredAgents.Count < MaxAgents)
				{
					break;
				}
			}

			await CollectMetrics();
		}

		private async Task CollectMetrics()
		{
			List<IAgent> agentList = await Agents.FindAsync();
			int numAgentsTotal = agentList.Count;
			int numAgentsTotalEnabled = agentList.Count(a => a.Enabled);
			int numAgentsTotalDisabled = agentList.Count(a => !a.Enabled);
			int numAgentsTotalOk = agentList.Count(a => a.Enabled && a.Status == AgentStatus.Ok);
			int numAgentsTotalStopping = agentList.Count(a => a.Enabled && a.Status == AgentStatus.Stopping);
			int numAgentsTotalUnhealthy = agentList.Count(a => a.Enabled && a.Status == AgentStatus.Unhealthy);
			int numAgentsTotalUnspecified = agentList.Count(a => a.Enabled && a.Status == AgentStatus.Unspecified);
			
			// TODO: utilize tags argument in a smarter way below
			_dogStatsd.Gauge("agents.total.count", numAgentsTotal);
			_dogStatsd.Gauge("agents.total.enabled.count", numAgentsTotalEnabled);
			_dogStatsd.Gauge("agents.total.disabled.count", numAgentsTotalDisabled);
			_dogStatsd.Gauge("agents.total.ok.count", numAgentsTotalOk);
			_dogStatsd.Gauge("agents.total.stopping.count", numAgentsTotalStopping);
			_dogStatsd.Gauge("agents.total.unhealthy.count", numAgentsTotalUnhealthy);
			_dogStatsd.Gauge("agents.total.unspecified.count", numAgentsTotalUnspecified);
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular agent
		/// </summary>
		/// <param name="agent">The agent to check</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to authorize</param>
		/// <param name="cache">The permissions cache</param>
		/// <returns>True if the action is authorized</returns>
		public async Task<bool> AuthorizeAsync(IAgent agent, AclAction action, ClaimsPrincipal user, GlobalPermissionsCache? cache)
		{
			bool? result = agent.Acl?.Authorize(action, user);
			if (result == null)
			{
				return await _aclService.AuthorizeAsync(action, user, cache);
			}
			else
			{
				return result.Value;
			}
		}

		/// <summary>
		/// Determines if the user is authorized to perform an action on a particular agent
		/// </summary>
		/// <param name="agent">The agent to check</param>
		/// <param name="user">The principal to authorize</param>
		/// <returns>True if the action is authorized</returns>
		public bool AuthorizeSession(IAgent agent, ClaimsPrincipal user)
		{
			if (agent.SessionId != null && user.HasSessionClaim(agent.SessionId.Value) && agent.IsSessionValid(_clock.UtcNow))
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
}
