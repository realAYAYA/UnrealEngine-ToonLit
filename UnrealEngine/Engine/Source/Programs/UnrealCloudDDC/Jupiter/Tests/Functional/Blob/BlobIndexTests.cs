// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Mime;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Implementation;
using Jupiter.Implementation.Blob;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Logger = Serilog.Core.Logger;
using EpicGames.AspNet;

namespace Jupiter.FunctionalTests.Storage
{
	public abstract class BlobIndexTests
	{
		protected BlobIndexTests(string namespaceSuffix)
		{
			_testNamespaceName = new NamespaceId($"test-blobindex-{namespaceSuffix}");
			_testNamespaceListName = new NamespaceId($"test-blobindex-list-{namespaceSuffix}");
		}
		private TestServer? _server;
		private HttpClient? _httpClient;
		private readonly NamespaceId _testNamespaceName;
		private readonly NamespaceId _testNamespaceListName;

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

			// Seed storage
			await Seed(_server.Services);
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
		public async Task PutBlobToIndexAsync()
		{
			byte[] payload = Encoding.ASCII.GetBytes("I am a blob with contents that will be uploaded");
			using ByteArrayContent requestContent = new ByteArrayContent(payload);
			requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
			BlobId contentHash = BlobId.FromBlob(payload);
			HttpResponseMessage response = await _httpClient!.PutAsync(new Uri($"api/v1/s/{_testNamespaceName}/{contentHash}", UriKind.Relative), requestContent);
			response.EnsureSuccessStatusCode();

			IBlobIndex? index = _server!.Services.GetService<IBlobIndex>();
			Assert.IsNotNull(index);
			List<string> regions = await index.GetBlobRegionsAsync(_testNamespaceName, contentHash);

			Assert.IsTrue(regions.Contains("test"));
		}

		[TestMethod]
		public async Task UploadRefAsync()
		{
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", nameof(UploadRefAsync));
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			using HttpContent requestContent = new ByteArrayContent(objectData);
			requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
			requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());
			RefId putKey = RefId.FromName("newReferenceUploadObject");
			HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/refs/{_testNamespaceName}/bucket/{putKey}.uecb", UriKind.Relative), requestContent);
			result.EnsureSuccessStatusCode();

			IBlobIndex? index = _server!.Services.GetService<IBlobIndex>();
			Assert.IsNotNull(index);
			Assert.IsTrue(await index.BlobExistsInRegionAsync(_testNamespaceName, objectHash, "test"));

			IAsyncEnumerable<BaseBlobReference> blobReferences = index.GetBlobReferencesAsync(_testNamespaceName, objectHash);
			List<BaseBlobReference> references = await blobReferences.ToListAsync();
			Assert.AreEqual(1, references.Count);

			Assert.IsTrue(references[0] is RefBlobReference);
			RefBlobReference refBlob = (RefBlobReference)references[0];
			Assert.AreEqual("bucket", refBlob.Bucket.ToString());
			Assert.AreEqual(putKey, refBlob.Key);
		}

		[TestMethod]
		public async Task DeleteBlobAsync()
		{
			// upload a blob
			byte[] payload = Encoding.ASCII.GetBytes("I am a blob with contents that will be deleted");
			BlobId contentHash = BlobId.FromBlob(payload);

			{
				using ByteArrayContent requestContent = new ByteArrayContent(payload);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
				HttpResponseMessage response = await _httpClient!.PutAsync(new Uri($"api/v1/s/{_testNamespaceName}/{contentHash}", UriKind.Relative), requestContent);
				response.EnsureSuccessStatusCode();
			}
			// verify its present in the blob index
			IBlobIndex? index = _server!.Services.GetService<IBlobIndex>();
			Assert.IsNotNull(index);
			Assert.IsTrue(await index.BlobExistsInRegionAsync(_testNamespaceName, contentHash));

			// delete the blob
			{
				HttpResponseMessage response = await _httpClient!.DeleteAsync(new Uri($"api/v1/s/{_testNamespaceName}/{contentHash}", UriKind.Relative));
				response.EnsureSuccessStatusCode();
			}

			List<string> regions = await index.GetBlobRegionsAsync(_testNamespaceName, contentHash);

			bool hasRegions = regions.Any();
			// but the blob info will not contain the current region
			Assert.IsTrue(!hasRegions);
		}

		[TestMethod]
		public async Task EnumerateAllBlobsAsync()
		{
			IBlobIndex? index = _server!.Services.GetService<IBlobIndex>();
			Assert.IsNotNull(index);

			// upload a blob
			BlobId contentHash;
			{
				byte[] payload = Encoding.ASCII.GetBytes("I am a blob with contents that will be enumerated");
				contentHash = BlobId.FromBlob(payload);

				{
					using ByteArrayContent requestContent = new ByteArrayContent(payload);
					requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
					HttpResponseMessage response = await _httpClient!.PutAsync(new Uri($"api/v1/blobs/{_testNamespaceListName}/{contentHash}", UriKind.Relative), requestContent);
					response.EnsureSuccessStatusCode();
				}
			}

			// upload a compressed blob
			BlobId compressedPayloadIdentifier;
			{
				byte[] texturePayload = await File.ReadAllBytesAsync($"ContentId/Payloads/UncompressedTexture_CAS_dea81b6c3b565bb5089695377c98ce0f1c13b0c3.udd");
				compressedPayloadIdentifier = BlobId.FromBlob(texturePayload);
				BlobId uncompressedPayloadIdentifier = new BlobId("DEA81B6C3B565BB5089695377C98CE0F1C13B0C3");

				using ByteArrayContent content = new ByteArrayContent(texturePayload);
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);
				HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/compressed-blobs/{_testNamespaceListName}/{uncompressedPayloadIdentifier}", UriKind.Relative), content);
				result.EnsureSuccessStatusCode();
			}

			{
				(NamespaceId, BlobId)[] blobInfos =  await index.GetAllBlobsAsync().Where(tuple => tuple.Item1 == _testNamespaceListName).ToArrayAsync();
				Assert.AreEqual(2, blobInfos.Length);

				Assert.IsNotNull(blobInfos.FirstOrDefault(info => info.Item2.Equals(compressedPayloadIdentifier)));
				Assert.IsNotNull(blobInfos.FirstOrDefault(info => info.Item2.Equals(contentHash)));
			}
		}
	}

	[TestClass()]
	public class MemoryBlobIndexTests : BlobIndexTests
	{
		public MemoryBlobIndexTests() : base("memory")
		{
		}
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[] { new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", UnrealCloudDDCSettings.BlobIndexImplementations.Memory.ToString()) };
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
	public class ScyllaBlobIndexTests : BlobIndexTests
	{
		public ScyllaBlobIndexTests() : base("scylla")
		{
		}
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[] { new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", UnrealCloudDDCSettings.BlobIndexImplementations.Scylla.ToString()) };
		}

		protected override Task Seed(IServiceProvider serverServices)
		{
			return Task.CompletedTask;
		}

		protected override async Task Teardown(IServiceProvider provider)
		{
			await Task.CompletedTask;
		}
	}

	[TestClass]
	[DoNotParallelize]
	public class CassandraBlobIndexTests : BlobIndexTests
	{
		public CassandraBlobIndexTests() : base("cassandra")
		{
		}

		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", UnrealCloudDDCSettings.BlobIndexImplementations.Scylla.ToString()),
				new KeyValuePair<string, string?>("Scylla:ConnectionString", "Contact Points=localhost,scylla;Default Keyspace=jupiter_cassandra"),
				new KeyValuePair<string, string?>("Scylla:UseAzureCosmosDB", "true"),
				new KeyValuePair<string, string?>("Scylla:UseSSL", "false"),
			};
		}

		protected override Task Seed(IServiceProvider provider)
		{
			IScyllaSessionManager scyllaSessionManager = provider.GetService<IScyllaSessionManager>()!;

			Assert.IsTrue(scyllaSessionManager.IsCassandra);
			return Task.CompletedTask;
		}

		protected override async Task Teardown(IServiceProvider provider)
		{
			await Task.CompletedTask;
		}
	}

	[TestClass]
	public class MongoBlobIndexTests : BlobIndexTests
	{
		public MongoBlobIndexTests() : base("mongo")
		{
		}

		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[] { new KeyValuePair<string, string?>("UnrealCloudDDC:BlobIndexImplementation", UnrealCloudDDCSettings.BlobIndexImplementations.Mongo.ToString()) };
		}

		protected override async Task Seed(IServiceProvider provider)
		{
			IBlobIndex blobIndex = provider.GetService<IBlobIndex>()!;
			Assert.IsTrue(blobIndex is MongoBlobIndex);

			await Task.CompletedTask;
		}

		protected override async Task Teardown(IServiceProvider provider)
		{
			await Task.CompletedTask;
		}
	}
}
