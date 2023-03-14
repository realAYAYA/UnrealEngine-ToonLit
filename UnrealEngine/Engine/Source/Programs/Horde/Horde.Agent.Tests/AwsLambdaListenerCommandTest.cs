// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using Google.Protobuf;
using Horde.Agent.Commands.Compute;
using Horde.Agent.Services;
using HordeCommon;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests
{
	[TestClass]
	public class AwsLambdaListenerCommandTest
	{
		private readonly ILogger<WorkerService> _workerLogger;
		private readonly ILogger<GrpcService> _grpcServiceLogger;
		private readonly ILogger<FakeHordeRpcServer> _hordeRpcServerLogger;

		public AwsLambdaListenerCommandTest()
		{
			using ILoggerFactory loggerFactory = LoggerFactory.Create(builder =>
			{
				builder.AddSimpleConsole(consoleLoggerOptions => consoleLoggerOptions.TimestampFormat = "[HH:mm:ss] ");
			});

			_workerLogger = loggerFactory.CreateLogger<WorkerService>();
			_grpcServiceLogger = loggerFactory.CreateLogger<GrpcService>();
			_hordeRpcServerLogger = loggerFactory.CreateLogger<FakeHordeRpcServer>();
		}
		
		[TestMethod]
		public async Task ShutdownIfNoLeasesArriveWithinMaxWaitTime()
		{
			using CancellationTokenSource cts = new (10000);
			FakeHordeRpcServer fakeServer = new("testServerName", _hordeRpcServerLogger, cts.Token);
			WorkerServiceLambda lambdaFunc = new((_) => WorkerServiceTest.GetWorkerService(_grpcServiceLogger, _workerLogger, (a, b, c) => WorkerServiceTest.NullExecutor,(c) => fakeServer.GetClient()));

			AwsLambdaListenResponse response = await InvokeProtoAsync(lambdaFunc, new() { MaxWaitTimeForLeaseMs = 1000 }, cts.Token);
			Assert.IsFalse(response.DidAcceptLease);
		}
		
		[TestMethod]
		public async Task HandleOneLeaseAndShutdown()
		{
			using CancellationTokenSource cts = new (10000);
			FakeHordeRpcServer fakeServer = new("testServerName", _hordeRpcServerLogger, cts.Token);
			WorkerServiceLambda lambdaFunc = new((_) => WorkerServiceTest.GetWorkerService(_grpcServiceLogger, _workerLogger, (a, b, c) => WorkerServiceTest.NullExecutor,(c) => fakeServer.GetClient()));

			fakeServer.AddTestLease("testLeaseId");
			AwsLambdaListenResponse response = await InvokeProtoAsync(lambdaFunc, new() { MaxWaitTimeForLeaseMs = 5000 }, cts.Token);
			Assert.AreEqual(LeaseState.Completed, fakeServer.GetLease("testLeaseId").State);
			Assert.AreEqual(LeaseOutcome.Success, fakeServer.GetLease("testLeaseId").Outcome);
			Assert.IsTrue(response.DidAcceptLease);
		}

		/// <summary>
		/// Helper to serialize/deserialize parameters to <see cref="IAwsLambdaFunction" /> interface.
		/// </summary>
		/// <param name="function">AWS Lambda interface to invoke</param>
		/// <param name="request">Protobuf-based request message</param>
		/// <param name="cancellationToken"></param>
		/// <returns>Deserialized Protobuf message</returns>
		private static async Task<AwsLambdaListenResponse> InvokeProtoAsync(IAwsLambdaFunction function, AwsLambdaListenRequest request, CancellationToken cancellationToken)
		{
			ReadOnlyMemory<byte> responseData = await function.OnLambdaInvokeAsync("testRequestId", request.ToByteArray(), cancellationToken);
			return AwsLambdaListenResponse.Parser.ParseFrom(responseData.Span);
		}
	}
}
