// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text;
using System.Threading.Tasks;
using Jupiter.Controllers;
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
using Jupiter.Common;

namespace Jupiter.FunctionalTests.GC
{
	/* This test fails intermittently on the farm but we are unable to reproduce locally, disabled for now for reliability
	[TestClass]
	[DoNotParallelize]
	public class MemoryGCReferencesTests : GCReferencesTests
	{
		protected override NamespaceId TestNamespace { get; } = new NamespaceId("test-namespace-gcref");
		protected override string GetImplementation()
		{
			return "Memory";
		}
	}*/

	[TestClass]
	[DoNotParallelize]
	public class ScyllaGCReferencesTests : GCReferencesTests
	{
		protected override NamespaceId TestNamespace { get; } = new NamespaceId("test-namespace-gcref");

		protected override string GetImplementation()
		{
			return "Scylla";
		}
	}

	
	[TestClass]
	[DoNotParallelize]
	public class ScyllaPerShardGCReferencesTests : GCReferencesTests
	{
		protected override string GetImplementation()
		{
			return "Scylla";
		}

		protected override NamespaceId TestNamespace { get; } = new NamespaceId("test-namespace-gcref-sharded");

		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			List<KeyValuePair<string, string?>> baseSettings = base.GetSettings().ToList();
			return baseSettings.Concat(new List<KeyValuePair<string, string?>>()
			{
				new KeyValuePair<string, string?>("Scylla:UsePerShardScanning", "true"),
				new KeyValuePair<string, string?>("Scylla:CountOfCoresPerNode", "2"),
				new KeyValuePair<string, string?>("Scylla:CountOfNodes", "1"),
			});
		}
	}

	public abstract class GCReferencesTests : IDisposable
	{
		private HttpClient? _httpClient;

		protected abstract NamespaceId TestNamespace { get; }
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
			await refService.PutAsync(TestNamespace, DefaultBucket, object0Name, ob0_hash, ob0_cb);
		   
			(BlobId ob1_hash, CbObject ob1_cb) = GetCBWithAttachment(object1id);
			await refService.PutAsync(TestNamespace, DefaultBucket, object1Name, ob1_hash, ob1_cb);

			(BlobId ob2_hash, CbObject ob2_cb) = GetCBWithAttachment(object2id);
			await refService.PutAsync(TestNamespace, DefaultBucket, object2Name, ob2_hash, ob2_cb);

			(BlobId ob3_hash, CbObject ob3_cb) = GetCBWithAttachment(object3id);
			await refService.PutAsync(TestNamespace, DefaultBucket, object3Name, ob3_hash, ob3_cb);

			(BlobId ob4_hash, CbObject ob4_cb) = GetCBWithAttachment(object4id);
			await refService.PutAsync(TestNamespace, DefaultBucket, object4Name, ob4_hash, ob4_cb);

			(BlobId ob5_hash, CbObject ob5_cb) = GetCBWithAttachment(object5id);
			await refService.PutAsync(TestNamespace, DefaultBucket, object5Name, ob5_hash, ob5_cb);

			(BlobId ob6_hash, CbObject ob6_cb) = GetCBWithAttachment(object6id);
			await refService.PutAsync(TestNamespace, DefaultBucket, object6Name, ob6_hash, ob6_cb);

			IReferencesStore referenceStore = _server.Services.GetService<IReferencesStore>()!;
			DateTime oldTimestamp = DateTime.Now.AddDays(-30);
			DateTime newTimestamp = DateTime.Now;
			await referenceStore.UpdateLastAccessTimeAsync(TestNamespace, DefaultBucket, object0Name, oldTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(TestNamespace, DefaultBucket, object1Name, newTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(TestNamespace, DefaultBucket, object2Name, oldTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(TestNamespace, DefaultBucket, object3Name, oldTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(TestNamespace, DefaultBucket, object4Name, newTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(TestNamespace, DefaultBucket, object5Name, newTimestamp);
			await referenceStore.UpdateLastAccessTimeAsync(TestNamespace, DefaultBucket, object6Name, oldTimestamp);
		}

		protected virtual IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new List<KeyValuePair<string, string?>>()
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", "Memory"),
				new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementation", GetImplementation()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", GetImplementation()),
				new KeyValuePair<string, string?>($"Namespaces:Policies:{TestNamespace}:GCMethod", NamespacePolicy.StoragePoolGCMethod.LastAccess.ToString()),
				new KeyValuePair<string, string?>("GC:DefaultGCPolicy", NamespacePolicy.StoragePoolGCMethod.None.ToString())
			};
		}

		protected abstract string GetImplementation();

		[TestMethod]
		public async Task RunRefCleanupAsync()
		{
			// trigger the cleanup
			using StringContent content = new StringContent(string.Empty);
			HttpResponseMessage cleanupResponse = await _httpClient!.PostAsync(new Uri($"api/v1/admin/refCleanup", UriKind.Relative), content);
			cleanupResponse.EnsureSuccessStatusCode();
			RemovedRefRecordsResponse? removedRefRecords = await cleanupResponse.Content.ReadFromJsonAsync<RemovedRefRecordsResponse>();
			Assert.IsNotNull(removedRefRecords);
			Assert.AreEqual(4, removedRefRecords.CountOfRemovedRecords);

			IRefService refService = _server!.Services.GetService<IRefService>()!;
			string testName = GetType().Name;
			// some object should have been deleted while others remain
			Assert.IsFalse(await refService.ExistsAsync(TestNamespace, DefaultBucket, object0Name), $"{object0Name} (\"object0Name\", {testName}) should have been deleted");
			Assert.IsTrue(await refService.ExistsAsync(TestNamespace, DefaultBucket, object1Name), $"{object1Name} (\"object1Name\", {testName}) should still be found");
			Assert.IsFalse(await refService.ExistsAsync(TestNamespace, DefaultBucket, object2Name), $"{object2Name} (\"object2Name\", {testName}) should have been deleted");
			Assert.IsFalse(await refService.ExistsAsync(TestNamespace, DefaultBucket, object3Name), $"{object3Name} (\"object3Name\", {testName}) should have been deleted");
			Assert.IsTrue(await refService.ExistsAsync(TestNamespace, DefaultBucket, object4Name), $"{object4Name} (\"object4Name\", {testName}) should still be found");
			Assert.IsTrue(await refService.ExistsAsync(TestNamespace, DefaultBucket, object5Name), $"{object5Name} (\"object5Name\", {testName}) should still be found");
			Assert.IsFalse(await refService.ExistsAsync(TestNamespace, DefaultBucket, object6Name), $"{object6Name} (\"object6Name\", {testName}) should have been deleted");
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
