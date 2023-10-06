// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Threading.Tasks;
using Amazon.CloudWatch;
using Horde.Server.Jobs;
using Horde.Server.Server;
using Horde.Server.Perforce;
using Horde.Server.Tests.Stubs.Services;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Horde.Server.Agents;
using Horde.Server.Streams;
using Horde.Server.Jobs.Templates;
using Horde.Server.Configuration;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.Artifacts;
using Moq;
using Microsoft.Extensions.Options;

namespace Horde.Server.Tests;

public class TestWebApplicationFactory<TStartup> : WebApplicationFactory<TStartup> where TStartup : class
{
	private readonly MongoDbInstance _mongoDbInstance;

	public TestWebApplicationFactory(MongoDbInstance mongoDbInstance)
	{
		_mongoDbInstance = mongoDbInstance;
	}

	protected override void ConfigureWebHost(IWebHostBuilder builder)
	{
		Dictionary<string, string?> dict = new()
		{
			{ "Horde:DatabaseConnectionString", _mongoDbInstance.ConnectionString },
			{ "Horde:DatabaseName", _mongoDbInstance.DatabaseName },
			{ "Horde:LogServiceWriteCacheType", "inmemory" },
			{ "Horde:DisableAuth", "true" },
			{ "Horde:OidcAuthority", null },
			{ "Horde:OidcClientId", null }
		};

		Mock<IAmazonCloudWatch> cloudWatchMock = new (MockBehavior.Strict);
		builder.ConfigureAppConfiguration((hostingContext, config) => { config.AddInMemoryCollection(dict); });
		builder.ConfigureTestServices(collection =>
		{
			collection.AddSingleton<IPerforceService, PerforceServiceStub>();
			collection.AddSingleton<IAmazonCloudWatch>(x => cloudWatchMock.Object);
		});
	}
}

public class ControllerIntegrationTest : IDisposable
{
	private readonly Lazy<Task<Fixture>> _fixture;

	public ControllerIntegrationTest()
	{
		MongoDbInstance = new MongoDbInstance();
		Factory = new TestWebApplicationFactory<Startup>(MongoDbInstance);
		Client = Factory.CreateClient();

		_fixture = new Lazy<Task<Fixture>>(Task.Run(() => CreateFixture()));
	}

	protected MongoDbInstance MongoDbInstance { get; }
	private TestWebApplicationFactory<Startup> Factory { get; }
	protected HttpClient Client { get; }

	public void Dispose()
	{
		Dispose(true);
		GC.SuppressFinalize(this);
	}

	protected virtual void Dispose(bool disposing)
	{
		MongoDbInstance.Dispose();
	}

	public Task<Fixture> GetFixture()
	{
		return _fixture.Value;
	}

	private async Task<Fixture> CreateFixture()
	{
		IServiceProvider services = Factory.Services;
		ConfigService configService = services.GetRequiredService<ConfigService>();
		ITemplateCollection templateService = services.GetRequiredService<ITemplateCollection>();
		JobService jobService = services.GetRequiredService<JobService>();
		IArtifactCollectionV1 artifactCollection = services.GetRequiredService<IArtifactCollectionV1>();
		AgentService agentService = services.GetRequiredService<AgentService>();
		IGraphCollection graphCollection = services.GetRequiredService<IGraphCollection>();
		IOptions<ServerSettings> serverSettings = services.GetRequiredService<IOptions<ServerSettings>>();

		return await Fixture.Create(configService, graphCollection, templateService, jobService, artifactCollection, agentService, serverSettings.Value);
	}
}