// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Grpc.Net.Client;
using Horde.Agent.Execution;
using Horde.Agent.Utility;
using Horde.Common.Rpc;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Tests
{
	// Stub for fulfilling IOptionsMonitor interface during testing
	// Copied from HordeServerTests until a good way to share code between these is decided.
	public class TestOptionsMonitor<T> : IOptionsMonitor<T>
		where T : class, new()
	{
		public TestOptionsMonitor(T value)
		{
			CurrentValue = value;
		}

		public T CurrentValue { get; }

		public T Get(string? name)
			=> CurrentValue;

		public IDisposable? OnChange(Action<T, string?> listener)
			=> null;
	}

	class RpcClientRefStub<TClient> : IRpcClientRef<TClient> where TClient : ClientBase<TClient>
	{
		public GrpcChannel Channel { get; }
		public TClient Client { get; }
		public Task DisposingTask { get; }

		public RpcClientRefStub(GrpcChannel channel, TClient client)
		{
			Channel = channel;
			Client = client;
			DisposingTask = new TaskCompletionSource<bool>().Task;
		}

		public void Dispose()
		{
		}
	}

	class RpcConnectionStub : IRpcConnection
	{
		private readonly GrpcChannel _grpcChannel;
		private readonly HordeRpc.HordeRpcClient _hordeRpcClient;
		private readonly JobRpc.JobRpcClient _jobRpcClient;

		public bool Healthy => true;
		public ILogger Logger => NullLogger.Instance;

		public RpcConnectionStub(GrpcChannel grpcChannel, HordeRpc.HordeRpcClient hordeRpcClient, JobRpc.JobRpcClient jobRpcClient)
		{
			_grpcChannel = grpcChannel;
			_hordeRpcClient = hordeRpcClient;
			_jobRpcClient = jobRpcClient;
		}

		public IRpcClientRef<TClient>? TryGetClientRef<TClient>() where TClient : ClientBase<TClient>
		{
			return (IRpcClientRef<TClient>)(object)new RpcClientRefStub<HordeRpc.HordeRpcClient>(_grpcChannel, _hordeRpcClient);
		}

		public Task<IRpcClientRef<TClient>> GetClientRefAsync<TClient>(CancellationToken cancellationToken) where TClient : ClientBase<TClient>
		{
			IRpcClientRef<TClient> rpcClientRefStub;
			if (typeof(TClient) == typeof(HordeRpc.HordeRpcClient))
			{
				rpcClientRefStub = (IRpcClientRef<TClient>)(object)new RpcClientRefStub<HordeRpc.HordeRpcClient>(_grpcChannel, _hordeRpcClient);
			}
			else if (typeof(TClient) == typeof(JobRpc.JobRpcClient))
			{
				rpcClientRefStub = (IRpcClientRef<TClient>)(object)new RpcClientRefStub<JobRpc.JobRpcClient>(_grpcChannel, _jobRpcClient);
			}
			else
			{
				throw new NotImplementedException();
			}
			return Task.FromResult(rpcClientRefStub);
		}

		public ValueTask DisposeAsync()
		{
			return new ValueTask();
		}
	}

	class JobRpcClientStub : JobRpc.JobRpcClient
	{
		public readonly Queue<BeginStepResponse> BeginStepResponses = new Queue<BeginStepResponse>();
		public readonly List<UpdateStepRequest> UpdateStepRequests = new List<UpdateStepRequest>();
		public readonly Dictionary<GetStepRequest, GetStepResponse> GetStepResponses = new Dictionary<GetStepRequest, GetStepResponse>();
		public Func<GetStepRequest, GetStepResponse>? _getStepFunc = null;
		private readonly ILogger _logger;

		public JobRpcClientStub(ILogger logger)
		{
			_logger = logger;
		}

		public override AsyncUnaryCall<BeginBatchResponse> BeginBatchAsync(BeginBatchRequest request,
			CallOptions options)
		{
			_logger.LogDebug("HordeRpcClientStub.BeginBatchAsync()");
			BeginBatchResponse res = new BeginBatchResponse();

			res.AgentType = "agentType1";
			res.LogId = "logId1";
			res.Change = 1;

			return Wrap(res);
		}

		public override AsyncUnaryCall<Empty> FinishBatchAsync(FinishBatchRequest request, CallOptions options)
		{
			Empty res = new Empty();
			return Wrap(res);
		}

		public override AsyncUnaryCall<GetStreamResponse> GetStreamAsync(GetStreamRequest request, CallOptions options)
		{
			GetStreamResponse res = new GetStreamResponse();
			return Wrap(res);
		}

		public override AsyncUnaryCall<GetJobResponse> GetJobAsync(GetJobRequest request, CallOptions options)
		{
			GetJobResponse res = new GetJobResponse();
			return Wrap(res);
		}

		public override AsyncUnaryCall<BeginStepResponse> BeginStepAsync(BeginStepRequest request, CallOptions options)
		{
			if (BeginStepResponses.Count == 0)
			{
				BeginStepResponse completeRes = new BeginStepResponse();
				completeRes.State = BeginStepResponse.Types.Result.Complete;
				return Wrap(completeRes);
			}

			BeginStepResponse res = BeginStepResponses.Dequeue();
			res.State = BeginStepResponse.Types.Result.Ready;
			return Wrap(res);
		}

		public override AsyncUnaryCall<Empty> UpdateStepAsync(UpdateStepRequest request, CallOptions options)
		{
			_logger.LogDebug("UpdateStepAsync(Request: {Request})", request);
			UpdateStepRequests.Add(request);
			Empty res = new Empty();
			return Wrap(res);
		}

		public override AsyncUnaryCall<Empty> CreateEventsAsync(CreateEventsRequest request, CallOptions options)
		{
			_logger.LogDebug("CreateEventsAsync: {Request}", request);
			Empty res = new Empty();
			return Wrap(res);
		}

		public override AsyncUnaryCall<GetStepResponse> GetStepAsync(GetStepRequest request, CallOptions options)
		{
			if (_getStepFunc != null)
			{
				return Wrap(_getStepFunc(request));
			}

			if (GetStepResponses.TryGetValue(request, out GetStepResponse? res))
			{
				return Wrap(res);
			}

			return Wrap(new GetStepResponse());
		}

		public static AsyncUnaryCall<T> Wrap<T>(T res)
		{
			return new AsyncUnaryCall<T>(Task.FromResult(res), Task.FromResult(Metadata.Empty),
				() => Status.DefaultSuccess, () => Metadata.Empty, null!);
		}
	}

	class SimpleTestExecutor : IJobExecutor
	{
		public const string Name = "Simple";

		private readonly Func<JobStepInfo, ILogger, CancellationToken, Task<JobStepOutcome>> _func;

		public SimpleTestExecutor(Func<JobStepInfo, ILogger, CancellationToken, Task<JobStepOutcome>> func)
		{
			_func = func;
		}

		public void Dispose()
		{
		}

		public Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogDebug("SimpleTestExecutor.InitializeAsync()");
			return Task.CompletedTask;
		}

		public Task<JobStepOutcome> RunAsync(JobStepInfo step, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogDebug("SimpleTestExecutor.RunAsync(Step: {Step})", step);
			return _func(step, logger, cancellationToken);
		}

		public Task FinalizeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogDebug("SimpleTestExecutor.FinalizeAsync()");
			return Task.CompletedTask;
		}
	}

	class SimpleTestExecutorFactory : IJobExecutorFactory
	{
		readonly IJobExecutor _executor;

		public string Name => SimpleTestExecutor.Name;

		public SimpleTestExecutorFactory(IJobExecutor executor)
		{
			_executor = executor;
		}

		public IJobExecutor CreateExecutor(AgentWorkspace? workspaceInfo, AgentWorkspace? autoSdkWorkspaceInfo, JobExecutorOptions options) => _executor;
	}
}