// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using EpicGames.Core;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Perforce.Managed;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Leases
{
	using ByteString = Google.Protobuf.ByteString;

	/// <summary>
	/// Implements the message handling loop for an agent. Runs asynchronously until disposed.
	/// </summary>
	class LeaseManager
	{
		/// <summary>
		/// Stores information about an active session
		/// </summary>
		internal class LeaseInfo
		{
			/// <summary>
			/// The worker lease state
			/// </summary>
			public Lease Lease { get; set; }

			/// <summary>
			/// Identifier for this lease
			/// </summary>
			public LeaseId LeaseId { get; set; }

			/// <summary>
			/// The task being executed for this lease
			/// </summary>
			public Task? Task { get; set; }

			/// <summary>
			/// Source for cancellation tokens for this session.
			/// </summary>
			public CancellationTokenSource CancellationTokenSource { get; }

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="lease">The worker lease state</param>
			public LeaseInfo(Lease lease)
			{
				Lease = lease;
				LeaseId = LeaseId.Parse(lease.Id);
				CancellationTokenSource = new CancellationTokenSource();
			}
		}

		/// <summary>
		/// Object used for controlling access to the access tokens and active sessions list
		/// </summary>
		readonly object _lockObject = new object();

		/// <summary>
		/// The list of active leases.
		/// </summary>
		readonly List<LeaseInfo> _activeLeases = new List<LeaseInfo>();

		/// <summary>
		/// Number of leases completed
		/// </summary>
		public int NumLeasesCompleted { get; private set; }

		/// <summary>
		/// Whether the agent is currently in an unhealthy state
		/// </summary>
		readonly bool _unhealthy = false;

		/// <summary>
		/// Result from executing this session
		/// </summary>
		SessionResult? _sessionResult;

		/// <summary>
		/// Task completion source used to trigger the background thread to update the leases. Must take a lock on LockObject before 
		/// </summary>
		readonly AsyncEvent _updateLeasesEvent = new AsyncEvent();

		/// <summary>
		/// Number of times UpdateSession has failed
		/// </summary>
		int _updateSessionFailures;

		/// <summary>
		/// Delegate for lease active events
		/// </summary>
		public delegate void LeaseActiveEvent(Lease lease);

		/// <summary>
		/// Event triggered when a lease is accepted and set to active
		/// </summary>
		public event LeaseActiveEvent? OnLeaseActive;

		/// <summary>
		/// List of pool IDs this session is a member of
		/// </summary>
		public IReadOnlyList<string> PoolIds { get; private set; } = new List<string>();

		/// <summary>
		/// How long to wait before trying to reacquire a new connection
		/// Exposed as internal to ease testing. Using a lower delay can speed up tests. 
		/// </summary>
		internal TimeSpan _rpcConnectionRetryDelay = TimeSpan.FromSeconds(5);

		readonly ISession _session;
		readonly CapabilitiesService _capabilitiesService;
		readonly StatusService _statusService;
		readonly Dictionary<string, LeaseHandler> _typeUrlToLeaseHandler;
		readonly LeaseLoggerFactory _leaseLoggerFactory;
		readonly ILogger _logger;

		AgentCapabilities? _capabilities;

		public LeaseManager(ISession session, CapabilitiesService capabilitiesService, StatusService statusService, IEnumerable<LeaseHandler> leaseHandlers, LeaseLoggerFactory leaseLoggerFactory, ILogger logger)
		{
			_session = session;
			_capabilitiesService = capabilitiesService;
			_statusService = statusService;
			_typeUrlToLeaseHandler = leaseHandlers.ToDictionary(x => x.LeaseType, x => x);
			_leaseLoggerFactory = leaseLoggerFactory;
			_logger = logger;
		}

		public LeaseManager(ISession session, IServiceProvider serviceProvider)
			: this(session, serviceProvider.GetRequiredService<CapabilitiesService>(), serviceProvider.GetRequiredService<StatusService>(), serviceProvider.GetRequiredService<IEnumerable<LeaseHandler>>(), serviceProvider.GetRequiredService<LeaseLoggerFactory>(), serviceProvider.GetRequiredService<ILogger<LeaseManager>>())
		{
		}

		public async Task<SessionResult> RunAsync(bool shutdownAfterFinishedLease, CancellationToken stoppingToken)
		{
			SessionResult result;
			try
			{
				result = await HandleSessionAsync(shutdownAfterFinishedLease, stoppingToken);
			}
			catch (OperationCanceledException ex) when (stoppingToken.IsCancellationRequested)
			{
				_logger.LogInformation(ex, "Execution cancelled");
				result = new SessionResult(SessionOutcome.Terminate);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Exception while executing session. Restarting.");
				result = new SessionResult(SessionOutcome.BackOff);
			}

			while (_activeLeases.Count > 0)
			{
				try
				{
					_logger.LogInformation("Draining leases... ({NumLeases} remaining)", _activeLeases.Count);
					await DrainLeasesAsync();
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while draining leases. Agent may be in an inconsistent state.");
					await Task.Delay(TimeSpan.FromSeconds(30.0), CancellationToken.None);
				}
			}
			return result;
		}

		async Task DrainLeasesAsync()
		{
			for (int idx = 0; idx < _activeLeases.Count; idx++)
			{
				LeaseInfo activeLease = _activeLeases[idx];
				if (activeLease.Task == null)
				{
					_activeLeases.RemoveAt(idx--);
					_logger.LogInformation("Removed lease {LeaseId}", activeLease.Lease.Id);
				}
				else
				{
					_logger.LogInformation("Cancelling active lease {LeaseId}", activeLease.Lease.Id);
					activeLease.CancellationTokenSource.Cancel();
				}
			}

			while (_activeLeases.Count > 0)
			{
				List<Task> tasks = _activeLeases.Select(x => x.Task!).ToList();
				tasks.Add(Task.Delay(TimeSpan.FromMinutes(1.0)));
				await Task.WhenAny(tasks);

				for (int idx = 0; idx < _activeLeases.Count; idx++)
				{
					LeaseInfo activeLease = _activeLeases[idx];
					if (activeLease.Task!.IsCompleted)
					{
						_activeLeases.RemoveAt(idx--);
						try
						{
							await activeLease.Task;
						}
						catch (OperationCanceledException)
						{
						}
						catch (Exception ex)
						{
							_logger.LogError(ex, "Lease {LeaseId} threw an exception while terminating", activeLease.Lease.Id);
						}
						_logger.LogInformation("Lease {LeaseId} has completed", activeLease.Lease.Id);
					}
					else
					{
						_logger.LogInformation("Still waiting for lease {LeaseId} to terminate...", activeLease.Lease.Id);
					}
				}
			}
		}

		async Task<SessionResult> HandleSessionAsync(bool shutdownAfterFinishedLease, CancellationToken stoppingToken)
		{
			IRpcConnection rpcCon = _session.RpcConnection;
			HordeRpc.HordeRpcClient rpcClient = new(_session.GrpcChannel);

			// Terminate any remaining child processes from other instances
			await _session.TerminateProcessesAsync(TerminateCondition.BeforeSession, _logger, stoppingToken);

			// Track how many updates we get in 10 seconds. We'll start rate limiting this if it looks like we've got a problem that's causing us to spam the server.
			Stopwatch updateTimer = Stopwatch.StartNew();
			Queue<TimeSpan> updateTimes = new Queue<TimeSpan>();

			// Run a background task to update the capabilities of this agent
			await using BackgroundTask updateCapsTask = BackgroundTask.StartNew(ctx => UpdateCapabilitiesBackgroundAsync(_session.WorkingDir, ctx));

			// Loop until we're ready to exit
			Stopwatch updateCapabilitiesTimer = Stopwatch.StartNew();
			for (; ; )
			{
				Task waitTask = Task.WhenAny(_updateLeasesEvent.Task, _statusService.StatusChangedEvent.Task);

				// Flag for whether the service is stopping
				if (stoppingToken.IsCancellationRequested && _sessionResult == null)
				{
					_logger.LogInformation("Cancellation from token requested; setting session result to terminate.");
					_sessionResult = new SessionResult(SessionOutcome.Terminate);
				}

				bool stopping = false;
				if (_sessionResult != null)
				{
					_logger.LogInformation("Session termination requested (result: {Result})", _sessionResult.Outcome);
					stopping = true;
				}

				// Build the next update request
				UpdateSessionRequest updateSessionRequest = new UpdateSessionRequest();
				updateSessionRequest.AgentId = _session.AgentId.ToString();
				updateSessionRequest.SessionId = _session.SessionId.ToString();

				// Get the new the lease states. If a restart is requested and we have no active leases, signal to the server that we're stopping.
				lock (_lockObject)
				{
					foreach (LeaseInfo leaseInfo in _activeLeases)
					{
						updateSessionRequest.Leases.Add(new Lease(leaseInfo.Lease));
					}
					if (_sessionResult != null && _activeLeases.Count == 0)
					{
						stopping = true;
					}
				}

				// Get the new agent status
				bool busy = _statusService.IsBusy;
				if (stopping)
				{
					updateSessionRequest.Status = RpcAgentStatus.Stopping;
				}
				else if (_unhealthy)
				{
					updateSessionRequest.Status = RpcAgentStatus.Unhealthy;
				}
				else if (busy)
				{
					updateSessionRequest.Status = RpcAgentStatus.Busy;
				}
				else
				{
					updateSessionRequest.Status = RpcAgentStatus.Ok;
				}

				// Update the capabilities whenever the background task has generated a new instance
				updateSessionRequest.Capabilities = Interlocked.Exchange(ref _capabilities, null);

				// Complete the wait task if we subsequently stop
				using (stopping ? (CancellationTokenRegistration?)null : stoppingToken.Register(() => _updateLeasesEvent.Set()))
				{
					// Update the state with the server
					UpdateSessionResponse? updateSessionResponse = await UpdateSessionAsync(rpcClient, updateSessionRequest, waitTask);

					lock (_lockObject)
					{
						// Now reconcile the local state to match what the server reports
						if (updateSessionResponse != null)
						{
							bool atLeastOneLeaseFinished = _activeLeases.Any(x => x.Lease.State is RpcLeaseState.Completed or RpcLeaseState.Cancelled);
							if (atLeastOneLeaseFinished && shutdownAfterFinishedLease)
							{
								_logger.LogInformation("At least one lease executed. Requesting shutdown.");
								_sessionResult = new SessionResult(SessionOutcome.Terminate);
							}

							PoolIds = updateSessionResponse.PoolIds;

							// Remove any leases which have completed
							int numRemoved = _activeLeases.RemoveAll(x => (x.Lease.State == RpcLeaseState.Completed || x.Lease.State == RpcLeaseState.Cancelled) && !updateSessionResponse.Leases.Any(y => y.Id == x.Lease.Id && y.State != RpcLeaseState.Cancelled));
							NumLeasesCompleted += numRemoved;

							// Create any new leases and cancel any running leases
							foreach (Lease serverLease in updateSessionResponse.Leases)
							{
								if (serverLease.State == RpcLeaseState.Cancelled)
								{
									LeaseInfo? info = _activeLeases.FirstOrDefault(x => x.Lease.Id == serverLease.Id);
									if (info != null)
									{
										_logger.LogInformation("Cancelling lease {LeaseId}", serverLease.Id);
										info.CancellationTokenSource.Cancel();
									}
								}
								if (serverLease.State == RpcLeaseState.Pending && !_activeLeases.Any(x => x.Lease.Id == serverLease.Id))
								{
									serverLease.State = RpcLeaseState.Active;

									_logger.LogInformation("Adding lease {LeaseId}", serverLease.Id);
									LeaseInfo info = new LeaseInfo(serverLease);
									info.Task = Task.Run(() => HandleLeaseAsync(_session, info), CancellationToken.None);
									_activeLeases.Add(info);
									OnLeaseActive?.Invoke(serverLease);
								}
							}

							// Update the session result if we've transitioned to stopped
							if (updateSessionResponse.Status == RpcAgentStatus.Stopped)
							{
								_logger.LogInformation("Agent status is stopped; returning from session update loop.");
								return _sessionResult ?? new SessionResult(SessionOutcome.BackOff);
							}
						}

						// If there's nothing still running and cancellation was requested, exit
						if (_activeLeases.Count == 0 && _sessionResult != null)
						{
							_logger.LogInformation("No leases are active. Agent is stopping."); // TODO: Should not really hit this any more; server should report stopping state in lease update above.
							return _sessionResult;
						}
					}

					// Update the current status
					if (!rpcCon.Healthy)
					{
						_statusService.Set(false, _activeLeases.Count, "Attempting to connect to server...");
					}
					else if (busy)
					{
						_statusService.Set(true, 0, "Paused");

						if (_activeLeases.Count > 0)
						{
							_logger.LogInformation("Agent marked itself as busy. Draining any active leases to prevent them from using up local resources...");
							await DrainLeasesAsync();
						}
					}
					else if (_activeLeases.Count == 0)
					{
						_statusService.Set(true, 0, "Waiting for work");
					}
					else
					{
						_statusService.Set(true, _activeLeases.Count, $"Executing {_activeLeases.Count} lease(s)");
					}
				}

				// Update the historical update times
				TimeSpan updateTime = updateTimer.Elapsed;
				while (updateTimes.TryPeek(out TimeSpan firstTime) && firstTime < updateTime - TimeSpan.FromMinutes(1.0))
				{
					updateTimes.Dequeue();
				}
				updateTimes.Enqueue(updateTime);

				// If we're updating too much, introduce an artificial delay
				if (updateTimes.Count > 60)
				{
					_logger.LogWarning("Agent is issuing large number of UpdateSession() calls. Delaying for 10 seconds.");
					await Task.Delay(TimeSpan.FromSeconds(10.0), stoppingToken);
				}
			}
		}

		/// <summary>
		/// Wrapper for <see cref="UpdateSessionInternalAsync"/> which filters/logs exceptions
		/// </summary>
		/// <param name="rpcClient">The RPC client connection</param>
		/// <param name="updateSessionRequest">The session update request</param>
		/// <param name="waitTask">Task which can be used to jump out of the update early</param>
		/// <returns>Response from the call</returns>
		async Task<UpdateSessionResponse?> UpdateSessionAsync(HordeRpc.HordeRpcClient rpcClient, UpdateSessionRequest updateSessionRequest, Task waitTask)
		{
			UpdateSessionResponse? updateSessionResponse = null;
			try
			{
				updateSessionResponse = await UpdateSessionInternalAsync(rpcClient, updateSessionRequest, waitTask);
				_updateSessionFailures = 0;
			}
			catch (RpcException ex)
			{
				if (++_updateSessionFailures >= 3)
				{
					throw;
				}
				else if (ex.StatusCode == StatusCode.Unavailable)
				{
					_logger.LogInformation(ex, "Service unavailable while calling UpdateSessionAsync(). Will retry...");
				}
				else
				{
					_logger.LogError(ex, "Error while executing RPC. Will retry...");
				}
			}
			return updateSessionResponse;
		}

		/// <summary>
		/// Tries to update the session state on the server.
		/// 
		/// This operation is a little gnarly due to the fact that we want to long-poll for the result.
		/// Since we're doing the update via a gRPC call, the way to do that without using cancellation tokens is to keep the request stream open
		/// until we want to terminate the call (see https://github.com/grpc/grpc/issues/8277). In order to do that, we need to make a 
		/// bidirectional streaming call, even though we only expect one response/response.
		/// </summary>
		/// <param name="rpcClient">The RPC client</param>
		/// <param name="request">The session update request</param>
		/// <param name="waitTask">Task to use to terminate the wait</param>
		/// <returns>The response object</returns>
		async Task<UpdateSessionResponse?> UpdateSessionInternalAsync(HordeRpc.HordeRpcClient rpcClient, UpdateSessionRequest request, Task waitTask)
		{
			DateTime deadline = DateTime.UtcNow + TimeSpan.FromMinutes(2.0);
			using AsyncDuplexStreamingCall<UpdateSessionRequest, UpdateSessionResponse> call = rpcClient.UpdateSession(deadline: deadline);
			_logger.LogDebug("Updating session {SessionId} (Status={Status})", request.SessionId, request.Status);

			// Write the request to the server
			await call.RequestStream.WriteAsync(request);

			// Wait until the server responds or we need to trigger a new update
			Task<bool> moveNextAsync = call.ResponseStream.MoveNext();

			Task task = await Task.WhenAny(moveNextAsync, waitTask);
			if (task == waitTask)
			{
				_logger.LogDebug("Cancelling long poll from client side (new update)");
			}

			// Close the request stream to indicate that we're finished
			await call.RequestStream.CompleteAsync();

			// Wait for a response or a new update to come in, then close the request stream
			UpdateSessionResponse? response = null;
			while (await moveNextAsync)
			{
				response = call.ResponseStream.Current;
				moveNextAsync = call.ResponseStream.MoveNext();
			}
			return response;
		}

		/// <summary>
		/// Handle a lease request
		/// </summary>
		/// <param name="session">The current session</param>
		/// <param name="leaseInfo">Information about the lease</param>
		/// <returns>Async task</returns>
		async Task HandleLeaseAsync(ISession session, LeaseInfo leaseInfo)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("HandleLease").WithResourceName(leaseInfo.Lease.Id).StartActive();
			scope.Span.SetTag("LeaseId", leaseInfo.Lease.Id);
			scope.Span.SetTag("AgentId", session.AgentId.ToString());
			//			using IDisposable TraceProperty = LogContext.PushProperty("dd.trace_id", CorrelationIdentifier.TraceId.ToString());
			//			using IDisposable SpanProperty = LogContext.PushProperty("dd.span_id", CorrelationIdentifier.SpanId.ToString());

			_logger.LogInformation("Handling lease {LeaseId}", leaseInfo.Lease.Id);

			// Get the lease outcome
			LeaseResult result = LeaseResult.Failed;
			try
			{
				result = await HandleLeasePayloadAsync(session, leaseInfo);
			}
			catch (OperationCanceledException) when (leaseInfo.CancellationTokenSource.IsCancellationRequested)
			{
				_logger.LogInformation("Lease {LeaseId} cancelled", leaseInfo.Lease.Id);
			}
			catch (InsufficientSpaceException ex)
			{
				_logger.LogError(ex, "{Message}", ex.Message);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Unhandled exception while executing lease {LeaseId}: {Message}", leaseInfo.Lease.Id, ex.Message);
			}

			// Update the state of the lease
			lock (_lockObject)
			{
				if (leaseInfo.CancellationTokenSource.IsCancellationRequested)
				{
					leaseInfo.Lease.State = RpcLeaseState.Cancelled;
					leaseInfo.Lease.Outcome = RpcLeaseOutcome.Failed;
					leaseInfo.Lease.Output = ByteString.Empty;
				}
				else
				{
					leaseInfo.Lease.State = (result.Outcome == LeaseOutcome.Cancelled) ? RpcLeaseState.Cancelled : RpcLeaseState.Completed;
					leaseInfo.Lease.Outcome = (RpcLeaseOutcome)result.Outcome;
					leaseInfo.Lease.Output = (result.Output != null) ? ByteString.CopyFrom(result.Output) : ByteString.Empty;
				}
				_logger.LogInformation("Transitioning lease {LeaseId} to {State}, outcome={Outcome}", leaseInfo.Lease.Id, leaseInfo.Lease.State, leaseInfo.Lease.Outcome);

				if (result.SessionResult != null && _sessionResult == null)
				{
					_logger.LogInformation("Lease {LeaseId} is setting session result to {Result}", leaseInfo.Lease.Id, result.SessionResult.Outcome);
					_sessionResult = result.SessionResult;
				}
			}
			_updateLeasesEvent.Set();
		}

		/// <summary>
		/// Dispatch a lease payload to the appropriate handler
		/// </summary>
		/// <param name="session">The current session</param>
		/// <param name="leaseInfo">Information about the lease</param>
		/// <returns>Outcome from the lease</returns>
		internal async Task<LeaseResult> HandleLeasePayloadAsync(ISession session, LeaseInfo leaseInfo)
		{
			Any payload = leaseInfo.Lease.Payload;
			if (!_typeUrlToLeaseHandler.TryGetValue(payload.TypeUrl, out LeaseHandler? leaseHandler))
			{
				_logger.LogError("Invalid lease payload type ({PayloadType})", payload.TypeUrl);
				return LeaseResult.Failed;
			}

			using ILoggerFactory leaseLoggerFactory = _leaseLoggerFactory.CreateLoggerFactory(leaseInfo.LeaseId);
			ILogger leaseLogger = leaseLoggerFactory.CreateLogger(leaseHandler.GetType());

			GlobalTracer.Instance.ActiveSpan?.SetTag("task", payload.TypeUrl);
			return await leaseHandler.ExecuteAsync(session, LeaseId.Parse(leaseInfo.Lease.Id), payload, leaseLogger, leaseInfo.CancellationTokenSource.Token);
		}

		/// <summary>
		/// Background task that updates the capabilities of this agent
		/// </summary>
		async Task UpdateCapabilitiesBackgroundAsync(DirectoryReference workingDir, CancellationToken cancellationToken)
		{
			while (!cancellationToken.IsCancellationRequested)
			{
				await Task.Delay(TimeSpan.FromMinutes(5.0), cancellationToken);
				try
				{
					Interlocked.Exchange(ref _capabilities, await _capabilitiesService.GetCapabilitiesAsync(workingDir));
				}
				catch (Exception ex)
				{
					_logger.LogWarning(ex, "Unable to query agent capabilities. Ignoring.");
				}
			}
		}
	}
}
