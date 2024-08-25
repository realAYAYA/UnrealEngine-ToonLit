// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Streams;
using Horde.Server.Acls;
using Horde.Server.Artifacts;
using Horde.Server.Server;
using HordeCommon;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Artifacts
{
	[TestClass]
	public class ArtifactTests : TestSetup
	{
		[TestMethod]
		public async Task CreateArtifactAsync()
		{
			StreamId streamId = new StreamId("foo");

			IArtifactCollection artifactCollection = ServiceProvider.GetRequiredService<IArtifactCollection>();
			IArtifact artifact = await artifactCollection.AddAsync(new ArtifactName("default"), ArtifactType.StepOutput, null, streamId, 1, new string[] { "test1", "test2" }, Array.Empty<string>(), AclScopeName.Root);

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
				Assert.AreEqual(artifact.Id, artifacts[0].Id);
			}

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test2" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
				Assert.AreEqual(artifact.Id, artifacts[0].Id);
			}

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test3" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
		}

		[TestMethod]
		public async Task ExpireArtifactAsync()
		{
			FakeClock clock = ServiceProvider.GetRequiredService<FakeClock>();
			ArtifactExpirationService expirationService = ServiceProvider.GetRequiredService<ArtifactExpirationService>();

			ArtifactType type = new ArtifactType("my-artifact");
			UpdateConfig(config => config.ArtifactTypes.Add(new ArtifactTypeConfig { Type = type, KeepDays = 1 }));

			await expirationService.StartAsync(CancellationToken.None);

			StreamId streamId = new StreamId("foo");

			IArtifactCollection artifactCollection = ServiceProvider.GetRequiredService<IArtifactCollection>();
			IArtifact artifact = await artifactCollection.AddAsync(new ArtifactName("default"), type, null, streamId, 1, new string[] { "test1", "test2" }, Array.Empty<string>(), AclScopeName.Root);

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
				Assert.AreEqual(artifact.Id, artifacts[0].Id);
			}

			await clock.AdvanceAsync(TimeSpan.FromDays(2.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
		}

		[TestMethod]
		public async Task UpdateExpiryTimesAsync()
		{
			DateTime startTime = Clock.UtcNow;

			ArtifactType type = new ArtifactType("my-artifact");
			UpdateConfig(config => config.ArtifactTypes.Add(new ArtifactTypeConfig { Type = type, KeepDays = 1 }));

			ArtifactExpirationService expirationService = ServiceProvider.GetRequiredService<ArtifactExpirationService>();

			await expirationService.StartAsync(CancellationToken.None);

			StreamId streamId = new StreamId("foo");

			IArtifactCollection artifactCollection = ServiceProvider.GetRequiredService<IArtifactCollection>();
			IArtifact artifact = await artifactCollection.AddAsync(new ArtifactName("default"), type, null, streamId, 1, new string[] { "test1", "test2" }, Array.Empty<string>(), AclScopeName.Root);

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
				Assert.AreEqual(artifact.Id, artifacts[0].Id);
			}

			UpdateConfig(config => config.ArtifactTypes = new List<ArtifactTypeConfig> { new ArtifactTypeConfig { Type = type, KeepDays = 4 } });
			await Clock.AdvanceAsync(TimeSpan.FromDays(2.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(1, artifacts.Count);
			}

			UpdateConfig(config => config.ArtifactTypes = new List<ArtifactTypeConfig> { new ArtifactTypeConfig { Type = type, KeepDays = 1 } });
			await Clock.AdvanceAsync(TimeSpan.FromDays(1.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(streamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
		}

		[TestMethod]
		public async Task ExpireCountAsync()
		{
			DateTime startTime = Clock.UtcNow;

			ArtifactType type = new ArtifactType("my-artifact");
			UpdateConfig(config => config.ArtifactTypes.Add(new ArtifactTypeConfig { Type = type, KeepCount = 4 }));

			ArtifactExpirationService expirationService = ServiceProvider.GetRequiredService<ArtifactExpirationService>();

			await expirationService.StartAsync(CancellationToken.None);

			StreamId fooStreamId = new StreamId("foo");
			StreamId barStreamId = new StreamId("bar");

			IArtifactCollection artifactCollection = ServiceProvider.GetRequiredService<IArtifactCollection>();
			for (int idx = 0; idx < 10; idx++)
			{
				await artifactCollection.AddAsync(new ArtifactName($"default-{idx}"), type, null, fooStreamId, 1, new string[] { "test1", "test2" }, Array.Empty<string>(), AclScopeName.Root);
			}
			for (int idx = 0; idx < 10; idx++)
			{
				await artifactCollection.AddAsync(new ArtifactName($"default-{idx}"), type, null, barStreamId, 1, new string[] { "test1", "test2" }, Array.Empty<string>(), AclScopeName.Root);
			}

			await Clock.AdvanceAsync(TimeSpan.FromDays(2.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(fooStreamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(4, artifacts.Count);

				for (int idx = 0; idx < 4; idx++)
				{
					Assert.AreEqual(new ArtifactName($"default-{9 - idx}"), artifacts[idx].Name);
				}
			}
			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(barStreamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(4, artifacts.Count);

				for (int idx = 0; idx < 4; idx++)
				{
					Assert.AreEqual(new ArtifactName($"default-{9 - idx}"), artifacts[idx].Name);
				}
			}

			UpdateConfig(config => config.ArtifactTypes = new List<ArtifactTypeConfig> { new ArtifactTypeConfig { Type = type, KeepDays = 1 } });
			await Clock.AdvanceAsync(TimeSpan.FromDays(2.0));

			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(fooStreamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
			{
				List<IArtifact> artifacts = await artifactCollection.FindAsync(barStreamId, keys: new[] { "test1" }).ToListAsync();
				Assert.AreEqual(0, artifacts.Count);
			}
		}
	}
}
