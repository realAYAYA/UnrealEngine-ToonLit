// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Google.Protobuf;
using Grpc.Core;
using Horde.Agent.Execution;
using Horde.Agent.Parser;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using Horde.Common;
using Horde.Common.Rpc;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
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

		/// <summary>
		/// Current lease ID being executed
		/// </summary>
		public string? CurrentLeaseId { get; private set; } = null;
		
		/// <summary>
		/// Current job ID being executed
		/// </summary>
		public string? CurrentJobId { get; private set; } = null;
		
		/// <summary>
		/// Current job batch ID being executed
		/// </summary>
		public string? CurrentBatchId { get; private set; } = null;

		readonly IEnumerable<IJobExecutorFactory> _executorFactories;
		readonly AgentSettings _settings;
		readonly IServerStorageFactory _serverStorageFactory;
		readonly IServerLoggerFactory _serverLoggerFactory;
		readonly ILogger _defaultLogger;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobHandler(IEnumerable<IJobExecutorFactory> executorFactories, IOptions<AgentSettings> settings, IServerStorageFactory storageClientFactory, IServerLoggerFactory serverLoggerFactory, ILogger<JobHandler> defaultLogger)
		{
			_executorFactories = executorFactories;
			_settings = settings.Value;
			_serverStorageFactory = storageClientFactory;
			_serverLoggerFactory = serverLoggerFactory;
			_defaultLogger = defaultLogger;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, ExecuteJobTask executeTask, CancellationToken cancellationToken)
		{
			try
			{
				CurrentLeaseId = leaseId;
				CurrentJobId = executeTask.JobId;
				CurrentBatchId = executeTask.BatchId;
				
				executeTask.JobOptions ??= new JobOptions();

				if (executeTask.JobOptions.RunInSeparateProcess ?? false)
				{
					using (ManagedProcessGroup processGroup = new ManagedProcessGroup())
					{
						List<string> arguments = new List<string>();
						arguments.Add(Assembly.GetExecutingAssembly().Location);
						arguments.Add("execute");
						arguments.Add("job");
						arguments.Add($"-Server={_settings.GetCurrentServerProfile().Name}");
						arguments.Add($"-AgentId={session.AgentId}");
						arguments.Add($"-SessionId={session.SessionId}");
						arguments.Add($"-LeaseId={leaseId}");
						arguments.Add($"-WorkingDir={session.WorkingDir}");
						arguments.Add($"-Task={Convert.ToBase64String(executeTask.ToByteArray())}");

						string commandLine = CommandLineArguments.Join(arguments);
						_defaultLogger.LogInformation("Running child process with arguments: {CommandLine}",
							commandLine);

						using (ManagedProcess process = new ManagedProcess(processGroup, "dotnet", commandLine, null,
							       null, ProcessPriorityClass.Normal))
						{
							using (LogEventParser parser = new LogEventParser(_defaultLogger))
							{
								for (;;)
								{
									string? line = await process.ReadLineAsync(cancellationToken);
									if (line == null)
									{
										break;
									}

									parser.WriteLine(line);
								}
							}

							process.WaitForExit();

							if (process.ExitCode != 0)
							{
								return LeaseResult.Failed;
							}
						}
					}

					return LeaseResult.Success;
				}

				return await ExecuteInternalAsync(session, leaseId, executeTask, cancellationToken);
			}
			finally
			{
				CurrentLeaseId = null;
				CurrentJobId = null;
				CurrentBatchId = null;
			}
		}

		internal async Task<LeaseResult> ExecuteInternalAsync(ISession session, string leaseId, ExecuteJobTask executeTask, CancellationToken cancellationToken)
		{
			// Create a storage client for this session
			JobOptions jobOptions = executeTask.JobOptions;
			await using IServerLogger logger = _serverLoggerFactory.CreateLogger(session, executeTask.LogId, executeTask.JobId, executeTask.BatchId, null, null, jobOptions.UseNewLogStorage);

			logger.LogInformation("Executing job \"{JobName}\", jobId {JobId}, batchId {BatchId}, leaseId {LeaseId}", executeTask.JobName, executeTask.JobId, executeTask.BatchId, leaseId);
			GlobalTracer.Instance.ActiveSpan?.SetTag("jobId", executeTask.JobId.ToString());
			GlobalTracer.Instance.ActiveSpan?.SetTag("jobName", executeTask.JobName.ToString());
			GlobalTracer.Instance.ActiveSpan?.SetTag("batchId", executeTask.BatchId.ToString());

			logger.LogInformation("Executor: {Name}, UseNewLogStorage: {UseNewLogStorage}, UseNewTempStorage: {UseNewTempStorage}", jobOptions.Executor, jobOptions.UseNewLogStorage ?? false, jobOptions.UseNewTempStorage ?? false);

			// Start executing the current batch
			BeginBatchResponse batch = await session.RpcConnection.InvokeAsync<JobRpc.JobRpcClient, BeginBatchResponse>(x => x.BeginBatchAsync(new BeginBatchRequest(executeTask.JobId, executeTask.BatchId, leaseId), null, null, cancellationToken), cancellationToken);
			try
			{
				JobExecutorOptions options = new JobExecutorOptions(session, _serverStorageFactory, executeTask.JobId, executeTask.BatchId, batch, new NamespaceId(executeTask.NamespaceId), executeTask.StoragePrefix, executeTask.Token, jobOptions);
				await ExecuteBatchAsync(session, leaseId, executeTask.Workspace, executeTask.AutoSdkWorkspace, options, logger, cancellationToken);
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
			await session.RpcConnection.InvokeAsync((JobRpc.JobRpcClient x) => x.FinishBatchAsync(new FinishBatchRequest(executeTask.JobId, executeTask.BatchId, leaseId), null, null, cancellationToken), cancellationToken);
			logger.LogInformation("Done.");

			return LeaseResult.Success;
		}

		/// <summary>
		/// Executes a batch
		/// </summary>
		async Task ExecuteBatchAsync(ISession session, string leaseId, AgentWorkspace workspaceInfo, AgentWorkspace? autoSdkWorkspaceInfo, JobExecutorOptions options, ILogger batchLogger, CancellationToken cancellationToken)
		{
			IRpcConnection rpcClient = session.RpcConnection;

			// Create an executor for this job
			string executorName = String.IsNullOrEmpty(options.JobOptions.Executor) ? _settings.Executor : options.JobOptions.Executor;
			
			batchLogger.LogInformation("Executing batch {BatchId} using {Executor} executor", options.BatchId, executorName);
			await session.TerminateProcessesAsync(TerminateCondition.BeforeBatch, batchLogger, cancellationToken);

			IJobExecutorFactory? executorFactory = _executorFactories.FirstOrDefault(x => x.Name.Equals(executorName, StringComparison.OrdinalIgnoreCase));
			if (executorFactory == null)
			{
				batchLogger.LogError("Unable to find executor '{ExecutorName}'", executorName);
				return;
			}

			IJobExecutor executor = executorFactory.CreateExecutor(workspaceInfo, autoSdkWorkspaceInfo, options);

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
					BeginStepResponse step = await rpcClient.InvokeAsync((JobRpc.JobRpcClient x) => x.BeginStepAsync(new BeginStepRequest(options.JobId, options.BatchId, leaseId), null, null, cancellationToken), cancellationToken);
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

					batchLogger.LogInformation("Starting job {JobId}, batch {BatchId}, step {StepId} (Drive Space Left: {DriveSpaceRemaining} GB)", options.JobId, options.BatchId, step.StepId, availableFreeSpace.ToString("F1"));

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
						await using (IServerLogger stepLogger = _serverLoggerFactory.CreateLogger(session, step.LogId, options.JobId, options.BatchId, step.StepId, step.Warnings, options.JobOptions.UseNewLogStorage))
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
							Task stepPollTask = Task.Run(() => PollForStepAbort(rpcClient, options.JobId, options.BatchId, step.StepId, stepAbortSource, stepFinishedSource.Task, stepPollCancelSource.Token), cancellationToken);

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
							await session.TerminateProcessesAsync(TerminateCondition.AfterStep, batchLogger, cancellationToken);

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
						await rpcClient.InvokeAsync((JobRpc.JobRpcClient x) => x.UpdateStepAsync(new UpdateStepRequest(options.JobId, options.BatchId, step.StepId, stepState, stepOutcome), null, null, cancellationToken), cancellationToken);
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

			// Terminate any processes which are still running
			try
			{
				await session.TerminateProcessesAsync(TerminateCondition.AfterBatch, batchLogger, CancellationToken.None);
			}
			catch(Exception ex)
			{
				batchLogger.LogWarning(ex, "Exception while terminating processes: {Message}", ex.Message);
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
		internal static async Task<(JobStepOutcome, JobStepState)> ExecuteStepAsync(IJobExecutor executor, BeginStepResponse step, ILogger stepLogger, CancellationToken cancellationToken, CancellationToken stepCancellationToken)
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
					GetStepResponse res = await rpcClient.InvokeAsync((JobRpc.JobRpcClient x) => x.GetStepAsync(new GetStepRequest(jobId, batchId, stepId), null, null, cancellationToken), cancellationToken);
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

