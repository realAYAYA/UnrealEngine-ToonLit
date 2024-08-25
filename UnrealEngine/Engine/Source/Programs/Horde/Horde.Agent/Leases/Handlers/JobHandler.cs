// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage.Clients;
using Google.Protobuf;
using Grpc.Core;
using Horde.Agent.Execution;
using Horde.Agent.Services;
using Horde.Agent.Utility;
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
	record class JobTaskInfo(string JobName, JobId JobId, JobStepBatchId BatchId, JobOptions JobOptions, string Token, LogId LogId, AgentWorkspace Workspace, AgentWorkspace? AutoSdkWorkspace);

	class JobHandler : LeaseHandler<ExecuteJobTask>
	{
		/// <summary>
		/// How often to poll the server checking if a step has been aborted
		/// Exposed as internal to ease testing.
		/// </summary>
		internal TimeSpan _stepAbortPollInterval = TimeSpan.FromSeconds(5);

		/// <summary>
		/// How long to wait before retrying a failed step abort check request
		/// </summary>
		internal TimeSpan _stepAbortPollRetryDelay = TimeSpan.FromSeconds(30);

		/// <summary>
		/// Current lease ID being executed
		/// </summary>
		public LeaseId? CurrentLeaseId { get; private set; } = null;

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
		readonly HttpStorageClientFactory _serverStorageFactory;
		readonly IServerLoggerFactory _serverLoggerFactory;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobHandler(IEnumerable<IJobExecutorFactory> executorFactories, IOptions<AgentSettings> settings, HttpStorageClientFactory storageClientFactory, IServerLoggerFactory serverLoggerFactory)
		{
			_executorFactories = executorFactories;
			_settings = settings.Value;
			_serverStorageFactory = storageClientFactory;
			_serverLoggerFactory = serverLoggerFactory;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, ExecuteJobTask executeTask, ILogger localLogger, CancellationToken cancellationToken)
		{
			try
			{
				CurrentLeaseId = leaseId;
				CurrentJobId = executeTask.JobId;
				CurrentBatchId = executeTask.BatchId;

				executeTask.JobOptions ??= new JobOptions();

				if (executeTask.JobOptions.RunInSeparateProcess ?? false)
				{
					if (AgentApp.IsSelfContained)
					{
						// TODO: Implement handling for invoking a self-contained agent process (i.e handle "dotnet" below)
						throw new NotSupportedException("Running job in a separate process not supported for self-contained agents");
					}

					using (ManagedProcessGroup processGroup = new ManagedProcessGroup())
					{
						List<string> arguments = new List<string>();
#pragma warning disable IL3000 // Avoid accessing Assembly file path when publishing as a single file
						arguments.Add(Assembly.GetExecutingAssembly().Location);
#pragma warning restore IL3000 // Avoid accessing Assembly file path when publishing as a single file						
						arguments.Add("execute");
						arguments.Add("job");
						arguments.Add($"-Server={_settings.GetCurrentServerProfile().Name}");
						arguments.Add($"-AgentId={session.AgentId}");
						arguments.Add($"-SessionId={session.SessionId}");
						arguments.Add($"-LeaseId={leaseId}");
						arguments.Add($"-WorkingDir={session.WorkingDir}");
						arguments.Add($"-Task={Convert.ToBase64String(executeTask.ToByteArray())}");

						string commandLine = CommandLineArguments.Join(arguments);
						localLogger.LogInformation("Running child process with arguments: {CommandLine}", commandLine);

						using (ManagedProcess process = new ManagedProcess(processGroup, "dotnet", commandLine, null,
								   null, ProcessPriorityClass.Normal))
						{
							using (LogEventParser parser = new LogEventParser(localLogger))
							{
								for (; ; )
								{
									string? line = await process.ReadLineAsync(cancellationToken);
									if (line == null)
									{
										break;
									}

									parser.WriteLine(line);
								}
							}

							await process.WaitForExitAsync(CancellationToken.None);

							if (process.ExitCode != 0)
							{
								return LeaseResult.Failed;
							}
						}
					}

					return LeaseResult.Success;
				}

				return await ExecuteInternalAsync(session, leaseId, executeTask, localLogger, cancellationToken);
			}
			finally
			{
				CurrentLeaseId = null;
				CurrentJobId = null;
				CurrentBatchId = null;
			}
		}

		internal async Task<LeaseResult> ExecuteInternalAsync(ISession session, LeaseId leaseId, ExecuteJobTask executeTask, ILogger localLogger, CancellationToken cancellationToken)
		{
			JobTaskInfo jobTaskInfo = new JobTaskInfo(executeTask.JobName, JobId.Parse(executeTask.JobId), JobStepBatchId.Parse(executeTask.BatchId), executeTask.JobOptions, executeTask.Token, LogId.Parse(executeTask.LogId), executeTask.Workspace, executeTask.AutoSdkWorkspace);
			return await ExecuteInternalAsync(session, leaseId, jobTaskInfo, localLogger, cancellationToken);
		}

		internal async Task<LeaseResult> ExecuteInternalAsync(ISession session, LeaseId leaseId, JobTaskInfo executeTask, ILogger localLogger, CancellationToken cancellationToken)
		{
			// Create a storage client for this session
			JobOptions jobOptions = executeTask.JobOptions;
			await using IServerLogger logger = _serverLoggerFactory.CreateLogger(session, executeTask.LogId, localLogger, executeTask.JobId, executeTask.BatchId, null, null);

			logger.LogInformation("Executing job \"{JobName}\", jobId {JobId}, batchId {BatchId}, leaseId {LeaseId}, agentVersion {AgentVersion}", executeTask.JobName, executeTask.JobId, executeTask.BatchId, leaseId, AgentApp.Version);

			GlobalTracer.Instance.ActiveSpan?.SetTag("jobId", executeTask.JobId.ToString());
			GlobalTracer.Instance.ActiveSpan?.SetTag("jobName", executeTask.JobName.ToString());
			GlobalTracer.Instance.ActiveSpan?.SetTag("batchId", executeTask.BatchId.ToString());

			logger.LogInformation("Executor: {Name}, UseNewTempStorage: {UseNewTempStorage}", jobOptions.Executor, jobOptions.UseNewTempStorage ?? false);

			// Start executing the current batch
			BeginBatchResponse batch = await session.RpcConnection.InvokeAsync<JobRpc.JobRpcClient, BeginBatchResponse>(x => x.BeginBatchAsync(new BeginBatchRequest(executeTask.JobId, executeTask.BatchId, leaseId), null, null, cancellationToken), cancellationToken);
			try
			{
				JobExecutorOptions options = new JobExecutorOptions(session, _serverStorageFactory, executeTask.JobId, executeTask.BatchId, batch, executeTask.Token, jobOptions);
				await ExecuteBatchAsync(session, leaseId, executeTask.Workspace, executeTask.AutoSdkWorkspace, options, logger, localLogger, cancellationToken);
			}
			catch (Exception ex)
			{
				if (cancellationToken.IsCancellationRequested && ex.IsCancellationException())
				{
					if (session.RpcConnection.Healthy)
					{
						logger.LogInformation(ex, "Step was aborted");
					}
					else
					{
						logger.LogError(ex, "Connection to the server was lost; step aborted.");
					}
					throw;
				}
				else
				{
					logger.LogError(ex, "Exception while executing lease {LeaseId}: {Ex}", leaseId, ex.Message);
				}
			}

			// If this lease was cancelled, don't bother updating the job state.
			if (cancellationToken.IsCancellationRequested)
			{
				logger.LogInformation("Lease was cancelled.");
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
		async Task ExecuteBatchAsync(ISession session, LeaseId leaseId, AgentWorkspace workspaceInfo, AgentWorkspace? autoSdkWorkspaceInfo, JobExecutorOptions options, ILogger logger, ILogger localLogger, CancellationToken cancellationToken)
		{
			IRpcConnection rpcClient = session.RpcConnection;

			// Create an executor for this job
			string executorName = String.IsNullOrEmpty(options.JobOptions.Executor) ? _settings.Executor : options.JobOptions.Executor;

			logger.LogInformation("Executing batch {BatchId} using {Executor} executor", options.BatchId, executorName);
			await session.TerminateProcessesAsync(TerminateCondition.BeforeBatch, logger, cancellationToken);

			IJobExecutorFactory? executorFactory = _executorFactories.FirstOrDefault(x => x.Name.Equals(executorName, StringComparison.OrdinalIgnoreCase));
			if (executorFactory == null)
			{
				logger.LogError("Unable to find executor '{ExecutorName}'", executorName);
				return;
			}

			using IJobExecutor executor = executorFactory.CreateExecutor(workspaceInfo, autoSdkWorkspaceInfo, options);

			// Try to initialize the executor
			logger.LogInformation("Initializing executor...");
			using (logger.BeginIndentScope("  "))
			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("Initialize").StartActive();
				await executor.InitializeAsync(logger, cancellationToken);
			}

			try
			{
				// Execute the steps
				logger.LogInformation("Executing steps...");
				for (; ; )
				{
					// Get the next step to execute
					BeginStepResponse stepResponse = await rpcClient.InvokeAsync((JobRpc.JobRpcClient x) => x.BeginStepAsync(new BeginStepRequest(options.JobId, options.BatchId, leaseId), null, null, cancellationToken), cancellationToken);
					if (stepResponse.State == BeginStepResponse.Types.Result.Waiting)
					{
						logger.LogInformation("Waiting for dependency to be ready");
						await Task.Delay(TimeSpan.FromSeconds(20.0), cancellationToken);
						continue;
					}
					else if (stepResponse.State == BeginStepResponse.Types.Result.Complete)
					{
						logger.LogInformation("No more steps to execute; finalizing lease.");
						break;
					}
					else if (stepResponse.State != BeginStepResponse.Types.Result.Ready)
					{
						logger.LogError("Unexpected step state: {StepState}", stepResponse.State);
						break;
					}

					JobStepInfo step = new JobStepInfo(stepResponse);

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
							logger.LogWarning(ex, "Unable to query disk info for path '{DriveName}'", driveName);
						}
					}

					// Print the new state
					Stopwatch stepTimer = Stopwatch.StartNew();

					logger.LogInformation("Starting job {JobId}, batch {BatchId}, step {StepId} (Drive Space Left: {DriveSpaceRemaining} GB)", options.JobId, options.BatchId, step.StepId, availableFreeSpace.ToString("F1"));

					// Create a trace span
					using IScope scope = GlobalTracer.Instance.BuildSpan("Execute").WithResourceName(step.Name).StartActive();
					scope.Span.SetTag("stepId", step.StepId.ToString());
					scope.Span.SetTag("logId", step.LogId.ToString());
					//				using IDisposable TraceProperty = LogContext.PushProperty("dd.trace_id", CorrelationIdentifier.TraceId.ToString());
					//				using IDisposable SpanProperty = LogContext.PushProperty("dd.span_id", CorrelationIdentifier.SpanId.ToString());

					// Update the context to include information about this step
					JobStepOutcome stepOutcome;
					JobStepState stepState;
					using (logger.BeginIndentScope("  "))
					{
						// Start writing to the log file
#pragma warning disable CA2000 // Dispose objects before losing scope
						await using (IServerLogger stepLogger = _serverLoggerFactory.CreateLogger(session, step.LogId, localLogger, options.JobId, options.BatchId, step.StepId, step.Warnings))
						{
							// Execute the task
							using CancellationTokenSource stepPollCancelSource = new CancellationTokenSource();
							using CancellationTokenSource stepAbortSource = new CancellationTokenSource();
							TaskCompletionSource<bool> stepFinishedSource = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
							Task stepPollTask = Task.Run(() => PollForStepAbortAsync(rpcClient, options.JobId, options.BatchId, step.StepId, stepAbortSource, stepFinishedSource.Task, logger, stepPollCancelSource.Token), cancellationToken);

							try
							{
								ILogger forwardingLogger = new DefaultLoggerIndentHandler(stepLogger);
								(stepOutcome, stepState) = await ExecuteStepAsync(executor, step, forwardingLogger, cancellationToken, stepAbortSource.Token);
							}
							finally
							{
								// Will get called even when cancellation token for the lease/batch fires
								stepFinishedSource.SetResult(true); // Tell background poll task to stop
								await stepPollTask;
							}

							// Kill any processes spawned by the step
							await session.TerminateProcessesAsync(TerminateCondition.AfterStep, logger, cancellationToken);

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
						logger.LogInformation("Marking step as complete (Outcome={Outcome}, State={StepState})", stepOutcome, stepState);
						await rpcClient.InvokeAsync((JobRpc.JobRpcClient x) => x.UpdateStepAsync(new UpdateStepRequest(options.JobId, options.BatchId, step.StepId, stepState, stepOutcome), null, null, cancellationToken), cancellationToken);
					}

					// Print the finishing state
					stepTimer.Stop();
					logger.LogInformation("Completed in {Time}", stepTimer.Elapsed);
				}
			}
			catch (Exception ex)
			{
				if (cancellationToken.IsCancellationRequested && ex.IsCancellationException())
				{
					if (session.RpcConnection.Healthy)
					{
						logger.LogError("Lease was aborted");
					}
					else
					{
						logger.LogError("Connection to the server was lost; lease aborted.");
					}
				}
				else
				{
					logger.LogError(ex, "Exception while executing batch: {Ex}", ex);
				}
			}

			// Terminate any processes which are still running
			try
			{
				await session.TerminateProcessesAsync(TerminateCondition.AfterBatch, logger, CancellationToken.None);
			}
			catch (Exception ex)
			{
				logger.LogWarning(ex, "Exception while terminating processes: {Message}", ex.Message);
			}

			// Clean the environment
			logger.LogInformation("Finalizing...");
			using (logger.BeginIndentScope("  "))
			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("Finalize").StartActive();
				await executor.FinalizeAsync(logger, CancellationToken.None);
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
		internal static async Task<(JobStepOutcome, JobStepState)> ExecuteStepAsync(IJobExecutor executor, JobStepInfo step, ILogger stepLogger, CancellationToken cancellationToken, CancellationToken stepCancellationToken)
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

		internal async Task PollForStepAbortAsync(IRpcConnection rpcClient, JobId jobId, JobStepBatchId batchId, JobStepId stepId, CancellationTokenSource stepCancelSource, Task finishedTask, ILogger leaseLogger, CancellationToken cancellationToken)
		{
			while (!finishedTask.IsCompleted)
			{
				TimeSpan waitTime = _stepAbortPollInterval;
				try
				{
					GetStepResponse res = await rpcClient.InvokeAsync((JobRpc.JobRpcClient x) => x.GetStepAsync(new GetStepRequest(jobId, batchId, stepId), null, null, cancellationToken), cancellationToken);
					if (res.AbortRequested)
					{
						leaseLogger.LogDebug("Step was aborted by server (JobId={JobId} BatchId={BatchId} StepId={StepId})", jobId, batchId, stepId);
						stepCancelSource.Cancel();
						break;
					}
				}
				catch (RpcException ex)
				{
					// Don't let a single RPC failure abort the running step as there can be intermittent errors on the server
					// For example temporary downtime or overload
					leaseLogger.LogError(ex, "Poll for step abort failed (JobId={JobId} BatchId={BatchId} StepId={StepId}). Retrying...", jobId, batchId, stepId);
					waitTime = _stepAbortPollRetryDelay;
				}

				await Task.WhenAny(Task.Delay(waitTime, cancellationToken), finishedTask);
			}
		}
	}
}

