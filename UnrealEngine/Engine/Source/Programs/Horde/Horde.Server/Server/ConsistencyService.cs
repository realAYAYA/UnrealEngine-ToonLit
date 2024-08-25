// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using Horde.Server.Agents;
using Horde.Server.Agents.Leases;
using Horde.Server.Agents.Sessions;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Server
{
	/// <summary>
	/// Service which checks the database for consistency and fixes up any errors
	/// </summary>
	class ConsistencyService : IHostedService, IAsyncDisposable
	{
		readonly IAgentCollection _agentCollection;
		readonly ISessionCollection _sessionCollection;
		readonly ILeaseCollection _leaseCollection;
		readonly IClock _clock;
		readonly ITicker _ticker;
		readonly ILogger<ConsistencyService> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public ConsistencyService(IAgentCollection agentCollection, ISessionCollection sessionCollection, ILeaseCollection leaseCollection, IClock clock, ILogger<ConsistencyService> logger)
		{
			_agentCollection = agentCollection;
			_sessionCollection = sessionCollection;
			_leaseCollection = leaseCollection;
			_clock = clock;
			_ticker = clock.AddSharedTicker<ConsistencyService>(TimeSpan.FromMinutes(20.0), TickLeaderAsync, logger);
			_logger = logger;
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public async ValueTask DisposeAsync() => await _ticker.DisposeAsync();

		/// <summary>
		/// Poll for inconsistencies in the database
		/// </summary>
		/// <param name="cancellationToken">Stopping token</param>
		/// <returns>Async task</returns>
		async ValueTask TickLeaderAsync(CancellationToken cancellationToken)
		{
			_logger.LogInformation("Ticking consistency service...");

			// Find all the active sessions
			List<ISession> sessions = await _sessionCollection.FindActiveSessionsAsync(cancellationToken: cancellationToken);
			Dictionary<SessionId, ISession> sessionIdToInstance = sessions.ToDictionary(x => x.Id, x => x);

			// Find all the active agents
			IReadOnlyList<IAgent> agents = await _agentCollection.FindAsync(status: AgentStatus.Ok, cancellationToken: cancellationToken);
			Dictionary<AgentId, IAgent> agentIdToInstance = agents.ToDictionary(x => x.Id, x => x);

			// Find any sessions that do not have a finish time despite their agents running something else
			DateTime utcNow = _clock.UtcNow;
			foreach (ISession session in sessions)
			{
				if (!agentIdToInstance.TryGetValue(session.AgentId, out IAgent? agent) || agent.SessionId != session.Id)
				{
					agent = await _agentCollection.GetAsync(session.AgentId, cancellationToken);
					if (agent == null || agent.SessionId != session.Id)
					{
						_logger.LogWarning("Forcing agent {AgentId} session {SessionId} to complete.", session.AgentId, session.Id);
						await _sessionCollection.UpdateAsync(session.Id, utcNow, null, null, cancellationToken);
						sessionIdToInstance.Remove(session.Id);
					}
				}
			}

			// Find any leases that are still running when their session has terminated
			IReadOnlyList<ILease> leases = await _leaseCollection.FindActiveLeasesAsync(cancellationToken: cancellationToken);
			foreach (ILease lease in leases)
			{
				if (!sessionIdToInstance.ContainsKey(lease.SessionId))
				{
					ISession? session = await _sessionCollection.GetAsync(lease.SessionId, cancellationToken);
					DateTime finishTime = session?.FinishTime ?? DateTime.UtcNow;
					_logger.LogWarning("Setting finish time for lease {LeaseId} to {FinishTime}", lease.Id, finishTime);
					await _leaseCollection.TrySetOutcomeAsync(lease.Id, finishTime, LeaseOutcome.Cancelled, null, cancellationToken);
				}
			}
		}
	}
}
