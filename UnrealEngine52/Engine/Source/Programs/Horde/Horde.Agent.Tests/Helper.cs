// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Grpc.Net.Client;
using Horde.Agent.Execution;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Tests
{
	// Stub for fulfilling IOptionsMonitor interface during testing
	// Copied from HordeServerTests until a good way to share code between these is decided.
	public class TestOptionsMonitor<T> : IOptions<T>
		where T : class, new()
	{
		public TestOptionsMonitor(T value)
		{
			Value = value;
		}

		public T Value { get; }
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

		public ILogger Logger => NullLogger.Instance;

		public RpcConnectionStub(GrpcChannel grpcChannel, HordeRpc.HordeRpcClient hordeRpcClient)
		{
			_grpcChannel = grpcChannel;
			_hordeRpcClient = hordeRpcClient;
		}

		public IRpcClientRef<TClient>? TryGetClientRef<TClient>() where TClient : ClientBase<TClient>
		{
			return (IRpcClientRef<TClient>)(object)new RpcClientRefStub<HordeRpc.HordeRpcClient>(_grpcChannel, _hordeRpcClient);
		}

		public Task<IRpcClientRef<TClient>> GetClientRefAsync<TClient>(CancellationToken cancellationToken) where TClient : ClientBase<TClient>
		{
			IRpcClientRef<TClient> rpcClientRefStub = (IRpcClientRef<TClient>)(object)new RpcClientRefStub<HordeRpc.HordeRpcClient>(_grpcChannel, _hordeRpcClient);
			return Task.FromResult(rpcClientRefStub);
		}

		public ValueTask DisposeAsync()
		{
			return new ValueTask();
		}
	}

	class HordeRpcClientStub : HordeRpc.HordeRpcClient
	{
		public readonly Queue<BeginStepResponse> BeginStepResponses = new Queue<BeginStepResponse>();
		public readonly List<UpdateStepRequest> UpdateStepRequests = new List<UpdateStepRequest>();
		public readonly Dictionary<GetStepRequest, GetStepResponse> GetStepResponses = new Dictionary<GetStepRequest, GetStepResponse>();
		public Func<GetStepRequest, GetStepResponse>? _getStepFunc = null;
		private readonly ILogger _logger;

		public HordeRpcClientStub(ILogger logger)
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

		public override AsyncUnaryCall<Empty> WriteOutputAsync(WriteOutputRequest request, CallOptions options)
		{
			_logger.LogDebug("WriteOutputAsync: {Data}", request.Data);
			Empty res = new Empty();
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

	class SimpleTestExecutor : JobExecutor
	{
		public const string Name = "Simple";

		private readonly Func<BeginStepResponse, ILogger, CancellationToken, Task<JobStepOutcome>> _func;

		public SimpleTestExecutor(Func<BeginStepResponse, ILogger, CancellationToken, Task<JobStepOutcome>> func)
			: base(null!, null!, null!, null!, null!, NullLogger.Instance)
		{
			_func = func;
		}

		public override Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogDebug("SimpleTestExecutor.InitializeAsync()");
			return Task.CompletedTask;
		}

		public override Task<JobStepOutcome> RunAsync(BeginStepResponse step, ILogger logger,
			CancellationToken cancellationToken)
		{
			logger.LogDebug("SimpleTestExecutor.RunAsync(Step: {Step})", step);
			return _func(step, logger, cancellationToken);
		}

		public override Task FinalizeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogDebug("SimpleTestExecutor.FinalizeAsync()");
			return Task.CompletedTask;
		}

		protected override Task<bool> SetupAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			throw new NotImplementedException();
		}

		protected override Task<bool> ExecuteAsync(BeginStepResponse step, ILogger logger, CancellationToken cancellationToken)
		{
			throw new NotImplementedException();
		}
	}

	class SimpleTestExecutorFactory : JobExecutorFactory
	{
		readonly JobExecutor _executor;

		public override string Name => SimpleTestExecutor.Name;

		public SimpleTestExecutorFactory(JobExecutor executor)
		{
			_executor = executor;
		}

		public override JobExecutor CreateExecutor(ISession session, ExecuteJobTask executeJobTask, BeginBatchResponse beginBatchResponse) => _executor;
	}
}