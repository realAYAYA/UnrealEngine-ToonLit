// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Tools;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Horde.Server.Agents;
using Horde.Server.Logs;
using Horde.Server.Server;
using Horde.Server.Tools;
using Horde.Server.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Options;

namespace Horde.Server.Tasks
{
	class UpgradeTaskSource : TaskSourceBase<UpgradeTask>
	{
		public override string Type => "Upgrade";

		public override TaskSourceFlags Flags => TaskSourceFlags.AllowWhenDisabled | TaskSourceFlags.AllowDuringDowntime;

		readonly IToolCollection _toolCollection;
		readonly ILogFileService _logService;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly IOptions<ServerSettings> _serverSettings;
		readonly IClock _clock;

		public UpgradeTaskSource(IToolCollection toolCollection, ILogFileService logService, IOptionsMonitor<GlobalConfig> globalConfig, IOptions<ServerSettings> serverSettings, IClock clock)
		{
			_toolCollection = toolCollection;
			_logService = logService;
			_globalConfig = globalConfig;
			_serverSettings = serverSettings;
			_clock = clock;

			OnLeaseStartedProperties.Add(nameof(UpgradeTask.LogId), x => LogId.Parse(x.LogId));
		}

		public override async Task<Task<AgentLease?>> AssignLeaseAsync(IAgent agent, CancellationToken cancellationToken)
		{
			if (!_serverSettings.Value.EnableUpgradeTasks)
			{
				return SkipAsync(cancellationToken);
			}

			(ITool, IToolDeployment)? required = await GetRequiredSoftwareVersionAsync(agent, cancellationToken);
			if (required == null)
			{
				return SkipAsync(cancellationToken);
			}

			(ITool tool, IToolDeployment deployment) = required.Value;
			if (agent.Version == deployment.Version)
			{
				return SkipAsync(cancellationToken);
			}

			if (agent.Leases.Count > 0 || (agent.LastUpgradeTime != null && agent.LastUpgradeVersion == deployment.Version && _clock.UtcNow < agent.LastUpgradeTime.Value + TimeSpan.FromMinutes(5.0)))
			{
				return await DrainAsync(cancellationToken);
			}

			LeaseId leaseId = new LeaseId(BinaryIdUtils.CreateNew());
			ILogFile logFile = await _logService.CreateLogFileAsync(JobId.Empty, leaseId, agent.SessionId, LogType.Json, cancellationToken: cancellationToken);

			UpgradeTask task = new UpgradeTask();
			task.SoftwareId = $"{tool.Id}:{deployment.Version}";
			task.LogId = logFile.Id.ToString();

			byte[] payload = Any.Pack(task).ToByteArray();
			return LeaseAsync(new AgentLease(leaseId, null, $"Upgrade to {tool.Id} {deployment.Version}", null, null, logFile.Id, LeaseState.Pending, null, true, payload));
		}

		/// <summary>
		/// Determines the client software version that should be installed on an agent
		/// </summary>
		/// <param name="agent">The agent instance</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Unique id of the client version this agent should be running</returns>
		public async Task<(ITool, IToolDeployment)?> GetRequiredSoftwareVersionAsync(IAgent agent, CancellationToken cancellationToken)
		{
			GlobalConfig globalConfig = _globalConfig.CurrentValue;

			ToolId toolId = agent.GetSoftwareToolId(globalConfig);

			ITool? tool = await _toolCollection.GetAsync(toolId, globalConfig, cancellationToken);
			if (tool == null)
			{
				return null;
			}

			uint value = BuzHash.Add(0, Encoding.UTF8.GetBytes(agent.Id.ToString()));
			double phase = (value % 10000) / 10000.0;

			IToolDeployment? deployment = tool.GetCurrentDeployment(phase, _clock.UtcNow);
			if (deployment == null)
			{
				return null;
			}

			return (tool, deployment);
		}
	}
}
