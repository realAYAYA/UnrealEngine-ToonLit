// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Mime;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using Amazon.S3;
using Amazon.S3.Model;
using Azure.Storage.Blobs;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Controllers;
using Jupiter.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Serilog.Core;
using ContentHash = Jupiter.Implementation.ContentHash;
using IBlobStore = Jupiter.Implementation.IBlobStore;
using EpicGames.AspNet;
using Jupiter.Tests.Functional;

namespace Jupiter.FunctionalTests.Storage
{
	[TestClass]
	public class MixStorageTests : StorageTests
	{
		private IAmazonS3? _s3;
		private readonly string _localTestDir;

		public MixStorageTests()
		{
			_localTestDir = Path.Combine(Path.GetTempPath(), "MixFileSystemTests", Path.GetRandomFileName());
		}
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.FileSystem.ToString()),
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:1", UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString()),
				new KeyValuePair<string, string?>("Filesystem:RootDir", _localTestDir),
				new KeyValuePair<string, string?>("S3:BucketName", $"tests-mix-{TestNamespaceName}")
			};
		}

		protected override async Task Seed(IServiceProvider provider)
		{
			string s3BucketName = $"tests-mix-{TestNamespaceName}";

			_s3 = provider.GetService<IAmazonS3>();
			Assert.IsNotNull(_s3);
			try
			{
				await _s3.PutBucketAsync(s3BucketName);
			}
			catch (AmazonS3Exception e)
			{
				if (e.StatusCode != HttpStatusCode.Conflict)
				{
					// skip 409 as that means the bucket already existed
					throw;
				}
			}
			await _s3.PutObjectAsync(new PutObjectRequest { BucketName = s3BucketName, Key = SmallFileHash.AsS3Key(), ContentBody = SmallFileContents });
			await _s3.PutObjectAsync(new PutObjectRequest { BucketName = s3BucketName, Key = AnotherFileHash.AsS3Key(), ContentBody = AnotherFileContents });
			await _s3.PutObjectAsync(new PutObjectRequest { BucketName = s3BucketName, Key = DeleteFileHash.AsS3Key(), ContentBody = DeletableFileContents });
			await _s3.PutObjectAsync(new PutObjectRequest { BucketName = s3BucketName, Key = OldBlobFileHash.AsS3Key(), ContentBody = OldFileContents });
		}

		[TestMethod]
		public async Task GetBlobRedirectAsync()
		{
			HttpResponseMessage result = await HttpClient!.GetAsync(new Uri($"api/v1/s/{TestRedirectNamespaceName}/{SmallFileHash}", UriKind.Relative));
			Assert.AreEqual(HttpStatusCode.Redirect, result.StatusCode);
		}

		/// <summary>
		/// write a large file (bigger then that C# can have in memory) while forcing this to not exist in the filesystem cache so that we trigger a populate
		/// </summary>

		[TestMethod]
		[TestCategory("SlowTests")]
		public async Task PutGetLargePayloadForcePopulateAsync()
		{
			// we submit a blob so large that it can not fit using the memory blob store
			IBlobStore? blobStore = Server?.Services.GetService<IBlobStore>();
			Assert.IsFalse(blobStore is MemoryBlobStore);

			if (blobStore is AzureBlobStore)
			{
				Assert.Inconclusive("Azure blob store gets internal server errors when receiving large blobs");
			}

			FileSystemStore? filesystemStore = Server?.Services.GetService<FileSystemStore>();
			Assert.IsNotNull(filesystemStore, "Expected to find a configured filesystem store");

			FileInfo fi = new FileInfo(Path.GetTempFileName());

			FileInfo tempOutputFile = new FileInfo(Path.GetTempFileName());

			try
			{
				{
					await using FileStream fs = fi.OpenWrite();

					byte[] block = new byte[1024 * 1024];
					Array.Fill(block, (byte)'a');
					// we want a file larger then 2GB, each block is 1 MB
					int countOfBlocks = 2100;
					for (int i = 0; i < countOfBlocks; i++)
					{
						await fs.WriteAsync(block, 0, block.Length);
					}
				}

				BlobId blobIdentifier;
				{
					await using FileStream fs = fi.OpenRead();
					blobIdentifier = await BlobId.FromStreamAsync(fs);
				}

				{
					await using FileStream fs = fi.OpenRead();
					using StreamContent content = new StreamContent(fs);
					content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
					HttpResponseMessage result = await HttpClient!.PutAsync(new Uri($"api/v1/blobs/{TestNamespaceName}/{blobIdentifier}", UriKind.Relative), content);
					result.EnsureSuccessStatusCode();

					InsertResponse? response = await result.Content.ReadFromJsonAsync<InsertResponse>();
					Assert.IsNotNull(response);
					Assert.AreEqual(blobIdentifier, response.Identifier);
				}

				await filesystemStore.DeleteObjectAsync(TestNamespaceName, blobIdentifier);

				{
					// verify we can fetch the blob again
					HttpResponseMessage result = await HttpClient!.GetAsync(new Uri($"api/v1/blobs/{TestNamespaceName}/{blobIdentifier}", UriKind.Relative), HttpCompletionOption.ResponseHeadersRead);
					if (result.StatusCode == HttpStatusCode.InternalServerError)
					{
						throw new Exception("Error: " + await result.Content.ReadAsStringAsync());
					}
					result.EnsureSuccessStatusCode();
					Stream s = await result.Content.ReadAsStreamAsync();

					{
						// stream this to disk so we have something to look at in case there is an error
						await using FileStream fs = tempOutputFile.OpenWrite();
						await s.CopyToAsync(fs);
						fs.Close();
						s.Close();

						s = tempOutputFile.OpenRead();
					}

					BlobId downloadedBlobIdentifier = await BlobId.FromStreamAsync(s);
					Assert.AreEqual(blobIdentifier, downloadedBlobIdentifier);
					s.Close();
				}
			}
			finally
			{
				if (fi.Exists)
				{
					fi.Delete();
				}

				if (tempOutputFile.Exists)
				{
					tempOutputFile.Delete();
				}
			}
		}

		protected override async Task Teardown(IServiceProvider provider)
		{
			if (Directory.Exists(_localTestDir))
			{
				Directory.Delete(_localTestDir, true);
			}

			await Task.CompletedTask;
		}
	}

	[TestClass]
	public class S3StorageTests : StorageTests
	{
		private IAmazonS3? _s3;
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString()),
				new KeyValuePair<string, string?>("S3:BucketName", $"tests-{TestNamespaceName}")
			};
		}

		protected override async Task Seed(IServiceProvider provider)
		{
			string s3BucketName = $"tests-{TestNamespaceName}";

			_s3 = provider.GetService<IAmazonS3>();
			Assert.IsNotNull(_s3);
			try
			{
				await _s3.PutBucketAsync(s3BucketName);
			}
			catch (AmazonS3Exception e)
			{
				if (e.StatusCode != HttpStatusCode.Conflict)
				{
					// skip 409 as that means the bucket already existed
					throw;
				}
			}
			await _s3.PutObjectAsync(new PutObjectRequest { BucketName = s3BucketName, Key = SmallFileHash.AsS3Key(), ContentBody = SmallFileContents });
			await _s3.PutObjectAsync(new PutObjectRequest { BucketName = s3BucketName, Key = AnotherFileHash.AsS3Key(), ContentBody = AnotherFileContents });
			await _s3.PutObjectAsync(new PutObjectRequest { BucketName = s3BucketName, Key = DeleteFileHash.AsS3Key(), ContentBody = DeletableFileContents });
			await _s3.PutObjectAsync(new PutObjectRequest {BucketName = s3BucketName, Key = OldBlobFileHash.AsS3Key(), ContentBody = OldFileContents});
		}

		[TestMethod]
		public async Task GetBlobRedirectAsync()
		{
			HttpResponseMessage result = await HttpClient!.GetAsync(new Uri($"api/v1/s/{TestRedirectNamespaceName}/{SmallFileHash}", UriKind.Relative));
			Assert.AreEqual(HttpStatusCode.Redirect, result.StatusCode);
		}

		protected override async Task Teardown(IServiceProvider provider)
		{
			await Task.CompletedTask;
		}
	}

	[TestClass]
	public class AzureStorageTests : StorageTests
	{
		private AzureSettings? _settings;
		private string? _connectionString;

		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[] {new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.Azure.ToString())};
		}

		protected override async Task Seed(IServiceProvider provider)
		{
			_settings = provider.GetService<IOptionsMonitor<AzureSettings>>()!.CurrentValue;

			_connectionString = _settings.ConnectionString;
			BlobContainerClient container = new BlobContainerClient(_connectionString, DefaultContainerName);

			await container.CreateIfNotExistsAsync();

			BlobClient smallBlob = container.GetBlobClient(SmallFileHash.ToString());
			byte[] smallContents = Encoding.ASCII.GetBytes(SmallFileContents);
			await using MemoryStream smallBlobStream = new MemoryStream(smallContents);
			await smallBlob.UploadAsync(smallBlobStream, overwrite: true);

			BlobClient anotherBlob = container.GetBlobClient(AnotherFileHash.ToString());
			byte[] anotherContents = Encoding.ASCII.GetBytes(AnotherFileContents);
			await using MemoryStream anotherContentsStream = new MemoryStream(anotherContents);
			await anotherBlob.UploadAsync(anotherContentsStream, overwrite: true);

			BlobClient deleteBlob = container.GetBlobClient(DeleteFileHash.ToString());
			byte[] deleteContents = Encoding.ASCII.GetBytes(DeletableFileContents);
			await using MemoryStream deleteContentsStream = new MemoryStream(deleteContents);
			await deleteBlob.UploadAsync(deleteContentsStream, overwrite: true);

			BlobClient oldBlob = container.GetBlobClient(OldBlobFileHash.ToString());
			byte[] oldBlobContents = Encoding.ASCII.GetBytes(OldFileContents);
			await using MemoryStream oldBlobContentsSteam = new MemoryStream(oldBlobContents);
			await oldBlob.UploadAsync(oldBlobContentsSteam, overwrite: true);
		}

		private const string DefaultContainerName = "jupiter";

		[TestMethod]
		public async Task GetBlobRedirectAsync()
		{
			HttpResponseMessage result = await HttpClient!.GetAsync(new Uri($"api/v1/s/{TestRedirectNamespaceName}/{SmallFileHash}", UriKind.Relative));
			Assert.AreEqual(HttpStatusCode.Redirect, result.StatusCode);
		}

		protected override async Task Teardown(IServiceProvider provider)
		{
			await Task.CompletedTask;
		}
	}

	
	[TestClass]
	public class FileSystemStoreTests : StorageTests
	{
		private readonly string _localTestDir;
		private readonly NamespaceId _fooNamespace = new NamespaceId("foo");

		public FileSystemStoreTests()
		{
			_localTestDir = Path.Combine(Path.GetTempPath(), "IoFileSystemTests", Path.GetRandomFileName());
		}
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[] { 
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.FileSystem.ToString()),
				new KeyValuePair<string, string?>("Filesystem:RootDir", _localTestDir)
			};
		}

		protected override async Task Seed(IServiceProvider provider)
		{
			NamespaceId folderName = TestNamespaceName;
			
			Directory.CreateDirectory(_localTestDir);

			FileInfo smallFileInfo = FileSystemStore.GetFilesystemPath(_localTestDir, folderName, SmallFileHash);
			smallFileInfo.Directory?.Create();
			await File.WriteAllBytesAsync(
				smallFileInfo.FullName,
				Encoding.ASCII.GetBytes(SmallFileContents)
			);

			FileInfo anotherFileInfo = FileSystemStore.GetFilesystemPath(_localTestDir, folderName, AnotherFileHash);
			anotherFileInfo.Directory?.Create();
			await File.WriteAllBytesAsync(
				anotherFileInfo.FullName,
				Encoding.ASCII.GetBytes(AnotherFileContents)
			);
			
			FileInfo deleteFileInfo = FileSystemStore.GetFilesystemPath(_localTestDir, folderName, DeleteFileHash);
			deleteFileInfo.Directory?.Create();
			await File.WriteAllBytesAsync(
				deleteFileInfo.FullName,
				Encoding.ASCII.GetBytes(DeletableFileContents)
			);

			// a old file used to verify cutoff filtering
			FileInfo oldFileInfo = FileSystemStore.GetFilesystemPath(_localTestDir, folderName, OldBlobFileHash);
			oldFileInfo.Directory?.Create();
			await File.WriteAllBytesAsync(
				oldFileInfo.FullName,
				Encoding.ASCII.GetBytes(OldFileContents)
			);
			File.SetLastWriteTimeUtc(oldFileInfo.FullName, DateTime.Now.AddDays(-7));
		}

		protected override Task Teardown(IServiceProvider provider)
		{
			Directory.Delete(_localTestDir, true);

			return Task.CompletedTask;
		}

		// only a file system allows us to update the last modified time of the object to actually execute this test
		[TestMethod]
		public async Task ListOldBlobsAsync()
		{
			FileSystemStore? fsStore = Server!.Services.GetService<FileSystemStore>();
			Assert.IsNotNull(fsStore);

			// we fetch all objects that are more then a day old
			// as the old blob is set to be a week old this should be the only object returned
			DateTime cutoff = DateTime.Now.AddDays(-1);
			{
				BlobId[] blobs = await fsStore.ListObjectsAsync(TestNamespaceName).Where(tuple => tuple.Item2 < cutoff).Select(tuple => tuple.Item1).ToArrayAsync();
				Assert.AreEqual(OldBlobFileHash, blobs[0]);
			}
		}

		[TestMethod]
		public async Task ListNamespacesAsync()
		{
			FileSystemStore? fsStore = Server!.Services.GetService<FileSystemStore>();
			Assert.IsNotNull(fsStore);

			List<NamespaceId> namespaces = await fsStore.ListNamespaces().ToListAsync();
			Assert.AreEqual(1, namespaces.Count);
			
			await fsStore.PutObjectAsync(_fooNamespace, Encoding.ASCII.GetBytes(SmallFileContents), SmallFileHash);
			
			namespaces = await fsStore.ListNamespaces().ToListAsync();
			Assert.AreEqual(2, namespaces.Count);
		}

		[TestMethod]
		public async Task GarbageCollectAsync()
		{
			// Remove data added from test seeding
			Directory.Delete(_localTestDir, true);
			Directory.CreateDirectory(_localTestDir);
			
			FilesystemSettings fsSettings = Server!.Services.GetService<IOptionsMonitor<FilesystemSettings>>()!.CurrentValue;
			fsSettings.MaxSizeBytes = 600;
			FileSystemStore? fsStore = Server!.Services.GetService<FileSystemStore>();
			Assert.IsNotNull(fsStore);

			using CancellationTokenSource cts = new CancellationTokenSource();
			Assert.IsTrue(await fsStore.CleanupInternalAsync(cts.Token, batchSize: 2) == 0); // No garbage to collect, should return false
			
			FileInfo[] fooFiles = CreateFilesInNamespace(_fooNamespace, 10);

			Assert.AreEqual(10 * 100, await fsStore.CalculateDiskSpaceUsedAsync());

			Assert.IsTrue(await fsStore.CleanupInternalAsync(cts.Token, batchSize: 2) > 0);

			Assert.AreEqual(5 * 100, await fsStore.CalculateDiskSpaceUsedAsync());

			fooFiles.ToList().ForEach(x => x.Refresh());
			Assert.IsTrue(fooFiles[0].Exists); // Most recently accessed/modified
			Assert.IsTrue(fooFiles[1].Exists);
			Assert.IsTrue(fooFiles[2].Exists);
			Assert.IsTrue(fooFiles[3].Exists);
			Assert.IsTrue(fooFiles[4].Exists);
			Assert.IsFalse(fooFiles[5].Exists);
			Assert.IsFalse(fooFiles[6].Exists);
			Assert.IsFalse(fooFiles[7].Exists);
			Assert.IsFalse(fooFiles[8].Exists);
			Assert.IsFalse(fooFiles[9].Exists); // Least recently accessed/modified
		}
		
		[TestMethod]
		public void GetLeastRecentlyAccessedObjects()
		{
			FileInfo[] fooFiles = CreateFilesInNamespace(_fooNamespace, 10);
			CreateFilesInNamespace(new NamespaceId("bar"), 10);

			FileSystemStore? fsStore = Server!.Services.GetService<FileSystemStore>();
			Assert.IsNotNull(fsStore);

			Assert.AreEqual(10, (fsStore.GetLeastRecentlyAccessedObjects(_fooNamespace)).ToArray().Length);

			FileInfo[] results = fsStore.GetLeastRecentlyAccessedObjects(_fooNamespace, 3).ToArray();
			Assert.AreEqual(3, results.Length);
			Assert.AreEqual(fooFiles[7].LastAccessTime, results[2].LastAccessTime);
			Assert.AreEqual(fooFiles[8].LastAccessTime, results[1].LastAccessTime);
			Assert.AreEqual(fooFiles[9].LastAccessTime, results[0].LastAccessTime);
		}
		
		[TestMethod]
		public async Task CalculateUsedDiskSpaceAsync()
		{
			FileSystemStore? fsStore = Server!.Services.GetService<FileSystemStore>();
			Assert.IsNotNull(fsStore);
			await fsStore.PutObjectAsync(_fooNamespace, Encoding.ASCII.GetBytes(SmallFileContents), SmallFileHash);
			await fsStore.PutObjectAsync(_fooNamespace, Encoding.ASCII.GetBytes(AnotherFileContents), AnotherFileHash);
			
			Assert.AreEqual(SmallFileContents.Length + AnotherFileContents.Length, await fsStore.CalculateDiskSpaceUsedAsync(_fooNamespace));
		}
		
		private FileInfo[] CreateFilesInNamespace(NamespaceId ns, int numFiles)
		{
			FileInfo[] files = new FileInfo[numFiles];
			for (int i = 0; i < numFiles; i++)
			{
				byte[] content = Encoding.ASCII.GetBytes(i + 1000 + new string('j', 96));
				BlobId bi = BlobId.FromBlob(content);
				FileInfo fi = FileSystemStore.GetFilesystemPath(_localTestDir, ns, bi);
				fi.Directory?.Create();
				File.WriteAllBytes(fi.FullName, content);
				fi.LastWriteTime = DateTime.UtcNow.AddDays(-i);
				fi.Refresh();
				files[i] = fi;
			}

			return files;
		}
	}

	public abstract class StorageTests
	{
		protected TestServer? Server { get; set; }
		protected NamespaceId TestNamespaceName { get; } = new NamespaceId("testbucket");
		protected NamespaceId TestBundleNamespaceName { get; } = new NamespaceId("test-namespace-bundle");
		protected NamespaceId TestRedirectNamespaceName { get; } = new NamespaceId("test-namespace-redirect");

		private HttpClient? _httpClient;

		protected const string SmallFileContents = "Small file contents";
		protected const string AnotherFileContents = "Another file with contents";
		protected const string DeletableFileContents = "Delete Me";
		protected const string OldFileContents = "a old blob used for testing cutoff filtering";

		protected BlobId SmallFileHash { get; } = BlobId.FromBlob(Encoding.ASCII.GetBytes(SmallFileContents));
		protected BlobId AnotherFileHash { get; } = BlobId.FromBlob(Encoding.ASCII.GetBytes(AnotherFileContents));
		protected BlobId DeleteFileHash { get; } = BlobId.FromBlob(Encoding.ASCII.GetBytes(DeletableFileContents));
		protected BlobId OldBlobFileHash { get; } = BlobId.FromBlob(Encoding.ASCII.GetBytes(OldFileContents));

		protected HttpClient? HttpClient => _httpClient;

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
			Server = server;

			//foreach (NamespaceId ns in new [] { TestNamespaceName, TestListNamespaceName})
			{
				// Seed storage
				await Seed(Server.Services);
			}
		}

		protected abstract IEnumerable<KeyValuePair<string, string?>> GetSettings();

		protected abstract Task Seed(IServiceProvider serverServices);
		protected abstract Task Teardown(IServiceProvider serverServices);

		[TestCleanup]
		public async Task MyTeardownAsync()
		{
			await Teardown(Server!.Services);
		}

		[TestMethod]
		public async Task GetSmallFileAsync()
		{
			HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/{SmallFileHash}", UriKind.Relative));
			result.EnsureSuccessStatusCode();
			string content = await result.Content.ReadAsStringAsync();
			Assert.AreEqual(SmallFileContents, content);
		}

		[TestMethod]
		public async Task GetNotExistentFileAsync()
		{
			byte[] payload = Encoding.ASCII.GetBytes("This content does not exist");
			ContentHash contentHash = ContentHash.FromBlob(payload);
			HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/{contentHash}", UriKind.Relative));

			Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
		}

		[TestMethod]
		public async Task GetInvalidHashAsync()
		{
			HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/smallFile", UriKind.Relative));

			Assert.AreEqual(HttpStatusCode.BadRequest, result.StatusCode);
		}

		[TestMethod]
		public async Task PutSmallBlobAsync()
		{
			byte[] payload = Encoding.ASCII.GetBytes("I am a small blob");
			using ByteArrayContent requestContent = new ByteArrayContent(payload);
			requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
			requestContent.Headers.ContentLength = payload.Length;
			BlobId contentHash = BlobId.FromBlob(payload);
			HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/s/{TestNamespaceName}/{contentHash}", UriKind.Relative), requestContent);

			result.EnsureSuccessStatusCode();
			InsertResponse? content = await result.Content.ReadFromJsonAsync<InsertResponse>();
			Assert.IsNotNull(content);
			Assert.AreEqual(contentHash, content.Identifier);
		}

		[TestMethod]
		public async Task PostSmallBlobAsync()
		{
			byte[] payload = Encoding.ASCII.GetBytes("I am a small blob");
			using ByteArrayContent requestContent = new ByteArrayContent(payload);
			requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
			requestContent.Headers.ContentLength = payload.Length;
			BlobId contentHash = BlobId.FromBlob(payload);
			HttpResponseMessage result = await _httpClient!.PostAsync(new Uri($"api/v1/s/{TestNamespaceName}", UriKind.Relative), requestContent);

			result.EnsureSuccessStatusCode();
			InsertResponse? content = await result.Content.ReadFromJsonAsync<InsertResponse>();
			Assert.IsNotNull(content);
			Assert.AreEqual(contentHash, content.Identifier);
		}

		[TestMethod]
		public async Task DeleteBlobAsync()
		{
			HttpResponseMessage result = await  _httpClient!.DeleteAsync(new Uri($"api/v1/s/{TestNamespaceName}/{DeleteFileHash}", UriKind.Relative));
			result.EnsureSuccessStatusCode();

			Assert.AreEqual(HttpStatusCode.NoContent, result.StatusCode);
		}

		[TestMethod]
		public async Task BlobExistsAsync()
		{
			{
				using HttpRequestMessage message = new(HttpMethod.Head, new Uri($"api/v1/s/{TestNamespaceName}/{SmallFileHash}", UriKind.Relative));
				HttpResponseMessage result = await _httpClient!.SendAsync(message);
				Assert.AreEqual(HttpStatusCode.OK, result.StatusCode);
			}

			{
				ContentHash newContent = ContentHash.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));
				using HttpRequestMessage message = new (HttpMethod.Head, new Uri($"api/v1/s/{TestNamespaceName}/{newContent}", UriKind.Relative));
				HttpResponseMessage resultNew = await _httpClient!.SendAsync(message);
				Assert.AreEqual(HttpStatusCode.NotFound, resultNew.StatusCode);
				string content = await resultNew.Content.ReadAsStringAsync();
				ValidationProblemDetails result = JsonSerializer.Deserialize<ValidationProblemDetails>(content, JsonTestUtils.DefaultJsonSerializerSettings)!;
				Assert.AreEqual($"Blob {newContent} not found", result.Title);
			}
		}

		[TestMethod]
		public async Task BlobExistsBatchAsync()
		{
			BlobId newContent = BlobId.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));

			var ops = new
			{
				Operations = new object[]
				{
					new
					{
						Op = "HEAD",
						Namespace = TestNamespaceName,
						Id = SmallFileHash,
					},
					new
					{
						Op = "HEAD",
						Namespace = TestNamespaceName,
						Id = newContent,
					}
				}
			};

			HttpResponseMessage response = await _httpClient.PostAsJsonAsync("api/v1/s", ops);
			response.EnsureSuccessStatusCode();
			string content = await response.Content.ReadAsStringAsync();
			JsonNode? nodes = JsonNode.Parse(content)!;
			Assert.IsNotNull(nodes);

			JsonArray array = nodes.AsArray();
			Assert.IsNotNull(array);
			Assert.AreEqual(2, array!.Count);
			Assert.IsNull(array[0]);
			BlobId id = new BlobId(array[1]!.AsValue().ToString()!);
			Assert.AreEqual(newContent, id);
		}

		[TestMethod]
		public async Task BatchOpAsync()
		{
			var ops = new
			{
				Operations = new object[]
				{
					new
					{
						Op = "GET",
						Namespace = TestNamespaceName,
						Id = SmallFileHash,
					},
					new
					{
						Op = "PUT",
						Namespace = TestNamespaceName,
						Id = DeleteFileHash,
						Content = Encoding.ASCII.GetBytes(DeletableFileContents)
					}
				}
			};

			HttpResponseMessage response = await _httpClient.PostAsJsonAsync("api/v1/s", ops);
			response.EnsureSuccessStatusCode();
			string content = await response.Content.ReadAsStringAsync();
			JsonNode? nodes = JsonNode.Parse(content)!;
			JsonArray array = nodes.AsArray();
			Assert.IsNotNull(array);
			Assert.AreEqual(2, array!.Count);
			string base64Content = array[0]!.AsValue().ToString()!;
			string convertedContent = Encoding.ASCII.GetString(Convert.FromBase64String(base64Content));
			Assert.AreEqual(SmallFileContents, convertedContent);
			BlobId identifier = new BlobId(array[1]!.AsValue().ToString()!);
			Assert.AreEqual(DeleteFileHash, identifier);
		}

		[TestMethod]
		public async Task BatchOpBadRequestAsync()
		{
			var ops = new
			{
				Operations = new object[]
				{
					new
					{
						Op = "GET",
						Namespace = TestNamespaceName,
						Id =  BlobId.FromBlob(Encoding.ASCII.GetBytes("foo"))
					},
					new
					{
						Op = "PUT",
						Namespace = TestNamespaceName,
						// no content
					},
					new
					{
						Op = "notAValidEnumValue"
					}
				}
			};

			HttpResponseMessage response = await _httpClient.PostAsJsonAsync("api/v1/s", ops);
			Assert.AreEqual(HttpStatusCode.BadRequest, response.StatusCode);

			string content = await response.Content.ReadAsStringAsync();
			SerializableError? result = JsonSerializer.Deserialize<SerializableError>(content);
			Assert.IsNotNull(result);

			Assert.AreEqual(2, result.Keys.Count);
		}

		[TestMethod]
		public async Task BatchOpBlobNotPresentAsync()
		{
			var ops = new
			{
				Operations = new object[]
				{
					new
					{
						Op = "HEAD",
						Namespace = TestNamespaceName,
						Id = "1DEE7232FE05FEFE623D2119626F103E05D3EE98", // random hash that does not exist
					},
				}
			};

			HttpResponseMessage response = await _httpClient.PostAsJsonAsync("api/v1/s", ops);
			Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);

			string content = await response.Content.ReadAsStringAsync();
			string[]? result = JsonSerializer.Deserialize<string[]>(content);
			Assert.IsNotNull(result);

			Assert.AreEqual(1, result.Length);
			Assert.AreEqual("1DEE7232FE05FEFE623D2119626F103E05D3EE98", result[0]);
		}

		[TestMethod]
		public async Task MultipleBlobChecksAsync()
		{
			BlobId newContent = BlobId.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));
			using HttpRequestMessage message = new(HttpMethod.Post, new Uri($"api/v1/s/{TestNamespaceName}/exists?id={SmallFileHash}&id={newContent}", UriKind.Relative));
			HttpResponseMessage response = await _httpClient!.SendAsync(message);
			Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);
			Assert.AreEqual("application/json", response.Content.Headers.ContentType!.MediaType);

			string s = await response.Content.ReadAsStringAsync();
			HeadMultipleResponse? result = JsonSerializer.Deserialize<HeadMultipleResponse>(s, JsonTestUtils.DefaultJsonSerializerSettings);

			Assert.IsNotNull(result);
			Assert.AreEqual(1, result.Needs.Length);
			Assert.AreEqual(newContent, result.Needs[0]);
		}

		[TestMethod]
		public async Task MultipleBlobChecksBodyCBAsync()
		{
			BlobId newContent = BlobId.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));
			using HttpRequestMessage request = new(HttpMethod.Post, new Uri($"api/v1/s/{TestNamespaceName}/exist", UriKind.Relative));
			CbWriter writer = new CbWriter();
			writer.BeginUniformArray(CbFieldType.Hash);
			writer.WriteHashValue(SmallFileHash.AsIoHash());
			writer.WriteHashValue(newContent.AsIoHash());
			writer.EndUniformArray();
			byte[] buf = writer.ToByteArray();
			request.Content = new ByteArrayContent(buf);
			request.Content.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);

			HttpResponseMessage response = await _httpClient!.SendAsync(request);
			Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);
			Assert.AreEqual("application/json", response.Content.Headers.ContentType!.MediaType);

			string s = await response.Content.ReadAsStringAsync();
			HeadMultipleResponse? result = JsonSerializer.Deserialize<HeadMultipleResponse>(s, JsonTestUtils.DefaultJsonSerializerSettings);

			Assert.IsNotNull(result);
			Assert.AreEqual(1, result.Needs.Length);
			Assert.AreEqual(newContent, result.Needs[0]);
		}

		[TestMethod]
		public async Task MultipleBlobChecksBodyJsonAsync()
		{
			BlobId newContent = BlobId.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));
			using HttpRequestMessage request = new(HttpMethod.Post, new Uri($"api/v1/s/{TestNamespaceName}/exist", UriKind.Relative));
			string jsonBody = JsonSerializer.Serialize(new BlobId[] { SmallFileHash, newContent });
			request.Content = new StringContent(jsonBody, Encoding.UTF8, MediaTypeNames.Application.Json);

			HttpResponseMessage response = await _httpClient!.SendAsync(request);
			Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);
			Assert.AreEqual("application/json", response.Content.Headers.ContentType!.MediaType);

			string s = await response.Content.ReadAsStringAsync();
			HeadMultipleResponse? result = JsonSerializer.Deserialize<HeadMultipleResponse>(s, JsonTestUtils.DefaultJsonSerializerSettings);

			Assert.IsNotNull(result);
			Assert.AreEqual(1, result.Needs.Length);
			Assert.AreEqual(newContent, result.Needs[0]);
		}

		[TestMethod]
		public async Task MultipleBlobCompactBinaryResponseAsync()
		{
			BlobId newContent = BlobId.FromBlob(Encoding.ASCII.GetBytes("this content has never been submitted"));
			using HttpRequestMessage request = new(HttpMethod.Post, new Uri($"api/v1/s/{TestNamespaceName}/exists?id={SmallFileHash}&id={newContent}", UriKind.Relative));
			request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue(CustomMediaTypeNames.UnrealCompactBinary));
			HttpResponseMessage response = await _httpClient!.SendAsync(request);
			Assert.AreEqual(HttpStatusCode.OK, response.StatusCode);
			Assert.AreEqual(CustomMediaTypeNames.UnrealCompactBinary, response.Content.Headers.ContentType!.MediaType!);
			byte[] data = await response.Content.ReadAsByteArrayAsync();
			CbObject cb = new CbObject(data);

			Assert.AreEqual(1, cb.Count());
			CbField? needs = cb["needs"];
			Assert.IsNotNull(needs);
			IoHash[] neededBlobs = needs!.AsArray().Select(field => field.AsHash()).ToArray();

			Assert.AreEqual(1, neededBlobs.Length);
			Assert.AreEqual(newContent, BlobId.FromIoHash(neededBlobs[0]));
		}

		[TestMethod]
		public async Task FullFlowAsync()
		{
			byte[] payload = Encoding.ASCII.GetBytes("Foo bar");
			using ByteArrayContent requestContent = new ByteArrayContent(payload);
			requestContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
			requestContent.Headers.ContentLength = payload.Length;
			BlobId contentHash = BlobId.FromBlob(payload);
			HttpResponseMessage putResponse = await _httpClient!.PutAsync(new Uri($"api/v1/s/{TestNamespaceName}/{contentHash}", UriKind.Relative), requestContent);
			putResponse.EnsureSuccessStatusCode();
			InsertResponse? response = await putResponse.Content.ReadFromJsonAsync<InsertResponse>();
			Assert.IsNotNull(response);
			Assert.AreEqual(contentHash, response.Identifier);

			HttpResponseMessage getResponse = await _httpClient.GetAsync(new Uri($"api/v1/s/{TestNamespaceName}/{contentHash}", UriKind.Relative));
			getResponse.EnsureSuccessStatusCode();
			CollectionAssert.AreEqual(payload, actual: await getResponse.Content.ReadAsByteArrayAsync());

			HttpResponseMessage deleteResponse = await _httpClient.DeleteAsync(new Uri($"api/v1/s/{TestNamespaceName}/{contentHash}", UriKind.Relative));
			deleteResponse.EnsureSuccessStatusCode();
			Assert.AreEqual(HttpStatusCode.NoContent, deleteResponse.StatusCode);
		}

		/// <summary>
		/// write a large file (bigger then that C# can have in memory)
		/// </summary>
		
		[TestMethod]
		[TestCategory("SlowTests")]
		public async Task PutGetLargePayloadAsync()
		{
			// we submit a blob so large that it can not fit using the memory blob store
			IBlobStore? blobStore = Server?.Services.GetService<IBlobStore>();
			Assert.IsFalse(blobStore is MemoryBlobStore);

			if (blobStore is AzureBlobStore)
			{
				Assert.Inconclusive("Azure blob store gets internal server errors when receiving large blobs");
			}

			FileInfo fi = new FileInfo(Path.GetTempFileName());

			FileInfo tempOutputFile = new FileInfo(Path.GetTempFileName());

			try
			{
				{
					await using FileStream fs = fi.OpenWrite();

					byte[] block = new byte[1024 * 1024];
					Array.Fill(block, (byte)'a');
					// we want a file larger then 2GB, each block is 1 MB
					int countOfBlocks = 2100;
					for (int i = 0; i < countOfBlocks; i++)
					{
						await fs.WriteAsync(block, 0, block.Length);
					}
				}

				BlobId blobIdentifier;
				{
					await using FileStream fs = fi.OpenRead();
					blobIdentifier = await BlobId.FromStreamAsync(fs);
				}

				{
					await using FileStream fs = fi.OpenRead();
					using StreamContent content = new StreamContent(fs);
					content.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
					HttpResponseMessage result = await _httpClient!.PutAsync(new Uri($"api/v1/blobs/{TestNamespaceName}/{blobIdentifier}", UriKind.Relative), content);
					result.EnsureSuccessStatusCode();

					InsertResponse? response = await result.Content.ReadFromJsonAsync<InsertResponse>();
					Assert.IsNotNull(response);
					Assert.AreEqual(blobIdentifier, response.Identifier);
				}
				
				{
					// verify we can fetch the blob again
					HttpResponseMessage result = await _httpClient!.GetAsync(new Uri($"api/v1/blobs/{TestNamespaceName}/{blobIdentifier}", UriKind.Relative), HttpCompletionOption.ResponseHeadersRead);
					result.EnsureSuccessStatusCode();
					Stream s = await result.Content.ReadAsStreamAsync();

					{
						// stream this to disk so we have something to look at in case there is an error
						await using FileStream fs = tempOutputFile.OpenWrite();
						await s.CopyToAsync(fs);
						fs.Close();
						s.Close();

						s = tempOutputFile.OpenRead();
					}

					BlobId downloadedBlobIdentifier = await BlobId.FromStreamAsync(s);
					Assert.AreEqual(blobIdentifier, downloadedBlobIdentifier);
					s.Close();
				}
			}
			finally
			{
				if (fi.Exists)
				{
					fi.Delete();
				}

				if (tempOutputFile.Exists)
				{
					tempOutputFile.Delete();
				}
			}
		}
	}

	public class InsertResponse
	{
		public BlobId? Identifier { get; set; }
	}
}
