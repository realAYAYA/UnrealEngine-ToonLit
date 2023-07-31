// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using Cassandra;
using Horde.Storage.Controllers;
using Horde.Storage.Implementation;
using Horde.Storage.Implementation.TransactionLog;
using Jupiter.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using Moq.Contrib.HttpClient;
using Newtonsoft.Json;
using Serilog;
using Logger = Serilog.Core.Logger;
using EpicGames.Horde.Storage;

namespace Horde.Storage.FunctionalTests.Replication
{

    [TestClass]
    public class RefReplicatorTests
    {
        private static TestServer? _server;
        private IBlobService BlobStore { get; set; } = null!;

        private NamespaceId TestNamespace { get; } = new NamespaceId("test-namespace");
        private NamespaceId SnapshotNamespace { get; } = new NamespaceId("snapshot-namespace");
        private BucketId TestBucket { get; } = new BucketId("test");

        [TestInitialize]
        public async Task Setup()
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
                .UseSerilog(logger)
                .UseStartup<HordeStorageStartup>()
            );
            server.CreateClient();
            _server = server;

            BlobStore = _server.Services.GetService<IBlobService>()!;

            await Task.CompletedTask;
        }

        private static IEnumerable<KeyValuePair<string, string>> GetSettings()
        {
            return new[]
            {
                new KeyValuePair<string, string>("Horde_Storage:ReferencesDbImplementation", HordeStorageSettings.ReferencesDbImplementations.Scylla.ToString()),
                new KeyValuePair<string, string>("Horde_Storage:ReplicationLogWriterImplementation", HordeStorageSettings.ReplicationLogWriterImplementations.Scylla.ToString()),
            };
        }
        private static async Task TeardownDb(IServiceProvider provider)
        {
            IScyllaSessionManager scyllaSessionManager = provider.GetService<IScyllaSessionManager>()!;
            ISession session = scyllaSessionManager.GetSessionForLocalKeyspace();
            
            // remove replication log table as we expect it to be empty when starting the tests
            await session.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS replication_log;"));

            // remove the snapshots
            await session.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS replication_snapshot;"));
        }

        [TestCleanup]
        public async Task Teardown()
        {
            if (_server != null)
            {
                await TeardownDb(_server.Services);
            }
        }
        
        [TestMethod]
        public async Task ReplicationIncrementalState()
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
            Dictionary<BlobIdentifier, byte[]> blobs = new();

            const int countOfTestEvents = 100;
            for (int i = 0; i < countOfTestEvents; i++)
            {
                byte[] blobContents = Encoding.UTF8.GetBytes($"random content {i}");
                BlobIdentifier blob = BlobIdentifier.FromBlob(blobContents);
                blobs.Add(blob, blobContents);
                replicationEvents.Add(new ReplicationLogEvent(TestNamespace, TestBucket, IoHashKey.FromName($"event-{i}"), blob, Guid.NewGuid(), "refs-000", DateTime.Now, ReplicationLogEvent.OpType.Added));
            }
            string lastBucket = "refs-000";
            Guid lastEvent = Guid.NewGuid();

            Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();
            string s = JsonConvert.SerializeObject(new ReplicationLogEvents(replicationEvents));
            handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?lastBucket={lastBucket}&lastEvent={lastEvent}").ReturnsResponse(s, "application/json");
            // this will get called again with the last event in replication events list as the lastEvent, at which point we should return a empty list as there are no more objects available
            handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?lastBucket={replicationEvents.Last().TimeBucket}&lastEvent={replicationEvents.Last().EventId}").ReturnsResponse(JsonConvert.SerializeObject(new ReplicationLogEvents(new List<ReplicationLogEvent>())), "application/json");

            foreach (BlobIdentifier blob in blobs.Keys)
            {
                handler.SetupRequest($"http://localhost/api/v1/objects/{TestNamespace}/{blob}/references").ReturnsResponse(JsonConvert.SerializeObject(new ResolvedReferencesResult(Array.Empty<BlobIdentifier>())), "application/json").Verifiable();
            }

            foreach ((BlobIdentifier key, byte[] blobContent) in blobs)
            {
                handler.SetupRequest($"http://localhost/api/v1/blobs/{TestNamespace}/{key}").ReturnsResponse(blobContent, "application/octet-stream").Verifiable();
            }

            IHttpClientFactory httpClientFactory = handler.CreateClientFactory();
            using RefsReplicator replicator = ActivatorUtilities.CreateInstance<RefsReplicator>(_server!.Services, replicatorSettings, httpClientFactory);
            replicator.SetRefState(lastBucket, lastEvent);

            bool didRun = await replicator.TriggerNewReplications();

            Assert.IsTrue(didRun);

            handler.Verify();

            // Verify that the objects are present
            BlobIdentifier[] missingBlobs = await BlobStore.FilterOutKnownBlobs(TestNamespace, blobs.Keys.ToArray());
            Assert.IsFalse(missingBlobs.Any());
        }

        [TestMethod]
        public async Task ReplicationSnapshotState()
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
            Dictionary<BlobIdentifier, byte[]> blobs = new();

            const int countOfTestEvents = 100;
            for (int i = 0; i < countOfTestEvents; i++)
            {
                byte[] blobContents = Encoding.UTF8.GetBytes($"random content {i}");
                BlobIdentifier blob = BlobIdentifier.FromBlob(blobContents);
                blobs.Add(blob, blobContents);
                replicationEvents.Add(new ReplicationLogEvent(TestNamespace, TestBucket, IoHashKey.FromName($"event-{i}"), blob, Guid.NewGuid(), "refs-000", DateTime.Now, ReplicationLogEvent.OpType.Added));
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

            BlobIdentifier snapshotBlob = BlobIdentifier.FromBlob(snapshotContent);

            Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();
            string s = JsonConvert.SerializeObject(new ReplicationLogSnapshots(new List<SnapshotInfo>{new SnapshotInfo(TestNamespace, SnapshotNamespace, snapshotBlob, DateTime.Now)}));
            handler.SetupRequest($"http://localhost/api/v1/replication-log/snapshots/{TestNamespace}").ReturnsResponse(s, "application/json");

            // after processing a snapshot it will attempt to incrementally replicate from there, which should be empty
            handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?lastBucket={replicationEvents.Last().TimeBucket}&lastEvent={replicationEvents.Last().EventId}").ReturnsResponse(JsonConvert.SerializeObject(new ReplicationLogEvents(new List<ReplicationLogEvent>())), "application/json");

            handler.SetupRequest($"http://localhost/api/v1/blobs/{SnapshotNamespace}/{snapshotBlob}").ReturnsResponse(snapshotContent, "application/octet-stream").Verifiable();

            foreach (BlobIdentifier blob in blobs.Keys)
            {
                handler.SetupRequest($"http://localhost/api/v1/objects/{TestNamespace}/{blob}/references").ReturnsResponse(JsonConvert.SerializeObject(new ResolvedReferencesResult(Array.Empty<BlobIdentifier>())), "application/json").Verifiable();
            }

            foreach ((BlobIdentifier key, byte[] blobContent) in blobs)
            {
                handler.SetupRequest($"http://localhost/api/v1/blobs/{TestNamespace}/{key}").ReturnsResponse(blobContent, "application/octet-stream").Verifiable();
            }

            IHttpClientFactory httpClientFactory = handler.CreateClientFactory();
            using RefsReplicator replicator = ActivatorUtilities.CreateInstance<RefsReplicator>(_server!.Services, replicatorSettings, httpClientFactory);
            // no previous state, will download it from a snapshot
            replicator.SetRefState(null, null);

            bool didRun = await replicator.TriggerNewReplications();

            Assert.IsTrue(didRun);

            handler.Verify();

            // Verify that the objects are present
            BlobIdentifier[] missingBlobs = await BlobStore.FilterOutKnownBlobs(TestNamespace, blobs.Keys.ToArray());
            Assert.IsFalse(missingBlobs.Any());
        }

        [TestMethod]
        public async Task ReplicationStateBoth()
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
            Dictionary<BlobIdentifier, byte[]> blobs = new();

            const int CountOfTestEvents = 100;
            for (int i = 0; i < CountOfTestEvents; i++)
            {
                byte[] blobContents = Encoding.UTF8.GetBytes($"random content in snapshot {i}");
                BlobIdentifier blob = BlobIdentifier.FromBlob(blobContents);
                blobs.Add(blob, blobContents);
                snapshotEvents.Add(new ReplicationLogEvent(TestNamespace, TestBucket,IoHashKey.FromName($"event-{i}"), blob, Guid.NewGuid(), "refs-000", DateTime.Now, ReplicationLogEvent.OpType.Added));
            }

            for (int i = 0; i < CountOfTestEvents; i++)
            {
                byte[] blobContents = Encoding.UTF8.GetBytes($"random content {i}");
                BlobIdentifier blob = BlobIdentifier.FromBlob(blobContents);
                blobs.Add(blob, blobContents);
                incrementalEvents.Add(new ReplicationLogEvent(TestNamespace, TestBucket, IoHashKey.FromName($"incremental-event-{i}"), blob, Guid.NewGuid(), "refs-000", DateTime.Now, ReplicationLogEvent.OpType.Added));
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

            BlobIdentifier snapshotBlob = BlobIdentifier.FromBlob(snapshotContent);

            Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();
            string s = JsonConvert.SerializeObject(new ReplicationLogSnapshots(new List<SnapshotInfo>{new SnapshotInfo(TestNamespace, SnapshotNamespace, snapshotBlob, DateTime.Now)}));
            handler.SetupRequest($"http://localhost/api/v1/replication-log/snapshots/{TestNamespace}").ReturnsResponse(s, "application/json");

            // when the snapshot has been processed we have a set of incremental events as well
            handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?lastBucket={snapshotEvents.Last().TimeBucket}&lastEvent={snapshotEvents.Last().EventId}").ReturnsResponse(JsonConvert.SerializeObject(new ReplicationLogEvents(incrementalEvents)), "application/json");

            // after processing the incremental events there is nothing more to find
            handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?lastBucket={incrementalEvents.Last().TimeBucket}&lastEvent={incrementalEvents.Last().EventId}").ReturnsResponse(JsonConvert.SerializeObject(new ReplicationLogEvents(new List<ReplicationLogEvent>())), "application/json");

            handler.SetupRequest($"http://localhost/api/v1/blobs/{SnapshotNamespace}/{snapshotBlob}").ReturnsResponse(snapshotContent, "application/octet-stream").Verifiable();

            foreach (BlobIdentifier blob in blobs.Keys)
            {
                handler.SetupRequest($"http://localhost/api/v1/objects/{TestNamespace}/{blob}/references").ReturnsResponse(JsonConvert.SerializeObject(new ResolvedReferencesResult(Array.Empty<BlobIdentifier>())), "application/json").Verifiable();
            }

            foreach ((BlobIdentifier key, byte[] blobContent) in blobs)
            {
                handler.SetupRequest($"http://localhost/api/v1/blobs/{TestNamespace}/{key}").ReturnsResponse(blobContent, "application/octet-stream").Verifiable();
            }

            IHttpClientFactory httpClientFactory = handler.CreateClientFactory();
            using RefsReplicator replicator = ActivatorUtilities.CreateInstance<RefsReplicator>(_server!.Services, replicatorSettings, httpClientFactory);
            // no previous state, will download it from a snapshot
            replicator.SetRefState(null, null);

            bool didRun = await replicator.TriggerNewReplications();

            Assert.IsTrue(didRun);

            handler.Verify();

            // Verify that the objects are present
            BlobIdentifier[] missingBlobs = await BlobStore.FilterOutKnownBlobs(TestNamespace, blobs.Keys.ToArray());
            Assert.IsFalse(missingBlobs.Any());
        }
        
        [TestMethod]
        public async Task ReplicationStateSnapshotFallback()
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
            Dictionary<BlobIdentifier, byte[]> blobs = new();

            const int CountOfTestEvents = 100;
            for (int i = 0; i < CountOfTestEvents; i++)
            {
                byte[] blobContents = Encoding.UTF8.GetBytes($"random content in snapshot {i}");
                BlobIdentifier blob = BlobIdentifier.FromBlob(blobContents);
                blobs.Add(blob, blobContents);
                snapshotEvents.Add(new ReplicationLogEvent(TestNamespace, TestBucket, IoHashKey.FromName($"event-{i}"), blob, Guid.NewGuid(), "refs-000", DateTime.Now, ReplicationLogEvent.OpType.Added));
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

            BlobIdentifier snapshotBlob = BlobIdentifier.FromBlob(snapshotContent);

            string missingBucket = "this-does-not-exist";
            Guid missingId = Guid.NewGuid();

            Mock<HttpMessageHandler> handler = new Mock<HttpMessageHandler>();
            string s = JsonConvert.SerializeObject(new ReplicationLogSnapshots(new List<SnapshotInfo>{new SnapshotInfo(TestNamespace, SnapshotNamespace, snapshotBlob, DateTime.Now)}));
            handler.SetupRequest($"http://localhost/api/v1/replication-log/snapshots/{TestNamespace}").ReturnsResponse(s, "application/json");

            // mock a error being generated due to the lastBucket/event being to old
            handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?lastBucket={missingBucket}&lastEvent={missingId}").ReturnsResponse(HttpStatusCode.BadRequest, JsonConvert.SerializeObject(new ProblemDetails
            {
                Title = $"Log file is not available, use snapshot {snapshotBlob} instead",
                Type = "http://jupiter.epicgames.com/replication/useSnapshot",
                Extensions = { { "SnapshotId", snapshotBlob } }
            }), "application/json");

            // after processing the snapshot we do not replicate anything more
            handler.SetupRequest($"http://localhost/api/v1/replication-log/incremental/{TestNamespace}?lastBucket={snapshotEvents.Last().TimeBucket}&lastEvent={snapshotEvents.Last().EventId}").ReturnsResponse(JsonConvert.SerializeObject(new ReplicationLogEvents(new List<ReplicationLogEvent>())), "application/json");

            handler.SetupRequest($"http://localhost/api/v1/blobs/{SnapshotNamespace}/{snapshotBlob}").ReturnsResponse(snapshotContent, "application/octet-stream").Verifiable();

            foreach (BlobIdentifier blob in blobs.Keys)
            {
                handler.SetupRequest($"http://localhost/api/v1/objects/{TestNamespace}/{blob}/references").ReturnsResponse(JsonConvert.SerializeObject(new ResolvedReferencesResult(Array.Empty<BlobIdentifier>())), "application/json").Verifiable();
            }

            foreach ((BlobIdentifier key, byte[] blobContent) in blobs)
            {
                handler.SetupRequest($"http://localhost/api/v1/blobs/{TestNamespace}/{key}").ReturnsResponse(blobContent, "application/octet-stream").Verifiable();
            }

            IHttpClientFactory httpClientFactory = handler.CreateClientFactory();
            using RefsReplicator replicator = ActivatorUtilities.CreateInstance<RefsReplicator>(_server!.Services, replicatorSettings, httpClientFactory);
            // specify a bucket that does not exist, we should fallback to replicating a snapshot
            replicator.SetRefState(missingBucket, missingId);

            bool didRun = await replicator.TriggerNewReplications();

            Assert.IsTrue(didRun);

            handler.Verify();

            // Verify that the objects are present
            BlobIdentifier[] missingBlobs = await BlobStore.FilterOutKnownBlobs(TestNamespace, blobs.Keys.ToArray());
            Assert.IsFalse(missingBlobs.Any());
        }
    }
}
