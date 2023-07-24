// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Security.Claims;
using System.Text.Json;
using System.Threading.Tasks;
using Amazon.AutoScaling;
using Amazon.EC2;
using Datadog.Trace;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Horde.Build.Acls;
using Horde.Build.Agents;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Sessions;
using Horde.Build.Projects;
using Horde.Build.Streams;
using Horde.Build.Issues;
using Horde.Build.Jobs;
using Horde.Build.Logs;
using Horde.Build.Logs.Builder;
using Horde.Build.Notifications;
using Horde.Build.Server;
using Horde.Build.Storage;
using Horde.Build.Storage.Services;
using Horde.Build.Tests.Stubs.Collections;
using Horde.Build.Tests.Stubs.Services;
using Horde.Build.Tools;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using StatsdClient;
using Horde.Build.Users;
using Horde.Build.Agents.Software;
using Horde.Build.Ugs;
using Horde.Build.Jobs.Timing;
using Horde.Build.Configuration;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs.TestData;
using Horde.Build.Jobs.Templates;
using Horde.Build.Jobs.Artifacts;
using Horde.Build.Secrets;
using Horde.Build.Jobs.Schedules;
using Horde.Build.Perforce;
using Horde.Build.Agents.Fleet.Providers;
using Horde.Build.Agents.Fleet;
using Horde.Build.Agents.Telemetry;
using Horde.Build.Logs.Storage;
using Horde.Build.Tasks;
using Horde.Build.Auditing;
using Horde.Build.Storage.Backends;
using Horde.Build.Compute;
using Horde.Build.Devices;
using Moq;
using Horde.Build.Telemetry;

namespace Horde.Build.Tests
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
		public IIssueCollection IssueCollection => ServiceProvider.GetRequiredService <IIssueCollection>();
		public IPoolCollection PoolCollection => ServiceProvider.GetRequiredService <IPoolCollection>();
		public ILeaseCollection LeaseCollection => ServiceProvider.GetRequiredService <ILeaseCollection>();
		public ISessionCollection SessionCollection => ServiceProvider.GetRequiredService <ISessionCollection>();
		public IAgentSoftwareCollection AgentSoftwareCollection => ServiceProvider.GetRequiredService <IAgentSoftwareCollection>();
		public ITestDataCollection TestDataCollection => ServiceProvider.GetRequiredService<ITestDataCollection>();
		public IUserCollection UserCollection => ServiceProvider.GetRequiredService<IUserCollection>();
		public IDeviceCollection DeviceCollection => ServiceProvider.GetRequiredService<IDeviceCollection>();

		public AclService AclService => ServiceProvider.GetRequiredService<AclService>();
		public FleetService FleetService => ServiceProvider.GetRequiredService<FleetService>();
		public AgentSoftwareService AgentSoftwareService => ServiceProvider.GetRequiredService<AgentSoftwareService>();
		public AgentService AgentService => ServiceProvider.GetRequiredService<AgentService>();
		public ICommitService CommitService => ServiceProvider.GetRequiredService<ICommitService>();
		public Horde.Build.Compute.V1.ComputeService ComputeService => ServiceProvider.GetRequiredService<Horde.Build.Compute.V1.ComputeService>();
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
		public IArtifactCollection ArtifactCollection => ServiceProvider.GetRequiredService<IArtifactCollection>();
		public IDowntimeService DowntimeService => ServiceProvider.GetRequiredService<IDowntimeService>();
		public RpcService RpcService => ServiceProvider.GetRequiredService<RpcService>();
		public CredentialService CredentialService => ServiceProvider.GetRequiredService<CredentialService>();
		public PoolService PoolService => ServiceProvider.GetRequiredService<PoolService>();
		public LifetimeService LifetimeService => ServiceProvider.GetRequiredService<LifetimeService>();
		public ScheduleService ScheduleService => ServiceProvider.GetRequiredService<ScheduleService>();
		public DeviceService DeviceService => ServiceProvider.GetRequiredService<DeviceService>();
		public StorageService StorageService => ServiceProvider.GetRequiredService<StorageService>();
		public TestDataService TestDataService => ServiceProvider.GetRequiredService<TestDataService>();

		public ServerSettings ServerSettings => ServiceProvider.GetRequiredService<IOptions<ServerSettings>>().Value;
		public IOptionsMonitor<ServerSettings> ServerSettingsMon => ServiceProvider.GetRequiredService<IOptionsMonitor<ServerSettings>>();

		public JobsController JobsController => GetJobsController();
		public AgentsController AgentsController => GetAgentsController();
		public PoolsController PoolsController => GetPoolsController();
		public LeasesController LeasesController => GetLeasesController();
		public DevicesController DevicesController => GetDevicesController();
		public TestDataController TestDataController => GetTestDataController();

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
			ConfigService.Set(IoHash.Zero, globalConfig);
		}

		protected void UpdateConfig(Action<GlobalConfig> action)
		{
			GlobalConfig globalConfig = GlobalConfig.CurrentValue;
			action(globalConfig);
			globalConfig.PostLoad(ServerSettings);
			ConfigService.Set(IoHash.Zero, globalConfig);
		}

		protected override void ConfigureSettings(ServerSettings settings)
		{
			DirectoryReference baseDir = DirectoryReference.Combine(Program.DataDir, "Tests");
			try
			{
				FileUtils.ForceDeleteDirectoryContents(baseDir);
			}
			catch
			{
			}

			settings.AdminClaimType = HordeClaimTypes.Role;
			settings.AdminClaimValue = "app-horde-admins";
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

			services.AddLogging(builder => builder.AddConsole());
			services.AddSingleton<IMemoryCache>(sp => new MemoryCache(new MemoryCacheOptions { }));

			services.AddSingleton(typeof(IAuditLogFactory<>), typeof(AuditLogFactory<>));
			services.AddSingleton<IAuditLog<AgentId>>(sp => sp.GetRequiredService<IAuditLogFactory<AgentId>>().Create("Agents.Log", "AgentId"));
			services.AddSingleton<ITelemetrySink, NullTelemetrySink>();

			services.AddSingleton<ConfigService>();
			services.AddSingleton<IOptionsFactory<GlobalConfig>>(sp => sp.GetRequiredService<ConfigService>());
			services.AddSingleton<IOptionsChangeTokenSource<GlobalConfig>>(sp => sp.GetRequiredService<ConfigService>());

			services.AddSingleton<IConfigSource, FileConfigSource>();

			services.AddSingleton<IAgentCollection, AgentCollection>();
			services.AddSingleton<IAgentSoftwareCollection, AgentSoftwareCollection>();
			services.AddSingleton<IArtifactCollection, ArtifactCollection>();
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
			services.AddSingleton<ISessionCollection, SessionCollection>();
			services.AddSingleton<ISubscriptionCollection, SubscriptionCollection>();
			services.AddSingleton<IStreamCollection, StreamCollection>();
			services.AddSingleton<ITemplateCollection, TemplateCollection>();
			services.AddSingleton<ITestDataCollection, TestDataCollection>();
			services.AddSingleton<ITelemetryCollection, TelemetryCollection>();
			services.AddSingleton<ITemplateCollection, TemplateCollection>();
			services.AddSingleton<IUgsMetadataCollection, UgsMetadataCollection>();
			services.AddSingleton<IUserCollection, UserCollectionV1>();
			services.AddSingleton<IDeviceCollection, DeviceCollection>();

			services.AddSingleton<ToolCollection>();

			services.AddSingleton<FakeClock>();
			services.AddSingleton<IClock>(sp => sp.GetRequiredService<FakeClock>());
			services.AddSingleton<IHostApplicationLifetime, AppLifetimeStub>();
			
			// Empty mocked object to satisfy basic test runs
			services.AddSingleton<IAmazonEC2>(sp => new Mock<IAmazonEC2>().Object);
			services.AddSingleton<IAmazonAutoScaling>(sp => new Mock<IAmazonAutoScaling>().Object);
			services.AddSingleton<IFleetManagerFactory, FleetManagerFactory>();

			services.AddSingleton<AclService>();
			services.AddSingleton<AgentService>();
			services.AddSingleton<AgentSoftwareService>();
			services.AddSingleton<FleetService>();
			services.AddSingleton<ConsistencyService>();
			services.AddSingleton<Horde.Build.Compute.V1.ComputeService>();
			services.AddSingleton<RequestTrackerService>();
			services.AddSingleton<GlobalsService>();
			services.AddSingleton<CredentialService>();
			services.AddSingleton<JobTaskSource>();
			services.AddSingleton<IDowntimeService, DowntimeServiceStub>();
			services.AddSingleton<IDogStatsd, NoOpDogStatsd>();
			services.AddSingleton<IssueService>();
			services.AddSingleton<JobService>();
			services.AddSingleton<LifetimeService>();
			services.AddSingleton<ILogStorage, NullLogStorage>();
			services.AddSingleton<ILogBuilder, LocalLogBuilder>();
			services.AddSingleton<ILogFileService, LogFileService>();
			services.AddSingleton<LogTailService>();
			services.AddSingleton<INotificationService, NotificationService>();
			services.AddSingleton<IPerforceService, PerforceServiceStub>();
			services.AddSingleton<PerforceLoadBalancer>();
			services.AddSingleton<PoolService>();
			services.AddSingleton<RpcService>();
			services.AddSingleton<ScheduleService>();
			services.AddSingleton<DeviceService>();
			services.AddSingleton<TestDataService>();

			services.AddSingleton<ConformTaskSource>();
			services.AddSingleton<ICommitService, CommitService>();

			services.AddSingleton<IStorageBackend<PersistentLogStorage>>(sp => new MemoryStorageBackend().ForType<PersistentLogStorage>());
			services.AddSingleton<IStorageBackend<ArtifactCollection>>(sp => new MemoryStorageBackend().ForType<ArtifactCollection>());
			services.AddSingleton<IStorageBackend<BasicStorageClient>>(sp => new MemoryStorageBackend().ForType<BasicStorageClient>());

			services.AddSingleton<ILegacyStorageClient, BasicStorageClient>();

			services.AddSingleton<StorageService>();

			services.AddSingleton<ISingletonDocument<AgentSoftwareChannels>>(new SingletonDocumentStub<AgentSoftwareChannels>());
			services.AddSingleton<ISingletonDocument<DevicePlatformMapV1>>(new SingletonDocumentStub<DevicePlatformMapV1>());
		}

		public Task<Fixture> CreateFixtureAsync()
		{
			return Fixture.Create(ConfigService, GraphCollection, TemplateCollection, JobService, ArtifactCollection, AgentService, ServerSettings);
		}

		private JobsController GetJobsController()
        {
			ILogger<JobsController> logger = ServiceProvider.GetRequiredService<ILogger<JobsController>>();
			JobsController jobsCtrl = new JobsController(GraphCollection, CommitService, PerforceService, StreamCollection, JobService,
		        TemplateCollection, ArtifactCollection, UserCollection, NotificationService, AgentService, GlobalConfigSnapshot, logger);
	        jobsCtrl.ControllerContext = GetControllerContext();
	        return jobsCtrl;
        }

		private DevicesController GetDevicesController()
		{
			ILogger<DevicesController> logger = ServiceProvider.GetRequiredService<ILogger<DevicesController>>();
			DevicesController devicesCtrl = new DevicesController(UserCollection, DeviceService, GlobalConfigSnapshot, logger);
			devicesCtrl.ControllerContext = GetControllerContext();
			return devicesCtrl;
		}

		private TestDataController GetTestDataController()
		{
			TestDataController dataCtrl = new TestDataController(TestDataService, StreamCollection, JobService, TestDataCollection, GlobalConfigSnapshot);
			dataCtrl.ControllerContext = GetControllerContext();
			return dataCtrl;
		}

		private AgentsController GetAgentsController()
		{
			AgentsController agentCtrl = new AgentsController(AgentService, GlobalConfigSnapshot);
			agentCtrl.ControllerContext = GetControllerContext();
			return agentCtrl;
		}
		
		private PoolsController GetPoolsController()
		{
			PoolsController controller = new PoolsController(PoolService, GlobalConfigSnapshot);
			controller.ControllerContext = GetControllerContext();
			return controller;
		}
		
		private LeasesController GetLeasesController()
		{
			LeasesController controller = new LeasesController(AgentService, GlobalConfigSnapshot);
			controller.ControllerContext = GetControllerContext();
			return controller;
		}
		
		private ControllerContext GetControllerContext()
		{
			ControllerContext controllerContext = new ControllerContext();
			controllerContext.HttpContext = new DefaultHttpContext();
			controllerContext.HttpContext.User = new ClaimsPrincipal(new ClaimsIdentity(
				new List<Claim> {new Claim(ServerSettings.AdminClaimType, ServerSettings.AdminClaimValue)}, "TestAuthType"));
			return controllerContext;
		}
		
		private static int s_agentIdCounter = 1;
		public async Task<IAgent> CreateAgentAsync(IPool pool, bool enabled = true)
		{
			IAgent agent = await AgentService.CreateAgentAsync("TestAgent" + s_agentIdCounter++, enabled, new List<StringId<IPool>> { pool.Id });
			agent = await AgentService.CreateSessionAsync(agent, AgentStatus.Ok, new List<string>(), new Dictionary<string, int>(), null);
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
			
			FieldInfo? agentWriterField = Tracer.Instance.GetType().GetField("_agentWriter", BindingFlags.NonPublic | BindingFlags.Instance);
			if (agentWriterField == null)
			{
				Console.Error.WriteLine(msg);
				return;
			}
			
			object? agentWriterInstance = agentWriterField.GetValue(Tracer.Instance);
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
			
			TaskCompletionSource<bool>? processExitInstance = (TaskCompletionSource<bool>?) processExitField.GetValue(agentWriterInstance);
			if (processExitInstance == null)
			{
				Console.Error.WriteLine(msg);
				return;
			}
			
			processExitInstance.TrySetResult(true);
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