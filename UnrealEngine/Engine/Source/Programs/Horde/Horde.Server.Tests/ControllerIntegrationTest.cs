// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Threading.Tasks;
using Amazon.CloudWatch;
using Horde.Server.Agents;
using Horde.Server.Configuration;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Artifacts;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.Templates;
using Horde.Server.Perforce;
using Horde.Server.Server;
using Horde.Server.Tests.Stubs.Services;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Moq;
using Serilog;

namespace Horde.Server.Tests;

static class SerilogExtensions
{
	public static LoggerConfiguration Override<T>(this Serilog.Configuration.LoggerMinimumLevelConfiguration configuration, Serilog.Events.LogEventLevel minimumLevel) where T : class
	{
		return configuration.Override(typeof(T).FullName!, minimumLevel);
	}
}

public class TestWebApplicationFactory<TStartup> : WebApplicationFactory<TStartup> where TStartup : class
{
	private readonly MongoInstance _mongoInstance;
	private readonly RedisInstance _redisInstance;
	private readonly Dictionary<string, string> _extraSettings;

	public TestWebApplicationFactory(MongoInstance mongoInstance, RedisInstance redisInstance, Dictionary<string, string>? extraSettings = null)
	{
		_mongoInstance = mongoInstance;
		_redisInstance = redisInstance;
		_extraSettings = extraSettings ?? new Dictionary<string, string>();

		Environment.SetEnvironmentVariable("ASPNETCORE_ENVIRONMENT", "Testing");
		Serilog.Log.Logger = new LoggerConfiguration()
			.Enrich.FromLogContext()
			.WriteTo.Console()
			.MinimumLevel.Information()
			.MinimumLevel.Override<MongoService>(Serilog.Events.LogEventLevel.Warning)
			.MinimumLevel.Override("Redis", Serilog.Events.LogEventLevel.Warning)
			.CreateLogger();
	}

	protected override void ConfigureWebHost(IWebHostBuilder builder)
	{
		Dictionary<string, string?> dict = new()
		{
			{ "Horde:DatabaseConnectionString", _mongoInstance.ConnectionString },
			{ "Horde:DatabaseName", _mongoInstance.DatabaseName },
			{ "Horde:LogServiceWriteCacheType", "inmemory" },
			{ "Horde:AuthMethod", "Anonymous" },
			{ "Horde:OidcAuthority", null },
			{ "Horde:OidcClientId", null },

			{ "Horde:RedisConnectionConfig", _redisInstance.ConnectionString },
		};

		foreach ((string key, string value) in _extraSettings)
		{
			dict[key] = value;
		}

		Mock<IAmazonCloudWatch> cloudWatchMock = new(MockBehavior.Strict);
		builder.ConfigureAppConfiguration((hostingContext, config) => { config.AddInMemoryCollection(dict); });
		builder.ConfigureTestServices(collection =>
		{
			collection.AddSingleton<IPerforceService, PerforceServiceStub>();
			collection.AddSingleton<IAmazonCloudWatch>(x => cloudWatchMock.Object);
		});
	}
}

public class FakeHordeWebApp : IAsyncDisposable
{
	public MongoInstance MongoInstance { get; }
	public RedisInstance RedisInstance { get; }
	public HttpClient HttpClient { get; }
	public IServiceProvider ServiceProvider => Factory.Services;
	private TestWebApplicationFactory<Startup> Factory { get; }

	public FakeHordeWebApp(Dictionary<string, string>? settings = null, bool allowAutoRedirect = true)
	{
		MongoInstance = new MongoInstance();
		RedisInstance = new RedisInstance();
		Factory = new TestWebApplicationFactory<Startup>(MongoInstance, RedisInstance, settings);
		WebApplicationFactoryClientOptions opts = new() { AllowAutoRedirect = allowAutoRedirect };
		HttpClient = Factory.CreateClient(opts);
	}

	public async ValueTask DisposeAsync()
	{
		try
		{
			await Factory.DisposeAsync();
			MongoInstance.Dispose();
			RedisInstance.Dispose();
		}
		catch (Exception ex)
		{
			Console.WriteLine($"Exception running cleanup: {ex}");
			throw;
		}

		GC.SuppressFinalize(this);
	}
}

public class ControllerIntegrationTest : IAsyncDisposable
{
	protected HttpClient Client => _app.HttpClient;
	protected IServiceProvider ServiceProvider => _app.ServiceProvider;
	private readonly Lazy<Task<Fixture>> _fixture;
	private readonly FakeHordeWebApp _app;

	public ControllerIntegrationTest()
	{
		_app = new FakeHordeWebApp();
		_fixture = new Lazy<Task<Fixture>>(CreateFixtureAsync);
	}

	public virtual async ValueTask DisposeAsync()
	{
		await _app.DisposeAsync();
		GC.SuppressFinalize(this);
	}

	public Task<Fixture> GetFixtureAsync()
	{
		return _fixture.Value;
	}

	private async Task<Fixture> CreateFixtureAsync()
	{
		IServiceProvider services = _app.ServiceProvider;
		ConfigService configService = services.GetRequiredService<ConfigService>();
		ITemplateCollection templateService = services.GetRequiredService<ITemplateCollection>();
		JobService jobService = services.GetRequiredService<JobService>();
		IArtifactCollectionV1 artifactCollection = services.GetRequiredService<IArtifactCollectionV1>();
		AgentService agentService = services.GetRequiredService<AgentService>();
		IGraphCollection graphCollection = services.GetRequiredService<IGraphCollection>();
		IOptions<ServerSettings> serverSettings = services.GetRequiredService<IOptions<ServerSettings>>();

		return await Fixture.CreateAsync(configService, graphCollection, templateService, jobService, artifactCollection, agentService, serverSettings.Value);
	}
}