// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.CodeAnalysis;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;
using UnrealBuildTool.Artifacts;

namespace UnrealBuildTool
{
	/// <summary>
	/// Results from a run action
	/// </summary>
	/// <param name="LogLines">Console log lines</param>
	/// <param name="ExitCode">Process return code.  Zero is assumed to be success and all other values an error.</param>
	/// <param name="ExecutionTime">Wall time</param>
	/// <param name="ProcessorTime">CPU time</param>
	/// <param name="AdditionalDescription">Additional description of action</param>
	record ExecuteResults(List<string> LogLines, int ExitCode, TimeSpan ExecutionTime, TimeSpan ProcessorTime, string? AdditionalDescription = null);

	/// <summary>
	/// Defines the phase of execution
	/// </summary>
	enum ActionPhase : byte
	{

		/// <summary>
		/// Check for already existing artifacts for the inputs
		/// </summary>
		ArtifactCheck,

		/// <summary>
		/// Compile the action
		/// </summary>
		Compile,
	}

	/// <summary>
	/// Defines the type of the runner.
	/// </summary>
	enum ImmediateActionQueueRunnerType : byte
	{

		/// <summary>
		/// Will be used to queue jobs as part of general requests
		/// </summary>
		Automatic,

		/// <summary>
		/// Will only be used for manual requests to queue jobs
		/// </summary>
		Manual,
	}

	/// <summary>
	/// Actions are assigned a runner when they need executing
	/// </summary>
	/// <param name="Type">Type of the runner</param>
	/// <param name="ActionPhase">The action phase that this step supports</param>
	/// <param name="RunAction">Function to be run queue running a task</param>
	/// <param name="UseActionWeights">If true, use the action weight as a secondary limit</param>
	/// <param name="MaxActions">Maximum number of action actions</param>
	/// <param name="MaxActionWeight">Maximum weight of actions</param>
	record ImmediateActionQueueRunner(ImmediateActionQueueRunnerType Type, ActionPhase ActionPhase, Func<LinkedAction, Func<Task>?> RunAction, bool UseActionWeights = false, int MaxActions = Int32.MaxValue, double MaxActionWeight = Int32.MaxValue)
	{
		/// <summary>
		/// Current number of active actions
		/// </summary>
		public int ActiveActions = 0;

		/// <summary>
		/// Current weight of actions
		/// </summary>
		public double ActiveActionWeight = 0;

		/// <summary>
		/// Used to track if the runner has already scanned the action list
		/// for a given change count but found nothing to run.
		/// </summary>
		public int LastActionChange = 0;

		/// <summary>
		/// True if the current limits have not been reached.
		/// </summary>
		public bool IsUnderLimits => ActiveActions < MaxActions && (!UseActionWeights || ActiveActionWeight < MaxActionWeight);
	}

	/// <summary>
	/// Helper class to manage the action queue
	///
	/// Running the queue can be done with a mixture of automatic and manual runners. Runners are responsible for performing
	/// the work associated with an action. Automatic runners will have actions automatically queued to them up to the point
	/// that any runtime limits aren't exceeded (such as maximum number of concurrent processes).  Manual runners must have
	/// jobs queued to them by calling TryStartOneAction or StartManyActions with the runner specified.
	/// 
	/// For example:
	/// 
	///		ParallelExecutor uses an automatic runner exclusively.
	///		UBAExecutor uses an automatic runner to run jobs locally and a manual runner to run jobs remotely as processes 
	///			become available.
	/// </summary>
	class ImmediateActionQueue : IDisposable
	{

		/// <summary>
		/// Number of second of no completed actions to trigger an action stall report.
		/// If zero, stall reports will not be enabled.
		/// </summary>
		[CommandLine("-ActionStallReportTime=")]
		[XmlConfigFile(Category = "BuildConfiguration")]
		public int ActionStallReportTime = 0;

		/// <summary>
		/// Running status of the action
		/// </summary>
		private enum ActionStatus : byte
		{
			/// <summary>
			/// Queued waiting for execution
			/// </summary>
			Queued,

			/// <summary>
			/// Action is running
			/// </summary>
			Running,

			/// <summary>
			/// Action has successfully finished
			/// </summary>
			Finished,

			/// <summary>
			/// Action has finished with an error
			/// </summary>
			Error,
		}

		/// <summary>
		/// Used to track the state of each action
		/// </summary>
		private struct ActionState
		{
			/// <summary>
			/// Action to be executed
			/// </summary>
			public LinkedAction Action;

			/// <summary>
			/// Phase of the action
			/// </summary>
			public ActionPhase Phase;

			/// <summary>
			/// Current status of the execution
			/// </summary>
			public ActionStatus Status;

			/// <summary>
			/// Runner assigned to the action
			/// </summary>
			public ImmediateActionQueueRunner? Runner;

			/// <summary>
			/// Optional execution results
			/// </summary>
			public ExecuteResults? Results;

			/// <summary>
			/// Indices to prereq actions
			/// </summary>
			public int[] PrereqActionsSortIndex;
		};

		/// <summary>
		/// State information about each action
		/// </summary>
		private readonly ActionState[] Actions;

		/// <summary>
		/// Output logging
		/// </summary>
		public readonly ILogger Logger;

		/// <summary>
		/// Total number of actions
		/// </summary>
		public int TotalActions => Actions.Length;

		/// <summary>
		/// Process group
		/// </summary>		
		public readonly ManagedProcessGroup ProcessGroup = new();

		/// <summary>
		/// Source for the cancellation token
		/// </summary>
		public readonly CancellationTokenSource CancellationTokenSource = new();

		/// <summary>
		/// Cancellation token
		/// </summary>
		public CancellationToken CancellationToken => CancellationTokenSource.Token;

		/// <summary>
		/// Progress writer
		/// </summary>
		public readonly ProgressWriter ProgressWriter;

		/// <summary>
		/// Overall success of the action queue
		/// </summary>
		public bool Success = true;

		/// <summary>
		/// Whether to show compilation times along with worst offenders or not.
		/// </summary>
		public bool ShowCompilationTimes = false;

		/// <summary>
		/// Whether to show CPU utilization after the work is complete.
		/// </summary>
		public bool ShowCPUUtilization = false;

		/// <summary>
		/// Add target names for each action executed
		/// </summary>
		public bool PrintActionTargetNames = false;

		/// <summary>
		/// Whether to log command lines for actions being executed
		/// </summary>
		public bool LogActionCommandLines = false;

		/// <summary>
		/// Whether to show compilation times for each executed action
		/// </summary>
		public bool ShowPerActionCompilationTimes = false;

		/// <summary>
		/// Collapse non-error output lines
		/// </summary>
		public bool CompactOutput = false;

		/// <summary>
		/// When enabled, will stop compiling targets after a compile error occurs.
		/// </summary>
		public bool StopCompilationAfterErrors = false;

		/// <summary>
		/// Return true if the queue is done
		/// </summary>
		public bool IsDone => _doneTaskSource.Task.IsCompleted;

		/// <summary>
		///  Action that can be to notify when artifacts have been read for an action
		/// </summary>
		public Action<LinkedAction>? OnArtifactsRead = null;

		/// <summary>
		/// Collection of available runners
		/// </summary>
		private readonly List<ImmediateActionQueueRunner> _runners = new();

		/// <summary>
		/// First action to start scanning for actions to run
		/// </summary>
		private int _firstPendingAction;

		/// <summary>
		/// Action to invoke when writing tool output
		/// </summary>
		private readonly Action<string> _writeToolOutput;

		/// <summary>
		/// Flush the tool output after logging has completed
		/// </summary>
		private readonly System.Action _flushToolOutput;

		/// <summary>
		/// Timer used to collect CPU utilization
		/// </summary>
		private Timer? _cpuUtilizationTimer;

		/// <summary>
		/// Per-second logging of cpu utilization
		/// </summary>
		private List<float> _cpuUtilization = new();

		/// <summary>
		/// Collection of all actions remaining to be logged
		/// </summary>
		private readonly List<int> _actionsToLog = new();

		/// <summary>
		/// Task waiting to process logging
		/// </summary>
		private Task? _actionsToLogTask = null;

		/// <summary>
		/// Used only by the logger to track the [x,total] output
		/// </summary>
		private int _loggedCompletedActions = 0;

		/// <summary>
		/// Tracks the number of completed actions.
		/// </summary>
		private int _completedActions = 0;

		/// <summary>
		/// Flags used to track how StartManyActions should run
		/// </summary>
		private int _startManyFlags = 0;

		/// <summary>
		/// Number of changes to the action list
		/// </summary>
		private int _lastActionChange = 1;

		/// <summary>
		/// If true, a action stall has been reported for the current change count
		/// </summary>
		private bool _lastActionStallReported = false;

		/// <summary>
		/// Time of the last change to the action count.  This is updated by the timer.
		/// </summary>
		private DateTime _lastActionChangeTime;

		/// <summary>
		/// Copy of _lastActionChange used to detect updates by the time.
		/// </summary>
		private int _lastActionStallChange = 1;

		/// <summary>
		/// Used to terminate the run with status
		/// </summary>
		private readonly TaskCompletionSource _doneTaskSource = new();

		/// <summary>
		/// The last action group printed in multi-target builds
		/// </summary>
		private string? LastGroupPrefix = null;

		/// <summary>
		/// If set, artifact cache used to retrieve previously compiled results and save new results
		/// </summary>
		private IActionArtifactCache? _actionArtifactCache;

		static ExecuteResults s_copiedFromCacheResults = new(new List<string>(), 0, TimeSpan.Zero, TimeSpan.Zero, "copied from cache");

		/// <summary>
		/// Construct a new instance of the action queue
		/// </summary>
		/// <param name="actions">Collection of actions</param>
		/// <param name="actionArtifactCache">If true, the artifact cache is being used</param>
		/// <param name="maxActionArtifactCacheTasks">Max number of concurrent artifact cache tasks</param>
		/// <param name="progressWriterText">Text to be displayed with the progress writer</param>
		/// <param name="writeToolOutput">Action to invoke when writing tool output</param>
		/// <param name="flushToolOutput">Action to invoke when flushing tool output</param>
		/// <param name="logger">Logging interface</param>
		public ImmediateActionQueue(IEnumerable<LinkedAction> actions, IActionArtifactCache? actionArtifactCache, int maxActionArtifactCacheTasks, string progressWriterText, Action<string> writeToolOutput, System.Action flushToolOutput, ILogger logger)
		{
			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), this, logger);
			XmlConfig.ApplyTo(this);

			int count = actions.Count();
			Actions = new ActionState[count];

			Logger = logger;
			ProgressWriter = new(progressWriterText, false, logger);
			_actionArtifactCache = actionArtifactCache;
			_writeToolOutput = writeToolOutput;
			_flushToolOutput = flushToolOutput;

			bool readArtifacts = _actionArtifactCache != null && _actionArtifactCache.EnableReads;
			ActionPhase initialPhase = readArtifacts ? ActionPhase.ArtifactCheck : ActionPhase.Compile;
			int index = 0;
			foreach (LinkedAction action in actions)
			{
				action.SortIndex = index;
				Actions[index++] = new ActionState
				{
					Action = action,
					Status = ActionStatus.Queued,
					Phase = action.ArtifactMode.HasFlag(ArtifactMode.Enabled) ? initialPhase : ActionPhase.Compile,
					Results = null,
					PrereqActionsSortIndex = action.PrerequisiteActions.Select(a => a.SortIndex).ToArray()
				};
			}

			if (readArtifacts)
			{
				Func<LinkedAction, Func<Task>> runAction = (LinkedAction action) =>
				{
					return new Func<Task>(async () =>
					{
						bool success = await _actionArtifactCache!.CompleteActionFromCacheAsync(action, CancellationToken);
						if (success)
						{
							OnArtifactsRead?.Invoke(action);
							OnActionCompleted(action, success, s_copiedFromCacheResults);
						}
						else
						{
							RequeueAction(action);
						}
					});
				};

				_runners.Add(new(ImmediateActionQueueRunnerType.Automatic, ActionPhase.ArtifactCheck, runAction, false, maxActionArtifactCacheTasks, 0));
			}
		}

		/// <summary>
		/// Create a new automatic runner
		/// </summary>
		/// <param name="runAction">Action to be run queue running a task</param>
		/// <param name="useActionWeights">If true, use the action weight as a secondary limit</param>
		/// <param name="maxActions">Maximum number of action actions</param>
		/// <param name="maxActionWeight">Maximum weight of actions</param>
		/// <returns>Created runner</returns>
		public ImmediateActionQueueRunner CreateAutomaticRunner(Func<LinkedAction, Func<Task>?> runAction, bool useActionWeights, int maxActions, double maxActionWeight)
		{
			ImmediateActionQueueRunner runner = new(ImmediateActionQueueRunnerType.Automatic, ActionPhase.Compile, runAction, useActionWeights, maxActions, maxActionWeight);
			_runners.Add(runner);
			return runner;
		}

		/// <summary>
		/// Create a manual runner
		/// </summary>
		/// <param name="runAction">Action to be run queue running a task</param>
		/// <param name="useActionWeights">If true, use the action weight as a secondary limit</param>
		/// <param name="maxActions">Maximum number of action actions</param>
		/// <param name="maxActionWeight">Maximum weight of actions</param>
		/// <returns>Created runner</returns>
		public ImmediateActionQueueRunner CreateManualRunner(Func<LinkedAction, Func<Task>?> runAction, bool useActionWeights = false, int maxActions = Int32.MaxValue, double maxActionWeight = Double.MaxValue)
		{
			ImmediateActionQueueRunner runner = new(ImmediateActionQueueRunnerType.Manual, ActionPhase.Compile, runAction, useActionWeights, maxActions, maxActionWeight);
			_runners.Add(runner);
			return runner;
		}

		/// <summary>
		/// Start the process of running all the actions
		/// </summary>
		public void Start()
		{
			if (ShowCPUUtilization || ActionStallReportTime > 0)
			{
				_lastActionChangeTime = DateTime.Now;
				_cpuUtilizationTimer = new(x =>
				{
					if (ShowCPUUtilization)
					{
						lock (_cpuUtilization)
						{
							if (Utils.GetTotalCpuUtilization(out float cpuUtilization))
							{
								_cpuUtilization.Add(cpuUtilization);
							}
						}
					}

					if (ActionStallReportTime > 0)
					{
						lock (Actions)
						{

							// If there has been an action count change, reset the timer and enable the report again
							if (_lastActionStallChange != _lastActionChange)
							{
								_lastActionStallChange = _lastActionChange;
								_lastActionChangeTime = DateTime.Now;
								_lastActionStallReported = false;
							}

							// Otherwise, if we haven't already generated a report, test for a timeout in seconds and generate one on timeout.
							else if (!_lastActionStallReported && (DateTime.Now - _lastActionChangeTime).TotalSeconds > ActionStallReportTime)
							{
								_lastActionStallReported = true;
								GenerateStallReport();
							}
						}
					}

				}, null, 1000, 1000);
			}

			Logger.LogInformation("------ Building {TotalActions} action(s) started ------", TotalActions);
		}

		/// <summary>
		/// Run the actions until complete
		/// </summary>
		/// <returns>True if completed successfully</returns>
		public async Task<bool> RunTillDone()
		{
			await _doneTaskSource.Task;
			TraceSummary();
			return Success;
		}

		/// <summary>
		/// Return an enumeration of ready compile tasks.  This is not executed under a lock and 
		/// does not modify the state of any actions.
		/// </summary>
		/// <returns>Enumerations of all ready to compile actions.</returns>
		public IEnumerable<LinkedAction> EnumerateReadyToCompileActions()
		{
			for (int actionIndex = _firstPendingAction; actionIndex != Actions.Length; ++actionIndex)
			{
				var actionState = Actions[actionIndex];
				if (actionState.Status == ActionStatus.Queued &&
					actionState.Phase == ActionPhase.Compile &&
					GetActionReadyState(actionState) == ActionReadyState.Ready)
				{
					yield return actionState.Action;
				}
			}
		}

		/// <summary>
		/// Try to start one action
		/// </summary>
		/// <param name="runner">If specified, tasks will only be queued to this runner.  Otherwise all manual runners will be used.</param>
		/// <returns>True if an action was run, false if not</returns>
		public bool TryStartOneAction(ImmediateActionQueueRunner? runner = null)
		{

			// Loop until we have an action or no completed actions (i.e. no propagated errors)
			for (; ; )
			{
				bool hasCanceled = false;

				// Try to get the next action
				Func<Task>? runAction = null;
				LinkedAction? action = null;
				int completedActions = 0;
				lock (Actions)
				{
					hasCanceled = CancellationTokenSource.IsCancellationRequested;

					// Don't bother if we have been canceled or the specified running has already failed to find
					// anything at the given change number.
					if (!hasCanceled && (runner == null || runner.LastActionChange != _lastActionChange))
					{

						// Try to get an action
						(runAction, action, completedActions) = TryStartOneActionInternal(runner);

						// If we have completed actions (i.e. error propagations), then increment the change counter
						if (completedActions != 0)
						{
							_lastActionChange++;
						}
						
						// Otherwise if nothing was found, remember that we have already scanned at this change.
						else if ((runAction == null || action == null) && runner != null)
						{
							runner.LastActionChange = _lastActionChange;
						}
					}
				}

				// If we have an action, run it and account for any completed actions
				if (runAction != null && action != null)
				{
					if (runner != null && runner.Type == ImmediateActionQueueRunnerType.Manual)
					{
						try
						{
							runAction().Wait();
						}
						catch (Exception ex)
						{
							HandleException(action, ex);
						}
					}
					else
					{
						Task.Factory.StartNew(() =>
						{
							try
							{
								runAction().Wait();
							}
							catch (Exception ex)
							{
								HandleException(action, ex);
							}
						}, CancellationToken, TaskCreationOptions.LongRunning | TaskCreationOptions.PreferFairness, TaskScheduler.Default);
					}
					AddCompletedActions(completedActions);
					return true;
				}

				// If we have no completed actions, then we need to check for a stall
				if (completedActions == 0)
				{
					// We found nothing, check to see if we have no active running tasks and no manual runners.
					// If we don't, then it is assumed we can't queue any more work.
					bool prematureDone = true;
					foreach (ImmediateActionQueueRunner tryRunner in _runners)
					{
						if ((tryRunner.Type == ImmediateActionQueueRunnerType.Manual && !hasCanceled) || tryRunner.ActiveActions != 0)
						{
							prematureDone = false;
							break;
						}
					}
					if (prematureDone)
					{
						AddCompletedActions(Int32.MaxValue);
					}
					return false;
				}

				// Otherwise add the completed actions and test again for the possibility that errors still need propagating
				AddCompletedActions(completedActions);
			}
		}

		/// <summary>
		/// Handle an exception running a task
		/// </summary>
		/// <param name="action">Action that has been run</param>
		/// <param name="ex">Thrown exception</param>
		void HandleException(LinkedAction action, Exception ex)
		{
			if (ex is AggregateException aggregateEx)
			{
				if (aggregateEx.InnerException != null)
				{
					HandleException(action, aggregateEx.InnerException);
				}
				else
				{
					ExecuteResults results = new(new List<string>(), Int32.MaxValue, TimeSpan.Zero, TimeSpan.Zero);
					OnActionCompleted(action, false, results);
				}
			}
			else if (ex is OperationCanceledException)
			{
				ExecuteResults results = new(new List<string>(), Int32.MaxValue, TimeSpan.Zero, TimeSpan.Zero);
				OnActionCompleted(action, false, results);
			}
			else
			{
				List<string> text = new()
				{
					ExceptionUtils.FormatException(ex),
					ExceptionUtils.FormatExceptionDetails(ex),
				};
				ExecuteResults results = new(text, Int32.MaxValue, TimeSpan.Zero, TimeSpan.Zero);
				OnActionCompleted(action, false, results);
			}
		}

		/// <summary>
		/// Try to start one action.
		/// </summary>
		/// <param name="runner">If specified, tasks will only be queued to this runner.  Otherwise all manual runners will be used.</param>
		/// <returns>Action to be run if something was queued and the number of completed actions</returns>
		public (Func<Task>?, LinkedAction?, int) TryStartOneActionInternal(ImmediateActionQueueRunner? runner = null)
		{
			int completedActions = 0;

			// If we are starting deeper in the action collection, then never set the first pending action location
			bool foundFirstPending = false;

			// Loop through the items
			for (int actionIndex = _firstPendingAction; actionIndex != Actions.Length; ++actionIndex)
			{

				// If the action has already reached the compilation state, then just ignore
				if (Actions[actionIndex].Status != ActionStatus.Queued)
				{
					continue;
				}

				// If needed, update the first valid slot for searching for actions to run
				if (!foundFirstPending)
				{
					_firstPendingAction = actionIndex;
					foundFirstPending = true;
				}

				// Based on the ready state, use this action or mark as an error
				switch (GetActionReadyState(Actions[actionIndex]))
				{
					case ActionReadyState.NotReady:
						break;

					case ActionReadyState.Error:
						Actions[actionIndex].Status = ActionStatus.Error;
						Actions[actionIndex].Phase = ActionPhase.Compile;
						completedActions++;
						break;

					case ActionReadyState.Ready:
						ImmediateActionQueueRunner? selectedRunner = null;
						Func<Task>? runAction = null;
						if (runner != null)
						{
							if (runner.IsUnderLimits && runner.ActionPhase == Actions[actionIndex].Phase)
							{
								runAction = runner.RunAction(Actions[actionIndex].Action);
								if (runAction != null)
								{
									selectedRunner = runner;
								}
							}
						}
						else
						{
							foreach (ImmediateActionQueueRunner tryRunner in _runners)
							{
								if (tryRunner.Type == ImmediateActionQueueRunnerType.Automatic &&
									tryRunner.IsUnderLimits && tryRunner.ActionPhase == Actions[actionIndex].Phase)
								{
									runAction = tryRunner.RunAction(Actions[actionIndex].Action);
									if (runAction != null)
									{
										selectedRunner = tryRunner;
										break;
									}
								}
							}
						}

						if (runAction != null && selectedRunner != null)
						{
							Actions[actionIndex].Status = ActionStatus.Running;
							Actions[actionIndex].Runner = selectedRunner;
							selectedRunner.ActiveActions++;
							selectedRunner.ActiveActionWeight += Actions[actionIndex].Action.Weight;
							return (runAction, Actions[actionIndex].Action, completedActions);
						}
						break;

					default:
						throw new ApplicationException("Unexpected action ready state");
				}
			}
			return (null, null, completedActions);
		}

		/// <summary>
		/// Start as many actions as possible.
		/// </summary>
		/// <param name="runner">If specified, all actions will be limited to the runner</param>
		public void StartManyActions(ImmediateActionQueueRunner? runner = null)
		{
			const int Running = 1 << 1;
			const int ScanRequested = 1 << 0;

			// If both flags were clear, I need to start running actions
			int old = Interlocked.Or(ref _startManyFlags, Running | ScanRequested);
			if (old == 0)
			{
				for(; ; )
				{

					// Clear the changed flag since we are about to scan
					Interlocked.And(ref _startManyFlags, Running);

					// If nothing started
					if (!TryStartOneAction(runner))
					{

						// If we only have the running flag (nothing new changed), then exit
						if (Interlocked.CompareExchange(ref _startManyFlags, 0, Running) == Running)
						{
							return;
						}
					}
				}
			}
		}

		/// <summary>
		/// Dispose the object
		/// </summary>
		public void Dispose()
		{
			if (_cpuUtilizationTimer != null)
			{
				_cpuUtilizationTimer.Dispose();
			}
			ProcessGroup.Dispose();
		}

		/// <summary>
		/// Reset the status of the given action to the queued state.
		/// </summary>
		/// <param name="action">Action being re-queued</param>
		public void RequeueAction(LinkedAction action)
		{
			SetActionState(action, ActionStatus.Queued, null);
		}

		/// <summary>
		/// Notify the system that an action has been completed.
		/// </summary>
		/// <param name="action">Action being completed</param>
		/// <param name="success">If true, the action succeeded or false if it failed.</param>
		/// <param name="results">Optional execution results</param>
		public void OnActionCompleted(LinkedAction action, bool success, ExecuteResults? results)
		{
			SetActionState(action, success ? ActionStatus.Finished : ActionStatus.Error, results);
		}

		/// <summary>
		/// Set the new state of an action.  Can only set state to Queued, Finished, or Error
		/// </summary>
		/// <param name="action">Action being set</param>
		/// <param name="status">Status to set</param>
		/// <param name="results">Optional results</param>
		/// <exception cref="BuildException"></exception>
		private void SetActionState(LinkedAction action, ActionStatus status, ExecuteResults? results)
		{
			int actionIndex = action.SortIndex;
			int completedActions = 0;

			// If we are finished, then invalidate the file info for all the produced items.  This needs to be
			// done so the artifact system sees any compiled files.
			if (status == ActionStatus.Finished)
			{
				foreach (FileItem output in action.ProducedItems)
				{
					output.ResetCachedInfo();
				}
			}

			// Update the actions data
			lock (Actions)
			{
				ImmediateActionQueueRunner runner = Actions[actionIndex].Runner ?? throw new BuildException("Attempting to update action state but runner isn't set");
				runner.ActiveActions--;
				runner.ActiveActionWeight -= Actions[actionIndex].Action.Weight;

				// Use to track that action states have changed
				_lastActionChange++;

				// If we are doing an artifact check, then move to compile phase
				bool wasArtifactCheck = false;
				if (Actions[actionIndex].Phase == ActionPhase.ArtifactCheck)
				{
					wasArtifactCheck = true;
					Actions[actionIndex].Phase = ActionPhase.Compile;
					if (status != ActionStatus.Finished)
					{
						status = ActionStatus.Queued;
					}
				}

				Actions[actionIndex].Status = status;
				Actions[actionIndex].Runner = null;
				if (results != null)
				{
					Actions[actionIndex].Results = results;
				}

				// Add the action to the logging queue
				if (status != ActionStatus.Queued)
				{
					lock (_actionsToLog)
					{
						_actionsToLog.Add(action.SortIndex);
						if (_actionsToLogTask == null)
						{
							_actionsToLogTask = Task.Run(LogActions);
						}
					}
				}

				switch (status)
				{
					case ActionStatus.Queued:
						if (actionIndex < _firstPendingAction)
						{
							_firstPendingAction = actionIndex;
						}
						break;

					case ActionStatus.Finished:
						// Notify the artifact handler of the action completing.  We don't wait on the resulting task.  The
						// cache is required to remember any pending saves and a final call to Flush will wait for everything to complete.
						if (!wasArtifactCheck)
						{
							_actionArtifactCache?.ActionCompleteAsync(action, CancellationToken);
						}
						completedActions = 1;
						break;

					case ActionStatus.Error:
						Success = false;
						completedActions = 1;
						break;

					default:
						throw new BuildException("Unexpected action status set");
				}
			}

			// Outside of lock, update the completed actions
			AddCompletedActions(completedActions);

			// Since something has been completed or returned to the queue, try to run actions again
			StartManyActions();
		}

		/// <summary>
		/// Add the number of completed actions and signal done if all actions complete.
		/// </summary>
		/// <param name="count">Number of completed actions to add.  If int.MaxValue, then the number is immediately set to total actions</param>
		private void AddCompletedActions(int count)
		{
			if (count == 0)
			{
				// do nothing
			}
			else if (count != Int32.MaxValue)
			{
				if (Interlocked.Add(ref _completedActions, count) == TotalActions)
				{
					_doneTaskSource.SetResult();
				}
			}
			else
			{
				if (Interlocked.Exchange(ref _completedActions, TotalActions) != TotalActions)
				{
					_doneTaskSource.SetResult();
				}
			}
		}

		/// <summary>
		/// Returns the number of queued actions left (not including ArtifactCheck actions)
		/// Note, this method is lockless and will not always return accurate count
		/// </summary>
		/// <param name="filterFunc">Optional function to filter out actions. Return false if action should not be included</param>
		public uint GetQueuedActionsCount(Func<LinkedAction, bool>? filterFunc = null)
		{
			uint count = 0;

			for (int actionIndex = _firstPendingAction; actionIndex != Actions.Length; ++actionIndex)
			{

				if (Actions[actionIndex].Status != ActionStatus.Queued || Actions[actionIndex].Phase != ActionPhase.Compile)
				{
					continue;
				}
					
				if (filterFunc != null && !filterFunc(Actions[actionIndex].Action))
				{
					continue;
				}

				++count;
			}
			return count;
		}

		/// <summary>
		/// Purge the pending logging actions
		/// </summary>
		private void LogActions()
		{
			for (; ; )
			{
				int[]? actionsToLog = null;
				lock (_actionsToLog)
				{
					if (_actionsToLog.Count == 0)
					{
						_actionsToLogTask = null;
					}
					else
					{
						actionsToLog = _actionsToLog.ToArray();
						_actionsToLog.Clear();
					}
				}

				if (actionsToLog == null)
				{
					return;
				}

				foreach (int index in actionsToLog)
				{
					LogAction(Actions[index].Action, Actions[index].Results);
				}
			}
		}

		private static int s_previousLineLength = -1;

		/// <summary>
		/// Log an action that has completed
		/// </summary>
		/// <param name="action">Action that has completed</param>
		/// <param name="executeTaskResult">Results of the action</param>
		private void LogAction(LinkedAction action, ExecuteResults? executeTaskResult)
		{
			List<string>? logLines = null;
			int exitCode = Int32.MaxValue;
			TimeSpan executionTime = TimeSpan.Zero;
			TimeSpan processorTime = TimeSpan.Zero;
			string? additionalDescription = null;
			if (executeTaskResult != null)
			{
				logLines = executeTaskResult.LogLines;
				exitCode = executeTaskResult.ExitCode;
				executionTime = executeTaskResult.ExecutionTime;
				processorTime = executeTaskResult.ProcessorTime;
				additionalDescription = executeTaskResult.AdditionalDescription;
			}

			// Write it to the log
			string description = String.Empty;
			if (action.bShouldOutputStatusDescription || (logLines != null && logLines.Count == 0))
			{
				description = $"{(action.CommandDescription ?? action.CommandPath.GetFileNameWithoutExtension())} {action.StatusDescription}".Trim();
			}
			else if (logLines != null && logLines.Count > 0)
			{
				description = $"{(action.CommandDescription ?? action.CommandPath.GetFileNameWithoutExtension())} {logLines[0]}".Trim();
			}
			if (!String.IsNullOrEmpty(additionalDescription))
			{
				description = $"{description} {additionalDescription}";
			}

			lock (ProgressWriter)
			{
				int totalActions = Actions.Length;
				int completedActions = Interlocked.Increment(ref _loggedCompletedActions);
				ProgressWriter.Write(completedActions, Actions.Length);

				// Canceled
				if (exitCode == Int32.MaxValue)
				{
					Logger.LogInformation("[{CompletedActions}/{TotalActions}] {Description} canceled", completedActions, totalActions, description);
					return;
				}

				string targetDetails = "";
				TargetDescriptor? target = action.Target;
				if (PrintActionTargetNames && target != null)
				{
					targetDetails = $"[{target.Name} {target.Platform} {target.Configuration}]";
				}

				if (LogActionCommandLines)
				{
					Logger.LogDebug("[{CompletedActions}/{TotalActions}]{TargetDetails} Command: {CommandPath} {CommandArguments}", completedActions, totalActions, targetDetails, action.CommandPath, action.CommandArguments);
				}

				string compilationTimes = "";

				if (ShowPerActionCompilationTimes)
				{
					if (processorTime.Ticks > 0)
					{
						compilationTimes = $" (Wall: {executionTime.TotalSeconds:0.00}s CPU: {processorTime.TotalSeconds:0.00}s)";
					}
					else
					{
						compilationTimes = $" (Wall: {executionTime.TotalSeconds:0.00}s)";
					}
				}

				string message = ($"[{completedActions}/{totalActions}]{targetDetails}{compilationTimes} {description}");

				if (CompactOutput)
				{
					if (s_previousLineLength > 0)
					{
						// move the cursor to the far left position, one line back
						Console.CursorLeft = 0;
						Console.CursorTop -= 1;
						// clear the line
						Console.Write("".PadRight(s_previousLineLength));
						// move the cursor back to the left, so output is written to the desired location
						Console.CursorLeft = 0;
					}
				}
				else
				{
					// If the action group has changed for a multi target build, write it to the log
					if (action.GroupNames.Count > 0)
					{
						string ActionGroup = $"** For {String.Join(" + ", action.GroupNames)} **";
						if (!ActionGroup.Equals(LastGroupPrefix, StringComparison.Ordinal))
						{
							LastGroupPrefix = ActionGroup;
							_writeToolOutput(ActionGroup);
						}
					}
				}

				s_previousLineLength = message.Length;

				_writeToolOutput(message);
				if (logLines != null)
				{
					foreach (string Line in logLines.Skip(action.bShouldOutputStatusDescription ? 0 : 1))
					{
						// suppress library creation messages when writing compact output
						if (CompactOutput && Line.StartsWith("   Creating library ") && Line.EndsWith(".exp"))
						{
							continue;
						}

						_writeToolOutput(Line);

						// Prevent overwriting of logged lines
						s_previousLineLength = -1;
					}
				}

				if (exitCode != 0)
				{
					string exitCodeStr = String.Empty;
					if ((uint)exitCode == 0xC0000005)
					{
						exitCodeStr = "(Access violation)";
					}
					else if ((uint)exitCode == 0xC0000409)
					{
						exitCodeStr = "(Stack buffer overflow)";
					}

					// If we have an error code but no output, chances are the tool crashed.  Generate more detailed information to let the
					// user know something went wrong.
					if (logLines == null || logLines.Count <= (action.bShouldOutputStatusDescription ? 0 : 1))
					{
						Logger.LogError("{TargetDetails} {Description}: Exited with error code {ExitCode} {ExitCodeStr}. The build will fail.", targetDetails, description, exitCode, exitCodeStr);
						Logger.LogInformation("{TargetDetails} {Description}: WorkingDirectory {WorkingDirectory}", targetDetails, description, action.WorkingDirectory);
						Logger.LogInformation("{TargetDetails} {Description}: {CommandPath} {CommandArguments}", targetDetails, description, action.CommandPath, action.CommandArguments);
					}
					// Always print error details to to the log file
					else
					{
						Logger.LogDebug("{TargetDetails} {Description}: Exited with error code {ExitCode} {ExitCodeStr}. The build will fail.", targetDetails, description, exitCode, exitCodeStr);
						Logger.LogDebug("{TargetDetails} {Description}: WorkingDirectory {WorkingDirectory}", targetDetails, description, action.WorkingDirectory);
						Logger.LogDebug("{TargetDetails} {Description}: {CommandPath} {CommandArguments}", targetDetails, description, action.CommandPath, action.CommandArguments);
					}

					// prevent overwriting of error text
					s_previousLineLength = -1;

					// Cancel all other pending tasks
					if (StopCompilationAfterErrors)
					{
						CancellationTokenSource.Cancel();
					}
				}
			}
		}

		/// <summary>
		/// Generate the final summary display
		/// </summary>
		private void TraceSummary()
		{

			// Wait for logging to complete
			Task? loggingTask = null;
			lock (_actionsToLog)
			{
				loggingTask = _actionsToLogTask;
			}
			loggingTask?.Wait();
			_flushToolOutput();

			if (ShowCPUUtilization)
			{
				lock (_cpuUtilization)
				{
					if (_cpuUtilization.Count > 0)
					{
						Logger.LogInformation("");
						Logger.LogInformation("Average CPU Utilization: {CPUPercentage}%", (int)(_cpuUtilization.Average()));
					}
				}
			}

			if (!ShowCompilationTimes)
			{
				return;
			}

			Logger.LogInformation("");
			if (ProcessGroup.TotalProcessorTime.Ticks > 0)
			{
				Logger.LogInformation("Total CPU Time: {TotalSeconds} s", ProcessGroup.TotalProcessorTime.TotalSeconds);
				Logger.LogInformation("");
			}

			IEnumerable<int> CompletedActions = Enumerable.Range(0, Actions.Length)
				.Where(x => Actions[x].Results != null && Actions[x].Results!.ExecutionTime > TimeSpan.Zero)
				.OrderByDescending(x => Actions[x].Results!.ExecutionTime)
				.Take(20);

			if (CompletedActions.Any())
			{
				Logger.LogInformation("Compilation Time Top {CompletedTaskCount}", CompletedActions.Count());
				Logger.LogInformation("");
				foreach (int Index in CompletedActions)
				{
					IExternalAction Action = Actions[Index].Action.Inner;
					ExecuteResults Result = Actions[Index].Results!;

					string Description = $"{(Action.CommandDescription ?? Action.CommandPath.GetFileNameWithoutExtension())} {Action.StatusDescription}".Trim();
					if (Result.ProcessorTime.Ticks > 0)
					{
						Logger.LogInformation("{Description} [ Wall Time {ExecutionTime:0.00} s / CPU Time {ProcessorTime:0.00} s ]", Description, Result.ExecutionTime.TotalSeconds, Result.ProcessorTime.TotalSeconds);
					}
					else
					{
						Logger.LogInformation("{Description} [ Time {ExecutionTime:0.00} s ]", Description, Result.ExecutionTime.TotalSeconds);
					}
				}
				Logger.LogInformation("");
			}
		}

		private enum ActionReadyState
		{
			NotReady,
			Error,
			Ready,
		}

		/// <summary>
		/// Get the ready state of an action
		/// </summary>
		/// <param name="action">Action in question</param>
		/// <returns>Action ready state</returns>
		private ActionReadyState GetActionReadyState(ActionState action)
		{
			foreach (int prereqIndex in action.PrereqActionsSortIndex)
			{

				// To avoid doing artifact checks on actions that might need compiling,
				// we first make sure the action is in the compile phase
				if (Actions[prereqIndex].Phase != ActionPhase.Compile)
				{
					return ActionReadyState.NotReady;
				}

				// Respect the compile status of the action
				switch (Actions[prereqIndex].Status)
				{
					case ActionStatus.Finished:
						continue;

					case ActionStatus.Error:
						return ActionReadyState.Error;

					default:
						return ActionReadyState.NotReady;
				}
			}
			return ActionReadyState.Ready;
		}

		private void GenerateStallReport()
		{
			Logger.LogInformation("Action stall detected:");
			foreach (ImmediateActionQueueRunner runner in _runners)
			{
				Logger.LogInformation("Runner Type: {Type}, Running Actions: {ActionCount}", runner.Type.ToString(), runner.ActiveActions);
				if (runner.ActiveActions > 0)
				{
					int count = 0;
					foreach (ActionState state in Actions)
					{
						if (state.Runner == runner && state.Status == ActionStatus.Running)
						{
							string description = $"{(state.Action.CommandDescription ?? state.Action.CommandPath.GetFileNameWithoutExtension())} {state.Action.StatusDescription}".Trim();
							Logger.LogInformation("    Action[{Index}]: {Description}", count++, description);
						}
					}
				}
			}
			{
				int queued = 0;
				int running = 0;
				int error = 0;
				int finished = 0;
				foreach (ActionState state in Actions)
				{
					switch (state.Status)
					{
						case ActionStatus.Error:
							error++;
							break;
						case ActionStatus.Finished:
							finished++;
							break;
						case ActionStatus.Queued:
							queued++;
							break;
						case ActionStatus.Running:
							running++;
							break;
					}
				}
				Logger.LogInformation("Queue Counts: Queued = {Queued}, Running = {Running}, Finished = {Finished}, Error = {Error}", queued, running, finished, error);
			}
		}
	}
}
