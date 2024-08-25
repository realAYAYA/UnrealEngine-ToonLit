// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.Metrics;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Security.Claims;
using System.Threading.Tasks;
using Amazon.AutoScaling;
using Amazon.CloudWatch;
using Amazon.EC2;
using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.ObjectStores;
using Horde.Server.Accounts;
using Horde.Server.Acls;
using Horde.Server.Agents;
using Horde.Server.Agents.Enrollment;
using Horde.Server.Agents.Fleet;
using Horde.Server.Agents.Leases;
using Horde.Server.Agents.Pools;
using Horde.Server.Agents.Relay;
using Horde.Server.Agents.Sessions;
using Horde.Server.Agents.Utilization;
using Horde.Server.Artifacts;
using Horde.Server.Auditing;
using Horde.Server.Compute;
using Horde.Server.Configuration;
using Horde.Server.Dashboard;
using Horde.Server.Devices;
using Horde.Server.Issues;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Artifacts;
using Horde.Server.Jobs.Bisect;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.Schedules;
using Horde.Server.Jobs.Templates;
using Horde.Server.Jobs.TestData;
using Horde.Server.Jobs.Timing;
using Horde.Server.Logs;
using Horde.Server.Logs.Storage;
using Horde.Server.Notifications;
using Horde.Server.Perforce;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Streams;
using Horde.Server.Tasks;
using Horde.Server.Telemetry;
using Horde.Server.Telemetry.Sinks;
using Horde.Server.Tests.Server;
using Horde.Server.Tests.Stubs.Services;
using Horde.Server.Tools;
using Horde.Server.Ugs;
using Horde.Server.Users;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;
using OpenTelemetry.Trace;

namespace Horde.Server.Tests
{
	/// <summary>
	/// Handles set up of collections, services, fixtures etc during testing
	///
	/// Easier to pass all these things around in a single object.
	/// </summary>
	public class TestSetup : DatabaseIntegrationTest
	{
		public FakeClock Clock => ServiceProvider.GetRequiredService<FakeClock>();
		public IMemoryCache Cache => ServiceProvider.GetRequiredService<IMemoryCache>();

		public IGraphCollection GraphCollection => ServiceProvider.GetRequiredService<IGraphCollection>();
		public INotificationTriggerCollection NotificationTriggerCollection => ServiceProvider.GetRequiredService<INotificationTriggerCollection>();
		public IStreamCollection StreamCollection => ServiceProvider.GetRequiredService<IStreamCollection>();
		public IJobCollection JobCollection => ServiceProvider.GetRequiredService<IJobCollection>();
		public IAgentCollection AgentCollection => ServiceProvider.GetRequiredService<IAgentCollection>();
		public IJobStepRefCollection JobStepRefCollection => ServiceProvider.GetRequiredService<IJobStepRefCollection>();
		public IJobTimingCollection JobTimingCollection => ServiceProvider.GetRequiredService<IJobTimingCollection>();
		public IUgsMetadataCollection UgsMetadataCollection => ServiceProvider.GetRequiredService<IUgsMetadataCollection>();
		public IIssueCollection IssueCollection => ServiceProvider.GetRequiredService<IIssueCollection>();
		public IPoolCollection PoolCollection => ServiceProvider.GetRequiredService<IPoolCollection>();
		public ILeaseCollection LeaseCollection => ServiceProvider.GetRequiredService<ILeaseCollection>();
		public ILogFileCollection LogFileCollection => ServiceProvider.GetRequiredService<ILogFileCollection>();
		public ISessionCollection SessionCollection => ServiceProvider.GetRequiredService<ISessionCollection>();
		public ITestDataCollection TestDataCollection => ServiceProvider.GetRequiredService<ITestDataCollection>();
		public IUserCollection UserCollection => ServiceProvider.GetRequiredService<IUserCollection>();
		public IDeviceCollection DeviceCollection => ServiceProvider.GetRequiredService<IDeviceCollection>();
		public IDashboardPreviewCollection DashboardPreviewCollection => ServiceProvider.GetRequiredService<IDashboardPreviewCollection>();
		public IBisectTaskCollection BisectTaskCollection => ServiceProvider.GetRequiredService<IBisectTaskCollection>();

		public AclService AclService => ServiceProvider.GetRequiredService<AclService>();
		public FleetService FleetService => ServiceProvider.GetRequiredService<FleetService>();
		public AgentService AgentService => ServiceProvider.GetRequiredService<AgentService>();
		public AgentRelayService AgentRelayService => ServiceProvider.GetRequiredService<AgentRelayService>();
		public ICommitService CommitService => ServiceProvider.GetRequiredService<ICommitService>();
		public GlobalsService GlobalsService => ServiceProvider.GetRequiredService<GlobalsService>();
		public MongoService MongoService => ServiceProvider.GetRequiredService<MongoService>();
		public ITemplateCollection TemplateCollection => ServiceProvider.GetRequiredService<ITemplateCollection>();
		internal PerforceServiceStub PerforceService => (PerforceServiceStub)ServiceProvider.GetRequiredService<IPerforceService>();
		public ILogFileService LogFileService => ServiceProvider.GetRequiredService<ILogFileService>();
		public ISubscriptionCollection SubscriptionCollection => ServiceProvider.GetRequiredService<ISubscriptionCollection>();
		public INotificationService NotificationService => ServiceProvider.GetRequiredService<INotificationService>();
		public IssueService IssueService => ServiceProvider.GetRequiredService<IssueService>();
		public JobTaskSource JobTaskSource => ServiceProvider.GetRequiredService<JobTaskSource>();
		public JobService JobService => ServiceProvider.GetRequiredService<JobService>();
		public IArtifactCollectionV1 ArtifactCollection => ServiceProvider.GetRequiredService<IArtifactCollectionV1>();
		public IDowntimeService DowntimeService => ServiceProvider.GetRequiredService<IDowntimeService>();
		public RpcService RpcService => ServiceProvider.GetRequiredService<RpcService>();
		public PoolService PoolService => ServiceProvider.GetRequiredService<PoolService>();
		public LifetimeService LifetimeService => ServiceProvider.GetRequiredService<LifetimeService>();
		public ScheduleService ScheduleService => ServiceProvider.GetRequiredService<ScheduleService>();
		public DeviceService DeviceService => ServiceProvider.GetRequiredService<DeviceService>();
		public StorageService StorageService => ServiceProvider.GetRequiredService<StorageService>();
		public TestDataService TestDataService => ServiceProvider.GetRequiredService<TestDataService>();
		public ComputeService ComputeService => ServiceProvider.GetRequiredService<ComputeService>();
		public ServerStatusService ServerStatusService => ServiceProvider.GetRequiredService<ServerStatusService>();

		public ServerSettings ServerSettings => ServiceProvider.GetRequiredService<IOptions<ServerSettings>>().Value;
		public IOptionsMonitor<ServerSettings> ServerSettingsMon => ServiceProvider.GetRequiredService<IOptionsMonitor<ServerSettings>>();

		public JobsController JobsController => GetJobsController();
		public AgentsController AgentsController => GetAgentsController();
		public PoolsController PoolsController => GetPoolsController();
		public LeasesController LeasesController => GetLeasesController();
		public DevicesController DevicesController => GetDevicesController();
		public DashboardController DashboardController => GetDashboardController();
		public TestDataController TestDataController => GetTestDataController();
		public BisectTasksController BisectTasksController => GetBisectTasksController();

		public OpenTelemetry.Trace.Tracer Tracer => ServiceProvider.GetRequiredService<OpenTelemetry.Trace.Tracer>();
		public Meter Meter => ServiceProvider.GetRequiredService<Meter>();
		public ConfigService ConfigService => ServiceProvider.GetRequiredService<ConfigService>();
		public IOptionsMonitor<GlobalConfig> GlobalConfig => ServiceProvider.GetRequiredService<IOptionsMonitor<GlobalConfig>>();
		public IOptionsSnapshot<GlobalConfig> GlobalConfigSnapshot => ServiceProvider.GetRequiredService<IOptionsSnapshot<GlobalConfig>>();

		private static bool s_datadogWriterPatched;

		public TestSetup()
		{
			PatchDatadogWriter();
		}

		protected void SetConfig(GlobalConfig globalConfig)
		{
			globalConfig.PostLoad(ServerSettings);
			ConfigService.OverrideConfig(globalConfig);
		}

		protected void UpdateConfig(Action<GlobalConfig> action)
		{
			GlobalConfig globalConfig = GlobalConfig.CurrentValue;
			action(globalConfig);
			globalConfig.PostLoad(ServerSettings);
			ConfigService.OverrideConfig(globalConfig);
		}

		protected override void ConfigureSettings(ServerSettings settings)
		{
			DirectoryReference baseDir = DirectoryReference.Combine(ServerApp.DataDir, "Tests");
			try
			{
				FileUtils.ForceDeleteDirectoryContents(baseDir);
			}
			catch
			{
			}

			settings.WithAws = true;

			settings.ForceConfigUpdateOnStartup = true;
		}

		protected override void ConfigureServices(IServiceCollection services)
		{
			base.ConfigureServices(services);

			IConfiguration config = new ConfigurationBuilder().Build();
			services.Configure<ServerSettings>(ConfigureSettings);
			services.AddSingleton<IConfiguration>(config);

			services.AddHttpClient<RpcService>();

			services.AddLogging(builder => { builder.AddConsole().SetMinimumLevel(LogLevel.Debug); });
			services.AddSingleton<IMemoryCache>(sp => new MemoryCache(new MemoryCacheOptions { }));

			services.AddSingleton(typeof(IAuditLogFactory<>), typeof(AuditLogFactory<>));
			services.AddSingleton<IAuditLog<AgentId>>(sp => sp.GetRequiredService<IAuditLogFactory<AgentId>>().Create("Agents.Log", "AgentId"));
			services.AddSingleton<ITelemetrySink, NullTelemetrySink>();
			services.AddSingleton<ITelemetrySinkInternal, MetricTelemetrySink>();
			services.AddSingleton<OpenTelemetry.Trace.Tracer>(sp => TracerProvider.Default.GetTracer("TestTracer"));
			services.AddSingleton(sp => new Meter("TestMeter"));

			services.AddSingleton<ConfigService>();
			services.AddSingleton<IOptionsFactory<GlobalConfig>>(sp => sp.GetRequiredService<ConfigService>());
			services.AddSingleton<IOptionsChangeTokenSource<GlobalConfig>>(sp => sp.GetRequiredService<ConfigService>());

			services.AddSingleton<IConfigSource, FileConfigSource>();

			services.AddSingleton<IAccountCollection, AccountCollection>();
			services.AddSingleton<IAgentCollection, AgentCollection>();
			services.AddSingleton<IArtifactCollection, ArtifactCollection>();
			services.AddSingleton<IArtifactCollectionV1, ArtifactCollectionV1>();
			services.AddSingleton<ICommitService, CommitService>();
			services.AddSingleton<IGraphCollection, GraphCollection>();
			services.AddSingleton<IIssueCollection, IssueCollection>();
			services.AddSingleton<IJobCollection, JobCollection>();
			services.AddSingleton<IJobStepRefCollection, JobStepRefCollection>();
			services.AddSingleton<IJobTimingCollection, JobTimingCollection>();
			services.AddSingleton<ILeaseCollection, LeaseCollection>();
			services.AddSingleton<ILogEventCollection, LogEventCollection>();
			services.AddSingleton<ILogFileCollection, LogFileCollection>();
			services.AddSingleton<INotificationTriggerCollection, NotificationTriggerCollection>();
			services.AddSingleton<IPoolCollection, PoolCollection>();
			services.AddSingleton<IBisectTaskCollection, BisectTaskCollection>();
			services.AddSingleton<ISessionCollection, SessionCollection>();
			services.AddSingleton<ISubscriptionCollection, SubscriptionCollection>();
			services.AddSingleton<IStreamCollection, StreamCollection>();
			services.AddSingleton<ITemplateCollection, TemplateCollection>();
			services.AddSingleton<ITestDataCollection, TestDataCollection>();
			services.AddSingleton<IUtilizationDataCollection, UtilizationDataCollection>();
			services.AddSingleton<ITemplateCollection, TemplateCollection>();
			services.AddSingleton<IToolCollection, ToolCollection>();
			services.AddSingleton<IUgsMetadataCollection, UgsMetadataCollection>();
			services.AddSingleton<IUserCollection, UserCollectionV2>();
			services.AddSingleton<IDeviceCollection, DeviceCollection>();
			services.AddSingleton<IDashboardPreviewCollection, DashboardPreviewCollection>();

			services.AddSingleton<FakeClock>();
			services.AddSingleton<IClock>(sp => sp.GetRequiredService<FakeClock>());
			services.AddSingleton<IHostApplicationLifetime, AppLifetimeStub>();
			services.AddSingleton<IHostEnvironment, WebHostEnvironmentStub>();

			// Empty mocked object to satisfy basic test runs
			services.AddSingleton<IAmazonEC2>(sp => new Mock<IAmazonEC2>().Object);
			services.AddSingleton<IAmazonAutoScaling>(sp => new Mock<IAmazonAutoScaling>().Object);
			services.AddSingleton<IAmazonCloudWatch>(sp => new Mock<IAmazonCloudWatch>().Object);
			services.AddSingleton<IFleetManagerFactory, FleetManagerFactory>();

			services.AddSingleton<AclService>();
			services.AddSingleton<AgentService>();
			services.AddSingleton<AgentRelayService>();
			services.AddSingleton<AwsAutoScalingLifecycleService>();
			services.AddSingleton<ArtifactExpirationService>();
			services.AddSingleton<FleetService>();
			services.AddSingleton<ConsistencyService>();
			services.AddSingleton<RequestTrackerService>();
			services.AddSingleton<GlobalsService>();
			services.AddSingleton<JobTaskSource>();
			services.AddSingleton<IDowntimeService, DowntimeServiceStub>();
			services.AddSingleton<IssueService>();
			services.AddSingleton<JobService>();
			services.AddSingleton<LifetimeService>();
			services.AddSingleton<ILogStorage, NullLogStorage>();
			services.AddSingleton<ILogFileService, LogFileService>();
			services.AddSingleton<LogTailService>();
			services.AddSingleton<INotificationService, NotificationService>();
			services.AddSingleton<IPerforceService, PerforceServiceStub>();
			services.AddSingleton<PerforceLoadBalancer>();
			services.AddSingleton<PoolService>();
			services.AddSingleton<BisectService>();
			services.AddSingleton<RpcService>();
			services.AddSingleton<ScheduleService>();
			services.AddSingleton<DeviceService>();
			services.AddSingleton<TestDataService>();
			services.AddSingleton<ComputeService>();
			services.AddSingleton<EnrollmentService>();

			services.AddSingleton(typeof(IHealthMonitor<>), typeof(HealthMonitor<>));
			services.AddSingleton<ServerStatusService>();

			services.AddScoped<JobRpcCommon>();

			services.AddSingleton<ConformTaskSource>();
			services.AddSingleton<ICommitService, CommitService>();

			services.AddSingleton<FileObjectStoreFactory>();
			services.AddSingleton<IObjectStoreFactory, ObjectStoreFactory>();
			services.AddSingleton<IObjectStore<PersistentLogStorage>>(sp => new MemoryObjectStore().ForType<PersistentLogStorage>());
			services.AddSingleton<IObjectStore<ArtifactCollectionV1>>(sp => new MemoryObjectStore().ForType<ArtifactCollectionV1>());

			services.AddSingleton<StorageService>();
			services.AddSingleton<StorageBackendCache>();
			services.AddSingleton<BundleCache>();
		}

		public Task<Fixture> CreateFixtureAsync()
		{
			return Fixture.CreateAsync(ConfigService, GraphCollection, TemplateCollection, JobService, ArtifactCollection, AgentService, ServerSettings);
		}

		private JobsController GetJobsController()
		{
			JobsController jobsCtrl = ActivatorUtilities.CreateInstance<JobsController>(ServiceProvider);
			jobsCtrl.ControllerContext = GetControllerContext();
			return jobsCtrl;
		}

		private DevicesController GetDevicesController()
		{
			DevicesController devicesCtrl = ActivatorUtilities.CreateInstance<DevicesController>(ServiceProvider);
			devicesCtrl.ControllerContext = GetControllerContext();
			return devicesCtrl;
		}

		private DashboardController GetDashboardController()
		{
			DashboardController dashboardCtrl = ActivatorUtilities.CreateInstance<DashboardController>(ServiceProvider);
			dashboardCtrl.ControllerContext = GetControllerContext();
			return dashboardCtrl;
		}

		private TestDataController GetTestDataController()
		{
			TestDataController dataCtrl = ActivatorUtilities.CreateInstance<TestDataController>(ServiceProvider);
			dataCtrl.ControllerContext = GetControllerContext();
			return dataCtrl;
		}

		private BisectTasksController GetBisectTasksController()
		{
			BisectTasksController bisectCtrl = ActivatorUtilities.CreateInstance<BisectTasksController>(ServiceProvider);
			bisectCtrl.ControllerContext = GetControllerContext();
			return bisectCtrl;
		}

		private AgentsController GetAgentsController()
		{
			AgentsController agentCtrl = ActivatorUtilities.CreateInstance<AgentsController>(ServiceProvider);
			agentCtrl.ControllerContext = GetControllerContext();
			return agentCtrl;
		}

		private PoolsController GetPoolsController()
		{
			PoolsController controller = ActivatorUtilities.CreateInstance<PoolsController>(ServiceProvider);
			controller.ControllerContext = GetControllerContext();
			return controller;
		}

		private LeasesController GetLeasesController()
		{
			LeasesController controller = ActivatorUtilities.CreateInstance<LeasesController>(ServiceProvider);
			controller.ControllerContext = GetControllerContext();
			return controller;
		}

		private static ControllerContext GetControllerContext()
		{
			ControllerContext controllerContext = new ControllerContext();
			controllerContext.HttpContext = new DefaultHttpContext();
			controllerContext.HttpContext.User = new ClaimsPrincipal(new ClaimsIdentity(
				new List<Claim> { HordeClaims.AdminClaim.ToClaim() }, "TestAuthType"));
			return controllerContext;
		}

		private static int s_agentIdCounter = 1;
		public Task<IAgent> CreateAgentAsync(IPool pool, bool enabled = true, bool requestShutdown = false, List<string>? properties = null, TimeSpan? adjustClockBy = null)
		{
			return CreateAgentAsync(pool.Id, enabled, requestShutdown, properties, adjustClockBy);
		}

		/// <summary>
		/// Helper function for setting up agents to be used in tests
		/// </summary>
		/// <param name="poolId">Pool ID which the agent should belong to</param>
		/// <param name="enabled">Whether set the agent as enabled</param>
		/// <param name="requestShutdown">Mark it with a request for shutdown</param>
		/// <param name="properties">Any properties to assign</param>
		/// <param name="adjustClockBy">Time span to temporarily skew the clock when creating the agent</param>
		/// <param name="awsInstanceId">AWS instance ID for the agent (will be set in properties)</param>
		/// <param name="lease">A lease to assign the agent</param>
		/// <param name="ephemeral">Whether the agent is ephemeral</param>
		/// <returns>A new agent</returns>
		public async Task<IAgent> CreateAgentAsync(
			PoolId poolId, bool enabled = true, bool requestShutdown = false, List<string>? properties = null,
			TimeSpan? adjustClockBy = null, string? awsInstanceId = null, AgentLease? lease = null, bool ephemeral = false)
		{
			DateTime now = Clock.UtcNow;
			if (adjustClockBy != null)
			{
				Clock.UtcNow = now + adjustClockBy.Value;
			}

			Dictionary<string, int> resources = new();
			List<string> tempProps = new(properties ?? new List<string>());
			if (awsInstanceId != null)
			{
				tempProps.Add(KnownPropertyNames.AwsInstanceId + "=" + awsInstanceId);
			}

			IAgent? agent = await AgentService.CreateAgentAsync("TestAgent" + s_agentIdCounter++, ephemeral, "");
			Assert.IsNotNull(agent);

			agent = await AgentService.Agents.TryUpdateSettingsAsync(agent, enabled: enabled, pools: new List<PoolId> { poolId });
			Assert.IsNotNull(agent);

			agent = await AgentService.CreateSessionAsync(agent, AgentStatus.Ok, tempProps, resources, null);
			Assert.IsNotNull(agent);

			if (requestShutdown)
			{
				await AgentCollection.TryUpdateSettingsAsync(agent, requestShutdown: true);
			}

			if (lease != null)
			{
				await AgentCollection.TryAddLeaseAsync(agent, lease);
			}

			Clock.UtcNow = now;
			return agent;
		}

		/// <summary>
		/// Hack the Datadog tracing library to not block during shutdown of tests.
		/// Without this fix, the lib will try to send traces to a host that isn't running and block for +20 secs
		///
		/// Since so many of the interfaces and classes in the lib are internal it was difficult to replace Tracer.Instance
		/// </summary>
		private static void PatchDatadogWriter()
		{
			if (s_datadogWriterPatched)
			{
				return;
			}

			s_datadogWriterPatched = true;

			string msg = "Unable to patch Datadog agent writer! Tests will still work, but shutdown will block for +20 seconds.";

			FieldInfo? agentWriterField = Datadog.Trace.Tracer.Instance.GetType().GetField("_agentWriter", BindingFlags.NonPublic | BindingFlags.Instance);
			if (agentWriterField == null)
			{
				Console.Error.WriteLine(msg);
				return;
			}

			object? agentWriterInstance = agentWriterField.GetValue(Datadog.Trace.Tracer.Instance);
			if (agentWriterInstance == null)
			{
				Console.Error.WriteLine(msg);
				return;
			}

			FieldInfo? processExitField = agentWriterInstance.GetType().GetField("_processExit", BindingFlags.NonPublic | BindingFlags.Instance);
			if (processExitField == null)
			{
				Console.Error.WriteLine(msg);
				return;
			}

			TaskCompletionSource<bool>? processExitInstance = (TaskCompletionSource<bool>?)processExitField.GetValue(agentWriterInstance);
			if (processExitInstance == null)
			{
				Console.Error.WriteLine(msg);
				return;
			}

			processExitInstance.TrySetResult(true);
		}

		/// <summary>
		/// Find an available TCP/IP port
		/// </summary>
		/// <returns>Port number available</returns>
		public static int GetAvailablePort()
		{
			using TcpListener listener = new(IPAddress.Loopback, 0);
			listener.Start();
			int port = ((IPEndPoint)listener.LocalEndpoint).Port;
			listener.Stop();
			return port;
		}

		/// <summary>
		/// Create a console logger for tests
		/// </summary>
		/// <typeparam name="T">Type to instantiate</typeparam>
		/// <returns>A logger</returns>
		public static ILogger<T> CreateConsoleLogger<T>()
		{
			using ILoggerFactory loggerFactory = LoggerFactory.Create(builder =>
			{
				builder.SetMinimumLevel(LogLevel.Debug);
				builder.AddSimpleConsole(options => { options.SingleLine = true; });
			});

			return loggerFactory.CreateLogger<T>();
		}

		protected async Task<IPool> CreatePoolAsync(PoolConfig poolConfig)
		{
			UpdateConfig(config => config.Pools.Add(poolConfig));
			return await PoolCollection.GetAsync(poolConfig.Id) ?? throw new NotImplementedException();
		}
	}

	public class DowntimeServiceStub : IDowntimeService
	{
		public DowntimeServiceStub(bool isDowntimeActive = false)
		{
			IsDowntimeActive = isDowntimeActive;
		}

		public bool IsDowntimeActive { get; set; }
	}
}