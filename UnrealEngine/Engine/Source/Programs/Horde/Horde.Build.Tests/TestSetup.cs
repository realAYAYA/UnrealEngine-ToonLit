// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Security.Claims;
using System.Text.Json;
using System.Threading.Tasks;
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
using EpicGames.Horde.Storage.Bundles;
using Horde.Build.Compute;
using Horde.Build.Devices;

namespace Horde.Build.Tests
{
	using StreamId = StringId<IStream>;
	using ProjectId = StringId<IProject>;

	/// <summary>
	/// Handles set up of collections, services, fixtures etc during testing
	///
	/// Easier to pass all these things around in a single object.
	/// </summary>
	public class TestSetup : DatabaseIntegrationTest
	{
		public FakeClock Clock => ServiceProvider.GetRequiredService<FakeClock>();

		public ConfigCollection ConfigCollection => ServiceProvider.GetRequiredService<ConfigCollection>();
		public IGraphCollection GraphCollection => ServiceProvider.GetRequiredService<IGraphCollection>();
		public INotificationTriggerCollection NotificationTriggerCollection => ServiceProvider.GetRequiredService<INotificationTriggerCollection>();
		public IStreamCollection StreamCollection => ServiceProvider.GetRequiredService<IStreamCollection>();
		public IProjectCollection ProjectCollection => ServiceProvider.GetRequiredService<IProjectCollection>();
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
		public AgentSoftwareService AgentSoftwareService => ServiceProvider.GetRequiredService<AgentSoftwareService>();
		public AgentService AgentService => ServiceProvider.GetRequiredService<AgentService>();
		public ComputeService ComputeService => ServiceProvider.GetRequiredService<ComputeService>();
		public MongoService MongoService => ServiceProvider.GetRequiredService<MongoService>();
		public ITemplateCollection TemplateCollection => ServiceProvider.GetRequiredService<ITemplateCollection>();
		internal PerforceServiceStub PerforceService => (PerforceServiceStub)ServiceProvider.GetRequiredService<IPerforceService>();
		public ILogFileService LogFileService => ServiceProvider.GetRequiredService<ILogFileService>();
		public ProjectService ProjectService => ServiceProvider.GetRequiredService<ProjectService>();
		public StreamService StreamService => ServiceProvider.GetRequiredService<StreamService>();
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

		public ServerSettings ServerSettings => ServiceProvider.GetRequiredService<IOptions<ServerSettings>>().Value;
		public IOptionsMonitor<ServerSettings> ServerSettingsMon => ServiceProvider.GetRequiredService<IOptionsMonitor<ServerSettings>>();

		public JobsController JobsController => GetJobsController();
		public AgentsController AgentsController => GetAgentsController();
		public PoolsController PoolsController => GetPoolsController();
		public LeasesController LeasesController => GetLeasesController();
		public DevicesController DevicesController => GetDevicesController();

		private static bool s_datadogWriterPatched;

		public TestSetup()
		{
			PatchDatadogWriter();
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

			services.AddSingleton<IAgentCollection, AgentCollection>();
			services.AddSingleton<IAgentSoftwareCollection, AgentSoftwareCollection>();
			services.AddSingleton<IArtifactCollection, ArtifactCollection>();
			services.AddSingleton<ICommitCollection, CommitCollection>();
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
			services.AddSingleton<IProjectCollection, ProjectCollection>();
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

			services.AddSingleton<AclService>();
			services.AddSingleton<AgentService>();
			services.AddSingleton<AgentSoftwareService>();
			services.AddSingleton<AutoscaleService>();
			services.AddSingleton<AwsReuseFleetManager, AwsReuseFleetManager>();
			services.AddSingleton<ConsistencyService>();
			services.AddSingleton<ConfigCollection>();
			services.AddSingleton<ComputeService>();
			services.AddSingleton<RequestTrackerService>();
			services.AddSingleton<CredentialService>();
			services.AddSingleton<JobTaskSource>();
			services.AddSingleton<IDowntimeService, DowntimeServiceStub>();
			services.AddSingleton<IDogStatsd, NoOpDogStatsd>();
			services.AddSingleton<IssueService>();
			services.AddSingleton<IFleetManager, AwsReuseFleetManager>();
			services.AddSingleton<JobService>();
			services.AddSingleton<LifetimeService>();
			services.AddSingleton<ILogStorage, NullLogStorage>();
			services.AddSingleton<ILogBuilder, LocalLogBuilder>();
			services.AddSingleton<ILogFileService, LogFileService>();
			services.AddSingleton<INotificationService, NotificationService>();
			services.AddSingleton<IPerforceService, PerforceServiceStub>();
			services.AddSingleton<PerforceLoadBalancer>();
			services.AddSingleton<PoolService>();
			services.AddSingleton<ProjectService>();
			services.AddSingleton<RpcService>();
			services.AddSingleton<ScheduleService>();
			services.AddSingleton<StreamService>();
			services.AddSingleton<UpgradeService>();
			services.AddSingleton<DeviceService>();

			services.AddSingleton<ConformTaskSource>();
			services.AddSingleton<ICommitService, CommitService>();

			services.AddSingleton<IStorageBackend<PersistentLogStorage>>(sp => new TransientStorageBackend().ForType<PersistentLogStorage>());
			services.AddSingleton<IStorageBackend<ArtifactCollection>>(sp => new TransientStorageBackend().ForType<ArtifactCollection>());
			services.AddSingleton<IStorageBackend<BasicStorageClient>>(sp => new TransientStorageBackend().ForType<BasicStorageClient>());

			services.AddSingleton<ITreeStore<CommitService>>(sp =>
			{
				IBlobStore blobStore = new BasicBlobStore(sp.GetRequiredService<MongoService>(), new TransientStorageBackend(), sp.GetRequiredService<IMemoryCache>(), sp.GetRequiredService<ILogger<BasicBlobStore>>());
				return new BundleStore(blobStore, new BundleOptions()).ForType<CommitService>();
			});

			services.AddSingleton<IStorageClient, BasicStorageClient>();

			services.AddSingleton<ISingletonDocument<AgentSoftwareChannels>>(new SingletonDocumentStub<AgentSoftwareChannels>());
			services.AddSingleton<ISingletonDocument<DevicePlatformMapV1>>(new SingletonDocumentStub<DevicePlatformMapV1>());
		}

		public async Task<IStream?> CreateOrReplaceStreamAsync(StreamId streamId, IStream? stream, ProjectId projectId, StreamConfig config)
		{
			JsonSerializerOptions options = new JsonSerializerOptions();
			Startup.ConfigureJsonSerializer(options);

			byte[] data = JsonSerializer.SerializeToUtf8Bytes(config, options);
			IoHash hash = IoHash.Compute(data);
			string revision = hash.ToString();
			await ConfigCollection.AddConfigDataAsync(revision, data);

			return await StreamService.StreamCollection.TryCreateOrReplaceAsync(streamId, stream, hash.ToString(), projectId);
		}

		public Task<Fixture> CreateFixtureAsync()
		{
			return Fixture.Create(ConfigCollection, GraphCollection, TemplateCollection, JobService, ArtifactCollection, StreamService, AgentService);
		}

		private JobsController GetJobsController()
        {
			ILogger<JobsController> logger = ServiceProvider.GetRequiredService<ILogger<JobsController>>();
			JobsController jobsCtrl = new JobsController(GraphCollection, PerforceService, StreamService, JobService,
		        TemplateCollection, ArtifactCollection, UserCollection, NotificationService, AgentService, logger);
	        jobsCtrl.ControllerContext = GetControllerContext();
	        return jobsCtrl;
        }

		private DevicesController GetDevicesController()
		{
			ILogger<DevicesController> logger = ServiceProvider.GetRequiredService<ILogger<DevicesController>>();
			DevicesController devicesCtrl = new DevicesController(DeviceService, AclService, UserCollection, logger);
			devicesCtrl.ControllerContext = GetControllerContext();
			return devicesCtrl;
		}

		private AgentsController GetAgentsController()
		{
			AgentsController agentCtrl = new AgentsController(AclService, AgentService);
			agentCtrl.ControllerContext = GetControllerContext();
			return agentCtrl;
		}
		
		private PoolsController GetPoolsController()
		{
			PoolsController controller = new PoolsController(AclService, PoolService);
			controller.ControllerContext = GetControllerContext();
			return controller;
		}
		
		private LeasesController GetLeasesController()
		{
			LeasesController controller = new LeasesController(AclService, AgentService);
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
			IAgent agent = await AgentService.CreateAgentAsync("TestAgent" + s_agentIdCounter++, enabled, null, new List<StringId<IPool>> { pool.Id });
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
		
		private class DowntimeServiceStub : IDowntimeService
		{
			public bool IsDowntimeActive { get; } = false;
		}
	}
}