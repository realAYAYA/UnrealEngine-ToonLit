// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Mime;
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
using Serilog;
using Logger = Serilog.Core.Logger;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using EpicGames.AspNet;
using Jupiter.Implementation.Objects;
using Jupiter.Tests.Functional;

namespace Jupiter.FunctionalTests.References
{

	[TestClass]
	[DoNotParallelize]
	public class ScyllaReplicationTests : ReplicationTests
	{
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementation", UnrealCloudDDCSettings.ReferencesDbImplementations.Scylla.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReplicationLogWriterImplementation", UnrealCloudDDCSettings.ReplicationLogWriterImplementations.Scylla.ToString()),
			};
		}

		protected override async Task SeedDb(IServiceProvider provider)
		{
			IReferencesStore referencesStore = provider.GetService<IReferencesStore>()!;
			if (referencesStore is MemoryCachedReferencesStore memoryReferencesStore)
			{
				memoryReferencesStore.Clear();
				referencesStore = memoryReferencesStore.GetUnderlyingStore();
			}
			Assert.IsTrue(referencesStore.GetType() == typeof(ScyllaReferencesStore));

			IReplicationLog replicationLog = provider.GetService<IReplicationLog>()!;
			Assert.IsTrue(replicationLog.GetType() == typeof(ScyllaReplicationLog));

			await SeedTestDataAsync();
		}

		protected override async Task TeardownDb(IServiceProvider provider)
		{
			IScyllaSessionManager scyllaSessionManager = provider.GetService<IScyllaSessionManager>()!;

			ISession localKeyspace = scyllaSessionManager.GetSessionForLocalKeyspace();
			
			await Task.WhenAll(
				// remove replication log table as we expect it to be empty when starting the tests
				localKeyspace.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS replication_log;")),
				// remove the snapshots
				localKeyspace.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS replication_snapshot;")),
				// remove the namespaces
				localKeyspace.ExecuteAsync(new SimpleStatement("DROP TABLE IF EXISTS replication_namespace;"))
			);
		}
	}

	[TestClass]
	public class MemoryReplicationTests : ReplicationTests
	{
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementation", UnrealCloudDDCSettings.ReferencesDbImplementations.Memory.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReplicationLogWriterImplementation", UnrealCloudDDCSettings.ReplicationLogWriterImplementations.Memory.ToString()),
			};
		}

		protected override async Task SeedDb(IServiceProvider provider)
		{
			IReferencesStore referencesStore = provider.GetService<IReferencesStore>()!;
			if (referencesStore is MemoryCachedReferencesStore memoryReferencesStore)
			{
				memoryReferencesStore.Clear();
				referencesStore = memoryReferencesStore.GetUnderlyingStore();
			}
			//verify we are using the expected refs store
			Assert.IsTrue(referencesStore.GetType() == typeof(MemoryReferencesStore));

			IReplicationLog replicationLog = provider.GetService<IReplicationLog>()!;
			Assert.IsTrue(replicationLog.GetType() == typeof(MemoryReplicationLog));

			await SeedTestDataAsync();
		}

		protected override Task TeardownDb(IServiceProvider provider)
		{
			return Task.CompletedTask;
		}
	}
	
	[DoNotParallelize]
	public abstract class ReplicationTests
	{
		private TestServer? _server;
		private HttpClient? _httpClient;
		private IBlobService _blobStore = null!;
		private IReplicationLog _replicationLog = null!;

		private readonly NamespaceId TestNamespace = new NamespaceId("test-namespace-replication");
		private readonly NamespaceId SnapshotNamespace = new NamespaceId("snapshot-namespace");
		private readonly BucketId TestBucket = new BucketId("default");

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
			_httpClient = server.CreateClient();
			_server = server;

			_blobStore = _server.Services.GetService<IBlobService>()!;
			_replicationLog = _server.Services.GetService<IReplicationLog>()!;

			await SeedDb(server.Services);
		}

		protected virtual async Task SeedTestDataAsync()
		{
			await Task.CompletedTask;
		}

		[TestCleanup]
		public async Task TeardownAsync()
		{
			if (_server != null)
			{
				await TeardownDb(_server.Services);
			}
		}

		protected abstract IEnumerable<KeyValuePair<string, string?>> GetSettings();

		protected abstract Task SeedDb(IServiceProvider provider);
		protected abstract Task TeardownDb(IServiceProvider provider);
		
		
		[TestMethod]
		public async Task ReplicationLogCreationAsync()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			using HttpContent requestContent = new ByteArrayContent(objectData);
			requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
			requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

			RefId firstObjectKey = RefId.FromName("newReferenceObjectReplicationCreate");
			RefId secondObjectKey = RefId.FromName("secondObjectReplicationCreate");
			RefId thirdObjectKey = RefId.FromName("thirdObjectReplicationCreate");
			{
				HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{TestBucket}/{firstObjectKey}.uecb", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			{
				HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{TestBucket}/{secondObjectKey}.uecb", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}

			{
				HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{TestBucket}/{thirdObjectKey}.uecb", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}
			
			{
				HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				
				Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, MediaTypeNames.Application.Json);

				string s = await result.Content.ReadAsStringAsync();
				ReplicationLogEvents? events = JsonSerializer.Deserialize<ReplicationLogEvents>(s, JsonTestUtils.DefaultJsonSerializerSettings);
				Assert.IsNotNull(events);
				Assert.AreEqual(3, events!.Events.Count);

				// parse the events returned, make sure they are in the right order
				{
					ReplicationLogEvent e = events.Events[0];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(firstObjectKey, e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = events.Events[1];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(secondObjectKey, e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = events.Events[2];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(thirdObjectKey, e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}
			}
		}

		[TestMethod]
		public async Task ReplicationLogReadingAsync()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			DateTime oldestTimestamp = DateTime.Now.AddDays(-1);
			(string eventBucket, Guid eventId) = await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("firstObject"), objectHash, oldestTimestamp);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("secondObject"), objectHash, oldestTimestamp.AddHours(2.0));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("thirdObject"), objectHash, oldestTimestamp.AddHours(3.0));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("fourthObject"), objectHash, oldestTimestamp.AddDays(0.9));

			{
				HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}?lastBucket={eventBucket}&lastEvent={eventId}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				
				Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, MediaTypeNames.Application.Json);

				ReplicationLogEvents? events = await result.Content.ReadFromJsonAsync<ReplicationLogEvents>(JsonTestUtils.DefaultJsonSerializerSettings);
				Assert.IsNotNull(events);
				Assert.AreEqual(3, events!.Events.Count);

				// we will not get the first event, as if we ever were to fetch all events we could potentially have events that are missed and snapshots are used instead

				// parse the events returned, make sure they are in the right order
				{
					ReplicationLogEvent e = events.Events[0];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("secondObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = events.Events[1];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("thirdObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = events.Events[2];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("fourthObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}
			}
		 
			CollectionAssert.AreEqual(new [] {TestNamespace}, await _replicationLog.GetNamespacesAsync().ToArrayAsync());
		}

		[TestMethod]
		public async Task ReplicationLogResumeAsync()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			DateTime oldestTimestamp = DateTime.Now.AddDays(-3);
			// insert multiple objects in the same time bucket, verifying that we correctly get only the objects after this
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("firstObject"), objectHash, oldestTimestamp);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("secondObject"), objectHash, oldestTimestamp.AddHours(2.0));
			(string eventBucket, Guid eventId) = await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("thirdObject"), objectHash, oldestTimestamp.AddHours(2.1));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("fourthObject"), objectHash, oldestTimestamp.AddHours(2.11));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("fifthObject"), objectHash, oldestTimestamp.AddHours(2.12));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("sixthObject"), objectHash, oldestTimestamp.AddDays(2.13));

			{
				HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}?lastBucket={eventBucket}&lastEvent={eventId}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				
				Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, MediaTypeNames.Application.Json);

				ReplicationLogEvents? events = await result.Content.ReadFromJsonAsync<ReplicationLogEvents>(JsonTestUtils.DefaultJsonSerializerSettings);
				Assert.IsNotNull(events);
				Assert.AreEqual(3, events!.Events.Count);

				// we will not get the first event, as if we ever were to fetch all events we could potentially have events that are missed and snapshots are used instead

				// parse the events returned, make sure they are in the right order
				{
					ReplicationLogEvent e = events.Events[0];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("fourthObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = events.Events[1];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("fifthObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = events.Events[2];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("sixthObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}
			}
		 
			CollectionAssert.AreEqual(new [] {TestNamespace}, await _replicationLog.GetNamespacesAsync().ToArrayAsync());
		}

		[TestMethod]
		public async Task ReplicationLogReadingLimitAsync()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			DateTime oldestTimestamp = DateTime.Now.AddDays(-1);
			(string eventBucket, Guid eventId) = await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("firstObject"), objectHash, oldestTimestamp);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("secondObject"), objectHash, oldestTimestamp.AddHours(1));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("thirdObject"), objectHash, oldestTimestamp.AddHours(1.5));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("fourthObject"), objectHash, oldestTimestamp.AddDays(0.9));

			// start from the second event
			const int EventsToFetch = 2;
			{
				HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}?lastBucket={eventBucket}&lastEvent={eventId}&count={EventsToFetch}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				
				Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, MediaTypeNames.Application.Json);

				ReplicationLogEvents? events = await result.Content.ReadFromJsonAsync<ReplicationLogEvents>(JsonTestUtils.DefaultJsonSerializerSettings);
				Assert.IsNotNull(events);
				Assert.AreEqual(EventsToFetch, events!.Events.Count);

				// parse the events returned, make sure they are in the right order
				{
					ReplicationLogEvent e = events.Events[0];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("secondObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = events.Events[1];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("thirdObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}
			}
			
			CollectionAssert.AreEqual(new [] {TestNamespace}, await _replicationLog.GetNamespacesAsync().ToArrayAsync());
		}

		[TestMethod]
		public async Task ReplicationLogInvalidBucketAsync()
		{
			// the namespace exists but the bucket does not
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("firstObject"), objectHash, DateTime.Now.AddDays(-1));

			string eventBucket = "rep-00000000";
			Guid eventId = Guid.NewGuid();

			{
				HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}?lastBucket={eventBucket}&lastEvent={eventId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.BadRequest, result.StatusCode);
				
				ProblemDetails? problem = await result.Content.ReadFromJsonAsync<ProblemDetails?>();
				Assert.IsNotNull(problem);
			}

			CollectionAssert.AreEqual(new [] {TestNamespace}, await _replicationLog.GetNamespacesAsync().ToArrayAsync());
		}

		[TestMethod]
		public async Task ReplicationLogOldBucketAsync()
		{
			// the namespace exists but the bucket id is from a old bucket that does not exist anymore
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("firstObject"), objectHash, DateTime.Now.AddDays(-1));

			string eventBucket = DateTime.Now.AddDays(-60).ToReplicationBucketIdentifier();
			Guid eventId = Guid.NewGuid();

			{
				HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}?lastBucket={eventBucket}&lastEvent={eventId}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.BadRequest, result.StatusCode);
				
				ProblemDetails? problem = await result.Content.ReadFromJsonAsync<ProblemDetails?>();
				Assert.IsNotNull(problem);
			}

			CollectionAssert.AreEqual(new [] {TestNamespace}, await _replicationLog.GetNamespacesAsync().ToArrayAsync());
		}

		[TestMethod]
		public async Task ReplicationLogEmptyLogAsync()
		{
			// the namespace does not exist
			{
				HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
				ProblemDetails? problem = await result.Content.ReadFromJsonAsync<ProblemDetails?>();
				Assert.IsNotNull(problem);
			}

			CollectionAssert.AreEqual(Array.Empty<NamespaceId>(), await _replicationLog.GetNamespacesAsync().ToArrayAsync());
		}

		[TestMethod]
		public async Task ReplicationLogNoIncrementalLogAvailableAsync()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			DateTime oldestTimestamp = DateTime.Now.AddDays(-1);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("firstObject"), objectHash, oldestTimestamp);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("secondObject"), objectHash, oldestTimestamp.AddHours(1));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("thirdObject"), objectHash, oldestTimestamp.AddHours(1.5));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("fourthObject"), objectHash, oldestTimestamp.AddDays(0.9));

			// create a snapshot
			ReplicationLogSnapshotBuilder snapshotBuilder = ActivatorUtilities.CreateInstance<ReplicationLogSnapshotBuilder>(_server!.Services);
			Assert.IsNotNull(snapshotBuilder);
			BlobId snapshotBlobId = await snapshotBuilder.BuildSnapshotAsync(TestNamespace, SnapshotNamespace);
			Assert.IsTrue(await _blobStore.ExistsAsync(SnapshotNamespace, snapshotBlobId));

			// use a bucket that does not exist, should raise a message to use a snapshot instead
			string bucketThatDoesNotExist = "rep-0000";
			{
				HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}?lastBucket={bucketThatDoesNotExist}&lastEvent={Guid.NewGuid()}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.BadRequest, result.StatusCode);
				ProblemDetails? problem = await result.Content.ReadFromJsonAsync<ProblemDetails?>();
				Assert.IsNotNull(problem);
				Assert.AreEqual("http://jupiter.epicgames.com/replication/useSnapshot", problem!.Type);
				Assert.IsTrue(problem.Extensions.ContainsKey("SnapshotId"));
				Assert.AreEqual(snapshotBlobId, new BlobId(problem.Extensions["SnapshotId"]!.ToString()!));
				Assert.AreEqual(SnapshotNamespace, new NamespaceId(problem.Extensions["BlobNamespace"]!.ToString()!));
			}
		}

		[TestMethod]
		public async Task ReplicationLogSnapshotCreationAsync()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			DateTime oldestTimestamp = DateTime.Now.AddDays(-1);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("firstObject"), objectHash, oldestTimestamp);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("secondObject"), objectHash, oldestTimestamp.AddHours(1));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("thirdObject"), objectHash, oldestTimestamp.AddHours(1.5));
			(string lastEventBucket, Guid lastEventId) = await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("fourthObject"), objectHash, oldestTimestamp.AddDays(0.7));

			// verify the objects were added
			{
				List<ReplicationLogEvent> logEvents = await _replicationLog.GetAsync(TestNamespace, null, null).ToListAsync();
				Assert.AreEqual(4, logEvents.Count);

				// parse the events returned, make sure they are in the right order
				{
					ReplicationLogEvent e = logEvents[0];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("firstObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = logEvents[1];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("secondObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = logEvents[2];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("thirdObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = logEvents[3];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("fourthObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}
			}

			// verify there are no previous snapshots
			Assert.AreEqual(0, (await _replicationLog.GetSnapshotsAsync(TestNamespace).ToListAsync()).Count);

			ReplicationLogFactory replicationLogFactory = ActivatorUtilities.CreateInstance<ReplicationLogFactory>(_server!.Services);

			// create a snapshot
			ReplicationLogSnapshotBuilder snapshotBuilder = ActivatorUtilities.CreateInstance<ReplicationLogSnapshotBuilder>(_server!.Services);
			Assert.IsNotNull(snapshotBuilder);
			BlobId snapshotBlobId = await snapshotBuilder.BuildSnapshotAsync(TestNamespace, SnapshotNamespace);
			Assert.IsTrue(await _blobStore.ExistsAsync(SnapshotNamespace, snapshotBlobId));

			SnapshotInfo? snapshotInfo = await _replicationLog.GetSnapshotsAsync(TestNamespace).FirstAsync();
			Assert.IsNotNull(snapshotInfo);
			Assert.AreEqual(snapshotBlobId, snapshotInfo.SnapshotBlob);

			BlobContents blobContents = await _blobStore.GetObjectAsync(SnapshotNamespace, snapshotBlobId);
			ReplicationLogSnapshot snapshot = replicationLogFactory.DeserializeSnapshotFromStream(blobContents.Stream);

			Assert.AreEqual(lastEventId, snapshot.LastEvent);
			Assert.AreEqual(lastEventBucket, snapshot.LastBucket);

			List<SnapshotLiveObject> liveObjects = snapshot.GetLiveObjects().ToList();
			Assert.IsTrue(liveObjects.Any(o => o.Bucket == TestBucket && o.Key == RefId.FromName("firstObject")));
			Assert.IsTrue(liveObjects.Any(o => o.Bucket == TestBucket && o.Key == RefId.FromName("secondObject")));
			Assert.IsTrue(liveObjects.Any(o => o.Bucket == TestBucket && o.Key == RefId.FromName("thirdObject")));
			Assert.IsTrue(liveObjects.Any(o => o.Bucket == TestBucket && o.Key == RefId.FromName("fourthObject")));
		}

		[TestMethod]
		public async Task ReplicationLogSnapshotQueryingAsync()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			DateTime oldestTimestamp = DateTime.Now.AddDays(-1);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("firstObject"), objectHash, oldestTimestamp);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("secondObject"), objectHash, oldestTimestamp.AddHours(1));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("thirdObject"), objectHash, oldestTimestamp.AddHours(1.5));
			(string lastEventBucket, Guid lastEventId) = await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("fourthObject"), objectHash, oldestTimestamp.AddDays(0.9));

			// verify the objects were added
			List<ReplicationLogEvent> logEvents = await _replicationLog.GetAsync(TestNamespace, null, null).ToListAsync();
			Assert.AreEqual(4, logEvents.Count);

			// verify there are no previous snapshots
			Assert.AreEqual(0, (await _replicationLog.GetSnapshotsAsync(TestNamespace).ToListAsync()).Count);

			// create a snapshot
			ReplicationLogFactory replicationLogFactory = ActivatorUtilities.CreateInstance<ReplicationLogFactory>(_server!.Services);
			ReplicationLogSnapshotBuilder snapshotBuilder = ActivatorUtilities.CreateInstance<ReplicationLogSnapshotBuilder>(_server!.Services);
			Assert.IsNotNull(snapshotBuilder);
			BlobId snapshotBlobId = await snapshotBuilder.BuildSnapshotAsync(TestNamespace, SnapshotNamespace);
			Assert.IsTrue(await _blobStore.ExistsAsync(SnapshotNamespace, snapshotBlobId));

			SnapshotInfo? snapshotInfo = await _replicationLog.GetSnapshotsAsync(TestNamespace).FirstAsync();
			Assert.AreEqual(snapshotBlobId, snapshotInfo.SnapshotBlob);

			// make sure the snapshot is returned by the rest api
			{
				HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/replication-log/snapshots/{TestNamespace}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, MediaTypeNames.Application.Json);

				ReplicationLogSnapshots? snapshots = await result.Content.ReadFromJsonAsync<ReplicationLogSnapshots>();
				Assert.IsNotNull(snapshots);
				Assert.AreEqual(1, snapshots.Snapshots.Count);

				SnapshotInfo foundSnapshot = snapshots.Snapshots[0];

				Assert.AreEqual(snapshotBlobId, foundSnapshot.SnapshotBlob);
				BlobContents blobContents = await _blobStore.GetObjectAsync(SnapshotNamespace, snapshotBlobId);
				ReplicationLogSnapshot snapshot = replicationLogFactory.DeserializeSnapshotFromStream(blobContents.Stream);

				Assert.AreEqual(lastEventBucket, snapshot.LastBucket);
				Assert.AreEqual(lastEventId, snapshot.LastEvent);
			}
		}

		// builds a snapshot and make sure we can resume iterating after it
		[TestMethod]
		public async Task ReplicationLogSnapshotResumeAsync()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			DateTime oldestTimestamp = DateTime.Now.AddDays(-1);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("firstObject"), objectHash, oldestTimestamp);
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("secondObject"), objectHash, oldestTimestamp.AddHours(1));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("thirdObject"), objectHash, oldestTimestamp.AddHours(1.5));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("fourthObject"), objectHash, oldestTimestamp.AddDays(0.9));

			// verify the objects were added
			List<ReplicationLogEvent> logEvents = await _replicationLog.GetAsync(TestNamespace, null, null).ToListAsync();
			Assert.AreEqual(4, logEvents.Count);

			// verify there are no previous snapshots
			Assert.AreEqual(0, (await _replicationLog.GetSnapshotsAsync(TestNamespace).ToListAsync()).Count);

			// create a snapshot
			ReplicationLogFactory replicationLogFactory = ActivatorUtilities.CreateInstance<ReplicationLogFactory>(_server!.Services);
			ReplicationLogSnapshotBuilder snapshotBuilder = ActivatorUtilities.CreateInstance<ReplicationLogSnapshotBuilder>(_server!.Services);
			Assert.IsNotNull(snapshotBuilder);
			BlobId snapshotBlobId = await snapshotBuilder.BuildSnapshotAsync(TestNamespace, SnapshotNamespace);
			Assert.IsTrue(await _blobStore.ExistsAsync(SnapshotNamespace, snapshotBlobId));

			SnapshotInfo? snapshotInfo = await _replicationLog.GetSnapshotsAsync(TestNamespace).FirstAsync();
			Assert.AreEqual(snapshotBlobId, snapshotInfo.SnapshotBlob);

			BlobContents blobContents = await _blobStore.GetObjectAsync(SnapshotNamespace, snapshotBlobId);
			ReplicationLogSnapshot snapshot = replicationLogFactory.DeserializeSnapshotFromStream(blobContents.Stream);

			// insert more events
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("fifthObject"), objectHash, oldestTimestamp.AddDays(0.91));
			await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName("sixthObject"), objectHash, oldestTimestamp.AddDays(0.92));

			// verify the new events can be found when resuming from the snapshot
			{
				HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}?lastBucket={snapshot.LastBucket}&lastEvent={snapshot.LastEvent}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				
				Assert.AreEqual(result!.Content.Headers.ContentType!.MediaType, MediaTypeNames.Application.Json);

				ReplicationLogEvents? events = await result.Content.ReadFromJsonAsync<ReplicationLogEvents>(JsonTestUtils.DefaultJsonSerializerSettings);
				Assert.IsNotNull(events);
				Assert.AreEqual(2, events!.Events.Count);

				// parse the events returned, make sure they are in the right order
				{
					ReplicationLogEvent e = events.Events[0];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("fifthObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}

				{
					ReplicationLogEvent e = events.Events[1];
					Assert.AreEqual(TestNamespace, e.Namespace);
					Assert.AreEqual(TestBucket, e.Bucket);
					Assert.AreEqual(RefId.FromName("sixthObject"), e.Key);
					Assert.AreEqual(objectHash, e.Blob);
				}
			}
		}

		[TestMethod]
		public async Task ReplicationLogSnapshotCleanupAsync()
		{
			const int maxCountOfSnapshots = 10;
			int countOfSnapshotsToCreate = maxCountOfSnapshots + 2;

			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", "thisIsAField");
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			List<BlobId> createdSnapshots = new List<BlobId>();
			for (int i = 0; i < countOfSnapshotsToCreate ; i++)
			{
				await _replicationLog.InsertAddEventAsync(TestNamespace, TestBucket, RefId.FromName($"object {i}"), objectHash);

				ReplicationLogSnapshotBuilder snapshotBuilder = ActivatorUtilities.CreateInstance<ReplicationLogSnapshotBuilder>(_server!.Services);
				Assert.IsNotNull(snapshotBuilder);
				BlobId snapshotBlobId = await snapshotBuilder.BuildSnapshotAsync(TestNamespace, SnapshotNamespace);
				Assert.IsTrue(await _blobStore.ExistsAsync(SnapshotNamespace, snapshotBlobId));
				createdSnapshots.Add(snapshotBlobId);
			}

			List<BlobId> snapshots = await _replicationLog.GetSnapshotsAsync(TestNamespace).Select(info => info.SnapshotBlob).ToListAsync();
			// snapshots are returned newest first so we inverse this order
			snapshots.Reverse();

			// verify we hit the max number of snapshots
			Assert.AreEqual(maxCountOfSnapshots, snapshots.Count);

			// the first two snapshots should have been removed
			createdSnapshots.RemoveAt(0);
			createdSnapshots.RemoveAt(0);

			CollectionAssert.AreEqual(createdSnapshots, snapshots);
		}
	}
}
