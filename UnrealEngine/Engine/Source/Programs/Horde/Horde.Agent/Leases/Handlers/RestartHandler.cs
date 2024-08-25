// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents.Leases;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Leases.Handlers
{
	class RestartHandler : LeaseHandler<RestartTask>
	{
		/// <inheritdoc/>
		public override Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, RestartTask task, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("Scheduling restart task for agent {AgentId}", session.AgentId);
			SessionResult result = new SessionResult((logger, ctx) => Shutdown.ExecuteAsync(true, logger, ctx));
			return Task.FromResult(new LeaseResult(result));
		}
	}
}
