// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using Cassandra;
using Jupiter.Controllers;
using Jupiter.Implementation;
using Jupiter.Implementation.TransactionLog;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using Moq.Contrib.HttpClient;
using Serilog;
using Logger = Serilog.Core.Logger;
using EpicGames.Horde.Storage;

namespace Jupiter.FunctionalTests.Replication
{

	[TestClass]
	[DoNotParallelize]
	public class RefReplicatorTests
	{
		private TestServer? _server;
		private IBlobService BlobStore { get; set; } = null!;

		private NamespaceId TestNamespace { get; } = new NamespaceId("test-namespace");
		private NamespaceId SnapshotNamespace { get; } = new NamespaceId("snapshot-namespace");
		private BucketId TestBucket { get; } = new BucketId("test");

		[TestInitialize]
		public async Task SetupAsync()
		{
			IConfigurationRoot configuration = new ConfigurationBuilder()
				// we are not reading the base appSettings here as we want exact control over what runs in the tests
				.AddJsonFile("appsettings.Testing.json", true)
				.AddInMemoryCollection(GetSettings())
				.AddEnvironmentVariables()
				.Build();

			Logger logger = new LoggerConfiguration()
				.ReadFrom.Configuration(configuration)
				.CreateLogger();

			TestServer server = new TestServer(new WebHostBuilder()
				.UseConfiguration(configuration)
				.UseEnvironment("Testing")
				.ConfigureServices(collection => collection.AddSerilog(logger))
				.UseStartup<JupiterStartup>()
			);
			server.CreateClient();
			_server = server;

			BlobStore = _server.Services.GetService<IBlobService>()!;

			await Task.CompletedTask;
		}

		private static IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementation", UnrealCloudDDCSettings.ReferencesDbImplementations.Scylla.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReplicationLogWriterImplementation", UnrealCloudDDCSettings.ReplicationLogWriterImplementations.Scylla.ToString()),
			};
		}
		private static async Task TeardownDbAsync(IServiceProvider provider)
		{
			await Task.CompletedTask;
			IScyllaSessionManager scyllaSessionManager = provider.GetService<IScyllaSessionManager>()!;
			ISession session = scyllaSessionManager.GetSessionForLocalKeyspace();
			
			// remove replication log table as we expect it to be empty when starting the tests
			await session.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS replication_log;"));

			// remove the snapshots
			await session.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS replication_snapshot;"));
		}

		[TestCleanup]
		public async Task TeardownAsync()
		{
			if (_server != null)
			{
				await TeardownDbAsync(_server.Services);
			}
		}
		
		[TestMethod]
		public async Task ReplicationIncrementalStateAsync()
		{
			ReplicatorSettings replicatorSettings = new()
			{
				ConnectionString = "http://localhost",
				MaxParallelReplications = 16,
				NamespaceToReplicate = TestNamespace.ToString(),
				ReplicatorName = "test-replicator",
				Version = ReplicatorVersion.Refs
			};

			List<ReplicationLogEvent> replicationEvents = new();
			Dictionary<BlobId, byte[]> blobs = new();

			const int countOfTestEvents = 100;
			for (int i = 0; i < countOfTestEvents; i++)
			{
				byte[] blobContents = Encoding.UTF8.GetBytes($"random content {i}");
				BlobId blob = BlobId.FromBlob(blobContents);
				blobs.Add(blob, blobContents);
				replicationEvents.Add(new ReplicationLogEvent(TestNamespace, TestBucket, RefId.FromName($"event-{i}"), blob, Guid.NewGuid(), "refs-000", DateTime.Now, ReplicationLogEvent.OpType.Added));
			}
			string lastBucket = "refs-000";
			Guid lastEvent = Guid.NewGuid();

			Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();
			string s = JsonSerializer.Serialize(new ReplicationLogEvents(replicationEvents));
			handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?count=1000&lastBucket={lastBucket}&lastEvent={lastEvent}").ReturnsResponse(s, "application/json");
			// this will get called again with the last event in replication events list as the lastEvent, at which point we should return a empty list as there are no more objects available
			handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?count=1000&lastBucket={replicationEvents.Last().TimeBucket}&lastEvent={replicationEvents.Last().EventId}").ReturnsResponse(JsonSerializer.Serialize(new ReplicationLogEvents(new List<ReplicationLogEvent>())), "application/json");

			foreach (BlobId blob in blobs.Keys)
			{
				handler.SetupRequest($"http://localhost/api/v1/objects/{TestNamespace}/{blob}/references").ReturnsResponse(JsonSerializer.Serialize(new ResolvedReferencesResult(Array.Empty<BlobId>())), "application/json").Verifiable();
			}

			foreach ((BlobId key, byte[] blobContent) in blobs)
			{
				handler.SetupRequest($"http://localhost/api/v1/blobs/{TestNamespace}/{key}").ReturnsResponse(blobContent, "application/octet-stream").Verifiable();
			}

			IHttpClientFactory httpClientFactory = handler.CreateClientFactory();
			using RefsReplicator replicator = ActivatorUtilities.CreateInstance<RefsReplicator>(_server!.Services, replicatorSettings, httpClientFactory);
			replicator.SetRefState(lastBucket, lastEvent);

			bool didRun = await replicator.TriggerNewReplicationsAsync();

			Assert.IsTrue(didRun);

			handler.Verify();

			// Verify that the objects are present
			BlobId[] missingBlobs = await BlobStore.FilterOutKnownBlobsAsync(TestNamespace, blobs.Keys.ToArray());
			Assert.IsFalse(missingBlobs.Any());
		}

		[TestMethod]
		public async Task ReplicationSnapshotStateAsync()
		{
			ReplicatorSettings replicatorSettings = new()
			{
				ConnectionString = "http://localhost",
				MaxParallelReplications = 16,
				NamespaceToReplicate = TestNamespace.ToString(),
				ReplicatorName = "test-replicator",
				Version = ReplicatorVersion.Refs
			};

			List<ReplicationLogEvent> replicationEvents = new();
			Dictionary<BlobId, byte[]> blobs = new();

			const int countOfTestEvents = 100;
			for (int i = 0; i < countOfTestEvents; i++)
			{
				byte[] blobContents = Encoding.UTF8.GetBytes($"random content {i}");
				BlobId blob = BlobId.FromBlob(blobContents);
				blobs.Add(blob, blobContents);
				replicationEvents.Add(new ReplicationLogEvent(TestNamespace, TestBucket, RefId.FromName($"event-{i}"), blob, Guid.NewGuid(), "refs-000", DateTime.Now, ReplicationLogEvent.OpType.Added));
			}

			// Build snapshot
			ReplicationLogSnapshot snapshot = ReplicationLogFactory.CreateEmptySnapshot(TestNamespace);
			foreach (ReplicationLogEvent logEvent in replicationEvents)
			{
				snapshot.ProcessEvent(logEvent);
			}

			byte[] snapshotContent;
			{
				await using MemoryStream ms = new MemoryStream();
				snapshot.Serialize(ms);
				snapshotContent = ms.ToArray();
			}

			BlobId snapshotBlob = BlobId.FromBlob(snapshotContent);

			Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();
			string s = JsonSerializer.Serialize(new ReplicationLogSnapshots(new List<SnapshotInfo>{new SnapshotInfo(TestNamespace, SnapshotNamespace, snapshotBlob, DateTime.Now)}));
			handler.SetupRequest($"http://localhost/api/v1/replication-log/snapshots/{TestNamespace}").ReturnsResponse(s, "application/json");

			// after processing a snapshot it will attempt to incrementally replicate from there, which should be empty
			handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?count=1000&lastBucket={replicationEvents.Last().TimeBucket}&lastEvent={replicationEvents.Last().EventId}").ReturnsResponse(JsonSerializer.Serialize(new ReplicationLogEvents(new List<ReplicationLogEvent>())), "application/json");

			handler.SetupRequest($"http://localhost/api/v1/blobs/{SnapshotNamespace}/{snapshotBlob}").ReturnsResponse(snapshotContent, "application/octet-stream").Verifiable();

			foreach (BlobId blob in blobs.Keys)
			{
				handler.SetupRequest($"http://localhost/api/v1/objects/{TestNamespace}/{blob}/references").ReturnsResponse(JsonSerializer.Serialize(new ResolvedReferencesResult(Array.Empty<BlobId>())), "application/json").Verifiable();
			}

			foreach ((BlobId key, byte[] blobContent) in blobs)
			{
				handler.SetupRequest($"http://localhost/api/v1/blobs/{TestNamespace}/{key}").ReturnsResponse(blobContent, "application/octet-stream").Verifiable();
			}

			IHttpClientFactory httpClientFactory = handler.CreateClientFactory();
			using RefsReplicator replicator = ActivatorUtilities.CreateInstance<RefsReplicator>(_server!.Services, replicatorSettings, httpClientFactory);
			// no previous state, will download it from a snapshot
			replicator.SetRefState(null, null);

			bool didRun = await replicator.TriggerNewReplicationsAsync();

			Assert.IsTrue(didRun);

			handler.Verify();

			// Verify that the objects are present
			BlobId[] missingBlobs = await BlobStore.FilterOutKnownBlobsAsync(TestNamespace, blobs.Keys.ToArray());
			Assert.IsFalse(missingBlobs.Any());
		}

		[TestMethod]
		public async Task ReplicationStateBothAsync()
		{
			ReplicatorSettings replicatorSettings = new()
			{
				ConnectionString = "http://localhost",
				MaxParallelReplications = 16,
				NamespaceToReplicate = TestNamespace.ToString(),
				ReplicatorName = "test-replicator",
				Version = ReplicatorVersion.Refs
			};

			List<ReplicationLogEvent> snapshotEvents = new();
			List<ReplicationLogEvent> incrementalEvents = new();
			Dictionary<BlobId, byte[]> blobs = new();

			const int CountOfTestEvents = 100;
			for (int i = 0; i < CountOfTestEvents; i++)
			{
				byte[] blobContents = Encoding.UTF8.GetBytes($"random content in snapshot {i}");
				BlobId blob = BlobId.FromBlob(blobContents);
				blobs.Add(blob, blobContents);
				snapshotEvents.Add(new ReplicationLogEvent(TestNamespace, TestBucket,RefId.FromName($"event-{i}"), blob, Guid.NewGuid(), "refs-000", DateTime.Now, ReplicationLogEvent.OpType.Added));
			}

			for (int i = 0; i < CountOfTestEvents; i++)
			{
				byte[] blobContents = Encoding.UTF8.GetBytes($"random content {i}");
				BlobId blob = BlobId.FromBlob(blobContents);
				blobs.Add(blob, blobContents);
				incrementalEvents.Add(new ReplicationLogEvent(TestNamespace, TestBucket, RefId.FromName($"incremental-event-{i}"), blob, Guid.NewGuid(), "refs-000", DateTime.Now, ReplicationLogEvent.OpType.Added));
			}
			
			// Build snapshot
			ReplicationLogSnapshot snapshot = ReplicationLogFactory.CreateEmptySnapshot(TestNamespace);
			foreach (ReplicationLogEvent logEvent in snapshotEvents)
			{
				snapshot.ProcessEvent(logEvent);
			}

			byte[] snapshotContent;
			{
				await using MemoryStream ms = new MemoryStream();
				snapshot.Serialize(ms);
				snapshotContent = ms.ToArray();
			}

			BlobId snapshotBlob = BlobId.FromBlob(snapshotContent);

			Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();
			string s = JsonSerializer.Serialize(new ReplicationLogSnapshots(new List<SnapshotInfo>{new SnapshotInfo(TestNamespace, SnapshotNamespace, snapshotBlob, DateTime.Now)}));
			handler.SetupRequest($"http://localhost/api/v1/replication-log/snapshots/{TestNamespace}").ReturnsResponse(s, "application/json");

			// when the snapshot has been processed we have a set of incremental events as well
			handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?count=1000&lastBucket={snapshotEvents.Last().TimeBucket}&lastEvent={snapshotEvents.Last().EventId}").ReturnsResponse(JsonSerializer.Serialize(new ReplicationLogEvents(incrementalEvents)), "application/json");

			// after processing the incremental events there is nothing more to find
			handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?count=1000&lastBucket={incrementalEvents.Last().TimeBucket}&lastEvent={incrementalEvents.Last().EventId}").ReturnsResponse(JsonSerializer.Serialize(new ReplicationLogEvents(new List<ReplicationLogEvent>())), "application/json");

			handler.SetupRequest($"http://localhost/api/v1/blobs/{SnapshotNamespace}/{snapshotBlob}").ReturnsResponse(snapshotContent, "application/octet-stream").Verifiable();

			foreach (BlobId blob in blobs.Keys)
			{
				handler.SetupRequest($"http://localhost/api/v1/objects/{TestNamespace}/{blob}/references").ReturnsResponse(JsonSerializer.Serialize(new ResolvedReferencesResult(Array.Empty<BlobId>())), "application/json").Verifiable();
			}

			foreach ((BlobId key, byte[] blobContent) in blobs)
			{
				handler.SetupRequest($"http://localhost/api/v1/blobs/{TestNamespace}/{key}").ReturnsResponse(blobContent, "application/octet-stream").Verifiable();
			}

			IHttpClientFactory httpClientFactory = handler.CreateClientFactory();
			using RefsReplicator replicator = ActivatorUtilities.CreateInstance<RefsReplicator>(_server!.Services, replicatorSettings, httpClientFactory);
			// no previous state, will download it from a snapshot
			replicator.SetRefState(null, null);

			bool didRun = await replicator.TriggerNewReplicationsAsync();

			Assert.IsTrue(didRun);

			handler.Verify();

			// Verify that the objects are present
			BlobId[] missingBlobs = await BlobStore.FilterOutKnownBlobsAsync(TestNamespace, blobs.Keys.ToArray());
			Assert.IsFalse(missingBlobs.Any());
		}
		
		[TestMethod]
		public async Task ReplicationStateSnapshotFallbackAsync()
		{
			ReplicatorSettings replicatorSettings = new()
			{
				ConnectionString = "http://localhost",
				MaxParallelReplications = 16,
				NamespaceToReplicate = TestNamespace.ToString(),
				ReplicatorName = "test-replicator",
				Version = ReplicatorVersion.Refs
			};

			List<ReplicationLogEvent> snapshotEvents = new();
			Dictionary<BlobId, byte[]> blobs = new();

			const int CountOfTestEvents = 100;
			for (int i = 0; i < CountOfTestEvents; i++)
			{
				byte[] blobContents = Encoding.UTF8.GetBytes($"random content in snapshot {i}");
				BlobId blob = BlobId.FromBlob(blobContents);
				blobs.Add(blob, blobContents);
				snapshotEvents.Add(new ReplicationLogEvent(TestNamespace, TestBucket, RefId.FromName($"event-{i}"), blob, Guid.NewGuid(), "refs-000", DateTime.Now, ReplicationLogEvent.OpType.Added));
			}

			// Build snapshot
			ReplicationLogSnapshot snapshot = ReplicationLogFactory.CreateEmptySnapshot(TestNamespace);
			foreach (ReplicationLogEvent logEvent in snapshotEvents)
			{
				snapshot.ProcessEvent(logEvent);
			}

			byte[] snapshotContent;
			{
				await using MemoryStream ms = new MemoryStream();
				snapshot.Serialize(ms);
				snapshotContent = ms.ToArray();
			}

			BlobId snapshotBlob = BlobId.FromBlob(snapshotContent);

			string missingBucket = "this-does-not-exist";
			Guid missingId = Guid.NewGuid();

			Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();
			string s = JsonSerializer.Serialize(new ReplicationLogSnapshots(new List<SnapshotInfo>{new SnapshotInfo(TestNamespace, SnapshotNamespace, snapshotBlob, DateTime.Now)}));
			handler.SetupRequest($"http://localhost/api/v1/replication-log/snapshots/{TestNamespace}").ReturnsResponse(s, "application/json");

			// mock a error being generated due to the lastBucket/event being to old
			handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?count=1000&lastBucket={missingBucket}&lastEvent={missingId}").ReturnsResponse(HttpStatusCode.BadRequest, JsonSerializer.Serialize(new ProblemDetails
			{
				Title = $"Log file is not available, use snapshot {snapshotBlob} instead",
				Type = "http://jupiter.epicgames.com/replication/useSnapshot",
				Extensions = { { "SnapshotId", snapshotBlob }, { "BlobNamespace", SnapshotNamespace } }
			}), "application/json");

			// after processing the snapshot we do not replicate anything more
			handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?count=1000&lastBucket={snapshotEvents.Last().TimeBucket}&lastEvent={snapshotEvents.Last().EventId}").ReturnsResponse(JsonSerializer.Serialize(new ReplicationLogEvents(new List<ReplicationLogEvent>())), "application/json");

			handler.SetupRequest($"http://localhost/api/v1/blobs/{SnapshotNamespace}/{snapshotBlob}").ReturnsResponse(snapshotContent, "application/octet-stream").Verifiable();

			foreach (BlobId blob in blobs.Keys)
			{
				handler.SetupRequest($"http://localhost/api/v1/objects/{TestNamespace}/{blob}/references").ReturnsResponse(JsonSerializer.Serialize(new ResolvedReferencesResult(Array.Empty<BlobId>())), "application/json").Verifiable();
			}

			foreach ((BlobId key, byte[] blobContent) in blobs)
			{
				handler.SetupRequest($"http://localhost/api/v1/blobs/{TestNamespace}/{key}").ReturnsResponse(blobContent, "application/octet-stream").Verifiable();
			}

			IHttpClientFactory httpClientFactory = handler.CreateClientFactory();
			using RefsReplicator replicator = ActivatorUtilities.CreateInstance<RefsReplicator>(_server!.Services, replicatorSettings, httpClientFactory);
			// specify a bucket that does not exist, we should fallback to replicating a snapshot
			replicator.SetRefState(missingBucket, missingId);

			bool didRun = await replicator.TriggerNewReplicationsAsync();

			Assert.IsTrue(didRun);

			handler.Verify();

			// Verify that the objects are present
			BlobId[] missingBlobs = await BlobStore.FilterOutKnownBlobsAsync(TestNamespace, blobs.Keys.ToArray());
			Assert.IsFalse(missingBlobs.Any());
		}
	}
}
