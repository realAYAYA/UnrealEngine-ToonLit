// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using Amazon.AutoScaling;
using Amazon.AutoScaling.Model;
using Horde.Server.Agents;
using Horde.Server.Agents.Fleet;
using Horde.Server.Agents.Pools;
using HordeCommon;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;

namespace Horde.Server.Tests.Fleet;

[TestClass]
public class AwsAutoScalingLifecycleServiceTest : TestSetup
{
	private AwsAutoScalingLifecycleService _asgLifecycleService = default!;
	private Mock<IAmazonAutoScaling> _asgMock = default!;
	private readonly List<CompleteLifecycleActionRequest> _lifecycleUpdates = new();
	private readonly FakeAmazonSqs _fakeSqs = new();
	private readonly string _queueUrl = "https://sqs.us-east-1.amazonaws.com/123456789/MyQueue";

	protected override void Dispose(bool disposing)
	{
		if (disposing)
		{
			_asgLifecycleService?.Dispose();
			_fakeSqs?.Dispose();
		}

		base.Dispose(disposing);
	}

	[TestInitialize]
	public async Task Setup()
	{
		ILogger<AwsAutoScalingLifecycleService> logger = ServiceProvider.GetRequiredService<ILogger<AwsAutoScalingLifecycleService>>();
		_asgMock = new Mock<IAmazonAutoScaling>(MockBehavior.Strict);

		Task<CompleteLifecycleActionResponse> OnCompleteLifecycleActionAsync(CompleteLifecycleActionRequest request, CancellationToken cancellationToken)
		{
			_lifecycleUpdates.Add(request);
			return Task.FromResult(new CompleteLifecycleActionResponse() { HttpStatusCode = HttpStatusCode.OK });
		}

		_asgMock
			.Setup(x => x.CompleteLifecycleActionAsync(It.IsAny<CompleteLifecycleActionRequest>(), It.IsAny<CancellationToken>()))
			.Returns(OnCompleteLifecycleActionAsync);
		
		ServerSettings ss = new () { AwsAutoScalingQueueUrl = _queueUrl };
		_asgLifecycleService = new AwsAutoScalingLifecycleService(AgentService, GetRedisServiceSingleton(), AgentCollection, Clock, new TestOptionsMonitor<ServerSettings>(ss), ServiceProvider, Tracer, logger);
		_asgLifecycleService.SetAmazonClientsTesting(_asgMock.Object, _fakeSqs);
		await _asgLifecycleService.StartAsync(CancellationToken.None);
	}
	
	[TestMethod]
	public async Task TerminationRequested_WithAgentInService_MarkedForShutdown()
	{
		// Arrange
		LifecycleActionEvent lae = new() { Ec2InstanceId = "i-1234", LifecycleActionToken = "action-token-test", Origin = "AutoScalingGroup" };
		IAgent agent = await CreateAgentAsync(new PoolId("pool1"), properties: new() { KnownPropertyNames.AwsInstanceId + "=" + lae.Ec2InstanceId });
		
		// Act
		await _fakeSqs.SendMessageAsync(_queueUrl, JsonSerializer.Serialize(lae));
		await _asgLifecycleService.ReceiveLifecycleEventsAsync(CancellationToken.None);

		// Assert
		agent = (await AgentService.GetAgentAsync(agent.Id))!;
		Assert.IsTrue(agent.RequestShutdown);
	}
	
	[TestMethod]
	public async Task TerminationRequested_WithAgentOnline_LifecycleIsContinued()
	{
		// Arrange
		LifecycleActionEvent lae = new() { Ec2InstanceId = "i-1234", LifecycleActionToken = "action-token-test", Origin = "AutoScalingGroup" };
		await CreateAgentAsync(new PoolId("pool1"), properties: new() { KnownPropertyNames.AwsInstanceId + "=" + lae.Ec2InstanceId });
		await _asgLifecycleService.InitiateTerminationAsync(lae, CancellationToken.None);
		
		// Act
		await Clock.AdvanceAsync(_asgLifecycleService.LifecycleUpdaterInterval + TimeSpan.FromSeconds(10));
		
		// Assert
		AssertLifecycleUpdate(lae, AwsAutoScalingLifecycleService.ActionContinue, _lifecycleUpdates[0]);
	}
	
	[TestMethod]
	public async Task TerminationRequested_WithAgentGoingFromOnlineToOffline()
	{
		// Arrange
		LifecycleActionEvent lae = new() { Ec2InstanceId = "i-1234", LifecycleActionToken = "action-token-test", Origin = "AutoScalingGroup" };
		List<string> props = new() { KnownPropertyNames.AwsInstanceId + "=" + lae.Ec2InstanceId };
		IAgent agent = await CreateAgentAsync(new PoolId("pool1"), properties: props);
		agent = await AgentService.CreateSessionAsync(agent, AgentStatus.Ok, props, new Dictionary<string, int>(), "v1");
		await _asgLifecycleService.InitiateTerminationAsync(lae, CancellationToken.None);
		TimeSpan extraMargin = TimeSpan.FromSeconds(10);
		
		// Agent is online (e.g still running a job) and the lifecycle update tick is triggered
		await Clock.AdvanceAsync(_asgLifecycleService.LifecycleUpdaterInterval + extraMargin);
		AssertLifecycleUpdate(lae, AwsAutoScalingLifecycleService.ActionContinue, _lifecycleUpdates[0]);
		
		// Agent still online
		await Clock.AdvanceAsync(_asgLifecycleService.LifecycleUpdaterInterval + extraMargin);
		AssertLifecycleUpdate(lae, AwsAutoScalingLifecycleService.ActionContinue, _lifecycleUpdates[1]);
		
		// Agent responded to shutdown request, now safe to notify AWS ASG the termination can take place
		agent = await AgentService.CreateSessionAsync(agent, AgentStatus.Stopped, props, new Dictionary<string, int>(), "v1");
		await Clock.AdvanceAsync(_asgLifecycleService.LifecycleUpdaterInterval + extraMargin);
		AssertLifecycleUpdate(lae, AwsAutoScalingLifecycleService.ActionAbandon, _lifecycleUpdates[2]);
		
		// Trigger lifecycle update tick once again and no further updates should have been sent
		await Clock.AdvanceAsync(_asgLifecycleService.LifecycleUpdaterInterval + extraMargin);
		Assert.AreEqual(3, _lifecycleUpdates.Count);
	}
	
	[TestMethod]
	public async Task TerminationRequested_WithAgentInWarmPool_LifecycleIsAbandoned()
	{
		// Arrange
		LifecycleActionEvent lae = new() { Ec2InstanceId = "i-1234", LifecycleActionToken = "action-token-test", Origin = "WarmPool" };

		// Act
		await _asgLifecycleService.InitiateTerminationAsync(lae, CancellationToken.None);
		
		// Assert
		AssertLifecycleUpdate(lae, AwsAutoScalingLifecycleService.ActionAbandon, _lifecycleUpdates[0]);
	}
	
	[TestMethod]
	public void DeserializeLifecycleActionEvent()
	{
		string rawText = @"
{
	""Origin"": ""AutoScalingGroup"",
	""LifecycleHookName"": ""my-hook-name"",
	""Destination"": ""EC2"",
	""AccountId"": ""123456789"",
	""RequestId"": ""5ab929ac-06d9-11ee-be56-0242ac120002"",
	""LifecycleTransition"": ""autoscaling:EC2_INSTANCE_TERMINATING"",
	""AutoScalingGroupName"": ""my-asg-name"",
	""Service"": ""AWS Auto Scaling"",
	""Time"": ""2023-06-09T11:11:20.345Z"",
	""EC2InstanceId"": ""i-123456789"",
	""LifecycleActionToken"": ""5ec3537e-06d9-11ee-be56-0242ac120002""
}
";

		LifecycleActionEvent? ev = JsonSerializer.Deserialize<LifecycleActionEvent>(rawText);
		Assert.AreEqual("5ec3537e-06d9-11ee-be56-0242ac120002", ev!.LifecycleActionToken);
		Assert.AreEqual("my-asg-name", ev.AutoScalingGroupName);
		Assert.AreEqual("my-hook-name", ev.LifecycleHookName);
		Assert.AreEqual("i-123456789", ev.Ec2InstanceId);
		Assert.AreEqual("autoscaling:EC2_INSTANCE_TERMINATING", ev.LifecycleTransition);
		Assert.AreEqual("AutoScalingGroup", ev.Origin);
	}

	private static void AssertLifecycleUpdate(LifecycleActionEvent expectedEvent, string expectedResult, CompleteLifecycleActionRequest actual)
	{
		Assert.AreEqual(expectedEvent.Ec2InstanceId, actual.InstanceId);
		Assert.AreEqual(expectedResult, actual.LifecycleActionResult);
		Assert.AreEqual(expectedEvent.LifecycleActionToken, actual.LifecycleActionToken);
		Assert.AreEqual(expectedEvent.LifecycleHookName, actual.LifecycleHookName);
		Assert.AreEqual(expectedEvent.AutoScalingGroupName, actual.AutoScalingGroupName);
	}
}
