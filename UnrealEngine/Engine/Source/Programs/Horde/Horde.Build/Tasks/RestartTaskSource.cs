// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Horde.Build.Agents;
using Horde.Build.Agents.Leases;
using Horde.Build.Jobs;
using Horde.Build.Logs;
using Horde.Build.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;

namespace Horde.Build.Tasks
{
	using JobId = ObjectId<IJob>;
	using LeaseId = ObjectId<ILease>;
	using LogId = ObjectId<ILogFile>;

	class RestartTaskSource : TaskSourceBase<RestartTask>
	{
		public override string Type => "Restart";

		public override TaskSourceFlags Flags => TaskSourceFlags.AllowWhenDisabled | TaskSourceFlags.AllowDuringDowntime;

		readonly ILogFileService _logService;

		public RestartTaskSource(ILogFileService logService)
		{
			_logService = logService;

			OnLeaseStartedProperties.Add(nameof(RestartTask.LogId), x => new LogId(x.LogId));
		}

		public override async Task<Task<AgentLease?>> AssignLeaseAsync(IAgent agent, CancellationToken cancellationToken)
		{
			if (!agent.RequestRestart)
			{
				return Skip(cancellationToken);
			}
			if (agent.Leases.Count > 0)
			{
				return await DrainAsync(cancellationToken);
			}

			ILogFile log = await _logService.CreateLogFileAsync(JobId.Empty, agent.SessionId, LogType.Json);

			RestartTask task = new RestartTask();
			task.LogId = log.Id.ToString();

			byte[] payload = Any.Pack(task).ToByteArray();

			return Lease(new AgentLease(LeaseId.GenerateNewId(), "Restart", null, null, log.Id, LeaseState.Pending, null, true, payload));
		}
	}
}
