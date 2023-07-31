// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using HordeCommon;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Execution.Interfaces
{
	interface IExecutor
	{
		Task InitializeAsync(ILogger logger, CancellationToken cancellationToken);
		Task<JobStepOutcome> RunAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken);
		Task FinalizeAsync(ILogger logger, CancellationToken cancellationToken);
	}
}
