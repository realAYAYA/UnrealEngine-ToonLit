// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using Amazon.EC2.Model;
using EpicGames.Core;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Grpc.Net.Client;
using Horde.Agent.Execution;
using Horde.Agent.Leases;
using Horde.Agent.Leases.Handlers;
using Horde.Agent.Parser;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;

namespace Horde.Agent.Tests
{
	[TestClass]
	public class WorkerServiceTest
	{
		private readonly ServiceCollection _serviceCollection;

		class FakeServerLogger : IServerLogger
		{
			public JobStepOutcome Outcome => JobStepOutcome.Success;

			public IDisposable BeginScope<TState>(TState state) => NullLogger.Instance.BeginScope<TState>(state);

			public ValueTask DisposeAsync() => new ValueTask();

			public bool IsEnabled(LogLevel logLevel) => true;

			public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter) { }

			public Task StopAsync() => Task.CompletedTask;
		}

		class FakeServerLoggerFactory : IServerLoggerFactory
		{
			public IServerLogger CreateLogger(ISession session, string logId, string? jobId, string? batchId, string? stepId, bool? warnings = null) => new FakeServerLogger();
		}

		internal static JobExecutor NullExecutor = new SimpleTestExecutor(async (step, logger, cancellationToken) =>
		{
			await Task.Delay(1, cancellationToken);
			return JobStepOutcome.Success;
		});

		public WorkerServiceTest()
		{
			_serviceCollection = new ServiceCollection();
			_serviceCollection.AddLogging();
			_serviceCollection.AddSingleton<IServerLoggerFactory, FakeServerLoggerFactory>();

			_serviceCollection.Configure<AgentSettings>(settings =>
			{
				ServerProfile profile = new();
				profile.Name = "test";
				profile.Environment = "test-env";
				profile.Token = "bogus-token";
				profile.Url = new Uri("http://localhost");

				settings.ServerProfiles.Add(profile);
				settings.Server = "test";
				settings.WorkingDir = Path.GetTempPath();
				settings.Executor = TestExecutor.Name; // Not really used since the executor is overridden in the tests
			});

			_serviceCollection.AddSingleton<WorkerService>();

			_serviceCollection.AddSingleton<JobHandler>();
			_serviceCollection.AddSingleton<LeaseHandler>(sp => sp.GetRequiredService<JobHandler>());
		}

		[TestMethod]
		public async Task AbortExecuteStepTest()
		{
			{
				using CancellationTokenSource cancelSource = new CancellationTokenSource();
				using CancellationTokenSource stepCancelSource = new CancellationTokenSource();

				JobExecutor executor = new SimpleTestExecutor(async (stepResponse, logger, cancelToken) =>
				{
					cancelSource.CancelAfter(10);
					await Task.Delay(5000, cancelToken);
					return JobStepOutcome.Success;
				});

				await Assert.ThrowsExceptionAsync<TaskCanceledException>(() => JobHandler.ExecuteStepAsync(executor,
					new BeginStepResponse(), NullLogger.Instance, cancelSource.Token, stepCancelSource.Token));
			}

			{
				using CancellationTokenSource cancelSource = new CancellationTokenSource();
				using CancellationTokenSource stepCancelSource = new CancellationTokenSource();

				JobExecutor executor = new SimpleTestExecutor(async (stepResponse, logger, cancelToken) =>
				{
					stepCancelSource.CancelAfter(10);
					await Task.Delay(5000, cancelToken);
					return JobStepOutcome.Success;
				});
				(JobStepOutcome stepOutcome, JobStepState stepState) = await JobHandler.ExecuteStepAsync(executor, new BeginStepResponse(), NullLogger.Instance,
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
			executeJobTask.Executor = SimpleTestExecutor.Name;
			executeJobTask.AutoSdkWorkspace = new AgentWorkspace();
			executeJobTask.Workspace = new AgentWorkspace();

			HordeRpcClientStub client = new HordeRpcClientStub(NullLogger.Instance);
			await using RpcConnectionStub rpcConnection = new RpcConnectionStub(null!, client);

			await using ISession session = FakeServerSessionFactory.CreateSession(rpcConnection);

			client.BeginStepResponses.Enqueue(new BeginStepResponse {Name = "stepName1", StepId = "stepId1"});
			client.BeginStepResponses.Enqueue(new BeginStepResponse {Name = "stepName2", StepId = "stepId2"});
			client.BeginStepResponses.Enqueue(new BeginStepResponse {Name = "stepName3", StepId = "stepId3"});

			GetStepRequest step2Req = new GetStepRequest(executeJobTask.JobId, executeJobTask.BatchId, "stepId2");
			GetStepResponse step2Res = new GetStepResponse(JobStepOutcome.Unspecified, JobStepState.Unspecified, true);
			client.GetStepResponses[step2Req] = step2Res;

			JobExecutor executor = new SimpleTestExecutor(async (step, logger, cancelToken) =>
			{
				await Task.Delay(50, cancelToken);
				return JobStepOutcome.Success;
			});

			_serviceCollection.AddSingleton<JobExecutorFactory>(x => new SimpleTestExecutorFactory(executor));
			using ServiceProvider serviceProvider = _serviceCollection.BuildServiceProvider();

			JobHandler jobHandler = serviceProvider.GetRequiredService<JobHandler>();
			jobHandler._stepAbortPollInterval = TimeSpan.FromMilliseconds(1);

			LeaseOutcome outcome = (await jobHandler.ExecuteAsync(session, "leaseId1", executeJobTask,
				token)).Outcome;

			Assert.AreEqual(LeaseOutcome.Success, outcome);
			Assert.AreEqual(3, client.UpdateStepRequests.Count);
			Assert.AreEqual(JobStepOutcome.Success, client.UpdateStepRequests[0].Outcome);
			Assert.AreEqual(JobStepState.Completed, client.UpdateStepRequests[0].State);
			Assert.AreEqual(JobStepOutcome.Failure, client.UpdateStepRequests[1].Outcome);
			Assert.AreEqual(JobStepState.Aborted, client.UpdateStepRequests[1].State);
			Assert.AreEqual(JobStepOutcome.Success, client.UpdateStepRequests[2].Outcome);
			Assert.AreEqual(JobStepState.Completed, client.UpdateStepRequests[2].State);
		}
		
		[TestMethod]
		public async Task PollForStepAbortFailureTest()
		{
			JobExecutor executor = new SimpleTestExecutor(async (step, logger, cancelToken) =>
			{
				await Task.Delay(50, cancelToken);
				return JobStepOutcome.Success;
			});

			_serviceCollection.AddSingleton<JobExecutorFactory>(x => new SimpleTestExecutorFactory(executor));
			using ServiceProvider serviceProvider = _serviceCollection.BuildServiceProvider();

			JobHandler jobHandler = serviceProvider.GetRequiredService<JobHandler>();
			jobHandler._stepAbortPollInterval = TimeSpan.FromMilliseconds(5);

			HordeRpcClientStub client = new HordeRpcClientStub(NullLogger.Instance);
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

			await jobHandler.PollForStepAbort(rpcConnection, "jobId1", "batchId1", "logId1", stepCancelSource, stepFinishedSource.Task, stepPollCancelSource.Token);
			Assert.IsTrue(stepCancelSource.IsCancellationRequested);
		}

		[TestMethod]
		public async Task Shutdown()
		{
			JobExecutor executor = new SimpleTestExecutor(async (step, logger, cancellationToken) =>
			{
				await Task.Delay(50, cancellationToken);
				return JobStepOutcome.Success;
			});

			_serviceCollection.AddSingleton<JobExecutorFactory>(x => new SimpleTestExecutorFactory(executor));
			using ServiceProvider serviceProvider = _serviceCollection.BuildServiceProvider();

			using CancellationTokenSource cts = new ();
			cts.CancelAfter(20000);

			await using FakeHordeRpcServer fakeServer = new();
			await using ISession session = FakeServerSessionFactory.CreateSession(fakeServer.GetConnection());

			LeaseManager manager = new LeaseManager(session, null!, serviceProvider.GetRequiredService<IEnumerable<LeaseHandler>>(), serviceProvider.GetRequiredService<IOptions<AgentSettings>>(), NullLogger.Instance);

			Task handleSessionTask = Task.Run(() => manager.RunAsync(false, cts.Token), cts.Token);
			await fakeServer.UpdateSessionReceived.Task.WaitAsync(cts.Token);
			cts.Cancel();
			await handleSessionTask; // Ensure it runs to completion and no exceptions are raised
		}
	}

	internal class FakeServerSessionFactory : ISessionFactoryService
	{
		readonly FakeHordeRpcServer _fakeServer;

		public FakeServerSessionFactory(FakeHordeRpcServer fakeServer) => _fakeServer = fakeServer;

		public Task<ISession> CreateAsync(CancellationToken cancellationToken)
		{
			return Task.FromResult(CreateSession(_fakeServer.GetConnection()));
		}

		public static ISession CreateSession(IRpcConnection rpcConnection)
		{
			Mock<ISession> fakeSession = new Mock<ISession>(MockBehavior.Strict);
			fakeSession.Setup(x => x.AgentId).Returns("LocalAgent");
			fakeSession.Setup(x => x.SessionId).Returns("Session");
			fakeSession.Setup(x => x.RpcConnection).Returns(rpcConnection);
			fakeSession.Setup(x => x.TerminateProcessesAsync(It.IsAny<ILogger>(), It.IsAny<CancellationToken>())).Returns(Task.CompletedTask);
			fakeSession.Setup(x => x.DisposeAsync()).Returns(new ValueTask());
			fakeSession.Setup(x => x.WorkingDir).Returns(DirectoryReference.GetCurrentDirectory());
			return fakeSession.Object;
		}
	}

	/// <summary>
	/// Fake implementation of a HordeRpc gRPC server.
	/// Provides a corresponding gRPC client class that can be used with the WorkerService
	/// to test client-server interactions.
	/// </summary>
	internal class FakeHordeRpcServer : IAsyncDisposable
	{
		private readonly string _serverName;
		private bool _isStopping = false;
		private Dictionary<string, Lease> _leases = new();

		private readonly Dictionary<string, GetStreamResponse> _streamIdToStreamResponse = new();
		private readonly Dictionary<string, GetJobResponse> _jobIdToJobResponse = new();
		private readonly Mock<HordeRpc.HordeRpcClient> _mockClient;
		private readonly Mock<IRpcClientRef<HordeRpc.HordeRpcClient>> _mockClientRef;
		private readonly Mock<IRpcConnection> _mockConnection;
		private readonly ILogger<FakeHordeRpcServer> _logger;
		public readonly TaskCompletionSource<bool> CreateSessionReceived = new();
		public readonly TaskCompletionSource<bool> UpdateSessionReceived = new();

		private readonly RpcConnectionStub _connection;
		private readonly FakeHordeRpcClient _client;

		private class FakeHordeRpcClient : HordeRpc.HordeRpcClient
		{
			private readonly FakeHordeRpcServer _outer;

			public FakeHordeRpcClient(FakeHordeRpcServer outer)
			{
				_outer = outer;
			}

			public override AsyncUnaryCall<GetStreamResponse> GetStreamAsync(GetStreamRequest request, CallOptions options)
			{
				if (_outer._streamIdToStreamResponse.TryGetValue(request.StreamId, out GetStreamResponse? streamResponse))
				{
					return HordeRpcClientStub.Wrap(streamResponse);
				}

				throw new RpcException(new Status(StatusCode.NotFound, $"Stream ID {request.StreamId} not found"));
			}

			public override AsyncUnaryCall<GetJobResponse> GetJobAsync(GetJobRequest request, CallOptions options)
			{
				if (_outer._jobIdToJobResponse.TryGetValue(request.JobId, out GetJobResponse? jobResponse))
				{
					return HordeRpcClientStub.Wrap(jobResponse);
				}

				throw new RpcException(new Status(StatusCode.NotFound, $"Job ID {request.JobId} not found"));
			}

			public override AsyncDuplexStreamingCall<UpdateSessionRequest, UpdateSessionResponse> UpdateSession(Metadata headers = null!, DateTime? deadline = null, CancellationToken cancellationToken = default)
			{
				return _outer.GetUpdateSessionCall(CancellationToken.None);
			}
		}
		
		public FakeHordeRpcServer()
		{
			_serverName = "FakeServer";
			_mockClient = new (MockBehavior.Strict);
			_logger = NullLogger<FakeHordeRpcServer>.Instance;
			_client = new FakeHordeRpcClient(this);
			_connection = new RpcConnectionStub(null!, _client);

			_mockClientRef = new Mock<IRpcClientRef<HordeRpc.HordeRpcClient>>();
			_mockClientRef
				.Setup(m => m.Client)
				.Returns(() => _client);

			_mockConnection = new(MockBehavior.Strict);
			_mockConnection
				.Setup(m => m.TryGetClientRef<HordeRpc.HordeRpcClient>())
				.Returns(() => _mockClientRef.Object);
			_mockConnection
				.Setup(m => m.GetClientRefAsync<HordeRpc.HordeRpcClient>(It.IsAny<CancellationToken>()))
				.Returns(() => Task.FromResult(_mockClientRef.Object));
			_mockConnection
				.Setup(m => m.DisposeAsync())
				.Returns(() => new ValueTask());
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

		public void AddStream(string streamId, string streamName)
		{
			if (_streamIdToStreamResponse.ContainsKey(streamId))
			{
				throw new Exception($"Stream ID {streamId} already added");
			}
			
			_streamIdToStreamResponse[streamId] = new GetStreamResponse
			{
				Name = streamName,
			};
		}

		public void AddAgentType(string streamId, string agentType)
		{
			if (!_streamIdToStreamResponse.TryGetValue(streamId, out GetStreamResponse? streamResponse))
			{
				throw new Exception($"Stream ID {streamId} not found");
			}
			
			string tempDir = Path.Join(Path.GetTempPath(), $"horde-agent-type-{agentType}-" + Guid.NewGuid().ToString()[..8]);
			Directory.CreateDirectory(tempDir);

			streamResponse.AgentTypes[agentType] = new GetAgentTypeResponse
			{
				TempStorageDir = tempDir
			};
		}
		
		public void AddJob(string jobId, string streamId, int change, int preflightChange)
		{
			if (!_streamIdToStreamResponse.ContainsKey(streamId))
			{
				throw new Exception($"Stream ID {streamId} not found");
			}

			_jobIdToJobResponse[jobId] = new GetJobResponse
			{
				StreamId = streamId, Change = change, PreflightChange = preflightChange
			};
		}

		public IRpcConnection GetConnection()
		{
			return _connection;
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

		public async ValueTask DisposeAsync()
		{
			await _connection.DisposeAsync();
			
			foreach (GetStreamResponse stream in _streamIdToStreamResponse.Values)
			{
				foreach (GetAgentTypeResponse agentType in stream.AgentTypes.Values)
				{
					if (Directory.Exists(agentType.TempStorageDir))
					{
						Directory.Delete(agentType.TempStorageDir, true);
					}
				}
			}
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
			if (_isCompleted)
			{
				throw new InvalidOperationException("Stream is marked as complete");
			}
			if (_onWrite != null)
			{
				await _onWrite(message);
			}
		}

		/// <inheritdoc/>
		public WriteOptions? WriteOptions { get; set; }
		
		/// <inheritdoc/>
		public async Task CompleteAsync()
		{
			_isCompleted = true;
			if (_onComplete != null)
			{
				await _onComplete();
			}
		}
	}
}
