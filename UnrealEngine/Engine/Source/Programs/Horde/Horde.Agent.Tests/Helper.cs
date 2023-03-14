// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Google.Protobuf.WellKnownTypes;
using Grpc.Core;
using Grpc.Net.Client;
using Horde.Agent.Execution.Interfaces;
using Horde.Agent.Utility;
using HordeCommon;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;
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

	class RpcClientRefStub : IRpcClientRef
	{
		public GrpcChannel Channel { get; }
		public HordeRpc.HordeRpcClient Client { get; }
		public Task DisposingTask { get; }

		public RpcClientRefStub(GrpcChannel channel, HordeRpc.HordeRpcClient client)
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

		public RpcConnectionStub(GrpcChannel grpcChannel, HordeRpc.HordeRpcClient hordeRpcClient)
		{
			_grpcChannel = grpcChannel;
			_hordeRpcClient = hordeRpcClient;
		}

		public IRpcClientRef? TryGetClientRef(RpcContext context)
		{
			return new RpcClientRefStub(_grpcChannel, _hordeRpcClient);
		}

		public Task<IRpcClientRef> GetClientRef(RpcContext context, CancellationToken cancellationToken)
		{
			IRpcClientRef rpcClientRefStub = new RpcClientRefStub(_grpcChannel, _hordeRpcClient);
			return Task.FromResult(rpcClientRefStub);
		}

		public Task<T> InvokeOnceAsync<T>(Func<HordeRpc.HordeRpcClient, Task<T>> func, RpcContext context,
			CancellationToken cancellationToken)
		{
			return func(_hordeRpcClient);
		}

		public Task<T> InvokeOnceAsync<T>(Func<HordeRpc.HordeRpcClient, AsyncUnaryCall<T>> func, RpcContext context,
			CancellationToken cancellationToken)
		{
			return func(_hordeRpcClient).ResponseAsync;
		}

		public Task<T> InvokeAsync<T>(Func<HordeRpc.HordeRpcClient, Task<T>> func, RpcContext context,
			CancellationToken cancellationToken)
		{
			return func(_hordeRpcClient);
		}

		public Task<T> InvokeAsync<T>(Func<HordeRpc.HordeRpcClient, AsyncUnaryCall<T>> func, RpcContext context,
			CancellationToken cancellationToken)
		{
			return func(_hordeRpcClient).ResponseAsync;
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

		private static AsyncUnaryCall<T> Wrap<T>(T res)
		{
			return new AsyncUnaryCall<T>(Task.FromResult(res), Task.FromResult(Metadata.Empty),
				() => Status.DefaultSuccess, () => Metadata.Empty, null!);
		}
	}

	class SimpleTestExecutor : IExecutor
	{
		private readonly Func<BeginStepResponse, ILogger, CancellationToken, Task<JobStepOutcome>> _func;

		public SimpleTestExecutor(Func<BeginStepResponse, ILogger, CancellationToken, Task<JobStepOutcome>> func)
		{
			_func = func;
		}

		public Task InitializeAsync(ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogDebug("SimpleTestExecutor.InitializeAsync()");
			return Task.CompletedTask;
		}

		public Task<JobStepOutcome> RunAsync(BeginStepResponse step, ILogger logger,
			CancellationToken cancellationToken)
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
}