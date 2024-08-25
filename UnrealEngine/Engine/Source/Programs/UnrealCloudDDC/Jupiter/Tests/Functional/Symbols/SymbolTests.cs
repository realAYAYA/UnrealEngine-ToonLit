// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Mime;
using System.Threading.Tasks;
using EpicGames.AspNet;
using EpicGames.Horde.Storage;
using Jupiter.Controllers;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Serilog;
using Logger = Serilog.Core.Logger;

namespace Jupiter.FunctionalTests.Symbols
{
	public abstract class SymbolTests
	{
		protected SymbolTests(string namespaceSuffix)
		{
			_testNamespaceName = new NamespaceId($"test-symbols-{namespaceSuffix}");
		}

		private TestServer? _server;
		private HttpClient? _httpClient;
		private readonly NamespaceId _testNamespaceName;

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

		protected abstract Task Seed(IServiceProvider services);
		protected abstract Task Teardown(IServiceProvider services);

		[TestCleanup]
		public async Task MyTeardownAsync()
		{
			await Teardown(_server!.Services);
		}

		[TestMethod]
		public async Task PutGetSymbolsAsync()
		{
			byte[] pdbPayload = await File.ReadAllBytesAsync($"Symbols/Payloads/EpicGames.Serialization.pdb");

			CompressedBufferUtils bufferUtils = _server!.Services.GetService<CompressedBufferUtils>()!;
			using MemoryStream compressedStream = new MemoryStream();
			bufferUtils.CompressContent(compressedStream, OoodleCompressorMethod.Mermaid, OoodleCompressionLevel.VeryFast, pdbPayload);

			byte[] compressedBufferPayload = compressedStream.ToArray();

			string moduleName = "EpicGames.Serialization.pdb";
			string pdbAge = "1";
			string pdgIdentifier = "6A976472EE4B4BC2BE73BFAC3B2C3EC2";
			string fileName = "EpicGames.Serialization.pdb";

			{
				using ByteArrayContent requestContent = new ByteArrayContent(compressedBufferPayload);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer);
				HttpResponseMessage response =
					await _httpClient!.PutAsync(new Uri($"api/v1/symbols/{_testNamespaceName}/{moduleName}", UriKind.Relative), requestContent);
				response.EnsureSuccessStatusCode();

				PutSymbolResponse? putSymbolResponse = await response.Content.ReadFromJsonAsync<PutSymbolResponse>();
				Assert.IsNotNull(putSymbolResponse);
				Assert.AreEqual(moduleName, putSymbolResponse.ModuleName);
				Assert.AreEqual(pdgIdentifier, putSymbolResponse.PdbIdentifier);
				Assert.AreEqual(pdbAge, putSymbolResponse.PdbAge.ToString());
			}

			{
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, new Uri($"api/v1/symbols/{_testNamespaceName}/{moduleName}/{pdgIdentifier}{pdbAge}/{fileName}", UriKind.Relative));
				request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue(CustomMediaTypeNames.UnrealCompressedBuffer));
				HttpResponseMessage response = await _httpClient!.SendAsync(request);
				response.EnsureSuccessStatusCode();

				byte[] downloadedPayload = await response.Content.ReadAsByteArrayAsync();
				CollectionAssert.AreEqual(compressedBufferPayload, downloadedPayload);
			}
		}

		[TestMethod]
		public async Task PutGetSymbolsUncompressedAsync()
		{
			byte[] pdbPayload = await File.ReadAllBytesAsync($"Symbols/Payloads/EpicGames.Serialization.pdb");

			string moduleName = "EpicGames.Serialization.pdb";
			string pdbIdentifier = "6A976472EE4B4BC2BE73BFAC3B2C3EC2";
			string pdbAge = "1";
			string fileName = "EpicGames.Serialization.pdb";

			{
				using ByteArrayContent requestContent = new ByteArrayContent(pdbPayload);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
				HttpResponseMessage response = await _httpClient!.PutAsync(new Uri($"api/v1/symbols/{_testNamespaceName}/{moduleName}", UriKind.Relative), requestContent);
				response.EnsureSuccessStatusCode();

				PutSymbolResponse? putSymbolResponse = await response.Content.ReadFromJsonAsync<PutSymbolResponse>();
				Assert.IsNotNull(putSymbolResponse);
				Assert.AreEqual(moduleName, putSymbolResponse.ModuleName);
				Assert.AreEqual(pdbIdentifier, putSymbolResponse.PdbIdentifier);
			}

			{
				using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, new Uri($"api/v1/symbols/{_testNamespaceName}/{moduleName}/{pdbIdentifier}{pdbAge}/{fileName}", UriKind.Relative));
				request.Headers.Accept.Add(new MediaTypeWithQualityHeaderValue(MediaTypeNames.Application.Octet));
				HttpResponseMessage response = await _httpClient!.SendAsync(request);
				response.EnsureSuccessStatusCode();

				byte[] downloadedPayload = await response.Content.ReadAsByteArrayAsync();
				CollectionAssert.AreEqual(pdbPayload, downloadedPayload);
			}
		}

		[TestMethod]
		public async Task PutSymbolsAsync()
		{
			byte[] pdbPayload = await File.ReadAllBytesAsync($"Symbols/Payloads/EpicGames.OIDC.pdb");

			string moduleName = "EpicGames.OIDC.pdb";

			{
				using ByteArrayContent requestContent = new ByteArrayContent(pdbPayload);
				requestContent.Headers.ContentType = new MediaTypeHeaderValue(MediaTypeNames.Application.Octet);
				HttpResponseMessage response =
					await _httpClient!.PutAsync(new Uri($"api/v1/symbols/{_testNamespaceName}/{moduleName}", UriKind.Relative), requestContent);
				response.EnsureSuccessStatusCode();

				PutSymbolResponse? putSymbolResponse = await response.Content.ReadFromJsonAsync<PutSymbolResponse>();
				Assert.IsNotNull(putSymbolResponse);
				Assert.AreEqual(moduleName, putSymbolResponse.ModuleName);
				Assert.AreEqual("CB6EC3206FE34712BF5245CA54A61FEE1", $"{putSymbolResponse.PdbIdentifier}{putSymbolResponse.PdbAge}");
			}
		}
	}

	[TestClass]
	public class MemorySymbolTests : SymbolTests
	{
		public MemorySymbolTests() : base("memory")
		{
		}
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[] { new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementations", UnrealCloudDDCSettings.ReferencesDbImplementations.Memory.ToString()) };
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
	public class ScyllaSymbolsTests : SymbolTests
	{
		public ScyllaSymbolsTests() : base("scylla")
		{
		}
		protected override IEnumerable<KeyValuePair<string, string?>> GetSettings()
		{
			return new[] { new KeyValuePair<string, string?>("UnrealCloudDDC:ReferencesDbImplementations", UnrealCloudDDCSettings.ReferencesDbImplementations.Scylla.ToString()) };
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
}
