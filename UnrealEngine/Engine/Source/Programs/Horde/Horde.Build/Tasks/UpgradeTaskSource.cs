// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Horde.Build.Agents;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Software;
using Horde.Build.Jobs;
using Horde.Build.Logs;
using Horde.Build.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;

namespace Horde.Build.Tasks
{
	using AgentSoftwareChannelName = StringId<AgentSoftwareChannels>;
	using JobId = ObjectId<IJob>;
	using LeaseId = ObjectId<ILease>;
	using LogId = ObjectId<ILogFile>;

	class UpgradeTaskSource : TaskSourceBase<UpgradeTask>
	{
		public override string Type => "Upgrade";

		public override TaskSourceFlags Flags => TaskSourceFlags.AllowWhenDisabled | TaskSourceFlags.AllowDuringDowntime;

		readonly AgentSoftwareService _agentSoftwareService;
		readonly ILogFileService _logService;
		readonly IClock _clock;

		public UpgradeTaskSource(AgentSoftwareService agentSoftwareService, ILogFileService logService, IClock clock)
		{
			_agentSoftwareService = agentSoftwareService;
			_logService = logService;
			_clock = clock;

			OnLeaseStartedProperties.Add(nameof(UpgradeTask.LogId), x => new LogId(x.LogId));
		}

		public override async Task<Task<AgentLease?>> AssignLeaseAsync(IAgent agent, CancellationToken cancellationToken)
		{
			string? requiredVersion = await GetRequiredSoftwareVersion(agent);
			if (requiredVersion == null || agent.Version == requiredVersion)
			{
				return Skip(cancellationToken);
			}
			if (agent.Leases.Count > 0 || !(agent.LastUpgradeTime == null || agent.LastUpgradeTime.Value + TimeSpan.FromMinutes(5.0) < _clock.UtcNow || agent.LastUpgradeVersion != requiredVersion.ToString()))
			{
				return await DrainAsync(cancellationToken);
			}

			ILogFile logFile = await _logService.CreateLogFileAsync(JobId.Empty, agent.SessionId, LogType.Json);

			UpgradeTask task = new UpgradeTask();
			task.SoftwareId = requiredVersion.ToString();
			task.LogId = logFile.Id.ToString();

			byte[] payload;
			if (agent.Version == "5.0.0-17425336" || agent.Version == "5.0.0-17448746")
			{
				Any any = new Any();
				any.TypeUrl = "type.googleapis.com/Horde.UpgradeTask";
				any.Value = task.ToByteString();
				payload = any.ToByteArray();
			}
			else
			{
				payload = Any.Pack(task).ToByteArray();
			}

			return Lease(new AgentLease(LeaseId.GenerateNewId(), $"Upgrade to {requiredVersion}", null, null, logFile.Id, LeaseState.Pending, null, true, payload));
		}

		/// <summary>
		/// Determines the client software version that should be installed on an agent
		/// </summary>
		/// <param name="agent">The agent instance</param>
		/// <returns>Unique id of the client version this agent should be running</returns>
		public async Task<string?> GetRequiredSoftwareVersion(IAgent agent)
		{
			AgentSoftwareChannelName channelName = agent.Channel ?? AgentSoftwareService.DefaultChannelName;
			IAgentSoftwareChannel? channel = await _agentSoftwareService.GetCachedChannelAsync(channelName);
			return channel?.Version;
		}
	}
}
