// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Horde.Server.Agents;
using Horde.Server.Logs;
using Horde.Server.Utilities;
using HordeCommon.Rpc.Tasks;

namespace Horde.Server.Tasks
{
	class RestartTaskSource : TaskSourceBase<RestartTask>
	{
		public override string Type => "Restart";

		public override TaskSourceFlags Flags => TaskSourceFlags.AllowWhenDisabled | TaskSourceFlags.AllowDuringDowntime;

		readonly ILogFileService _logService;

		public RestartTaskSource(ILogFileService logService)
		{
			_logService = logService;

			OnLeaseStartedProperties.Add(nameof(RestartTask.LogId), x => LogId.Parse(x.LogId));
		}

		public override async Task<Task<AgentLease?>> AssignLeaseAsync(IAgent agent, CancellationToken cancellationToken)
		{
			if (!agent.RequestForceRestart)
			{
				if (!agent.RequestRestart)
				{
					return SkipAsync(cancellationToken);
				}
				if (agent.Leases.Count > 0)
				{
					return await DrainAsync(cancellationToken);
				}
			}

			LeaseId leaseId = new LeaseId(BinaryIdUtils.CreateNew());
			ILogFile log = await _logService.CreateLogFileAsync(JobId.Empty, leaseId, agent.SessionId, LogType.Json, cancellationToken: cancellationToken);

			RestartTask task = new RestartTask();
			task.LogId = log.Id.ToString();

			byte[] payload = Any.Pack(task).ToByteArray();

			return LeaseAsync(new AgentLease(leaseId, null, "Restart", null, null, log.Id, LeaseState.Pending, null, true, payload));
		}
	}
}
