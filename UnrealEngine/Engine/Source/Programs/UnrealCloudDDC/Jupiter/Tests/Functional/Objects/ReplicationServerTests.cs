// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Threading.Tasks;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Logger = Serilog.Core.Logger;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using EpicGames.AspNet;

namespace Jupiter.FunctionalTests.References
{

	[TestClass]
	public class ReplicationServerTests
	{
		private IWebHost? _server;

		private readonly NamespaceId TestNamespace = new NamespaceId("test-namespace");
		private readonly BucketId TestBucket = new BucketId("default");

		[TestInitialize]
		public async Task SetupAsync()

		{
			IConfigurationRoot configuration = new ConfigurationBuilder()
				// we are not reading the base appSettings here as we want exact control over what runs in the tests
				.AddJsonFile("appsettings.Testing.json", true)
				.AddEnvironmentVariables()
				.Build();

			Logger logger = new LoggerConfiguration()
				.ReadFrom.Configuration(configuration)
				.CreateLogger();

			IWebHostBuilder webHostBuilder = new WebHostBuilder()
				.UseConfiguration(configuration)
				.UseEnvironment("Testing")
				.ConfigureServices(collection => collection.AddSerilog(logger))
				.UseStartup<JupiterStartup>()
				.UseKestrel(options =>
				{
					options.Listen(IPAddress.Any, 80);
					options.Listen(IPAddress.Any, 8080);
				})
			;
			_server = webHostBuilder.Build();
			await _server.StartAsync();
		}

		[TestMethod]
		[Ignore("Needs to be run manually as it starts up proper http servers and that can cause port conflicts")]
		public async Task ReplicationPublicEndpointAsync()
		{
			// insert a random object
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteString("stringField", nameof(ReplicationPublicEndpointAsync));
			writer.EndObject();

			byte[] objectData = writer.ToByteArray();
			BlobId objectHash = BlobId.FromBlob(objectData);

			using HttpContent requestContent = new ByteArrayContent(objectData);
			requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompactBinary);
			requestContent.Headers.Add(CommonHeaders.HashHeaderName, objectHash.ToString());

			{
				using HttpClient httpClient = new HttpClient();
				httpClient.BaseAddress = new Uri("http://localhost:8080");
				HttpResponseMessage result = await httpClient!.PutAsync(new Uri($"api/v1/refs/{TestNamespace}/{TestBucket}/{RefId.FromName("newReferenceObjectReplication")}.uecb", UriKind.Relative), requestContent);
				result.EnsureSuccessStatusCode();
			}
			// access the replication-log using a public endpoint and a internal endpoint, only the internal endpoint should return results
			{
				using HttpClient httpClient = new HttpClient();
				httpClient.BaseAddress = new Uri("http://localhost:8080");
				HttpResponseMessage result = await httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}", UriKind.Relative));
				result.EnsureSuccessStatusCode();
			}

			{
				using HttpClient httpClient = new HttpClient();
				httpClient.BaseAddress = new Uri("http://localhost:80");

				HttpResponseMessage result = await httpClient!.GetAsync(new Uri($"api/v1/replication-log/incremental/{TestNamespace}", UriKind.Relative));
				Assert.AreEqual(HttpStatusCode.NotFound, result.StatusCode);
			}
		}
	}
}
