// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using Horde.Agent.Services;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Leases.Handlers
{
	class RestartHandler : LeaseHandler<RestartTask>
	{
		readonly ILogger _logger;

		public RestartHandler(ILogger<RestartHandler> logger)
		{
			_logger = logger;
		}

		/// <inheritdoc/>
		public override Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, RestartTask task, CancellationToken cancellationToken)
		{
			_logger.LogInformation("Setting shutdown flag");
			return Task.FromResult(new LeaseResult(SessionResult.Shutdown));
		}
	}
}
