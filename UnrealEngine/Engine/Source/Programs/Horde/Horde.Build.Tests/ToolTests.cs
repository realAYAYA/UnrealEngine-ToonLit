// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using Horde.Build.Tools;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	using ToolId = StringId<Tool>;
	using ToolDeploymentId = ObjectId<ToolDeployment>;

	[TestClass]
    public class ToolTests : TestSetup
    {
        [TestMethod]
        public async Task AddTool()
        {
			ToolCollection collection = ServiceProvider.GetRequiredService<ToolCollection>();

			ToolId toolId = new ToolId("ugs");
			await collection.ConfigureAsync(new List<ToolOptions> { new ToolOptions(toolId) { Name = "UnrealGameSync", Description = "Tool for syncing content from source control" } });

			Tool tool = Deref(await collection.GetAsync(toolId));
			Assert.AreEqual(tool.Id, new ToolId("ugs"));
			Assert.AreEqual(tool.Name, "UnrealGameSync");
			Assert.AreEqual(tool.Description, "Tool for syncing content from source control");

			Tool tool2 = Deref(await collection.GetAsync(tool.Id));
			Assert.AreEqual(tool.Id, tool2.Id);
			Assert.AreEqual(tool.LastUpdateTime, tool2.LastUpdateTime);
			Assert.AreEqual(tool.Name, tool2.Name);
			Assert.AreEqual(tool.Description, tool2.Description);
        }

		[TestMethod]
		public async Task AddDeployment()
		{
			ToolCollection collection = ServiceProvider.GetRequiredService<ToolCollection>();

			ToolId toolId = new ToolId("ugs");
			await collection.ConfigureAsync(new List<ToolOptions> { new ToolOptions(toolId) { Name = "UnrealGameSync", Description = "Tool for syncing content from source control" } });

			Tool tool = Deref(await collection.GetAsync(toolId));
			Assert.AreEqual(new ToolId("ugs"), tool.Id);
			Assert.AreEqual("UnrealGameSync", tool.Name);
			Assert.AreEqual("Tool for syncing content from source control", tool.Description);
			Assert.AreEqual(0, tool.Deployments.Count);

			byte[] deploymentData = Encoding.UTF8.GetBytes("hello world");

			ToolDeploymentId deploymentId;
			using (MemoryStream stream = new MemoryStream(deploymentData))
			{
				tool = Deref(await collection.CreateDeploymentAsync(tool, new ToolDeploymentOptions { Version = "1.0", Duration = TimeSpan.FromMinutes(5.0), CreatePaused = true }, stream));
				Assert.AreEqual(1, tool.Deployments.Count);
				Assert.IsNull(tool.Deployments[0].StartedAt);
				deploymentId = tool.Deployments[^1].Id;
			}

			// Check that the deployment doesn't do anything until started
			FakeClock clock = ServiceProvider.GetRequiredService<FakeClock>();
			await clock.AdvanceAsync(TimeSpan.FromHours(1.0));

			tool = Deref(await collection.GetAsync(tool.Id));
			Assert.AreEqual(1, tool.Deployments.Count);
			Assert.IsNull(tool.Deployments[0].StartedAt);

			// Start the deployment
			tool = Deref(await collection.UpdateDeploymentAsync(tool, deploymentId, ToolDeploymentState.Active));
			Assert.AreEqual(1, tool.Deployments.Count);
			Assert.IsNotNull(tool.Deployments[0].StartedAt);
			Assert.IsTrue(Math.Abs((tool.Deployments[0].StartedAt!.Value - clock.UtcNow).TotalSeconds) < 1.0);

			// Check it updates
			await clock.AdvanceAsync(TimeSpan.FromMinutes(2.5));
			tool = Deref(await collection.UpdateDeploymentAsync(tool, deploymentId, ToolDeploymentState.Paused));
			Assert.AreEqual(1, tool.Deployments.Count);
			Assert.IsNull(tool.Deployments[0].StartedAt);
			Assert.IsTrue((tool.Deployments[0].Progress - 0.5) < 0.1);

			// Check it stays paused
			await clock.AdvanceAsync(TimeSpan.FromHours(1.0));
			tool = Deref(await collection.GetAsync(tool.Id));
			Assert.AreEqual(1, tool.Deployments.Count);
			Assert.IsNull(tool.Deployments[0].StartedAt);
			Assert.IsTrue((tool.Deployments[0].Progress - 0.5) < 0.1);

			// Get the deployment data
			byte[] outputDeploymentData;
			using (MemoryStream stream = new MemoryStream())
			{
				Stream dataStream = await collection.GetDeploymentPayloadAsync(tool, tool.Deployments[0]);
				await dataStream.CopyToAsync(stream);
				outputDeploymentData = stream.ToArray();
			}
			Assert.IsTrue(deploymentData.AsSpan().SequenceEqual(outputDeploymentData));
		}
	}
}