// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Compute;
using EpicGames.Horde.Storage;

namespace HordeCommon.Rpc.Tasks
{
	/// <summary>
	/// 
	/// </summary>
	partial class ComputeTaskResultMessage
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeTaskResultMessage(RefId resultRefId, ComputeTaskExecutionStatsMessage? executionStats)
		{
			ResultRefId = resultRefId;
			ExecutionStats = executionStats;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ComputeTaskResultMessage(ComputeTaskOutcome outcome, string? detail = null)
		{
			Outcome = (int)outcome;
			Detail = detail;
		}
	}

	/// <summary>
	/// 
	/// </summary>
	partial class ComputeTaskExecutionStatsMessage
	{
		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public ComputeTaskExecutionStats ToNative()
		{
			return new ComputeTaskExecutionStats(StartTime.ToDateTime(), DownloadRefMs, DownloadInputMs, ExecMs, UploadLogMs, UploadOutputMs, UploadRefMs);
		}
	}
}
