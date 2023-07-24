// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using Google.Protobuf;
using Horde.Agent.Commands.Compute;
using Horde.Agent.Leases;
using Horde.Agent.Services;
using HordeCommon;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests
{
	[TestClass]
	public sealed class AwsLambdaListenerCommandTest : IDisposable
	{
		class TestTaskHandler : LeaseHandler<TestTask>
		{
			public override Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, TestTask message, CancellationToken cancellationToken)
			{
				return Task.FromResult(LeaseResult.Success);
			}
		}

		private readonly FakeHordeRpcServer _fakeServer = new();
		private readonly ServiceCollection _serviceCollection;

		public AwsLambdaListenerCommandTest()
		{
			_serviceCollection = new ServiceCollection();
			_serviceCollection.AddLogging();
			_serviceCollection.AddSingleton<CapabilitiesService>();
			_serviceCollection.AddSingleton<LeaseHandler, TestTaskHandler>();
			_serviceCollection.AddSingleton<ISessionFactoryService>(sp => new FakeServerSessionFactory(_fakeServer));
		}

		public void Dispose()
		{
			_fakeServer.DisposeAsync().AsTask().Wait();
		}

		[TestMethod]
		public async Task ShutdownIfNoLeasesArriveWithinMaxWaitTime()
		{
			using CancellationTokenSource cts = new(10000);
			using ServiceProvider serviceProvider = _serviceCollection.BuildServiceProvider();

			WorkerServiceLambda lambdaFunc = new(serviceProvider);

			AwsLambdaListenResponse response = await InvokeProtoAsync(lambdaFunc, new AwsLambdaListenRequest { MaxWaitTimeForLeaseMs = 1000 }, cts.Token);
			Assert.IsFalse(response.DidAcceptLease);
		}

		[TestMethod]
		public async Task HandleOneLeaseAndShutdown()
		{
			using ServiceProvider serviceProvider = _serviceCollection.BuildServiceProvider();

			using CancellationTokenSource cts = new (10000);
			WorkerServiceLambda lambdaFunc = new(serviceProvider);

			_fakeServer.AddTestLease("testLeaseId");
			AwsLambdaListenResponse response = await InvokeProtoAsync(lambdaFunc, new AwsLambdaListenRequest { MaxWaitTimeForLeaseMs = 5000 }, cts.Token);
			Assert.AreEqual(LeaseState.Completed, _fakeServer.GetLease("testLeaseId").State);
			Assert.AreEqual(LeaseOutcome.Success, _fakeServer.GetLease("testLeaseId").Outcome);
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
