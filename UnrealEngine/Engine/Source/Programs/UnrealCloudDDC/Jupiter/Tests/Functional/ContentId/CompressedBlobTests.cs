// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Mime;
using System.Text;
using System.Threading.Tasks;
using Blake3;
using Jupiter.FunctionalTests.Storage;
using Jupiter.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Serilog.Core;
using EpicGames.AspNet;
using EpicGames.Core;

namespace Jupiter.FunctionalTests.CompressedBlobs
{
	[TestClass]
	[DoNotParallelize]
	public class ScyllaCompressedBlobTests : CompressedBlobTests
	{
		public ScyllaCompressedBlobTests() : base("scylla")
		{
		}

		protected override IEnumerable<KeyValuePair<string, string?>> GetSettingsAsync()
		{
			// Use the S3 storage backend so we can handle large blobs
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:ContentIdStoreImplementation", UnrealCloudDDCSettings.ContentIdStoreImplementations.Scylla.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString()),
				new KeyValuePair<string, string?>("CacheContentId:Enabled", "false"),
				new KeyValuePair<string, string?>("S3:BucketName", $"tests-{TestNamespace}")
			};
		}

		protected override async Task SeedAsync(IServiceProvider provider)
		{
			await Task.CompletedTask;
		}

		protected override async Task TeardownAsync()
		{
			await Task.CompletedTask;
		}
	}

	[TestClass]
	public class MongoCompressedBlobTests : CompressedBlobTests
	{
		public MongoCompressedBlobTests() : base("mongo")
		{
		}

		protected override IEnumerable<KeyValuePair<string, string?>> GetSettingsAsync()
		{
			// Use the S3 storage backend so we can handle large blobs
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:ContentIdStoreImplementation", UnrealCloudDDCSettings.ContentIdStoreImplementations.Mongo.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString()),
				new KeyValuePair<string, string?>("CacheContentId:Enabled", "false"),
				new KeyValuePair<string, string?>("S3:BucketName", $"tests-{TestNamespace}")
			};
		}

		protected override async Task SeedAsync(IServiceProvider provider)
		{
			await Task.CompletedTask;
		}

		protected override async Task TeardownAsync()
		{
			await Task.CompletedTask;
		}
	}

	public abstract class CompressedBlobTests
	{
		protected CompressedBlobTests(string namespaceSuffix)
		{
			TestNamespace = $"test-namespace-{namespaceSuffix}";
		}

		private TestServer? Server { get; set; }
		private HttpClient? Client { get; set; }

		protected string TestNamespace { get; init; }

		[TestInitialize]
		public async Task SetupAsync()
		{
			IConfigurationRoot configuration = new ConfigurationBuilder()
				// we are not reading the base appSettings here as we want exact control over what runs in the tests
				.AddJsonFile("appsettings.Testing.json", true)
				.AddInMemoryCollection(GetSettingsAsync())
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
			Client = server.CreateClient();
			Server = server;

			// Seed storage
			await SeedAsync(Server.Services);
		}

		protected abstract IEnumerable<KeyValuePair<string, string?>> GetSettingsAsync();
		protected abstract Task SeedAsync(IServiceProvider serverServices);
		protected abstract Task TeardownAsync();

		[TestCleanup]
		public async Task MyTeardownAsync()
		{
			await TeardownAsync();
		}

		[TestMethod]
		[DataTestMethod]
		[DataRow("7835c353d7dc67e8a0531c88fbc75ddfda10dee4", "7835c353d7dc67e8a0531c88fbc75ddfda10dee4")]
		[DataRow("4958689fe783e02fb35b13c14b0c3d7beb91e50c", "4958689fe783e02fb35b13c14b0c3d7beb91e50c")]
		[DataRow("dce31eb416f3dcb4c8250ac545eda3930919d3ff", "dce31eb416f3dcb4c8250ac545eda3930919d3ff")]
		[DataRow("05d7c699a2668efdecbe48f10db0d621d736f449.uecomp", "05D7C699A2668EFDECBE48F10DB0D621D736F449")]
		[DataRow("dce31eb416f3dcb4c8250ac545eda3930919d3ff", "dce31eb416f3dcb4c8250ac545eda3930919d3ff")]
		[DataRow("UncompressedTexture_CAS_dea81b6c3b565bb5089695377c98ce0f1c13b0c3.udd", "DEA81B6C3B565BB5089695377C98CE0F1C13B0C3")]
		[DataRow("Oodle-f895ea954b37217270e88d8b728bd3c09152689c", "F895EA954B37217270E88D8B728BD3C09152689C")]
		[DataRow("Oodle-f0b9c675fe21951ca27699f9baab9f9f5040b202", "F0B9C675FE21951CA27699F9BAAB9F9F5040B202")]
		[DataRow("OodleTexture_CAS_dbda9040e75c4674fcec173f982fddf12b021e24.udd", "DBDA9040E75C4674FCEC173F982FDDF12B021E24")]
		public async Task PutPayloadsAsync(string payloadFilename, string uncompressedHash)
		{
			byte[] texturePayload = await File.ReadAllBytesAsync($"ContentId/Payloads/{payloadFilename}");
			BlobId compressedPayloadIdentifier = BlobId.FromBlob(texturePayload);
			BlobId uncompressedPayloadIdentifier = new BlobId(uncompressedHash);

			using ByteArrayContent content = new(texturePayload);
			content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);
			HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{uncompressedPayloadIdentifier}", UriKind.Relative), content);
			if (result.StatusCode == HttpStatusCode.InternalServerError)
			{
				Assert.Fail($"Internal server error with message: {await result.Content.ReadAsStringAsync()}");
			}
			result.EnsureSuccessStatusCode();

			InsertResponse? response = await result.Content.ReadFromJsonAsync<InsertResponse>();
			Assert.IsNotNull(response);
			Assert.AreNotEqual(compressedPayloadIdentifier, response.Identifier);
			Assert.AreEqual(uncompressedPayloadIdentifier, response.Identifier);
		}

		[TestMethod]
		public async Task PutGetComplexTextureAsync()
		{
			byte[] texturePayload = await File.ReadAllBytesAsync("ContentId/Payloads/UncompressedTexture_CAS_dea81b6c3b565bb5089695377c98ce0f1c13b0c3.udd");
			BlobId compressedPayloadIdentifier = BlobId.FromBlob(texturePayload);
			ContentId uncompressedPayloadIdentifier = new ContentId("DEA81B6C3B565BB5089695377C98CE0F1C13B0C3");

			{
				using ByteArrayContent content = new(texturePayload);
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);
				HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{uncompressedPayloadIdentifier}", UriKind.Relative), content);
				result.EnsureSuccessStatusCode();

				InsertResponse? response = await result.Content.ReadFromJsonAsync<InsertResponse>();
				Assert.IsNotNull(response);
				Assert.IsNotNull(response.Identifier);
				Assert.AreNotEqual(compressedPayloadIdentifier, response.Identifier);
				Assert.AreEqual(uncompressedPayloadIdentifier, ContentId.FromBlobIdentifier(response.Identifier));
			}

			{
				HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{uncompressedPayloadIdentifier}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				Assert.AreEqual(CustomMediaTypeNames.UnrealCompressedBuffer, result.Content.Headers.ContentType!.MediaType);

				byte[] blobContent = await result.Content.ReadAsByteArrayAsync();
				CollectionAssert.AreEqual(texturePayload, blobContent);
			}

			{
				// verify the compressed blob can be retrieved in the blob store
				HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/blobs/{TestNamespace}/{compressedPayloadIdentifier}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				Assert.AreEqual(MediaTypeNames.Application.Octet, result.Content.Headers.ContentType!.MediaType);

				byte[] blobContent = await result.Content.ReadAsByteArrayAsync();
				CollectionAssert.AreEqual(texturePayload, blobContent);
			}

			{
				// the uncompressed payload should not be valid in the blob endpoint
				HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/blobs/{TestNamespace}/{uncompressedPayloadIdentifier}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
			}
		}

		/// <summary>
		/// write a large file (bigger then what C# can have in memory)
		/// </summary>

		[TestMethod]
		[TestCategory("SlowTests")]
		public async Task PutGetLargeCompressedPayloadAsync()
		{
			// we submit a blob so large that it can not fit using the memory blob store
			IBlobStore? blobStore = Server?.Services.GetService<IBlobStore>();
			Assert.IsFalse(blobStore is MemoryBlobStore);

			if (blobStore is AzureBlobStore)
			{
				Assert.Inconclusive("Azure blob store gets internal server errors when receiving large blobs");
			}

			FileInfo tempOutputFile = new FileInfo(Path.GetTempFileName());
			FileInfo tempCompressedFile = new FileInfo(Path.GetTempFileName());

			ILogger logger = Server!.Services.GetService<ILogger>()!;

			try
			{
				logger.Information("Generating large file");
				int blockSize = 1024 * 1024;
				// we want a file larger then 6GB, each block is 1 MB
				int countOfBlocks = 6500;
				IoHash uncompressedContentHash;
				{
					byte[] block = new byte[blockSize];
					Random.Shared.NextBytes(block);

					List<byte[]> blocksToCompress = new List<byte[]>();

					using Hasher hasher = Hasher.New();
					for (int i = 0; i < countOfBlocks; i++)
					{
						hasher.UpdateWithJoin(new ReadOnlySpan<byte>(block, 0, blockSize));
						blocksToCompress.Add(block);
					}
					Hash blake3Hash = hasher.Finalize();
					byte[] hash = blake3Hash.AsSpan().Slice(0, 20).ToArray();
					uncompressedContentHash = new IoHash(hash);

					await using FileStream fs = tempCompressedFile.OpenWrite();
					CompressedBufferUtils bufferUtils = Server!.Services.GetService<CompressedBufferUtils>()!;
					bufferUtils.CompressContent(fs, OoodleCompressorMethod.Mermaid, OoodleCompressionLevel.HyperFast4, blocksToCompress, blockSize);
				}

				logger.Information("Hashing generated file");
				BlobId blobIdentifier = BlobId.FromIoHash(uncompressedContentHash);
				BlobId compressedContentHash;
				{
					await using FileStream fs = tempCompressedFile.OpenRead();
					compressedContentHash = await BlobId.FromStreamAsync(fs);
				}

				logger.Information("Uploading large file");

				// it takes a long time to upload this content and we will not get any response while it happens so we have to bump the timeout
				Client!.Timeout = TimeSpan.FromMinutes(5.0);
				{
					await using FileStream fs = tempCompressedFile.OpenRead();
					using StreamContent content = new StreamContent(fs);
					content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);
					HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{blobIdentifier}", UriKind.Relative), content);
					result.EnsureSuccessStatusCode();

					InsertResponse? response = await result.Content.ReadFromJsonAsync<InsertResponse>();
					Assert.IsNotNull(response);
					Assert.AreEqual(blobIdentifier, response.Identifier);
				}

				logger.Information("Large file uploaded");
				logger.Information("Downloading large file");

				{
					// verify we can fetch the blob again
					HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{blobIdentifier}", UriKind.Relative), HttpCompletionOption.ResponseHeadersRead);
					result.EnsureSuccessStatusCode();
					
					{
						await using Stream s = await result.Content.ReadAsStreamAsync();

						// stream this to disk so we have something to look at in case there is an error
						await using FileStream fs = tempOutputFile.OpenWrite();
						await s.CopyToAsync(fs);
					}

					await using FileStream downloadedFile = tempOutputFile.OpenRead();

					BlobId downloadedBlobIdentifier = await BlobId.FromStreamAsync(downloadedFile);
					Assert.AreEqual(compressedContentHash, downloadedBlobIdentifier);
				}

				logger.Information("Download completed");
			}
			finally
			{
				if (tempCompressedFile.Exists)
				{
					tempCompressedFile.Delete();
				}

				if (tempOutputFile.Exists)
				{
					tempOutputFile.Delete();
				}
			}
		}
		[TestMethod]
		public async Task RecompressionTestAsync()
		{
			ContentId uncompressedPayloadIdentifier = new ContentId("A2AC0ECED768698F7413F131D064D36B7EC6F7DA");
			byte[] texturePayloadSmaller = await File.ReadAllBytesAsync("ContentId/Payloads/smallerfile");
			BlobId compressedPayloadIdentifierSmaller = BlobId.FromBlob(texturePayloadSmaller);
		   
			{
				using ByteArrayContent content = new(texturePayloadSmaller);
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);
				HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{uncompressedPayloadIdentifier}", UriKind.Relative), content);
				result.EnsureSuccessStatusCode();

				InsertResponse response = await result.Content.ReadAsAsync<InsertResponse>();
				Assert.IsNotNull(response.Identifier);
				Assert.AreNotEqual(compressedPayloadIdentifierSmaller, response.Identifier);
				Assert.AreEqual(uncompressedPayloadIdentifier, ContentId.FromBlobIdentifier(response.Identifier));
			}

			byte[] texturePayloadLarger = await File.ReadAllBytesAsync("ContentId/Payloads/largerfile");
			BlobId compressedPayloadIdentifierLarger = BlobId.FromBlob(texturePayloadLarger);

			{
				using ByteArrayContent content = new(texturePayloadLarger);
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);
				HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{uncompressedPayloadIdentifier}", UriKind.Relative), content);
				result.EnsureSuccessStatusCode();

				InsertResponse response = await result.Content.ReadAsAsync<InsertResponse>();
				Assert.IsNotNull(response.Identifier);
				Assert.AreNotEqual(compressedPayloadIdentifierLarger, response.Identifier);
				Assert.AreEqual(uncompressedPayloadIdentifier, ContentId.FromBlobIdentifier(response.Identifier));
			}

			{
				// fetch the overloaded content id, it should be returning the smaller payload
				HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{uncompressedPayloadIdentifier}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
				Assert.AreEqual(CustomMediaTypeNames.UnrealCompressedBuffer, result.Content.Headers.ContentType!.MediaType);

				byte[] blobContent = await result.Content.ReadAsByteArrayAsync();
				Assert.AreEqual(compressedPayloadIdentifierSmaller, BlobId.FromBlob(blobContent));
				CollectionAssert.AreEqual(texturePayloadSmaller, blobContent);
			}
		}

		[TestMethod]
		public async Task PutWrongIdentifierAsync()
		{
			byte[] texturePayload = await File.ReadAllBytesAsync("ContentId/Payloads/UncompressedTexture_CAS_dea81b6c3b565bb5089695377c98ce0f1c13b0c3.udd");
			BlobId compressedPayloadIdentifier = BlobId.FromBlob(texturePayload);

			{
				using ByteArrayContent content = new(texturePayload);
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);
				// we purposefully use the compressed identifier here which is not what is expected
				HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{compressedPayloadIdentifier}", UriKind.Relative), content);

				Assert.AreEqual(HttpStatusCode.BadRequest, result.StatusCode);
			}
		}

		[TestMethod]
		public async Task PostNoIdentifierAsync()
		{
			byte[] texturePayload = await File.ReadAllBytesAsync("ContentId/Payloads/UncompressedTexture_CAS_dea81b6c3b565bb5089695377c98ce0f1c13b0c3.udd");
			BlobId compressedPayloadIdentifier = BlobId.FromBlob(texturePayload);
			BlobId uncompressedPayloadIdentifier = new BlobId("DEA81B6C3B565BB5089695377C98CE0F1C13B0C3");
			{
				using ByteArrayContent content = new(texturePayload);
				content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);
				// we purposefully use the compressed identifier here which is not what is expected
				HttpResponseMessage result = await Client!.PostAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}", UriKind.Relative), content);

				result.EnsureSuccessStatusCode();

				InsertResponse? response = await result.Content.ReadFromJsonAsync<InsertResponse>();
				Assert.IsNotNull(response);
				Assert.IsNotNull(response.Identifier);
				Assert.AreEqual(uncompressedPayloadIdentifier, response.Identifier);
				Assert.AreNotEqual(compressedPayloadIdentifier, response.Identifier);
			}
		}

		[TestMethod]
		public async Task GetUncompressedContentAsync()
		{
			string stringContent = "this is just a random string";
			byte[] payload = Encoding.ASCII.GetBytes(stringContent);
			BlobId blobIdentifier = BlobId.FromBlob(payload);

			// upload a uncompressed blob
			{
				using ByteArrayContent content = new(payload);
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
				HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/blobs/{TestNamespace}/{blobIdentifier}", UriKind.Relative), content);
				result.EnsureSuccessStatusCode();
			}

			{
				HttpResponseMessage result = await Client!.GetAsync(new Uri($"api/v1/compressed-blobs/{TestNamespace}/{blobIdentifier}", UriKind.Relative));
				result.EnsureSuccessStatusCode();

				// verify that we return the uncompressed content but also that the media type indicates that
				byte[] blobContent = await result.Content.ReadAsByteArrayAsync();
				Assert.AreEqual(MediaTypeNames.Application.Octet, result.Content.Headers.ContentType!.MediaType);
				CollectionAssert.AreEqual(payload, blobContent);
			}
		}

		
		[TestMethod]
		public async Task GetUncompressedContentAsCompressedBufferAsync()
		{
			string stringContent = "this is just a random string";
			byte[] payload = Encoding.ASCII.GetBytes(stringContent);
			BlobId blobIdentifier = BlobId.FromBlob(payload);

			// upload a uncompressed blob
			{
				using ByteArrayContent content = new(payload);
				content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
				HttpResponseMessage result = await Client!.PutAsync(new Uri($"api/v1/blobs/{TestNamespace}/{blobIdentifier}", UriKind.Relative), content);
				result.EnsureSuccessStatusCode();
			}

			{
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, $"api/v1/compressed-blobs/{TestNamespace}/{blobIdentifier}");
				request.Headers.Add("Accept", CustomMediaTypeNames.UnrealCompactBinary);

				// asking for a compressed buffer when we only have the uncompressed content results in a 415 (unsupported media type) as transcoding this is to complicated when the compressed buffer header requires the full blake3 hash of the content
				HttpResponseMessage result = await Client!.SendAsync(request);
				Assert.AreEqual(HttpStatusCode.UnsupportedMediaType, result.StatusCode);
			}

			{
				// ask for any accept type
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, $"api/v1/compressed-blobs/{TestNamespace}/{blobIdentifier}");
				request.Headers.Add("Accept", "*/*");

				HttpResponseMessage result = await Client!.SendAsync(request);
				result.EnsureSuccessStatusCode();
				Assert.AreEqual(MediaTypeNames.Application.Octet, result.Content.Headers.ContentType!.MediaType);

				byte[] blobContent = await result.Content.ReadAsByteArrayAsync();
				CollectionAssert.AreEqual(payload, blobContent);
			}
		}
	}
}
