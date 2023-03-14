// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using Horde.Build.Agents;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests;

[TestClass]
public class AgentControllerDbTest : TestSetup
{
	[TestMethod]
	public async Task UpdateAgent()
	{
		Fixture fixture = await CreateFixtureAsync();
		IAgent fixtureAgent = fixture.Agent1;

		ActionResult<object> obj = await AgentsController.GetAgentAsync(fixtureAgent.Id);
		GetAgentResponse getRes = (obj.Value as GetAgentResponse)!;
		Assert.AreEqual(fixture!.Agent1Name.ToUpper(), getRes.Name);
		Assert.IsNull(getRes.Comment);

		UpdateAgentRequest updateReq = new();
		updateReq.Comment = "foo bar baz";
		await AgentsController.UpdateAgentAsync(fixtureAgent.Id, updateReq);

		obj = await AgentsController.GetAgentAsync(fixtureAgent.Id);
		getRes = (obj.Value as GetAgentResponse)!;
		Assert.AreEqual("foo bar baz", getRes.Comment);
	}
}