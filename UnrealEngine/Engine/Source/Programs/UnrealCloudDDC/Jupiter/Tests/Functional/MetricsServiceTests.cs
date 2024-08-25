// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using Jupiter.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Serilog.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Implementation.Blob;

namespace Jupiter.FunctionalTests.Metrics
{
	[TestClass]
	[DoNotParallelize]
	public class ScyllaMetricsServiceTests : MetricsServiceTests
	{
		protected override NamespaceId TestNamespace { get; } = new NamespaceId("test-namespace-metrics");

		protected override string GetImplementation()
		{
			return "Scylla";
		}
	}

	[TestClass]
	[DoNotParallelize]
	public class MongoMetricsServiceTests : MetricsServiceTests
	{
		protected override NamespaceId TestNamespace { get; } = new NamespaceId("test-namespace-metrics");

		protected override string GetImplementation()
		{
			return "Mongo";
		}
	}
	
	public abstract class MetricsServiceTests : IDisposable
	{
		private HttpClient? _httpClient;

		protected abstract NamespaceId TestNamespace { get; }
		private readonly BucketId Bucket0 = new BucketId("bucket0");
		private readonly BucketId Bucket1 = new BucketId("bucket1");

		private static readonly byte[] s_objectContents0 = Encoding.ASCII.GetBytes("I am a small string");
		private static readonly byte[] s_objectContents1 = Encoding.ASCII.GetBytes("Blob");
		private static readonly byte[] s_objectContents2 = Encoding.ASCII.GetBytes("This is a much larger string then the others");
		private static readonly byte[] s_objectContents3 = Encoding.ASCII.GetBytes("FooBar");
		private static readonly byte[] s_objectContents4 = Encoding.ASCII.GetBytes("Medium sized string contents");
		private static readonly byte[] s_objectContents5 = Encoding.ASCII.GetBytes("Baz");
		private static readonly byte[] s_objectContents6 = Encoding.ASCII.GetBytes("A");

		private readonly BlobId object0id = BlobId.FromBlob(s_objectContents0);
		private readonly BlobId object1id = BlobId.FromBlob(s_objectContents1);
		private readonly BlobId object2id = BlobId.FromBlob(s_objectContents2);
		private readonly BlobId object3id = BlobId.FromBlob(s_objectContents3);
		private readonly BlobId object4id = BlobId.FromBlob(s_objectContents4);
		private readonly BlobId object5id = BlobId.FromBlob(s_objectContents5);
		private readonly BlobId object6id = BlobId.FromBlob(s_objectContents6);

		private readonly RefId object0Name = RefId.FromName("object0");
		private readonly RefId object1Name = RefId.FromName("object1");
		private readonly RefId object2Name = RefId.FromName("object2");
		private readonly RefId object3Name = RefId.FromName("object3");
		private readonly RefId object4Name = RefId.FromName("object4");
		private readonly RefId object5Name = RefId.FromName("object5");
		private readonly RefId object6Name = RefId.FromName("object6");
		private TestServer? _server;

		[TestInitialize]
		public async Task SetupAsync()
		{
			IConfigurationRoot configuration = new ConfigurationBuilder()
				// we are not reading the base appSettings here as we want exact control over what runs in the tests
				.AddJsonFile("appsettings.Testing.json", false)
				.AddEnvironmentVariables()
				.AddInMemoryCollection(GetSettings())
				.Build();
			Logger logger = new LoggerConfiguration()
				.ReadFrom.Configuration(configuration)
				.CreateLogger();

			_server = new TestServer(new WebHostBuilder()
				.UseConfiguration(configuration)
				.UseEnvironment("Testing")
				.ConfigureServices(collection => collection.AddSerilog(logger))
				.UseStartup<JupiterStartup>()
			);
			_httpClient = _server.CreateClient();

			IBlobService blobService = _server.Services.GetService<IBlobService>()!;
			await blobService.PutObjectAsync(TestNamespace, s_objectContents0, object0id);
			await blobService.PutObjectAsync(TestNamespace, s_objectContents1, object1id);
			await blobService.PutObjectAsync(TestNamespace, s_objectContents2, object2id);
			await blobService.PutObjectAsync(TestNamespace, s_objectContents3, object3id);
			await blobService.PutObjectAsync(TestNamespace, s_objectContents4, object4id);
			await blobService.PutObjectAsync(TestNamespace, s_objectContents5, object5id);
			await blobService.PutObjectAsync(TestNamespace, s_objectContents6, object6id);

			IRefService? refService = _server.Services.GetService<IRefService>()!;
			Assert.IsNotNull(refService);
			(BlobId ob0_hash, CbObject ob0_cb) = GetCBWithAttachment(object0id);
			await refService.PutAsync(TestNamespace, Bucket0, object0Name, ob0_hash, ob0_cb);
		   
			(BlobId ob1_hash, CbObject ob1_cb) = GetCBWithAttachment(object1id);
			await refService.PutAsync(TestNamespace, Bucket1, object1Name, ob1_hash, ob1_cb);

			(BlobId ob2_hash, CbObject ob2_cb) = GetCBWithAttachment(object2id);
			await refService.PutAsync(TestNamespace, Bucket0, object2Name, ob2_hash, ob2_cb);

			(BlobId ob3_hash, CbObject ob3_cb) = GetCBWithAttachment(object3id);
			await refService.PutAsync(TestNamespace, Bucket1, object3Name, ob3_hash, ob3_cb);

			(BlobId ob4_hash, CbObject ob4_cb) = GetCBWithAttachment(object4id);
			await refService.PutAsync(TestNamespace, Bucket0, object4Name, ob4_hash, ob4_cb);

			(BlobId ob5_hash, CbObject ob5_cb) = GetCBWithAttachment(object5id);
			await refService.PutAsync(TestNamespace, Bucket0, object5Name, ob5_hash, ob5_cb);

			(BlobId ob6_hash, CbObject ob6_cb) = GetCBWithAttachment(object6id);
			await refService.PutAsync(TestNamespace, Bucket0, object6Name, ob6_hash, ob6_cb);
		}

		protected virtual IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new List<KeyValuePair<string, string?>>()
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", "Memory"),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementation", GetImplementation()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", GetImplementation()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:EnableBucketStatsTracking", "true"),
			};
		}

		protected abstract string GetImplementation();

		[TestMethod]
		public async Task RunMetricServiceAsync()
		{
			MetricsCalculator calculator = ActivatorUtilities.CreateInstance<MetricsCalculator>(_server!.Services);
			BucketStats? stats0 = await calculator.CalculateStatsForBucketAsync(TestNamespace, Bucket0);
			BucketStats? stats1 = await calculator.CalculateStatsForBucketAsync(TestNamespace, Bucket1);

			Assert.IsNotNull(stats0);
			Assert.IsNotNull(stats1);
			Assert.AreEqual(Bucket0, stats0.Bucket);
			Assert.AreEqual(Bucket1, stats1.Bucket);

			Assert.AreEqual(5, stats0.CountOfRefs);
			Assert.AreEqual(2, stats1.CountOfRefs);

			Assert.AreEqual(10, stats0.CountOfBlobs);
			Assert.AreEqual(4, stats1.CountOfBlobs);

			Assert.AreEqual(265, stats0.TotalSize);
			Assert.AreEqual(1, stats0.SmallestBlobFound);
			Assert.AreEqual(44, stats0.LargestBlob);
			Assert.AreEqual(26.5, stats0.AvgSize);

			Assert.AreEqual(78, stats1.TotalSize);
			Assert.AreEqual(4, stats1.SmallestBlobFound);
			Assert.AreEqual(34, stats1.LargestBlob);
			Assert.AreEqual(19.5, stats1.AvgSize);
		}

		[TestMethod]
		public async Task GetBucketsTestAsync()
		{
			IReferencesStore referenceStore = (IReferencesStore)_server!.Services.GetService(typeof(IReferencesStore))!;

			IAsyncEnumerable<BucketId> buckets = referenceStore.GetBuckets(TestNamespace);
			List<BucketId> _ = await buckets.ToListAsync();
		}

		private static (BlobId, CbObject) GetCBWithAttachment(BlobId blobIdentifier)
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteBinaryAttachment("Attachment", blobIdentifier.AsIoHash());
			writer.EndObject();

			byte[] b = writer.ToByteArray();
			return (BlobId.FromBlob(b), new CbObject(b));
		}

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
				_httpClient?.Dispose();
				_server?.Dispose();
			}
		}

		public void Dispose()
		{
			Dispose(true);
			System.GC.SuppressFinalize(this);
		}
	}
}
