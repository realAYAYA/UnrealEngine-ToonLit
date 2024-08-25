// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;
using UnrealBuildTool.Artifacts;

namespace UnrealBuildTool
{

	/// <summary>
	/// This executor uses async Tasks to process the action graph
	/// </summary>
	class ParallelExecutor : ActionExecutor
	{
		/// <summary>
		/// Maximum processor count for local execution. 
		/// </summary>
		[XmlConfigFile]
		[Obsolete("ParallelExecutor.MaxProcessorCount is deprecated. Please update xml to use BuildConfiguration.MaxParallelActions")]
#pragma warning disable 0169
		private static int MaxProcessorCount;
#pragma warning restore 0169

		/// <summary>
		/// Processor count multiplier for local execution. Can be below 1 to reserve CPU for other tasks.
		/// When using the local executor (not XGE), run a single action on each CPU core. Note that you can set this to a larger value
		/// to get slightly faster build times in many cases, but your computer's responsiveness during compiling may be much worse.
		/// This value is ignored if the CPU does not support hyper-threading.
		/// </summary>
		[XmlConfigFile]
		private static double ProcessorCountMultiplier = 1.0;

		/// <summary>
		/// Free memory per action in bytes, used to limit the number of parallel actions if the machine is memory starved.
		/// Set to 0 to disable free memory checking.
		/// </summary>
		[XmlConfigFile]
		private static double MemoryPerActionBytes = 1.5 * 1024 * 1024 * 1024;

		/// <summary>
		/// The priority to set for spawned processes.
		/// Valid Settings: Idle, BelowNormal, Normal, AboveNormal, High
		/// Default: BelowNormal or Normal for an Asymmetrical processor as BelowNormal can cause scheduling issues.
		/// </summary>
		[XmlConfigFile]
		protected static ProcessPriorityClass ProcessPriority = Utils.IsAsymmetricalProcessor() ? ProcessPriorityClass.Normal : ProcessPriorityClass.BelowNormal;

		/// <summary>
		/// When enabled, will stop compiling targets after a compile error occurs.
		/// </summary>
		[XmlConfigFile]
		private static bool bStopCompilationAfterErrors = false;

		/// <summary>
		/// Whether to show compilation times along with worst offenders or not.
		/// </summary>
		[XmlConfigFile]
		private static bool bShowCompilationTimes = Unreal.IsBuildMachine();

		/// <summary>
		/// Whether to show compilation times for each executed action
		/// </summary>
		[XmlConfigFile]
		private static bool bShowPerActionCompilationTimes = Unreal.IsBuildMachine();

		/// <summary>
		/// Whether to log command lines for actions being executed
		/// </summary>
		[XmlConfigFile]
		private static bool bLogActionCommandLines = false;

		/// <summary>
		/// Add target names for each action executed
		/// </summary>
		[XmlConfigFile]
		private static bool bPrintActionTargetNames = false;

		/// <summary>
		/// Whether to take into account the Action's weight when determining to do more work or not.
		/// </summary>
		[XmlConfigFile]
		protected static bool bUseActionWeights = false;

		/// <summary>
		/// Whether to show CPU utilization after the work is complete.
		/// </summary>
		[XmlConfigFile]
		protected static bool bShowCPUUtilization = Unreal.IsBuildMachine();

		/// <summary>
		/// Collapse non-error output lines
		/// </summary>
		private bool bCompactOutput = false;

		/// <summary>
		/// How many processes that will be executed in parallel
		/// </summary>
		public int NumParallelProcesses { get; private set; }

		private static readonly char[] LineEndingSplit = new char[] { '\n', '\r' };

		public static int GetDefaultNumParallelProcesses(int MaxLocalActions, bool bAllCores, ILogger Logger)
		{
			double MemoryPerActionBytesComputed = Math.Max(MemoryPerActionBytes, MemoryPerActionBytesOverride);
			if (MemoryPerActionBytesComputed > MemoryPerActionBytes)
			{
				Logger.LogInformation("Overriding MemoryPerAction with target-defined value of {Memory} bytes", MemoryPerActionBytesComputed / 1024 / 1024 / 1024);
			}

			return Utils.GetMaxActionsToExecuteInParallel(MaxLocalActions, bAllCores ? 1.0f : ProcessorCountMultiplier, bAllCores, Convert.ToInt64(MemoryPerActionBytesComputed));
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="MaxLocalActions">How many actions to execute in parallel</param>
		/// <param name="bAllCores">Consider logical cores when determining how many total cpu cores are available</param>
		/// <param name="bCompactOutput">Should output be written in a compact fashion</param>
		/// <param name="Logger">Logger for output</param>
		public ParallelExecutor(int MaxLocalActions, bool bAllCores, bool bCompactOutput, ILogger Logger)
			: base(Logger)
		{
			XmlConfig.ApplyTo(this);

			// Figure out how many processors to use
			NumParallelProcesses = GetDefaultNumParallelProcesses(MaxLocalActions, bAllCores, Logger);

			this.bCompactOutput = bCompactOutput;
		}

		/// <summary>
		/// Returns the name of this executor
		/// </summary>
		public override string Name => "Parallel";

		/// <summary>
		/// Checks whether the task executor can be used
		/// </summary>
		/// <returns>True if the task executor can be used</returns>
		public static bool IsAvailable()
		{
			return true;
		}

		/// <summary>
		/// Create an action queue
		/// </summary>
		/// <param name="actionsToExecute">Actions to be executed</param>
		/// <param name="actionArtifactCache">Artifact cache</param>
		/// <param name="logger">Logging interface</param>
		/// <returns>Action queue</returns>
		public ImmediateActionQueue CreateActionQueue(IEnumerable<LinkedAction> actionsToExecute, IActionArtifactCache? actionArtifactCache, ILogger logger)
		{
			return new(actionsToExecute, actionArtifactCache, NumParallelProcesses, "Compiling C++ source code...", x => WriteToolOutput(x), () => FlushToolOutput(), logger)
			{
				ShowCompilationTimes = bShowCompilationTimes,
				ShowCPUUtilization = bShowCPUUtilization,
				PrintActionTargetNames = bPrintActionTargetNames,
				LogActionCommandLines = bLogActionCommandLines,
				ShowPerActionCompilationTimes = bShowPerActionCompilationTimes,
				CompactOutput = bCompactOutput,
				StopCompilationAfterErrors = bStopCompilationAfterErrors,
			};
		}

		/// <inheritdoc/>
		public override async Task<bool> ExecuteActionsAsync(IEnumerable<LinkedAction> ActionsToExecute, ILogger Logger, IActionArtifactCache? actionArtifactCache)
		{
			if (!ActionsToExecute.Any())
			{
				return true;
			}

			// The "useAutomaticQueue" should always be true unless manual queue is being tested
			bool useAutomaticQueue = true;
			if (useAutomaticQueue)
			{
				using ImmediateActionQueue queue = CreateActionQueue(ActionsToExecute, actionArtifactCache, Logger);
				int actionLimit = Math.Min(NumParallelProcesses, queue.TotalActions);
				queue.CreateAutomaticRunner(action => RunAction(queue, action), bUseActionWeights, actionLimit, NumParallelProcesses);
				queue.Start();
				queue.StartManyActions();
				return await queue.RunTillDone();
			}
			else
			{
				using ImmediateActionQueue queue = CreateActionQueue(ActionsToExecute, actionArtifactCache, Logger);
				int actionLimit = Math.Min(NumParallelProcesses, queue.TotalActions);
				ImmediateActionQueueRunner runner = queue.CreateManualRunner(action => RunAction(queue, action), bUseActionWeights, actionLimit, actionLimit);
				queue.Start();
				using Timer timer = new((_) => queue.StartManyActions(runner), null, 0, 500);
				queue.StartManyActions();
				return await queue.RunTillDone();
			}
		}

		private static Func<Task>? RunAction(ImmediateActionQueue queue, LinkedAction action)
		{
			return async () =>
			{
				ExecuteResults results = await RunAction(action, queue.ProcessGroup, queue.CancellationToken);
				queue.OnActionCompleted(action, results.ExitCode == 0, results);
			};
		}

		protected static async Task<ExecuteResults> RunAction(LinkedAction Action, ManagedProcessGroup ProcessGroup, CancellationToken CancellationToken, string? AdditionalDescription = null)
		{
			CancellationToken.ThrowIfCancellationRequested();

			using ManagedProcess Process = new ManagedProcess(ProcessGroup, Action.CommandPath.FullName, Action.CommandArguments, Action.WorkingDirectory.FullName, null, null, ProcessPriority);

			using MemoryStream StdOutStream = new MemoryStream();
			await Process.CopyToAsync(StdOutStream, CancellationToken);

			CancellationToken.ThrowIfCancellationRequested();

			await Process.WaitForExitAsync(CancellationToken);

			List<string> LogLines = Console.OutputEncoding.GetString(StdOutStream.GetBuffer(), 0, Convert.ToInt32(StdOutStream.Length)).Split(LineEndingSplit, StringSplitOptions.RemoveEmptyEntries).ToList();
			int ExitCode = Process.ExitCode;
			TimeSpan ProcessorTime = Process.TotalProcessorTime;
			TimeSpan ExecutionTime = Process.ExitTime - Process.StartTime;
			return new ExecuteResults(LogLines, ExitCode, ExecutionTime, ProcessorTime, AdditionalDescription);
		}
	}

	/// <summary>
	/// Publicly visible static class that allows external access to the parallel executor config
	/// </summary>
	public static class ParallelExecutorConfiguration
	{
		/// <summary>
		/// Maximum number of processes that should be used for execution
		/// </summary>
		public static int GetMaxParallelProcesses(ILogger Logger) => ParallelExecutor.GetDefaultNumParallelProcesses(0, false, Logger);

		/// <summary>
		/// Maximum number of processes that should be used for execution
		/// </summary>
		public static int GetMaxParallelProcesses(int MaxLocalActions, bool bAllCores, ILogger Logger) => ParallelExecutor.GetDefaultNumParallelProcesses(MaxLocalActions, bAllCores, Logger);
	}
}
