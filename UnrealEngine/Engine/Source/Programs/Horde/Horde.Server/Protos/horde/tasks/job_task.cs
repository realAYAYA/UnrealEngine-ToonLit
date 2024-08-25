// Copyright Epic Games, Inc. All Rights Reserved.

namespace HordeCommon.Rpc.Tasks
{
	partial class JobOptions
	{
		/// <summary>
		/// Merge settings from another JobOptions object
		/// </summary>
		public void MergeDefaults(JobOptions other)
		{
			Executor ??= other.Executor;
			UseNewTempStorage ??= other.UseNewTempStorage;
		}
	}
}
