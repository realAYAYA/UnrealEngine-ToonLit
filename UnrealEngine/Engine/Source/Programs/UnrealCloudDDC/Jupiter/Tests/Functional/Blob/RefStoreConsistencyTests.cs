// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Text;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Logger = Serilog.Core.Logger;

namespace Jupiter.FunctionalTests.References
{
	public abstract class RefStoreConsistencyTests
	{
		protected RefStoreConsistencyTests(string namespaceSuffix)
		{
			_testNamespaceName = new NamespaceId($"ref-consistency-{namespaceSuffix}");
		}

		private TestServer? _server;
		private readonly NamespaceId _testNamespaceName;

		private readonly BucketId DefaultBucket = new BucketId("default");

		private static readonly byte[] s_objectContents0 = Encoding.ASCII.GetBytes("blob_00");
		private static readonly byte[] s_objectContents1 = Encoding.ASCII.GetBytes("blob_11");
		private static readonly byte[] s_objectContents2 = Encoding.ASCII.GetBytes("blob_22");
		private static readonly byte[] s_objectContents3 = Encoding.ASCII.GetBytes("blob_33");
		private static readonly byte[] s_objectContents4 = Encoding.ASCII.GetBytes("blob_44");
		private static readonly byte[] s_objectContents5 = Encoding.ASCII.GetBytes("blob_55");
		private static readonly byte[] s_objectContents6 = Encoding.ASCII.GetBytes("blob_66");

		private readonly BlobId object0id = BlobId.FromBlob(s_objectContents0);
		private readonly BlobId object1id = BlobId.FromBlob(s_objectContents1);
		private readonly BlobId object2id = BlobId.FromBlob(s_objectContents2);
		private readonly BlobId object3id = BlobId.FromBlob(s_objectContents3);
		private readonly BlobId object4id = BlobId.FromBlob(s_objectContents4);
		private readonly BlobId object5id = BlobId.FromBlob(s_objectContents5);
		private readonly BlobId object6id = BlobId.FromBlob(s_objectContents6);

		private readonly RefId object0Name = RefId.FromName("object_ref_consistency_0");
		private readonly RefId object1Name = RefId.FromName("object_ref_consistency_1");
		private readonly RefId object2Name = RefId.FromName("object_ref_consistency_2");
		private readonly RefId object3Name = RefId.FromName("object_ref_consistency_3");
		private readonly RefId object4Name = RefId.FromName("object_ref_consistency_4");
		private readonly RefId object5Name = RefId.FromName("object_ref_consistency_5");
		private readonly RefId object6Name = RefId.FromName("object_ref_consistency_6");

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
			_server = server;

			// Seed storage
			await Seed(_server.Services);

			IBlobService blobService = _server.Services.GetService<IBlobService>()!;
			await blobService.PutObjectAsync(_testNamespaceName, s_objectContents0, object0id);
			await blobService.PutObjectAsync(_testNamespaceName, s_objectContents1, object1id);
			await blobService.PutObjectAsync(_testNamespaceName, s_objectContents2, object2id);
			await blobService.PutObjectAsync(_testNamespaceName, s_objectContents3, object3id);
			await blobService.PutObjectAsync(_testNamespaceName, s_objectContents4, object4id);
			await blobService.PutObjectAsync(_testNamespaceName, s_objectContents5, object5id);
			await blobService.PutObjectAsync(_testNamespaceName, s_objectContents6, object6id);

			IRefService? refService = _server.Services.GetService<IRefService>()!;
			Assert.IsNotNull(refService);
			(BlobId ob0_hash, CbObject ob0_cb) = GetCBWithAttachment(object0id);
			await refService.PutAsync(_testNamespaceName, DefaultBucket, object0Name, ob0_hash, ob0_cb);

			(BlobId ob1_hash, CbObject ob1_cb) = GetCBWithAttachment(object1id);
			await refService.PutAsync(_testNamespaceName, DefaultBucket, object1Name, ob1_hash, ob1_cb);

			(BlobId ob2_hash, CbObject ob2_cb) = GetCBWithAttachment(object2id);
			await refService.PutAsync(_testNamespaceName, DefaultBucket, object2Name, ob2_hash, ob2_cb);

			(BlobId ob3_hash, CbObject ob3_cb) = GetCBWithAttachment(object3id);
			await refService.PutAsync(_testNamespaceName, DefaultBucket, object3Name, ob3_hash, ob3_cb);

			(BlobId ob4_hash, CbObject ob4_cb) = GetCBWithAttachment(object4id);
			await refService.PutAsync(_testNamespaceName, DefaultBucket, object4Name, ob4_hash, ob4_cb);

			(BlobId ob5_hash, CbObject ob5_cb) = GetCBWithAttachment(object5id);
			await refService.PutAsync(_testNamespaceName, DefaultBucket, object5Name, ob5_hash, ob5_cb);

			(BlobId ob6_hash, CbObject ob6_cb) = GetCBWithAttachment(object6id);
			await refService.PutAsync(_testNamespaceName, DefaultBucket, object6Name, ob6_hash, ob6_cb);

			IReferencesStore referenceStore = _server.Services.GetService<IReferencesStore>()!;
			DateTime oldTimestamp = DateTime.Now.AddDays(-30);
			DateTime newTimestamp = DateTime.Now;
			await referenceStore.UpdateLastAccessTimeAsync(_testNamespaceName, DefaultBucket, object0Name, oldTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(_testNamespaceName, DefaultBucket, object1Name, newTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(_testNamespaceName, DefaultBucket, object2Name, oldTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(_testNamespaceName, DefaultBucket, object3Name, oldTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(_testNamespaceName, DefaultBucket, object4Name, newTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(_testNamespaceName, DefaultBucket, object5Name, newTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(_testNamespaceName, DefaultBucket, object6Name, oldTimestamp);
		}

		protected abstract IEnumerable<KeyValuePair<string, string?>> GetSettings();

		protected abstract Task Seed(IServiceProvider serverServices);
		protected abstract Task Teardown(IServiceProvider serverServices);

		[TestCleanup]
		public async Task MyTeardownAsync()
		{
			await Teardown(_server!.Services);
		}

		[TestMethod]
		public async Task RunRefConsistencyTestsAsync()
		{
			RefStoreConsistencyCheckService? consistencyCheckService = _server!.Services.GetService<RefStoreConsistencyCheckService>();
			Assert.IsNotNull(consistencyCheckService);

			// manually invoke the consistency check
			await consistencyCheckService.RunConsistencyCheckAsync();
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
	}

	[TestClass]
	public class MemoryRefStoreTests : RefStoreConsistencyTests
	{
		public MemoryRefStoreTests() : base("memory")
		{
		}

		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", UnrealCloudDDCSettings.BlobIndexImplementations.Memory.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementation", UnrealCloudDDCSettings.ReferencesDbImplementations.Memory.ToString()),
			};
		}

		protected override Task Seed(IServiceProvider serverServices)
		{
			return Task.CompletedTask;
		}

		protected override Task Teardown(IServiceProvider serverServices)
		{
			return Task.CompletedTask;
		}
	}

	[TestClass]
	[DoNotParallelize]
	public class ScyllaRefStoreTests : RefStoreConsistencyTests
	{
		public ScyllaRefStoreTests() : base("scylla")
		{
		}

		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", UnrealCloudDDCSettings.BlobIndexImplementations.Scylla.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementation", UnrealCloudDDCSettings.ReferencesDbImplementations.Scylla.ToString()),

			};
		}

		protected override Task Seed(IServiceProvider serverServices)
		{
			return Task.CompletedTask;
		}

		protected override Task Teardown(IServiceProvider serverServices)
		{
			return Task.CompletedTask;
		}
	}
}
