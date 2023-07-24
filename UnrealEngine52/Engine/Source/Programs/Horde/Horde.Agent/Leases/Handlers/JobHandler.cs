// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Grpc.Core;
using Horde.Agent.Execution;
using Horde.Agent.Parser;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Leases.Handlers
{
	class JobHandler : LeaseHandler<ExecuteJobTask>
	{
		/// <summary>
		/// How often to poll the server checking if a step has been aborted
		/// Exposed as internal to ease testing.
		/// </summary>
		internal TimeSpan _stepAbortPollInterval = TimeSpan.FromSeconds(5);

		readonly IEnumerable<JobExecutorFactory> _executorFactories;
		readonly AgentSettings _settings;
		readonly IServerLoggerFactory _serverLoggerFactory;
		readonly ILogger _defaultLogger;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobHandler(IEnumerable<JobExecutorFactory> executorFactories, IOptions<AgentSettings> settings, IServerLoggerFactory serverLoggerFactory, ILogger<JobHandler> defaultLogger)
		{
			_executorFactories = executorFactories;
			_settings = settings.Value;
			_serverLoggerFactory = serverLoggerFactory;
			_defaultLogger = defaultLogger;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, ExecuteJobTask executeTask, CancellationToken cancellationToken)
		{
			await using IServerLogger logger = _serverLoggerFactory.CreateLogger(session, executeTask.LogId, executeTask.JobId, executeTask.BatchId, null);

			logger.LogInformation("Executing job \"{JobName}\", jobId {JobId}, batchId {BatchId}, leaseId {LeaseId}", executeTask.JobName, executeTask.JobId, executeTask.BatchId, leaseId);
			GlobalTracer.Instance.ActiveSpan?.SetTag("jobId", executeTask.JobId.ToString());
			GlobalTracer.Instance.ActiveSpan?.SetTag("jobName", executeTask.JobName.ToString());
			GlobalTracer.Instance.ActiveSpan?.SetTag("batchId", executeTask.BatchId.ToString());

			// Start executing the current batch
			BeginBatchResponse batch = await session.RpcConnection.InvokeAsync<HordeRpc.HordeRpcClient, BeginBatchResponse>(x => x.BeginBatchAsync(new BeginBatchRequest(executeTask.JobId, executeTask.BatchId, leaseId), null, null, cancellationToken), cancellationToken);

			// Execute the batch
			try
			{
				await ExecuteBatchAsync(session, leaseId, executeTask, batch, logger, cancellationToken);
			}
			catch (Exception ex)
			{
				if (cancellationToken.IsCancellationRequested && ex.IsCancellationException())
				{
					logger.LogError("Step was aborted");
					throw;
				}
				else
				{
					logger.LogError(ex, "Exception while executing batch: {Ex}", ex);
				}
			}

			// If this lease was cancelled, don't bother updating the job state.
			if (cancellationToken.IsCancellationRequested)
			{
				return LeaseResult.Cancelled;
			}

			// Mark the batch as complete
			await session.RpcConnection.InvokeAsync((HordeRpc.HordeRpcClient x) => x.FinishBatchAsync(new FinishBatchRequest(executeTask.JobId, executeTask.BatchId, leaseId), null, null, cancellationToken), cancellationToken);
			logger.LogInformation("Done.");

			return LeaseResult.Success;
		}

		/// <summary>
		/// Executes a batch
		/// </summary>
		async Task ExecuteBatchAsync(ISession session, string leaseId, ExecuteJobTask executeTask, BeginBatchResponse batch, ILogger batchLogger, CancellationToken cancellationToken)
		{
			IRpcConnection rpcClient = session.RpcConnection;

			batchLogger.LogInformation("Executing batch {BatchId} using {Executor} executor", executeTask.BatchId, _settings.Executor.ToString());
			await session.TerminateProcessesAsync(batchLogger, cancellationToken);

			// Create an executor for this job
			string executorName = String.IsNullOrEmpty(executeTask.Executor) ? _settings.Executor : executeTask.Executor;

			JobExecutorFactory? executorFactory = _executorFactories.FirstOrDefault(x => x.Name.Equals(executorName, StringComparison.OrdinalIgnoreCase));
			if (executorFactory == null)
			{
				batchLogger.LogError("Unable to find executor '{ExecutorName}'", executorName);
				return;
			}

			JobExecutor executor = executorFactory.CreateExecutor(session, executeTask, batch);

			// Try to initialize the executor
			batchLogger.LogInformation("Initializing...");
			using (batchLogger.BeginIndentScope("  "))
			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("Initialize").StartActive();
				await executor.InitializeAsync(batchLogger, cancellationToken);
			}

			try
			{
				// Execute the steps
				for (; ; )
				{
					// Get the next step to execute
					BeginStepResponse step = await rpcClient.InvokeAsync((HordeRpc.HordeRpcClient x) => x.BeginStepAsync(new BeginStepRequest(executeTask.JobId, executeTask.BatchId, leaseId), null, null, cancellationToken), cancellationToken);
					if (step.State == BeginStepResponse.Types.Result.Waiting)
					{
						batchLogger.LogInformation("Waiting for dependency to be ready");
						await Task.Delay(TimeSpan.FromSeconds(20.0), cancellationToken);
						continue;
					}
					else if (step.State == BeginStepResponse.Types.Result.Complete)
					{
						break;
					}
					else if (step.State != BeginStepResponse.Types.Result.Ready)
					{
						batchLogger.LogError("Unexpected step state: {StepState}", step.State);
						break;
					}

					// Get current disk space available. This will allow us to more easily spot steps that eat up a lot of disk space.
					string? driveName;
					if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
					{
						driveName = Path.GetPathRoot(session.WorkingDir.FullName);
					}
					else
					{
						driveName = session.WorkingDir.FullName;
					}

					float availableFreeSpace = 0;
					if (driveName != null)
					{
						try
						{
							DriveInfo info = new DriveInfo(driveName);
							availableFreeSpace = (1.0f * info.AvailableFreeSpace) / 1024 / 1024 / 1024;
						}
						catch (Exception ex)
						{
							batchLogger.LogWarning(ex, "Unable to query disk info for path '{DriveName}'", driveName);
						}
					}

					// Print the new state
					Stopwatch stepTimer = Stopwatch.StartNew();

					batchLogger.LogInformation("Starting job {JobId}, batch {BatchId}, step {StepId} (Drive Space Left: {DriveSpaceRemaining} GB)", executeTask.JobId, executeTask.BatchId, step.StepId, availableFreeSpace.ToString("F1"));

					// Create a trace span
					using IScope scope = GlobalTracer.Instance.BuildSpan("Execute").WithResourceName(step.Name).StartActive();
					scope.Span.SetTag("stepId", step.StepId);
					scope.Span.SetTag("logId", step.LogId);
					//				using IDisposable TraceProperty = LogContext.PushProperty("dd.trace_id", CorrelationIdentifier.TraceId.ToString());
					//				using IDisposable SpanProperty = LogContext.PushProperty("dd.span_id", CorrelationIdentifier.SpanId.ToString());

					// Update the context to include information about this step
					JobStepOutcome stepOutcome;
					JobStepState stepState;
					using (batchLogger.BeginIndentScope("  "))
					{
						// Start writing to the log file
#pragma warning disable CA2000 // Dispose objects before losing scope
						await using (IServerLogger stepLogger = _serverLoggerFactory.CreateLogger(session, step.LogId, executeTask.JobId, executeTask.BatchId, step.StepId, step.Warnings))
						{
							// Execute the task
							ILogger forwardingLogger = new DefaultLoggerIndentHandler(stepLogger);
							if (_settings.WriteStepOutputToLogger)
							{
								forwardingLogger = new ForwardingLogger(_defaultLogger, forwardingLogger);
							}

							using CancellationTokenSource stepPollCancelSource = new CancellationTokenSource();
							using CancellationTokenSource stepAbortSource = new CancellationTokenSource();
							TaskCompletionSource<bool> stepFinishedSource = new TaskCompletionSource<bool>();
							Task stepPollTask = Task.Run(() => PollForStepAbort(rpcClient, executeTask.JobId, executeTask.BatchId, step.StepId, stepAbortSource, stepFinishedSource.Task, stepPollCancelSource.Token), cancellationToken);

							try
							{
								(stepOutcome, stepState) = await ExecuteStepAsync(executor, step, forwardingLogger, cancellationToken, stepAbortSource.Token);
							}
							finally
							{
								// Will get called even when cancellation token for the lease/batch fires
								stepFinishedSource.SetResult(true); // Tell background poll task to stop
								await stepPollTask;
							}

							// Kill any processes spawned by the step
							await session.TerminateProcessesAsync(batchLogger, cancellationToken);

							// Wait for the logger to finish
							await stepLogger.StopAsync();

							// Reflect the warnings/errors in the step outcome
							if (stepOutcome > stepLogger.Outcome)
							{
								stepOutcome = stepLogger.Outcome;
							}
						}
#pragma warning restore CA2000 // Dispose objects before losing scope

						// Update the server with the outcome from the step
						batchLogger.LogInformation("Marking step as complete (Outcome={Outcome}, State={StepState})", stepOutcome, stepState);
						await rpcClient.InvokeAsync((HordeRpc.HordeRpcClient x) => x.UpdateStepAsync(new UpdateStepRequest(executeTask.JobId, executeTask.BatchId, step.StepId, stepState, stepOutcome), null, null, cancellationToken), cancellationToken);
					}

					// Print the finishing state
					stepTimer.Stop();
					batchLogger.LogInformation("Completed in {Time}", stepTimer.Elapsed);
				}
			}
			catch (Exception ex)
			{
				if (cancellationToken.IsCancellationRequested && ex.IsCancellationException())
				{
					batchLogger.LogError("Step was aborted");
				}
				else
				{
					batchLogger.LogError(ex, "Exception while executing batch: {Ex}", ex);
				}
			}

			// Clean the environment
			batchLogger.LogInformation("Finalizing...");
			using (batchLogger.BeginIndentScope("  "))
			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("Finalize").StartActive();
				await executor.FinalizeAsync(batchLogger, CancellationToken.None);
			}
		}

		/// <summary>
		/// Executes a step
		/// </summary>
		/// <param name="executor">The executor to run this step</param>
		/// <param name="step">Step to execute</param>
		/// <param name="stepLogger">Logger for the step</param>
		/// <param name="cancellationToken">Cancellation token to abort the batch</param>
		/// <param name="stepCancellationToken">Cancellation token to abort only this individual step</param>
		/// <returns>Async task</returns>
		internal static async Task<(JobStepOutcome, JobStepState)> ExecuteStepAsync(JobExecutor executor, BeginStepResponse step, ILogger stepLogger, CancellationToken cancellationToken, CancellationToken stepCancellationToken)
		{
			using CancellationTokenSource combined = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, stepCancellationToken);
			try
			{
				JobStepOutcome stepOutcome = await executor.RunAsync(step, stepLogger, combined.Token);
				return (stepOutcome, JobStepState.Completed);
			}
			catch (Exception ex)
			{
				if (cancellationToken.IsCancellationRequested && ex.IsCancellationException())
				{
					stepLogger.LogError("The step was cancelled by batch/lease");
					throw;
				}

				if (stepCancellationToken.IsCancellationRequested && ex.IsCancellationException())
				{
					stepLogger.LogError("The step was intentionally cancelled");
					return (JobStepOutcome.Failure, JobStepState.Aborted);
				}

				stepLogger.LogError(ex, "Exception while executing step: {Ex}", ex);
				return (JobStepOutcome.Failure, JobStepState.Completed);
			}
		}

		internal async Task PollForStepAbort(IRpcConnection rpcClient, string jobId, string batchId, string stepId, CancellationTokenSource stepCancelSource, Task finishedTask, CancellationToken cancellationToken)
		{
			Stopwatch timer = Stopwatch.StartNew();
			while (!finishedTask.IsCompleted)
			{
				try
				{
					GetStepResponse res = await rpcClient.InvokeAsync((HordeRpc.HordeRpcClient x) => x.GetStepAsync(new GetStepRequest(jobId, batchId, stepId), null, null, cancellationToken), cancellationToken);
					if (res.AbortRequested)
					{
						_defaultLogger.LogDebug("Step was aborted by server (JobId={JobId} BatchId={BatchId} StepId={StepId})", jobId, batchId, stepId);
						stepCancelSource.Cancel();
						break;
					}
				}
				catch (RpcException ex)
				{
					_defaultLogger.LogError(ex, "Poll for step abort has failed. Aborting (JobId={JobId} BatchId={BatchId} StepId={StepId})", jobId, batchId, stepId);
					stepCancelSource.Cancel();
					break;
				}

				await Task.WhenAny(Task.Delay(_stepAbortPollInterval, cancellationToken), finishedTask);
			}
		}
	}
}

