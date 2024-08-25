// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Logs;
using Horde.Agent.Execution;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Leases.Handlers
{
	class ConformHandler : LeaseHandler<ConformTask>
	{
		readonly AgentSettings _settings;
		readonly IServerLoggerFactory _serverLoggerFactory;

		/// <summary>
		/// Constructor
		/// </summary>
		public ConformHandler(IOptions<AgentSettings> settings, IServerLoggerFactory serverLoggerFactory)
		{
			_settings = settings.Value;
			_serverLoggerFactory = serverLoggerFactory;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, ConformTask conformTask, ILogger localLogger, CancellationToken cancellationToken)
		{
			await using IServerLogger serverLogger = _serverLoggerFactory.CreateLogger(session, LogId.Parse(conformTask.LogId), localLogger, null);
			try
			{
				LeaseResult result = await ExecuteInternalAsync(session, leaseId, conformTask, serverLogger, cancellationToken);
				return result;
			}
			catch (Exception ex)
			{
				serverLogger.LogError(ex, "Unhandled exception while running conform: {Message}", ex.Message);
				throw;
			}
		}

		async Task<LeaseResult> ExecuteInternalAsync(ISession session, LeaseId leaseId, ConformTask conformTask, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("Conforming, lease {LeaseId}", leaseId);
			await session.TerminateProcessesAsync(TerminateCondition.BeforeConform, logger, cancellationToken);

			bool removeUntrackedFiles = conformTask.RemoveUntrackedFiles;
			IList<AgentWorkspace> pendingWorkspaces = conformTask.Workspaces;
			for (; ; )
			{
				bool isPerforceExecutor = _settings.Executor.Equals(PerforceExecutor.Name, StringComparison.OrdinalIgnoreCase);
				bool isWorkspaceExecutor = _settings.Executor.Equals(WorkspaceExecutor.Name, StringComparison.OrdinalIgnoreCase);

				// When using WorkspaceExecutor, only job options can override exact materializer to use
				// It will default to ManagedWorkspaceMaterializer, which is compatible with the conform call below
				// Therefore, compatibility is assumed for now. Exact materializer to use should be changed to a per workspace setting.
				// See WorkspaceExecutorFactory.CreateExecutor
				bool isExecutorConformCompatible = isPerforceExecutor || isWorkspaceExecutor;

				// Run the conform task
				if (isExecutorConformCompatible && _settings.PerforceExecutor.RunConform)
				{
					await PerforceExecutor.ConformAsync(session.WorkingDir, pendingWorkspaces, removeUntrackedFiles, logger, cancellationToken);
				}
				else
				{
					logger.LogInformation("Skipping conform. Executor={Executor} RunConform={RunConform}", _settings.Executor, _settings.PerforceExecutor.RunConform);
				}

				// Update the new set of workspaces
				UpdateAgentWorkspacesRequest request = new UpdateAgentWorkspacesRequest();
				request.AgentId = session.AgentId.ToString();
				request.Workspaces.AddRange(pendingWorkspaces);
				request.RemoveUntrackedFiles = removeUntrackedFiles;

				UpdateAgentWorkspacesResponse response = await session.RpcConnection.InvokeAsync((HordeRpc.HordeRpcClient x) => x.UpdateAgentWorkspacesAsync(request, null, null, cancellationToken), cancellationToken);
				if (!response.Retry)
				{
					logger.LogInformation("Conform finished");
					break;
				}

				logger.LogInformation("Pending workspaces have changed - running conform again...");
				pendingWorkspaces = response.PendingWorkspaces;
				removeUntrackedFiles = response.RemoveUntrackedFiles;
			}

			return LeaseResult.Success;
		}
	}
}

