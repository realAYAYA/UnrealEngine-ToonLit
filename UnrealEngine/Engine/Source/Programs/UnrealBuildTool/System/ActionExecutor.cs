// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;

namespace UnrealBuildTool
{
	abstract class ActionExecutor
	{
		public abstract string Name
		{
			get;
		}

		static protected double MemoryPerActionBytesOverride
		{
			get;
			private set;
		} = 0.0;

		/// <summary>
		/// Allow targets to override the expected amount of memory required for compiles, used to control the number
		/// of parallel action processes.
		/// </summary>
		/// <param name="MemoryPerActionOverrideGB"></param>
		public static void SetMemoryPerActionOverride(double MemoryPerActionOverrideGB)
		{
			MemoryPerActionBytesOverride = Math.Max(MemoryPerActionBytesOverride, MemoryPerActionOverrideGB * 1024 * 1024 * 1024);
		}

		public abstract bool ExecuteActions(List<LinkedAction> ActionsToExecute, ILogger Logger);
	}

}
