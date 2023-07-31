// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Grpc.Net.Client;
using Horde.Agent.Execution.Interfaces;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;

namespace Horde.Agent.Tests
{
	[TestClass]
	public class WorkerServiceTest
	{
		private readonly ILogger<WorkerService> _workerLogger;
		private readonly ILogger<GrpcService> _grpcServiceLogger;
		private readonly ILogger<FakeHordeRpcServer> _hordeRpcServerLogger;
		
		internal static IExecutor NullExecutor = new SimpleTestExecutor(async (step, logger, cancellationToken) =>
		{
			await Task.Delay(1, cancellationToken);
			return JobStepOutcome.Success;
		});

		public WorkerServiceTest()
		{
			using ILoggerFactory loggerFactory = LoggerFactory.Create(builder =>
			{
				builder.AddSimpleConsole(consoleLoggerOptions => consoleLoggerOptions.TimestampFormat = "[HH:mm:ss] ");
			});

			_workerLogger = loggerFactory.CreateLogger<WorkerService>();
			_grpcServiceLogger = loggerFactory.CreateLogger<GrpcService>();
			_hordeRpcServerLogger = loggerFactory.CreateLogger<FakeHordeRpcServer>();
		}

		internal static WorkerService GetWorkerService(
			ILogger<GrpcService> grpcServiceLogger,
			ILogger<WorkerService> workerServiceLogger,
			Func<IRpcConnection, ExecuteJobTask, BeginBatchResponse, IExecutor>? createExecutor = null,
			Func<GrpcChannel, HordeRpc.HordeRpcClient>? createHordeRpcClient = null)
		{
			AgentSettings settings = new ();
			ServerProfile profile = new ();
			profile.Name = "test";
			profile.Environment = "test-env";
			profile.Token = "bogus-token";
			profile.Url = new Uri("http://localhost");
			settings.ServerProfiles.Add(profile);
			settings.Server = "test";
			settings.WorkingDir = Path.GetTempPath();
			settings.Executor = ExecutorType.Test; // Not really used since the executor is overridden in the tests
			IOptions<AgentSettings> settingsMonitor = new TestOptionsMonitor<AgentSettings>(settings);

			GrpcService grpcService = new (settingsMonitor, grpcServiceLogger);
			WorkerService ws = new (workerServiceLogger, settingsMonitor, grpcService, null!, createExecutor, createHordeRpcClient);
			ws._rpcConnectionRetryDelay = TimeSpan.FromSeconds(0.1);
			return ws;
		}

		private WorkerService GetWorkerService(
			Func<IRpcConnection, ExecuteJobTask, BeginBatchResponse, IExecutor>? createExecutor = null,
			Func<GrpcChannel, HordeRpc.HordeRpcClient>? createHordeRpcClient = null)
		{
			return GetWorkerService(_grpcServiceLogger, _workerLogger, createExecutor, createHordeRpcClient);
		}

		[TestMethod]
		public async Task AbortExecuteStepTest()
		{
			using WorkerService ws = GetWorkerService();

			{
				using CancellationTokenSource cancelSource = new CancellationTokenSource();
				using CancellationTokenSource stepCancelSource = new CancellationTokenSource();

				IExecutor executor = new SimpleTestExecutor(async (stepResponse, logger, cancelToken) =>
				{
					cancelSource.CancelAfter(10);
					await Task.Delay(5000, cancelToken);
					return JobStepOutcome.Success;
				});
				await Assert.ThrowsExceptionAsync<TaskCanceledException>(() => WorkerService.ExecuteStepAsync(executor,
					new BeginStepResponse(), _workerLogger, cancelSource.Token, stepCancelSource.Token));
			}

			{
				using CancellationTokenSource cancelSource = new CancellationTokenSource();
				using CancellationTokenSource stepCancelSource = new CancellationTokenSource();

				IExecutor executor = new SimpleTestExecutor(async (stepResponse, logger, cancelToken) =>
				{
					stepCancelSource.CancelAfter(10);
					await Task.Delay(5000, cancelToken);
					return JobStepOutcome.Success;
				});
				(JobStepOutcome stepOutcome, JobStepState stepState) = await WorkerService.ExecuteStepAsync(executor, new BeginStepResponse(), _workerLogger,
					cancelSource.Token, stepCancelSource.Token);
				Assert.AreEqual(JobStepOutcome.Failure, stepOutcome);
				Assert.AreEqual(JobStepState.Aborted, stepState);
			}
		}

		[TestMethod]
		public async Task AbortExecuteJobTest()
		{
			using CancellationTokenSource source = new CancellationTokenSource();
			CancellationToken token = source.Token;

			ExecuteJobTask executeJobTask = new ExecuteJobTask();
			executeJobTask.JobId = "jobId1";
			executeJobTask.BatchId = "batchId1";
			executeJobTask.LogId = "logId1";
			executeJobTask.JobName = "jobName1";
			executeJobTask.AutoSdkWorkspace = new AgentWorkspace();
			executeJobTask.Workspace = new AgentWorkspace();

			HordeRpcClientStub client = new HordeRpcClientStub(_workerLogger);
			await using RpcConnectionStub rpcConnection = new RpcConnectionStub(null!, client);

			client.BeginStepResponses.Enqueue(new BeginStepResponse {Name = "stepName1", StepId = "stepId1"});
			client.BeginStepResponses.Enqueue(new BeginStepResponse {Name = "stepName2", StepId = "stepId2"});
			client.BeginStepResponses.Enqueue(new BeginStepResponse {Name = "stepName3", StepId = "stepId3"});

			GetStepRequest step2Req = new GetStepRequest(executeJobTask.JobId, executeJobTask.BatchId, "stepId2");
			GetStepResponse step2Res = new GetStepResponse(JobStepOutcome.Unspecified, JobStepState.Unspecified, true);
			client.GetStepResponses[step2Req] = step2Res;

			IExecutor executor = new SimpleTestExecutor(async (step, logger, cancelToken) =>
			{
				await Task.Delay(50, cancelToken);
				return JobStepOutcome.Success;
			});

			using WorkerService ws = GetWorkerService((a, b, c) => executor);
			ws._stepAbortPollInterval = TimeSpan.FromMilliseconds(1);
			LeaseOutcome outcome = (await ws.ExecuteJobAsync(rpcConnection, "leaseId1", executeJobTask,
				_workerLogger, token)).Outcome;

			Assert.AreEqual(LeaseOutcome.Success, outcome);
			Assert.AreEqual(4, client.UpdateStepRequests.Count);
			// An extra UpdateStep request is sent by JsonRpcLogger on failures which is why four requests
			// are returned instead of three.
			Assert.AreEqual(JobStepOutcome.Success, client.UpdateStepRequests[0].Outcome);
			Assert.AreEqual(JobStepState.Completed, client.UpdateStepRequests[0].State);
			Assert.AreEqual(JobStepOutcome.Failure, client.UpdateStepRequests[1].Outcome);
			Assert.AreEqual(JobStepState.Unspecified, client.UpdateStepRequests[1].State);
			Assert.AreEqual(JobStepOutcome.Failure, client.UpdateStepRequests[2].Outcome);
			Assert.AreEqual(JobStepState.Aborted, client.UpdateStepRequests[2].State);
			Assert.AreEqual(JobStepOutcome.Success, client.UpdateStepRequests[3].Outcome);
			Assert.AreEqual(JobStepState.Completed, client.UpdateStepRequests[3].State);
		}
		
		[TestMethod]
		public async Task PollForStepAbortFailureTest()
		{
			IExecutor executor = new SimpleTestExecutor(async (step, logger, cancelToken) =>
			{
				await Task.Delay(50, cancelToken);
				return JobStepOutcome.Success;
			});

			using WorkerService ws = GetWorkerService((a, b, c) => executor);
			ws._stepAbortPollInterval = TimeSpan.FromMilliseconds(5);
			
			HordeRpcClientStub client = new HordeRpcClientStub(_workerLogger);
			await using RpcConnectionStub rpcConnection = new RpcConnectionStub(null!, client);

			int c = 0;
			client._getStepFunc = (request) =>
			{
				return ++c switch
				{
					1 => new GetStepResponse {AbortRequested = false},
					2 => throw new RpcException(new Status(StatusCode.Cancelled, "Fake cancel from test")),
					3 => new GetStepResponse {AbortRequested = true},
					_ => throw new Exception("Should never reach here")
				};
			};

			using CancellationTokenSource stepPollCancelSource = new CancellationTokenSource();
			using CancellationTokenSource stepCancelSource = new CancellationTokenSource();
			TaskCompletionSource<bool> stepFinishedSource = new TaskCompletionSource<bool>();

			await ws.PollForStepAbort(rpcConnection, "jobId1", "batchId1", "logId1", stepCancelSource, stepFinishedSource.Task, stepPollCancelSource.Token);
			Assert.IsTrue(stepCancelSource.IsCancellationRequested);
		}

		[TestMethod]
		public async Task Shutdown()
		{
			IExecutor executor = new SimpleTestExecutor(async (step, logger, cancellationToken) =>
			{
				await Task.Delay(50, cancellationToken);
				return JobStepOutcome.Success;
			});
			
			using CancellationTokenSource cts = new ();
			cts.CancelAfter(20000);

			FakeHordeRpcServer fakeServer = new("bogusServerName", _hordeRpcServerLogger, cts.Token);
			using WorkerService ws = GetWorkerService((a, b, c) => executor,(c) => fakeServer.GetClient());

			Task handleSessionTask = ws.HandleSessionAsync(false, false, cts.Token);
			await fakeServer.CreateSessionReceived.Task.WaitAsync(cts.Token);
			await fakeServer.UpdateSessionReceived.Task.WaitAsync(cts.Token);
			await ws.ShutdownAsync(_workerLogger);
			await handleSessionTask; // Ensure it runs to completion and no exceptions are raised
		}
	}

	/// <summary>
	/// Fake implementation of a HordeRpc gRPC server.
	/// Provides a corresponding gRPC client class that can be used with the WorkerService
	/// to test client-server interactions.
	/// </summary>
	internal class FakeHordeRpcServer
	{
		private readonly string _serverName;
		private bool _isStopping = false;
		private Dictionary<string, Lease> _leases = new();
		private readonly Mock<HordeRpc.HordeRpcClient> _mockClient;
		private readonly ILogger<FakeHordeRpcServer> _logger;
		public readonly TaskCompletionSource<bool> CreateSessionReceived = new();
		public readonly TaskCompletionSource<bool> UpdateSessionReceived = new();

		public FakeHordeRpcServer(string serverName,ILogger<FakeHordeRpcServer> logger, CancellationToken cancellationToken)
		{
			_serverName = serverName;
			_mockClient = new (MockBehavior.Strict);
			_logger = logger;

			_mockClient
				.Setup(m => m.CreateSessionAsync(It.IsAny<CreateSessionRequest>(), null, null, It.IsAny<CancellationToken>()))
				.Returns<CreateSessionRequest, Metadata, DateTime?, CancellationToken>((request, metadata, expireTime, cancellationToken) => CreateAsyncUnaryCall(OnCreateSessionRequest(request)));
			
			_mockClient
				.Setup(m => m.QueryServerStateV2(null, null, It.IsAny<CancellationToken>()))
				.Returns(() => GetQueryServerStateCall(cancellationToken));
			
			_mockClient
				.Setup(m => m.UpdateSession(null, It.IsAny<DateTime>(), It.IsAny<CancellationToken>()))
				.Returns(() => GetUpdateSessionCall(cancellationToken));
		}

		public void AddTestLease(string leaseId)
		{
			if (_leases.ContainsKey(leaseId))
			{
				throw new ArgumentException($"Lease ID {leaseId} already exists");
			}
			
			TestTask testTask = new();
			_leases[leaseId] = new Lease
			{
				Id = leaseId,
				State = LeaseState.Pending,
				Payload = Any.Pack(testTask)
			};
		}

		public Lease GetLease(string leaseId)
		{
			return _leases[leaseId];
		}

		public HordeRpc.HordeRpcClient GetClient()
		{
			return _mockClient.Object;
		}

		public CreateSessionResponse OnCreateSessionRequest(CreateSessionRequest request)
		{
			CreateSessionReceived.TrySetResult(true);
			_logger.LogInformation("OnCreateSessionRequest: {Name} {Status}", request.Name, request.Status);
			CreateSessionResponse response = new()
			{
				AgentId = "bogusAgentId",
				Token = "bogusToken",
				SessionId = "bogusSessionId",
				ExpiryTime = Timestamp.FromDateTime(DateTime.UtcNow.AddHours(3)),
			};

			return response;
		}

		public AsyncDuplexStreamingCall<QueryServerStateRequest, QueryServerStateResponse> GetQueryServerStateCall(CancellationToken cancellationToken)
		{
			FakeAsyncStreamReader<QueryServerStateResponse> responseStream = new(cancellationToken);
			FakeClientStreamWriter<QueryServerStateRequest> requestStream = new(onComplete: () =>
			{
				responseStream.Complete();
				return Task.CompletedTask;
			});

			responseStream.Write(new QueryServerStateResponse { Name = _serverName, Stopping = _isStopping });
			
			return new (
				requestStream,
				responseStream,
				Task.FromResult(new Metadata()),
				() => Status.DefaultSuccess,
				() => new Metadata(),
				() => { /*isDisposed = true;*/ });
		}
		
		public AsyncDuplexStreamingCall<UpdateSessionRequest, UpdateSessionResponse> GetUpdateSessionCall(CancellationToken cancellationToken)
		{
			FakeAsyncStreamReader<UpdateSessionResponse> responseStream = new(cancellationToken);
			
			async Task OnRequest(UpdateSessionRequest request)
			{
				UpdateSessionReceived.TrySetResult(true);

				foreach (Lease agentLease in request.Leases)
				{
					Lease serverLease = _leases[agentLease.Id];
					serverLease.State = agentLease.State;
					serverLease.Outcome = agentLease.Outcome;
					serverLease.Output = agentLease.Output;
				}
				
				_logger.LogInformation("OnUpdateSessionRequest: {AgentId} {SessionId} {Status}", request.AgentId, request.SessionId, request.Status);
				await Task.Delay(100, cancellationToken);
				UpdateSessionResponse response = new () { ExpiryTime = Timestamp.FromDateTime(DateTime.UtcNow + TimeSpan.FromMinutes(120)) };
				response.Leases.AddRange(_leases.Values.Where(x => x.State != LeaseState.Completed));
				await responseStream.Write(response);
			}
			
			FakeClientStreamWriter<UpdateSessionRequest> requestStream = new(OnRequest, () => {
				responseStream.Complete();
				return Task.CompletedTask;
			});
			
			return new (
				requestStream,
				responseStream,
				Task.FromResult(new Metadata()),
				() => Status.DefaultSuccess,
				() => new Metadata(),
				() => { });
		}
		
		public static AsyncUnaryCall<TResponse> CreateAsyncUnaryCall<TResponse>(TResponse response)
		{
			return new AsyncUnaryCall<TResponse>(
				Task.FromResult(response),
				Task.FromResult(new Metadata()),
				() => Status.DefaultSuccess,
				() => new Metadata(),
				() => { });
		}
	}

	/// <summary>
	/// Fake stream reader used for testing gRPC clients
	/// </summary>
	/// <typeparam name="T">Message type reader will handle</typeparam>
	internal class FakeAsyncStreamReader<T> : IAsyncStreamReader<T> where T : class
	{
		private readonly Channel<T> _channel = System.Threading.Channels.Channel.CreateUnbounded<T>();
		private T? _current;
		private CancellationToken? _cancellationTokenOverride;

		public FakeAsyncStreamReader(CancellationToken? cancellationTokenOverride = null)
		{
			_cancellationTokenOverride = cancellationTokenOverride;
		}

		public Task Write(T message)
		{
			if (!_channel.Writer.TryWrite(message))
			{
				throw new InvalidOperationException("Unable to write message.");
			}
			
			return Task.CompletedTask;
		}

		public void Complete()
		{
			_channel.Writer.Complete();
		}
		
		/// <inheritdoc/>
		public async Task<bool> MoveNext(CancellationToken cancellationToken)
		{
			if (_cancellationTokenOverride != null)
			{
				cancellationToken = _cancellationTokenOverride.Value;
			}
				
			if (await _channel.Reader.WaitToReadAsync(cancellationToken))
			{
				if (_channel.Reader.TryRead(out T? message))
				{
					_current = message;
					return true;
				}
			}

			_current = null!;
			return false;
		}
		
		/// <inheritdoc/>
		public T Current
		{
			get
			{
				if (_current == null)
				{
					throw new InvalidOperationException("No current element is available.");
				}
				return _current;
			}
		}
	}
	
	/// <summary>
	/// Fake stream writer used for testing gRPC clients
	/// </summary>
	/// <typeparam name="T">Message type writer will handle</typeparam>
	internal class FakeClientStreamWriter<T> : IClientStreamWriter<T> where T : class
	{
		private readonly Func<T, Task>? _onWrite;
		private readonly Func<Task>? _onComplete;
		private bool _isCompleted;

		public FakeClientStreamWriter(Func<T, Task>? onWrite = null, Func<Task>? onComplete = null)
		{
			_onWrite = onWrite;
			_onComplete = onComplete;
		}

		/// <inheritdoc/>
		public async Task WriteAsync(T message)
		{
			if (_isCompleted) throw new InvalidOperationException("Stream is marked as complete");
			if (_onWrite != null) await _onWrite(message);
		}

		/// <inheritdoc/>
		public WriteOptions? WriteOptions { get; set; }
		
		/// <inheritdoc/>
		public async Task CompleteAsync()
		{
			_isCompleted = true;
			if (_onComplete != null) await _onComplete();
		}
	}
}
