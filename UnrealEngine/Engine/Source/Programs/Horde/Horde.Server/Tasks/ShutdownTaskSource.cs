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
	class ShutdownTaskSource : TaskSourceBase<ShutdownTask>
	{
		public override string Type => "Shutdown";

		public override TaskSourceFlags Flags => TaskSourceFlags.AllowWhenDisabled | TaskSourceFlags.AllowDuringDowntime;

		readonly ILogFileService _logService;

		public ShutdownTaskSource(ILogFileService logService)
		{
			_logService = logService;
			OnLeaseStartedProperties.Add(nameof(ShutdownTask.LogId), x => LogId.Parse(x.LogId));
		}

		public override async Task<Task<AgentLease?>> AssignLeaseAsync(IAgent agent, CancellationToken cancellationToken)
		{
			if (!agent.RequestShutdown)
			{
				return SkipAsync(cancellationToken);
			}
			if (agent.Leases.Count > 0)
			{
				return await DrainAsync(cancellationToken);
			}

			LeaseId leaseId = new LeaseId(BinaryIdUtils.CreateNew());
			ILogFile log = await _logService.CreateLogFileAsync(JobId.Empty, leaseId, agent.SessionId, LogType.Json, cancellationToken: cancellationToken);

			ShutdownTask task = new ShutdownTask();
			task.LogId = log.Id.ToString();

			byte[] payload = Any.Pack(task).ToByteArray();

			return LeaseAsync(new AgentLease(leaseId, null, "Shutdown", null, null, log.Id, LeaseState.Pending, null, true, payload));
		}
	}
}
