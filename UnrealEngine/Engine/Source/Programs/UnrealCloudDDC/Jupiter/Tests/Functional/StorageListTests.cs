// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using Amazon.S3;
using Amazon.S3.Model;
using Azure.Storage.Blobs;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Microsoft.AspNetCore.TestHost;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Serilog.Core;

namespace Jupiter.FunctionalTests.Storage
{
	[TestClass]
	public class S3StorageListTests : StorageListTests
	{
		private IAmazonS3? _s3;
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString()),
				new KeyValuePair<string, string?>("S3:BucketName", $"tests-{TestListNamespaceName}")
			};
		}

		protected override async Task Seed(IServiceProvider provider)
		{
			string s3BucketName = $"tests-{TestListNamespaceName}";

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
	}

	[TestClass]
	public class AzureStorageListTests : StorageListTests
	{
		private AzureSettings? _settings;
		private string? _connectionString;

		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.Azure.ToString()),
				// override the container used for the default storage pool (empty string)
				new KeyValuePair<string, string?>($"Azure:StoragePoolContainerOverride:", DefaultContainerName)
			};
		}

		protected override async Task Seed(IServiceProvider provider)
		{
			_settings = provider.GetService<IOptionsMonitor<AzureSettings>>()!.CurrentValue;

			_connectionString = _settings.ConnectionString;
			BlobContainerClient container = new BlobContainerClient(_connectionString, DefaultContainerName);

			if (!await container.ExistsAsync())
			{
				await container.CreateAsync();
			}

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

		private const string DefaultContainerName = "tests-test-namespace-list";
	}

	
	[TestClass]
	public class FileSystemStoreListTests : StorageListTests
	{
		private readonly string _localTestDir;

		public FileSystemStoreListTests()
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
			NamespaceId folderName = TestListNamespaceName;
			
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
				BlobId[] blobs = await fsStore.ListObjectsAsync(TestListNamespaceName).Where(tuple => tuple.Item2 < cutoff).Select(tuple => tuple.Item1).ToArrayAsync();
				Assert.AreEqual(OldBlobFileHash, blobs[0]);
			}
		}
	}

	public abstract class StorageListTests
	{
		protected TestServer? Server { get; set; }
		protected NamespaceId TestListNamespaceName { get; } = new NamespaceId("test-namespace-list");

		protected const string SmallFileContents = "Small file contents";
		protected const string AnotherFileContents = "Another file with contents";
		protected const string DeletableFileContents = "Delete Me";
		protected const string OldFileContents = "a old blob used for testing cutoff filtering";

		protected BlobId SmallFileHash { get; } = BlobId.FromBlob(Encoding.ASCII.GetBytes(SmallFileContents));
		protected BlobId AnotherFileHash { get; } = BlobId.FromBlob(Encoding.ASCII.GetBytes(AnotherFileContents));
		protected BlobId DeleteFileHash { get; } = BlobId.FromBlob(Encoding.ASCII.GetBytes(DeletableFileContents));
		protected BlobId OldBlobFileHash { get; } = BlobId.FromBlob(Encoding.ASCII.GetBytes(OldFileContents));

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
			Server = server;

			await Seed(Server.Services);
		}

		protected abstract IEnumerable<KeyValuePair<string, string?>> GetSettings();

		protected abstract Task Seed(IServiceProvider serverServices);

		[TestMethod]
		public async Task ListBlobsAsync()
		{
			List<BlobId> validBlobHashes = new List<BlobId>
			{
				SmallFileHash,
				AnotherFileHash,
				DeleteFileHash,
				OldBlobFileHash
			};

			IBlobService blobService = Server!.Services.GetService<IBlobService>()!;

			BlobId[] oldObjects = await blobService.ListObjectsAsync(TestListNamespaceName).Select(tuple => tuple.Item1).ToArrayAsync();

			Assert.AreEqual(4, oldObjects.Length);
			Assert.IsTrue(validBlobHashes.Contains(oldObjects[0]));
			Assert.IsTrue(validBlobHashes.Contains(oldObjects[1]));
			Assert.IsTrue(validBlobHashes.Contains(oldObjects[2]));
			Assert.IsTrue(validBlobHashes.Contains(oldObjects[3]));
		}
	}
}
