// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Streams;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Grpc.Net.Client;
using Horde.Agent.Execution;
using Horde.Agent.Leases;
using Horde.Agent.Leases.Handlers;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using Horde.Common.Rpc;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;

namespace Horde.Agent.Tests
{
	[TestClass]
	public class WorkerServiceTest
	{
		private readonly ServiceCollection _serviceCollection;

		private readonly ILoggerFactory _loggerFactory;
		private readonly JobId _jobId = JobId.Parse("65bd0655591b5d5d7d047b58");
		private readonly JobStepBatchId _batchId = new JobStepBatchId(0x1234);
		private readonly JobStepId _stepId1 = new JobStepId(1);
		private readonly JobStepId _stepId2 = new JobStepId(2);
		private readonly JobStepId _stepId3 = new JobStepId(3);
		private readonly LogId _logId = LogId.Parse("65bd0655591b5d5d7d047b00");

		class FakeServerLogger : IServerLogger
		{
			public JobStepOutcome Outcome => JobStepOutcome.Success;

			public IDisposable? BeginScope<TState>(TState state) where TState : notnull => NullLogger.Instance.BeginScope<TState>(state);

			public ValueTask DisposeAsync() => new ValueTask();

			public bool IsEnabled(LogLevel logLevel) => true;

			public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter) { }

			public Task StopAsync() => Task.CompletedTask;
		}

		class FakeServerLoggerFactory : IServerLoggerFactory
		{
			public IServerLogger CreateLogger(ISession session, LogId logId, ILogger localLogger, JobId? jobId, JobStepBatchId? batchId, JobStepId? stepId, bool? warnings = null, LogLevel outputLevel = LogLevel.Information) => new FakeServerLogger();
		}

		internal static IJobExecutor NullExecutor = new SimpleTestExecutor(async (step, logger, cancellationToken) =>
		{
			await Task.Delay(1, cancellationToken);
			return JobStepOutcome.Success;
		});

		public WorkerServiceTest()
		{
			_loggerFactory = LoggerFactory.Create(builder =>
			{
				builder.SetMinimumLevel(LogLevel.Debug);
				builder.AddSimpleConsole(options => { options.SingleLine = true; });
			});

			_serviceCollection = new ServiceCollection();
			_serviceCollection.AddLogging();
			_serviceCollection.AddHordeHttpClient();
			_serviceCollection.AddSingleton<IServerLoggerFactory, FakeServerLoggerFactory>();
			_serviceCollection.AddSingleton<BundleCache>();
			_serviceCollection.AddSingleton<StorageBackendCache>();
			_serviceCollection.AddSingleton<HttpStorageBackendFactory>();
			_serviceCollection.AddSingleton<HttpStorageClientFactory>();
			_serviceCollection.AddSingleton<LeaseLoggerFactory>();

			_serviceCollection.Configure<AgentSettings>(settings =>
			{
				ServerProfile profile = new();
				profile.Name = "test";
				profile.Environment = "test-env";
				profile.Token = "bogus-token";
				profile.Url = new Uri("http://localhost");

				settings.ServerProfiles.Add(profile.Name, profile);
				settings.Server = "test";
				settings.WorkingDir = new DirectoryReference(Path.GetTempPath());
				settings.Executor = TestExecutor.Name; // Not really used since the executor is overridden in the tests
			});

			_serviceCollection.AddSingleton<WorkerService>();
			_serviceCollection.AddSingleton<StatusService>();

			_serviceCollection.AddSingleton<JobHandler>();
			_serviceCollection.AddSingleton<LeaseHandler>(sp => sp.GetRequiredService<JobHandler>());
		}

		[TestMethod]
		public async Task AbortExecuteStepTestAsync()
		{
			{
				using CancellationTokenSource cancelSource = new CancellationTokenSource();
				using CancellationTokenSource stepCancelSource = new CancellationTokenSource();

				using IJobExecutor executor = new SimpleTestExecutor(async (stepResponse, logger, cancelToken) =>
				{
					cancelSource.CancelAfter(10);
					await Task.Delay(5000, cancelToken);
					return JobStepOutcome.Success;
				});

				await Assert.ThrowsExceptionAsync<TaskCanceledException>(() => JobHandler.ExecuteStepAsync(executor,
					null!, NullLogger.Instance, cancelSource.Token, stepCancelSource.Token));
			}

			{
				using CancellationTokenSource cancelSource = new CancellationTokenSource();
				using CancellationTokenSource stepCancelSource = new CancellationTokenSource();

				using IJobExecutor executor = new SimpleTestExecutor(async (stepResponse, logger, cancelToken) =>
				{
					stepCancelSource.CancelAfter(10);
					await Task.Delay(5000, cancelToken);
					return JobStepOutcome.Success;
				});
				(JobStepOutcome stepOutcome, JobStepState stepState) = await JobHandler.ExecuteStepAsync(executor, null!, NullLogger.Instance,
					cancelSource.Token, stepCancelSource.Token);
				Assert.AreEqual(JobStepOutcome.Failure, stepOutcome);
				Assert.AreEqual(JobStepState.Aborted, stepState);
			}
		}

		[TestMethod]
		public async Task AbortExecuteJobTestAsync()
		{
			using CancellationTokenSource source = new CancellationTokenSource();
			CancellationToken token = source.Token;

			ExecuteJobTask executeJobTask = new ExecuteJobTask();
			executeJobTask.JobId = _jobId.ToString();
			executeJobTask.BatchId = _batchId.ToString();
			executeJobTask.LogId = _logId.ToString();
			executeJobTask.JobName = "jobName1";
			executeJobTask.JobOptions = new JobOptions { Executor = SimpleTestExecutor.Name };
			executeJobTask.AutoSdkWorkspace = new AgentWorkspace();
			executeJobTask.Workspace = new AgentWorkspace();

			JobRpcClientStub client = new JobRpcClientStub(NullLogger.Instance);
			await using RpcConnectionStub rpcConnection = new RpcConnectionStub(null!, null!, client);

			await using FakeHordeRpcServer fakeServer = new();
			await using ISession session = FakeServerSessionFactory.CreateSession(rpcConnection, fakeServer.GetGrpcChannel());

			client.BeginStepResponses.Enqueue(new BeginStepResponse { Name = "stepName1", StepId = _stepId1.ToString() });
			client.BeginStepResponses.Enqueue(new BeginStepResponse { Name = "stepName2", StepId = _stepId2.ToString() });
			client.BeginStepResponses.Enqueue(new BeginStepResponse { Name = "stepName3", StepId = _stepId3.ToString() });

			GetStepRequest step2Req = new GetStepRequest(_jobId, _batchId, _stepId2);
			GetStepResponse step2Res = new GetStepResponse(JobStepOutcome.Unspecified, JobStepState.Unspecified, true);
			client.GetStepResponses[step2Req] = step2Res;

			using SimpleTestExecutor executor = new SimpleTestExecutor(async (step, logger, cancelToken) =>
			{
				await Task.Delay(50, cancelToken);
				return JobStepOutcome.Success;
			});

			_serviceCollection.AddSingleton<IJobExecutorFactory>(x => new SimpleTestExecutorFactory(executor));
			await using ServiceProvider serviceProvider = _serviceCollection.BuildServiceProvider();

			JobHandler jobHandler = serviceProvider.GetRequiredService<JobHandler>();
			jobHandler._stepAbortPollInterval = TimeSpan.FromMilliseconds(1);

			LeaseResult result = await jobHandler.ExecuteAsync(session, new LeaseId(default), executeJobTask, NullLogger.Instance, token);
			LeaseOutcome outcome = result.Outcome;

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
		public async Task PollForStepAbortFailureTestAsync()
		{
			using IJobExecutor executor = new SimpleTestExecutor(async (step, logger, cancelToken) =>
			{
				await Task.Delay(50, cancelToken);
				return JobStepOutcome.Success;
			});

			_serviceCollection.AddSingleton<IJobExecutorFactory>(x => new SimpleTestExecutorFactory(executor));
			await using ServiceProvider serviceProvider = _serviceCollection.BuildServiceProvider();

			JobHandler jobHandler = serviceProvider.GetRequiredService<JobHandler>();
			jobHandler._stepAbortPollInterval = TimeSpan.FromMilliseconds(5);

			JobRpcClientStub client = new JobRpcClientStub(NullLogger.Instance);
			await using RpcConnectionStub rpcConnection = new RpcConnectionStub(null!, null!, client);

			int c = 0;
			client._getStepFunc = (request) =>
			{
				return ++c switch
				{
					1 => new GetStepResponse { AbortRequested = false },
					2 => throw new RpcException(new Status(StatusCode.Cancelled, "Fake cancel from test")),
					3 => new GetStepResponse { AbortRequested = true },
					_ => throw new Exception("Should never reach here")
				};
			};

			using CancellationTokenSource stepPollCancelSource = new CancellationTokenSource();
			using CancellationTokenSource stepCancelSource = new CancellationTokenSource();
			TaskCompletionSource<bool> stepFinishedSource = new TaskCompletionSource<bool>();

			await jobHandler.PollForStepAbortAsync(rpcConnection, _jobId, _batchId, _stepId2, stepCancelSource, stepFinishedSource.Task, NullLogger.Instance, stepPollCancelSource.Token);
			Assert.IsTrue(stepCancelSource.IsCancellationRequested);
		}

		[TestMethod]
		[Ignore("Does not work with new pure gRPC channel-based UpdateSession")]
		public async Task ShutdownAsync()
		{
			using IJobExecutor executor = new SimpleTestExecutor(async (step, logger, cancellationToken) =>
			{
				await Task.Delay(50, cancellationToken);
				return JobStepOutcome.Success;
			});

			_serviceCollection.AddSingleton<IJobExecutorFactory>(x => new SimpleTestExecutorFactory(executor));
			await using ServiceProvider serviceProvider = _serviceCollection.BuildServiceProvider();

			using CancellationTokenSource cts = new();
			cts.CancelAfter(20000);

			await using FakeHordeRpcServer fakeServer = new();
			await using ISession session = FakeServerSessionFactory.CreateSession(fakeServer.GetConnection(), fakeServer.GetGrpcChannel());

			LeaseManager manager = new LeaseManager(session, null!, serviceProvider.GetRequiredService<StatusService>(),
				serviceProvider.GetRequiredService<IEnumerable<LeaseHandler>>(), serviceProvider.GetRequiredService<LeaseLoggerFactory>(),
				_loggerFactory.CreateLogger<LeaseManager>());

			Task handleSessionTask = Task.Run(() => manager.RunAsync(false, cts.Token), cts.Token);
			await fakeServer.UpdateSessionReceived.Task.WaitAsync(cts.Token);
			cts.Cancel();
			await handleSessionTask; // Ensure it runs to completion and no exceptions are raised
		}
	}

	internal class FakeServerSessionFactory : ISessionFactory
	{
		readonly FakeHordeRpcServer _fakeServer;

		public FakeServerSessionFactory(FakeHordeRpcServer fakeServer) => _fakeServer = fakeServer;

		public Task<ISession> CreateAsync(CancellationToken cancellationToken)
		{
			return Task.FromResult(CreateSession(_fakeServer.GetConnection(), _fakeServer.GetGrpcChannel()));
		}

		public static ISession CreateSession(IRpcConnection rpcConnection, GrpcChannel grpcChannel)
		{
			Mock<ISession> fakeSession = new Mock<ISession>(MockBehavior.Strict);
			fakeSession.Setup(x => x.ServerUrl).Returns(new Uri("https://localhost:9999"));
			fakeSession.Setup(x => x.AgentId).Returns(new EpicGames.Horde.Agents.AgentId("LocalAgent"));
			fakeSession.Setup(x => x.SessionId).Returns(new EpicGames.Horde.Agents.Sessions.SessionId(default));
			fakeSession.Setup(x => x.RpcConnection).Returns(rpcConnection);
			fakeSession.Setup(x => x.GrpcChannel).Returns(grpcChannel);
			fakeSession.Setup(x => x.TerminateProcessesAsync(It.IsAny<TerminateCondition>(), It.IsAny<ILogger>(), It.IsAny<CancellationToken>())).Returns(Task.CompletedTask);
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
		private readonly bool _isStopping = false;
		private readonly Dictionary<string, Lease> _leases = new();

		private readonly Dictionary<StreamId, GetStreamResponse> _streamIdToStreamResponse = new();
		private readonly Dictionary<JobId, GetJobResponse> _jobIdToJobResponse = new();
		private readonly Mock<IRpcClientRef<HordeRpc.HordeRpcClient>> _mockClientRef;
		private readonly Mock<IRpcConnection> _mockConnection;
		private readonly ILogger<FakeHordeRpcServer> _logger;
		public readonly TaskCompletionSource<bool> CreateSessionReceived = new();
		public readonly TaskCompletionSource<bool> UpdateSessionReceived = new();

		private readonly RpcConnectionStub _connection;
		private readonly GrpcChannel _grpcChannel;
		private readonly FakeJobRpcClient _client;

		private class FakeHordeRpcClient : HordeRpc.HordeRpcClient
		{
			private readonly FakeHordeRpcServer _outer;

			public FakeHordeRpcClient(FakeHordeRpcServer outer)
			{
				_outer = outer;
			}

			public override AsyncDuplexStreamingCall<UpdateSessionRequest, UpdateSessionResponse> UpdateSession(Metadata headers = null!, DateTime? deadline = null, CancellationToken cancellationToken = default)
			{
				return _outer.GetUpdateSessionCall(CancellationToken.None);
			}
		}

		private class FakeJobRpcClient : JobRpc.JobRpcClient
		{
			private readonly FakeHordeRpcServer _outer;

			public FakeJobRpcClient(FakeHordeRpcServer outer)
			{
				_outer = outer;
			}

			public override AsyncUnaryCall<GetStreamResponse> GetStreamAsync(GetStreamRequest request, CallOptions options)
			{
				if (_outer._streamIdToStreamResponse.TryGetValue(new StreamId(request.StreamId), out GetStreamResponse? streamResponse))
				{
					return JobRpcClientStub.Wrap(streamResponse);
				}

				throw new RpcException(new Status(StatusCode.NotFound, $"Stream ID {request.StreamId} not found"));
			}

			public override AsyncUnaryCall<GetJobResponse> GetJobAsync(GetJobRequest request, CallOptions options)
			{
				if (_outer._jobIdToJobResponse.TryGetValue(JobId.Parse(request.JobId), out GetJobResponse? jobResponse))
				{
					return JobRpcClientStub.Wrap(jobResponse);
				}

				throw new RpcException(new Status(StatusCode.NotFound, $"Job ID {request.JobId} not found"));
			}
		}

		public FakeHordeRpcServer()
		{
			_serverName = "FakeServer";
			_logger = NullLogger<FakeHordeRpcServer>.Instance;
			FakeHordeRpcClient hordeClient = new FakeHordeRpcClient(this);
			_client = new FakeJobRpcClient(this);
			_connection = new RpcConnectionStub(null!, hordeClient, _client);
			_grpcChannel = GrpcChannel.ForAddress(new Uri("http://horde-agent-test"), new GrpcChannelOptions());

			_mockClientRef = new Mock<IRpcClientRef<HordeRpc.HordeRpcClient>>();
			_mockClientRef
				.Setup(m => m.Client)
				.Returns(() => hordeClient);

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
				State = RpcLeaseState.Pending,
				Payload = Any.Pack(testTask)
			};
		}

		public Lease GetLease(string leaseId)
		{
			return _leases[leaseId];
		}

		public void AddStream(StreamId streamId, string streamName)
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

		public void AddAgentType(StreamId streamId, string agentType)
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

		public void AddJob(JobId jobId, StreamId streamId, int change, int preflightChange)
		{
			if (!_streamIdToStreamResponse.ContainsKey(streamId))
			{
				throw new Exception($"Stream ID {streamId} not found");
			}

			_jobIdToJobResponse[jobId] = new GetJobResponse
			{
				StreamId = streamId.ToString(),
				Change = change,
				PreflightChange = preflightChange
			};
		}

		public IRpcConnection GetConnection()
		{
			return _connection;
		}

		public GrpcChannel GetGrpcChannel()
		{
			return _grpcChannel;
		}

		public CreateSessionResponse OnCreateSessionRequest(CreateSessionRequest request)
		{
			CreateSessionReceived.TrySetResult(true);
			_logger.LogInformation("OnCreateSessionRequest: {AgentId} {Status}", request.Id, request.Status);
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

			return new(
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
				UpdateSessionResponse response = new() { ExpiryTime = Timestamp.FromDateTime(DateTime.UtcNow + TimeSpan.FromMinutes(120)) };
				response.Leases.AddRange(_leases.Values.Where(x => x.State != RpcLeaseState.Completed));
				await responseStream.Write(response);
			}

			FakeClientStreamWriter<UpdateSessionRequest> requestStream = new(OnRequest, () =>
			{
				responseStream.Complete();
				return Task.CompletedTask;
			});

			return new(
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
			_grpcChannel.Dispose();

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
		private readonly CancellationToken? _cancellationTokenOverride;

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
