// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Threading.Tasks;
using Amazon.CloudWatch;
using Horde.Build.Jobs;
using Horde.Build.Server;
using Horde.Build.Perforce;
using Horde.Build.Tests.Stubs.Services;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Horde.Build.Agents;
using Horde.Build.Streams;
using Horde.Build.Jobs.Templates;
using Horde.Build.Configuration;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs.Artifacts;
using Moq;

namespace Horde.Build.Tests;

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
		ConfigCollection configCollection = services.GetRequiredService<ConfigCollection>();
		MongoService mongoService = services.GetRequiredService<MongoService>();
		ITemplateCollection templateService = services.GetRequiredService<ITemplateCollection>();
		JobService jobService = services.GetRequiredService<JobService>();
		IArtifactCollection artifactCollection = services.GetRequiredService<IArtifactCollection>();
		StreamService streamService = services.GetRequiredService<StreamService>();
		AgentService agentService = services.GetRequiredService<AgentService>();
		GraphCollection graphCollection = new(mongoService);

		return await Fixture.Create(configCollection, graphCollection, templateService, jobService, artifactCollection, streamService,
			agentService);
	}
}