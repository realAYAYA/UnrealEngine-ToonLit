// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Tools;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Tools;
using HordeCommon;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Tools
{
	[TestClass]
	public class ToolTests : TestSetup
	{
		ToolId _toolId = new ToolId("ugs");

		public ToolTests()
		{
			GlobalConfig globalConfig = new GlobalConfig();
			globalConfig.Storage.Backends.Clear();
			globalConfig.Storage.Backends.Add(new BackendConfig { Id = new BackendId("tools-backend"), Type = StorageBackendType.Memory });
			globalConfig.Storage.Namespaces.Clear();
			globalConfig.Storage.Namespaces.Add(new NamespaceConfig { Id = Namespace.Tools, Backend = new BackendId("tools-backend") });
			globalConfig.Tools.Add(new ToolConfig(_toolId) { Name = "UnrealGameSync", Description = "Tool for syncing content from source control" });
			SetConfig(globalConfig);
		}

		[TestMethod]
		public async Task AddToolAsync()
		{
			IToolCollection collection = ServiceProvider.GetRequiredService<IToolCollection>();

			ITool tool = Deref(await collection.GetAsync(_toolId, GlobalConfig.CurrentValue));
			Assert.AreEqual(tool.Id, new ToolId("ugs"));
			Assert.AreEqual(tool.Config.Name, "UnrealGameSync");
			Assert.AreEqual(tool.Config.Description, "Tool for syncing content from source control");

			ITool tool2 = Deref(await collection.GetAsync(tool.Id, GlobalConfig.CurrentValue));
			Assert.AreEqual(tool.Id, tool2.Id);
			Assert.AreEqual(tool.Config.Name, tool2.Config.Name);
			Assert.AreEqual(tool.Config.Description, tool2.Config.Description);
		}

		[TestMethod]
		public async Task AddDeploymentAsync()
		{
			IToolCollection collection = ServiceProvider.GetRequiredService<IToolCollection>();

			ITool tool = Deref(await collection.GetAsync(_toolId, GlobalConfig.CurrentValue));
			Assert.AreEqual(new ToolId("ugs"), tool.Id);
			Assert.AreEqual("UnrealGameSync", tool.Config.Name);
			Assert.AreEqual("Tool for syncing content from source control", tool.Config.Description);
			Assert.AreEqual(0, tool.Deployments.Count);

			const string FileName = "test.txt";
			byte[] fileData = Encoding.UTF8.GetBytes("hello world");

			byte[] zipData;
			using (MemoryStream stream = new MemoryStream())
			{
				using (ZipArchive archive = new ZipArchive(stream, ZipArchiveMode.Create))
				{
					ZipArchiveEntry entry = archive.CreateEntry(FileName);
					using (Stream entryStream = entry.Open())
					{
						await entryStream.WriteAsync(fileData);
					}
				}
				zipData = stream.ToArray();
			}

			ToolDeploymentId deploymentId;
			using (MemoryStream stream = new MemoryStream(zipData))
			{
				tool = Deref(await collection.CreateDeploymentAsync(tool, new ToolDeploymentConfig { Version = "1.0", Duration = TimeSpan.FromMinutes(5.0), CreatePaused = true }, stream, GlobalConfig.CurrentValue, CancellationToken.None));
				Assert.AreEqual(1, tool.Deployments.Count);
				Assert.IsNull(tool.Deployments[0].StartedAt);
				deploymentId = tool.Deployments[^1].Id;
			}

			// Check that the deployment doesn't do anything until started
			FakeClock clock = ServiceProvider.GetRequiredService<FakeClock>();
			await clock.AdvanceAsync(TimeSpan.FromHours(1.0));

			tool = Deref(await collection.GetAsync(tool.Id, GlobalConfig.CurrentValue));
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
			tool = Deref(await collection.GetAsync(tool.Id, GlobalConfig.CurrentValue));
			Assert.AreEqual(1, tool.Deployments.Count);
			Assert.IsNull(tool.Deployments[0].StartedAt);
			Assert.IsTrue((tool.Deployments[0].Progress - 0.5) < 0.1);

			// Get the deployment data
			using Stream dataStream = await collection.GetDeploymentZipAsync(tool, tool.Deployments[0], CancellationToken.None);
			using (ZipArchive archive = new ZipArchive(dataStream, ZipArchiveMode.Read))
			{
				ZipArchiveEntry entry = archive.Entries.First();
				Assert.AreEqual(entry.Name, FileName);

				byte[] outputFileData;
				using (MemoryStream stream = new MemoryStream())
				{
					using Stream entryStream = entry.Open();
					await entryStream.CopyToAsync(stream);
					outputFileData = stream.ToArray();
				}

				Assert.IsTrue(fileData.AsSpan().SequenceEqual(outputFileData.AsSpan()));
			}
		}
	}
}