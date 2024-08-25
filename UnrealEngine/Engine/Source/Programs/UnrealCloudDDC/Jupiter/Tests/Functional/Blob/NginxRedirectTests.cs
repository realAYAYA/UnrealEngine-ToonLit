// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Mime;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Serilog.Core;

namespace Jupiter.FunctionalTests.Storage
{
	[TestClass]
	public class NginxRedirectTests
	{
		protected const string FileContents = "This is some random contents";

		protected BlobId FileContentsHash { get; } = BlobId.FromBlob(Encoding.ASCII.GetBytes(FileContents));

		protected NamespaceId TestNamespaceName { get; } = new NamespaceId("testbucket");
		protected TestServer? Server { get; set; }

		private HttpClient? _httpClient;
		private string _localTestDir = null!;

		[TestInitialize]
		public void Setup()
		{
			_localTestDir = Path.Combine(Path.GetTempPath(), "NginxRedirectTests", Path.GetRandomFileName());

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
		}

		[TestCleanup]
		public void Teardown()
		{
			Directory.Delete(_localTestDir, true);
		}

		protected IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[]
			{
				new KeyValuePair<string, string?>("UnrealCloudDDC:StorageImplementations:0", UnrealCloudDDCSettings.StorageBackendImplementations.FileSystem.ToString()),
				new KeyValuePair<string, string?>("Filesystem:RootDir", _localTestDir),
				new KeyValuePair<string, string?>("Nginx:UseNginxRedirect", "true"),
				new KeyValuePair<string, string?>("S3:BucketName", $"tests-{TestNamespaceName}")
			};
		}

		[TestMethod]
		public async Task FetchBlobWithNginxRedirectAsync()
		{
			byte[] s3ContentBytes = Encoding.ASCII.GetBytes(FileContents);
			using ByteArrayContent requestContent = new ByteArrayContent(s3ContentBytes);
			requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
			requestContent.Headers.ContentLength = FileContents.Length;
			HttpResponseMessage putResponse = await _httpClient!.PutAsync(new Uri($"api/v1/blobs/{TestNamespaceName}/{FileContentsHash}", UriKind.Relative), requestContent);
			putResponse.EnsureSuccessStatusCode();
			InsertResponse? response = await putResponse.Content.ReadFromJsonAsync<InsertResponse>();
			Assert.IsNotNull(response);
			Assert.AreEqual(FileContentsHash, response.Identifier);

			using HttpRequestMessage getObjectRequest = new HttpRequestMessage(HttpMethod.Get, new Uri($"api/v1/blobs/{TestNamespaceName}/{FileContentsHash}", UriKind.Relative));
			getObjectRequest.Headers.Add("X-Jupiter-XAccel-Supported", "true");
			HttpResponseMessage getResponse = await _httpClient.SendAsync(getObjectRequest);

			getResponse.EnsureSuccessStatusCode();
			Assert.IsTrue(getResponse.Headers.Contains("X-Accel-Redirect"));
			string uri = getResponse.Headers.GetValues("X-Accel-Redirect").First();
			string contentType = getResponse.Content.Headers.ContentType?.MediaType ?? "";
			Assert.AreEqual(MediaTypeNames.Application.Octet, contentType);
			Assert.AreEqual($"/nginx-blobs/testbucket/E3/06/{FileContentsHash}", uri);

		}
	}
}
