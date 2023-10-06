// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Agents;
using Horde.Server.Auditing;
using Horde.Server.Jobs;
using Horde.Server.Server;
using Horde.Server.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Messages;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests;

/// <summary>
///     Testing the agent service
/// </summary>
[TestClass]
public class AgentServiceTest : TestSetup
{
	[TestMethod]
	public async Task GetJobs()
	{
		await CreateFixtureAsync();

		ActionResult<List<object>> res = await JobsController.FindJobsAsync();

		List<GetJobResponse> responses = res.Value!.ConvertAll(x => (GetJobResponse)x);
		responses.SortBy(x => x.Change);

		Assert.AreEqual(2, responses.Count);
		Assert.AreEqual("hello1", responses[0].Name);
		Assert.AreEqual("hello2", responses[1].Name);

		res = await JobsController.FindJobsAsync(includePreflight: false);
		Assert.AreEqual(1, res.Value!.Count);
		Assert.AreEqual("hello2", (res.Value[0] as GetJobResponse)!.Name);
	}

	[TestMethod]
	public async Task CreateSessionTest()
	{
		Fixture fixture = await CreateFixtureAsync();

		IAgent agent = await AgentService.CreateSessionAsync(fixture.Agent1, AgentStatus.Ok, new List<string>(),
			new Dictionary<string, int>(),
			"test");

		Assert.IsTrue(AgentService.AuthorizeSession(agent, GetUser(agent)));
		await Clock.AdvanceAsync(TimeSpan.FromMinutes(20));
		Assert.IsFalse(AgentService.AuthorizeSession(agent, GetUser(agent)));
	}
	
	private static long ToUnixTime(DateTime dateTime)
	{
		return new DateTimeOffset(dateTime).ToUnixTimeSeconds();
	}
	
	[TestMethod]
	public async Task LastStatusChangeDuringSessionCreate()
	{
		// No session created yet, status change timestamp is empty
		IAgent agent = await AgentService.CreateAgentAsync("agent1", true, null);
		Assert.AreEqual(AgentStatus.Unspecified, agent.Status);
		Assert.IsFalse(agent.LastStatusChange.HasValue);
		
		// A session has been created, status change timestamp is current time
		agent = await AgentService.CreateSessionAsync(agent, AgentStatus.Ok, new List<string>(), new Dictionary<string, int>(), "v1");
		Assert.AreEqual(AgentStatus.Ok, agent.Status);
		Assert.AreEqual(ToUnixTime(Clock.UtcNow), ToUnixTime(agent.LastStatusChange!.Value));
		DateTime lastStatusChange = agent.LastStatusChange!.Value;
		
		await Clock.AdvanceAsync(TimeSpan.FromMinutes(1));
		
		// The session is re-created, status change timestamp is same as when it first got created
		agent = await AgentService.CreateSessionAsync(agent, AgentStatus.Ok, new List<string>(), new Dictionary<string, int>(), "v1");
		Assert.AreEqual(AgentStatus.Ok, agent.Status);
		Assert.AreEqual(ToUnixTime(lastStatusChange), ToUnixTime(agent.LastStatusChange!.Value));
	}

	private static int s_agentId = 1;
	private async Task<IAgent> CreateAgentSession(bool enabled = true)
	{
		IAgent agent = await AgentService.CreateAgentAsync("agentServiceTest-" + s_agentId++, true, null);
		agent = await AgentService.CreateSessionAsync(agent, AgentStatus.Ok, new List<string>(), new Dictionary<string, int>(), "v1");
		return agent;
	}

	private static void AssertEqual(DateTime expected, DateTime? actual)
	{
		// When saved as MongoDB documents, some precision is lost. This compares only Unix seconds.
		Assert.AreEqual(ToUnixTime(expected), ToUnixTime(actual!.Value));
	}
	
	private static void AssertNotEqual(DateTime expected, DateTime? actual)
	{
		// When saved as MongoDB documents, some precision is lost. This compares only Unix seconds.
		Assert.AreNotEqual(ToUnixTime(expected), ToUnixTime(actual!.Value));
	}

	[TestMethod]
	[DataRow(AgentStatus.Ok, false)]
	[DataRow(AgentStatus.Unhealthy, true)]
	[DataRow(AgentStatus.Stopping, true)]
	[DataRow(AgentStatus.Stopped, true)]
	[DataRow(AgentStatus.Unspecified, true)]
	public async Task LastStatusChange(AgentStatus status, bool expectTimestampUpdate)
	{
		IAgent agent = await CreateAgentSession();
		DateTime lastStatusChange = agent.LastStatusChange!.Value;
		await Clock.AdvanceAsync(TimeSpan.FromMinutes(1));

		agent = (await AgentService.UpdateSessionAsync(agent, agent.SessionId!.Value, status, null, null, new List<Lease>()))!;
		if (expectTimestampUpdate)
		{
			AssertNotEqual(lastStatusChange, agent.LastStatusChange);
		}
		else
		{
			AssertEqual(lastStatusChange, agent.LastStatusChange);
		}
	}
	
	[TestMethod]
	public async Task AuditLogAwsInstanceTypeChanges()
	{
		Fixture fixture = await CreateFixtureAsync();
		IAuditLogChannel<AgentId> agentLogger = AgentCollection.GetLogger(fixture.Agent1.Id);

		async Task<bool> AuditLogContains(string text)
		{
			await agentLogger.FlushAsync();
			return await agentLogger.FindAsync().AnyAsync(x => x.Data.Contains(text, StringComparison.Ordinal));
		}
		
		List<string> props = new () { $"{KnownPropertyNames.AwsInstanceType}=m5.large" };
		IAgent agent = await AgentService.CreateSessionAsync(fixture.Agent1, AgentStatus.Ok, props, new Dictionary<string, int>(), "test");
		Assert.IsFalse(await AuditLogContains("AWS EC2 instance type changed"));
		
		props = new () { $"{KnownPropertyNames.AwsInstanceType}=c6.xlarge" };
		agent = await AgentService.CreateSessionAsync(agent, AgentStatus.Ok, props, new Dictionary<string, int>(), "test");
		Assert.IsTrue(await AuditLogContains("AWS EC2 instance type changed"));
	}
	
	[TestMethod]
	public async Task GetAgentRateTest()
	{
		IAgent agent1 = await AgentService.CreateAgentAsync("agent1", true, null);
		IAgent agent2 = await AgentService.CreateAgentAsync("agent2", true, null);
		await AgentService.CreateSessionAsync(agent1, AgentStatus.Ok, new List<string>() { "aws-instance-type=c5.24xlarge", "osfamily=windows" }, new Dictionary<string, int>(), "test");
		await AgentService.CreateSessionAsync(agent2, AgentStatus.Ok, new List<string>() { "aws-instance-type=c4.4xLARge", "osfamily=WinDowS" }, new Dictionary<string, int>(), "test");
		
		List<AgentRateConfig> agentRateConfigs = new()
		{
			new AgentRateConfig() { Condition = "aws-instance-type == 'c5.24xlarge' && osfamily == 'windows'", Rate = 200, },
			new AgentRateConfig() { Condition = "aws-instance-type == 'c4.4xlarge' && osfamily == 'windows'", Rate = 300 }
			
		};
		await AgentService.UpdateRateTableAsync(agentRateConfigs);
		
		double? rate1 = await AgentService.GetRateAsync(agent1.Id);
		Assert.AreEqual(200, rate1!.Value, 0.1);
		
		double? rate2 = await AgentService.GetRateAsync(agent2.Id);
		Assert.AreEqual(300, rate2!.Value, 0.1);
	}

	private static ClaimsPrincipal GetUser(IAgent agent)
	{
		return new ClaimsPrincipal(new ClaimsIdentity(
			new List<Claim> { new(HordeClaimTypes.AgentSessionId, agent.SessionId.ToString()!) }, "TestAuthType"));
	}
}