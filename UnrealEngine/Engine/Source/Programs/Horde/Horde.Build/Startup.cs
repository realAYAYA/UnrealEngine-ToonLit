// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using Amazon;
using Amazon.CloudWatch;
using Amazon.Extensions.NETCore.Setup;
using EpicGames.AspNet;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Grpc.Core;
using Grpc.Core.Interceptors;
using Horde.Build.Acls;
using Horde.Build.Authentication;
using Horde.Build.Agents;
using Horde.Build.Agents.Fleet;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Sessions;
using Horde.Build.Agents.Software;
using Horde.Build.Agents.Telemetry;
using Horde.Build.Compute;
using Horde.Build.Configuration;
using Horde.Build.Devices;
using Horde.Build.Issues;
using Horde.Build.Issues.External;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Artifacts;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Jobs.Schedules;
using Horde.Build.Jobs.Templates;
using Horde.Build.Jobs.TestData;
using Horde.Build.Jobs.Timing;
using Horde.Build.Logs;
using Horde.Build.Logs.Builder;
using Horde.Build.Logs.Storage;
using Horde.Build.Notifications;
using Horde.Build.Secrets;
using Horde.Build.Server;
using Horde.Build.Storage;
using Horde.Build.Storage.Backends;
using Horde.Build.Tasks;
using Horde.Build.Tools;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Authentication.Cookies;
using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.AspNetCore.Authentication.OpenIdConnect;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.DataProtection;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Hosting.Server.Features;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.HttpOverrides;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.ModelBinding;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Diagnostics.HealthChecks;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.OpenApi.Models;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Conventions;
using MongoDB.Bson.Serialization.Serializers;
using OpenTracing.Contrib.Grpc.Interceptors;
using OpenTracing.Util;
using Serilog;
using Serilog.Events;
using StackExchange.Redis;
using StatsdClient;
using Status = Grpc.Core.Status;
using Horde.Build.Users;
using Horde.Build.Perforce;
using Horde.Build.Projects;
using Horde.Build.Streams;
using Horde.Build.Ugs;
using Horde.Build.Auditing;
using Horde.Build.Agents.Fleet.Providers;
using Horde.Build.Server.Notices;
using Horde.Build.Notifications.Sinks;
using EpicGames.Horde.Storage.Bundles;
using StatusCode = Grpc.Core.StatusCode;
using Microsoft.Extensions.Caching.Memory;

namespace Horde.Build
{
	using JobId = ObjectId<IJob>;
	using LeaseId = ObjectId<ILease>;
	using LogId = ObjectId<ILogFile>;
	using UserId = ObjectId<IUser>;

	class Startup
	{
		class GrpcExceptionInterceptor : Interceptor
		{
			readonly ILogger<GrpcExceptionInterceptor> _logger;

			public GrpcExceptionInterceptor(ILogger<GrpcExceptionInterceptor> logger)
			{
				_logger = logger;
			}

			public override Task<TResponse> UnaryServerHandler<TRequest, TResponse>(TRequest request, ServerCallContext context, UnaryServerMethod<TRequest, TResponse> continuation)
			{
				return Guard(context, () => base.UnaryServerHandler(request, context, continuation));
			}

			public override Task<TResponse> ClientStreamingServerHandler<TRequest, TResponse>(IAsyncStreamReader<TRequest> requestStream, ServerCallContext context, ClientStreamingServerMethod<TRequest, TResponse> continuation) where TRequest : class where TResponse : class
			{
				return Guard(context, () => base.ClientStreamingServerHandler(requestStream, context, continuation));
			}

			public override Task ServerStreamingServerHandler<TRequest, TResponse>(TRequest request, IServerStreamWriter<TResponse> responseStream, ServerCallContext context, ServerStreamingServerMethod<TRequest, TResponse> continuation) where TRequest : class where TResponse : class
			{
				return Guard(context, () => base.ServerStreamingServerHandler(request, responseStream, context, continuation));
			}

			public override Task DuplexStreamingServerHandler<TRequest, TResponse>(IAsyncStreamReader<TRequest> requestStream, IServerStreamWriter<TResponse> responseStream, ServerCallContext context, DuplexStreamingServerMethod<TRequest, TResponse> continuation) where TRequest : class where TResponse : class
			{
				return Guard(context, () => base.DuplexStreamingServerHandler(requestStream, responseStream, context, continuation));
			}

			async Task<T> Guard<T>(ServerCallContext context, Func<Task<T>> callFunc) where T : class
			{
				T result = null!;
				await Guard(context, async () => { result = await callFunc(); });
				return result;
			}

			async Task Guard(ServerCallContext context, Func<Task> callFunc)
			{
				HttpContext httpContext = context.GetHttpContext();

				AgentId? agentId = AclService.GetAgentId(httpContext.User);
				if (agentId != null)
				{
					using IDisposable scope = _logger.BeginScope("Agent: {AgentId}, RemoteIP: {RemoteIP}, Method: {Method}", agentId.Value, httpContext.Connection.RemoteIpAddress, context.Method);
					await GuardInner(context, callFunc);
				}
				else
				{
					using IDisposable scope = _logger.BeginScope("RemoteIP: {RemoteIP}, Method: {Method}", httpContext.Connection.RemoteIpAddress, context.Method);
					await GuardInner(context, callFunc);
				}
			}

			async Task GuardInner(ServerCallContext context, Func<Task> callFunc)
			{
				try
				{
					await callFunc();
				}
				catch (StructuredRpcException ex)
				{
#pragma warning disable CA2254 // Template should be a static expression
					_logger.LogError(ex, ex.Format, ex.Args);
#pragma warning restore CA2254 // Template should be a static expression
					throw;
				}
				catch (Exception ex)
				{
					if (context.CancellationToken.IsCancellationRequested)
					{
						_logger.LogInformation(ex, "Call to method {Method} was cancelled", context.Method);
						throw;
					}
					else
					{
						_logger.LogError(ex, "Exception in call to {Method}", context.Method);
						throw new RpcException(new Status(StatusCode.Internal, $"An exception was thrown on the server: {ex}"));
					}
				}
			}
		}

		class BsonSerializationProvider : IBsonSerializationProvider
		{
			public IBsonSerializer? GetSerializer(Type type)
			{
				if (type == typeof(ContentHash))
				{
					return new ContentHashSerializer();
				}
				if (type == typeof(DateTimeOffset))
				{
					return new DateTimeOffsetStringSerializer();
				}
				return null;
			}
		}

		class JsonObjectIdConverter : JsonConverter<ObjectId>
		{
			public override ObjectId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
			{
				string? str = reader.GetString();
				if (str == null)
				{
					throw new InvalidDataException("Unable to parse object id");
				}
				return str.ToObjectId();
			}

			public override void Write(Utf8JsonWriter writer, ObjectId objectId, JsonSerializerOptions options)
			{
				writer.WriteStringValue(objectId.ToString());
			}
		}
		
		class JsonDateTimeConverter : JsonConverter<DateTime>
		{
			public override DateTime Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
			{
				Debug.Assert(typeToConvert == typeof(DateTime));

				string? str = reader.GetString();
				if (str == null)
				{
					throw new InvalidDataException("Unable to parse DateTime");
				}
				return DateTime.Parse(str, CultureInfo.CurrentCulture);
			}

			public override void Write(Utf8JsonWriter writer, DateTime dateTime, JsonSerializerOptions options)
			{
				writer.WriteStringValue(dateTime.ToUniversalTime().ToString("yyyy'-'MM'-'dd'T'HH':'mm':'ssZ", CultureInfo.CurrentCulture));
			}
		}

		class JsonTimeSpanConverter : JsonConverter<TimeSpan>
		{
			public override TimeSpan Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
			{
				string? str = reader.GetString();
				if (str == null)
				{
					throw new InvalidDataException("Unable to parse TimeSpan");
				}
				return TimeSpan.Parse(str, CultureInfo.CurrentCulture);
			}

			public override void Write(Utf8JsonWriter writer, TimeSpan timeSpan, JsonSerializerOptions options)
			{
				writer.WriteStringValue(timeSpan.ToString("c"));
			}
		}

		static IStorageBackend CreateStorageBackend(IServiceProvider sp, StorageBackendOptions options)
		{
			switch (options.Type ?? StorageBackendType.FileSystem)
			{
				case StorageBackendType.FileSystem:
					return new FileSystemStorageBackend(options);
				case StorageBackendType.Aws:
					return new AwsStorageBackend(sp.GetRequiredService<IConfiguration>(), options, sp.GetRequiredService<ILogger<AwsStorageBackend>>());
				case StorageBackendType.Transient:
					return new TransientStorageBackend();
				case StorageBackendType.Relay:
					return new RelayStorageBackend(options);
				default:
					throw new NotImplementedException();
			}
		}

		static IBlobStore CreateBlobStore(IServiceProvider sp, BlobStoreOptions options)
		{
			IStorageBackend backend = CreateStorageBackend(sp, options);
			return new BasicBlobStore(sp.GetRequiredService<MongoService>(), backend, sp.GetRequiredService<IMemoryCache>(), sp.GetRequiredService<ILogger<BasicBlobStore>>());
		}

		static ITreeStore CreateTreeStore(IServiceProvider sp, TreeStoreOptions options)
		{
			IBlobStore store = CreateBlobStore(sp, options);
			return new BundleStore(store, options.Bundle);
		}

		public Startup(IConfiguration configuration)
		{
			Configuration = configuration;
		}

		public IConfiguration Configuration { get; }

		// This method gets called *multiple times* by the runtime. Use this method to add services to the container.
		public void ConfigureServices(IServiceCollection services)
		{
			// IOptionsMonitor pattern for live updating of configuration settings
			services.Configure<ServerSettings>(Configuration.GetSection("Horde"));

			// Settings used for configuring services
			IConfigurationSection configSection = Configuration.GetSection("Horde");
			ServerSettings settings = new ServerSettings();
			configSection.Bind(settings);

			settings.Validate();

			services.Configure<CommitServiceOptions>(configSection.GetSection("Commits"));
			services.Configure<ReplicationServiceOptions>(configSection.GetSection("Replication"));

			if (settings.GlobalThreadPoolMinSize != null)
			{
				// Min thread pool size is set to combat timeouts seen with the Redis client.
				// See comments for <see cref="ServerSettings.GlobalThreadPoolMinSize" /> and
				// https://github.com/StackExchange/StackExchange.Redis/issues/1680
				int min = settings.GlobalThreadPoolMinSize.Value;
				ThreadPool.SetMinThreads(min, min);
			}

#pragma warning disable CA2000 // Dispose objects before losing scope
			RedisService redisService = new RedisService(settings);
#pragma warning restore CA2000 // Dispose objects before losing scope
			services.AddSingleton<RedisService>(sp => redisService);
			services.AddDataProtection().PersistKeysToStackExchangeRedis(() => redisService.Database, "aspnet-data-protection");

			if (settings.CorsEnabled)
			{
				services.AddCors(options =>
				{
					options.AddPolicy("CorsPolicy",
						builder => builder.WithOrigins(settings.CorsOrigin.Split(";"))
						.AllowAnyMethod()
						.AllowAnyHeader()
						.AllowCredentials());
				});
			}

			services.AddGrpc(options =>
			{
				options.EnableDetailedErrors = true;
				options.MaxReceiveMessageSize = 200 * 1024 * 1024; // 100 MB (packaged builds of Horde agent can be large) 
				options.Interceptors.Add(typeof(GrpcExceptionInterceptor));
				options.Interceptors.Add<ServerTracingInterceptor>(GlobalTracer.Instance);
			});
			services.AddGrpcReflection();

			services.AddHttpClient<RpcService>();

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
			services.AddSingleton<IServiceAccountCollection, ServiceAccountCollection>();
			services.AddSingleton<ISubscriptionCollection, SubscriptionCollection>();
			services.AddSingleton<IStreamCollection, StreamCollection>();
			services.AddSingleton<ITemplateCollection, TemplateCollection>();
			services.AddSingleton<ITestDataCollection, TestDataCollection>();
			services.AddSingleton<ITelemetryCollection, TelemetryCollection>();
			services.AddSingleton<ITemplateCollection, TemplateCollection>();
			services.AddSingleton<IUgsMetadataCollection, UgsMetadataCollection>();
			services.AddSingleton<IUserCollection, UserCollectionV2>();
			services.AddSingleton<IDeviceCollection, DeviceCollection>();
			services.AddSingleton<INoticeCollection, NoticeCollection>();

			services.AddSingleton<ConfigCollection>();
			services.AddSingleton<ToolCollection>();

			// Auditing
			services.AddSingleton<IAuditLog<AgentId>>(sp => sp.GetRequiredService<IAuditLogFactory<AgentId>>().Create("Agents.Log", "AgentId"));

			services.AddSingleton(typeof(IAuditLogFactory<>), typeof(AuditLogFactory<>));
			services.AddSingleton(typeof(ISingletonDocument<>), typeof(SingletonDocument<>));

			services.AddSingleton<AutoscaleService>();
			services.AddSingleton<AutoscaleServiceV2>();
			services.AddSingleton<LeaseUtilizationStrategy>();
			services.AddSingleton<JobQueueStrategy>();
			services.AddSingleton<NoOpPoolSizeStrategy>();
			services.AddSingleton<ComputeQueueAwsMetricStrategy>();
			
			switch (settings.FleetManager)
			{
				case FleetManagerType.Aws:
					services.AddSingleton<IFleetManager, AwsReuseFleetManager>();
					break;
				default:
					services.AddSingleton<IFleetManager, DefaultFleetManager>();
					break;
			}

			services.AddSingleton<AclService>();
			services.AddSingleton<AgentService>();			
			services.AddSingleton<AgentSoftwareService>();
			services.AddSingleton<ConsistencyService>();
			services.AddSingleton<RequestTrackerService>();
			services.AddSingleton<CredentialService>();
			services.AddSingleton<MongoService>();
			services.AddSingleton<IDogStatsd>(ctx =>
			{
				string? datadogAgentHost = Environment.GetEnvironmentVariable("DD_AGENT_HOST");
				if (datadogAgentHost != null)
				{
					// Datadog agent is configured, enable DogStatsD for metric collection
					StatsdConfig config = new StatsdConfig
					{
						StatsdServerName = datadogAgentHost,
						StatsdPort = 8125,
					};

					DogStatsdService dogStatsdService = new DogStatsdService();
					dogStatsdService.Configure(config);
					return dogStatsdService;
				}
				return new NoOpDogStatsd();
			});
			services.AddSingleton<CommitService>();
			services.AddSingleton<ICommitService>(sp => sp.GetRequiredService<CommitService>());
			services.AddSingleton<IClock, Clock>();
			services.AddSingleton<IDowntimeService, DowntimeService>();
			services.AddSingleton<IssueService>();
			services.AddSingleton<JobService>();
			services.AddSingleton<LifetimeService>();
			services.AddSingleton<ILogFileService, LogFileService>();
			services.AddSingleton<INotificationService, NotificationService>();
			services.AddSingleton<IPerforceService, PerforceService>();

			services.AddSingleton<PerforceLoadBalancer>();
			services.AddSingleton<PoolService>();
			services.AddSingleton<ProjectService>();
			services.AddSingleton<ReplicationService>();
			services.AddSingleton<ScheduleService>();
			services.AddSingleton<SlackNotificationSink>();
			services.AddSingleton<IAvatarService, SlackNotificationSink>(sp => sp.GetRequiredService<SlackNotificationSink>());
			services.AddSingleton<INotificationSink, SlackNotificationSink>(sp => sp.GetRequiredService<SlackNotificationSink>());
			services.AddSingleton<StreamService>();
			services.AddSingleton<UpgradeService>();
			services.AddSingleton<DeviceService>();						
			services.AddSingleton<NoticeService>();

			if (settings.JiraUrl != null)
			{
				services.AddSingleton<IExternalIssueService, JiraService>();
			}
			else
			{
				services.AddSingleton<IExternalIssueService, DefaultExternalIssueService>();
			}

			// Storage providers
			services.AddSingleton(sp => CreateStorageBackend(sp, settings.LogStorage).ForType<PersistentLogStorage>());
			services.AddSingleton(sp => CreateStorageBackend(sp, settings.ArtifactStorage).ForType<ArtifactCollection>());
			services.AddSingleton(sp => CreateTreeStore(sp, settings.CommitStorage).ForType<ReplicationService>());

			services.AddHordeStorage(settings => configSection.GetSection("Storage").Bind(settings));
			
			AWSOptions awsOptions = Configuration.GetAWSOptions();
			services.AddDefaultAWSOptions(awsOptions);
			if (awsOptions.Region == null && Environment.GetEnvironmentVariable("AWS_REGION") == null)
			{
				awsOptions.Region = RegionEndpoint.USEast1;
			}
			
			services.AddAWSService<IAmazonCloudWatch>();

			ConfigureLogStorage(services);

			AuthenticationBuilder authBuilder = services.AddAuthentication(options =>
				{
					switch (settings.AuthMethod)
					{
						case AuthMethod.Anonymous:
							options.DefaultAuthenticateScheme = AnonymousAuthenticationHandler.AuthenticationScheme;
							options.DefaultSignInScheme = AnonymousAuthenticationHandler.AuthenticationScheme;
							options.DefaultChallengeScheme = AnonymousAuthenticationHandler.AuthenticationScheme;
							break;
						
						case AuthMethod.Okta:
							// If an authentication cookie is present, use it to get authentication information
							options.DefaultAuthenticateScheme = CookieAuthenticationDefaults.AuthenticationScheme;
							options.DefaultSignInScheme = CookieAuthenticationDefaults.AuthenticationScheme;

							// If authentication is required, and no cookie is present, use OIDC to sign in
							options.DefaultChallengeScheme = OktaDefaults.AuthenticationScheme;
							break;
						
						case AuthMethod.OpenIdConnect:
							// If an authentication cookie is present, use it to get authentication information
							options.DefaultAuthenticateScheme = CookieAuthenticationDefaults.AuthenticationScheme;
							options.DefaultSignInScheme = CookieAuthenticationDefaults.AuthenticationScheme;

							// If authentication is required, and no cookie is present, use OIDC to sign in
							options.DefaultChallengeScheme = OpenIdConnectDefaults.AuthenticationScheme;
							break;
						
						default:
							throw new ArgumentException($"Invalid auth method {settings.AuthMethod}");
					}
				});

			List<string> schemes = new List<string>();

			authBuilder.AddCookie(options =>
				 {
					 options.Events.OnValidatePrincipal = context =>
					 {
						 if (context.Principal?.FindFirst(HordeClaimTypes.UserId) == null)
						 {
							 context.RejectPrincipal();
						 }
						 return Task.CompletedTask;
					 };

					 options.Events.OnRedirectToAccessDenied = context =>
					 {
						 context.Response.StatusCode = StatusCodes.Status403Forbidden;
						 return context.Response.CompleteAsync();
					 };
				 });
			schemes.Add(CookieAuthenticationDefaults.AuthenticationScheme);

			authBuilder.AddServiceAccount(options => { });
			schemes.Add(ServiceAccountAuthHandler.AuthenticationScheme);

			
			switch (settings.AuthMethod)
			{
				case AuthMethod.Anonymous:
					authBuilder.AddAnonymous(options =>
					{
						options.AdminClaimType = settings.AdminClaimType;
						options.AdminClaimValue = settings.AdminClaimValue;
					});
					schemes.Add(AnonymousAuthenticationHandler.AuthenticationScheme);
					break;
						
				case AuthMethod.Okta:
					authBuilder.AddOkta(OktaDefaults.AuthenticationScheme, OpenIdConnectDefaults.DisplayName, options =>
						{
							options.Authority = settings.OidcAuthority;
							options.ClientId = settings.OidcClientId;

							if (!String.IsNullOrEmpty(settings.OidcSigninRedirect))
							{
								options.Events = new OpenIdConnectEvents
								{
									OnRedirectToIdentityProvider = async redirectContext =>
									{
										redirectContext.ProtocolMessage.RedirectUri = settings.OidcSigninRedirect;
										await Task.CompletedTask;
									}
								};
							}
						});
					schemes.Add(OktaDefaults.AuthenticationScheme);
					break;
						
				case AuthMethod.OpenIdConnect:
					authBuilder.AddHordeOpenId(settings, OpenIdConnectDefaults.AuthenticationScheme, OpenIdConnectDefaults.DisplayName, options =>
						{
							options.Authority = settings.OidcAuthority;
							options.ClientId = settings.OidcClientId;
							options.ClientSecret = settings.OidcClientSecret;
							foreach (string scope in settings.OidcRequestedScopes)
							{
								options.Scope.Add(scope);
							}

							if (!String.IsNullOrEmpty(settings.OidcSigninRedirect))
							{
								options.Events = new OpenIdConnectEvents
								{
									OnRedirectToIdentityProvider = async redirectContext =>
									{
										redirectContext.ProtocolMessage.RedirectUri = settings.OidcSigninRedirect;
										await Task.CompletedTask;
									}
								};
							}
						});
					schemes.Add(OpenIdConnectDefaults.AuthenticationScheme);
					break;
						
				default:
					throw new ArgumentException($"Invalid auth method {settings.AuthMethod}");
			}

			authBuilder.AddScheme<JwtBearerOptions, HordeJwtBearerHandler>(HordeJwtBearerHandler.AuthenticationScheme, options => { });
			schemes.Add(HordeJwtBearerHandler.AuthenticationScheme);

			services.AddAuthorization(options =>
				{
					options.DefaultPolicy = new AuthorizationPolicyBuilder(schemes.ToArray())
						.RequireAuthenticatedUser()							
						.Build();
				});
			
			// Hosted service that needs to run no matter the run mode of the process (server vs worker)
			services.AddHostedService(provider => (DowntimeService)provider.GetRequiredService<IDowntimeService>());

			if (settings.IsRunModeActive(RunMode.Worker) && !settings.DatabaseReadOnlyMode)
			{
				services.AddHostedService<MongoUpgradeService>();

				services.AddHostedService(provider => provider.GetRequiredService<AutoscaleServiceV2>());
				
				services.AddHostedService(provider => provider.GetRequiredService<AgentService>());
				services.AddHostedService(provider => provider.GetRequiredService<CommitService>());
				services.AddHostedService(provider => provider.GetRequiredService<ConsistencyService>());
				services.AddHostedService(provider => provider.GetRequiredService<IssueService>());
				services.AddHostedService<IssueReportService>();
				services.AddHostedService(provider => (LogFileService)provider.GetRequiredService<ILogFileService>());
				services.AddHostedService(provider => (NotificationService)provider.GetRequiredService<INotificationService>());
				services.AddHostedService(provider => provider.GetRequiredService<ReplicationService>());
				if (!settings.DisableSchedules)
				{
					services.AddHostedService(provider => provider.GetRequiredService<ScheduleService>());
				}

				services.AddHostedService<MetricService>();
				services.AddHostedService(provider => provider.GetRequiredService<PerforceLoadBalancer>());
				services.AddHostedService<PoolUpdateService>();
				services.AddHostedService(provider => provider.GetRequiredService<SlackNotificationSink>());
				services.AddHostedService<ConfigUpdateService>();
				services.AddHostedService<TelemetryService>();
				services.AddHostedService(provider => provider.GetRequiredService<DeviceService>());
			}

			services.AddHostedService(provider => provider.GetRequiredService<IExternalIssueService>());
				
			// Task sources. Order of registration is important here; it dictates the priority in which sources are served.
			services.AddSingleton<JobTaskSource>();
			services.AddHostedService<JobTaskSource>(provider => provider.GetRequiredService<JobTaskSource>());
			services.AddSingleton<ConformTaskSource>();
			services.AddHostedService<ConformTaskSource>(provider => provider.GetRequiredService<ConformTaskSource>());
			services.AddSingleton<ComputeService>();
			services.AddSingleton<IComputeService, ComputeService>();
			services.AddHostedService(provider => provider.GetRequiredService<ComputeService>());

			services.AddSingleton<ITaskSource, UpgradeTaskSource>();
			services.AddSingleton<ITaskSource, ShutdownTaskSource>();
			services.AddSingleton<ITaskSource, RestartTaskSource>();
			services.AddSingleton<ITaskSource, ConformTaskSource>(provider => provider.GetRequiredService<ConformTaskSource>());
			services.AddSingleton<ITaskSource, JobTaskSource>(provider => provider.GetRequiredService<JobTaskSource>());
			services.AddSingleton<ITaskSource, ComputeService>();

			services.AddHostedService(provider => provider.GetRequiredService<ConformTaskSource>());

			// Allow longer to shutdown so we can debug missing cancellation tokens
			services.Configure<HostOptions>(options =>
			{
				options.ShutdownTimeout = TimeSpan.FromSeconds(30.0);
			});

			// Allow forwarded headers
			services.Configure<ForwardedHeadersOptions>(options =>
			{
				options.ForwardedHeaders = ForwardedHeaders.XForwardedFor | ForwardedHeaders.XForwardedProto;
				options.KnownProxies.Clear();
				options.KnownNetworks.Clear();
			});

			services.AddMvc().AddJsonOptions(options => ConfigureJsonSerializer(options.JsonSerializerOptions));

			services.AddControllers(options =>
			{
				options.InputFormatters.Add(new CbInputFormatter());
				options.OutputFormatters.Add(new CbOutputFormatter());
				options.OutputFormatters.Insert(0, new CbPreferredOutputFormatter());
				options.FormatterMappings.SetMediaTypeMappingForFormat("uecb", CustomMediaTypeNames.UnrealCompactBinary);
			});

			services.AddSwaggerGen(config =>
			{
				config.SwaggerDoc("v1", new OpenApiInfo { Title = "Horde Server API", Version = "v1" });
				config.IncludeXmlComments(Path.Combine(AppContext.BaseDirectory, $"{Assembly.GetExecutingAssembly().GetName().Name}.xml"));
			});

			services.Configure<ApiBehaviorOptions>(options =>
			{
				options.InvalidModelStateResponseFactory = context =>
				{
					foreach(KeyValuePair<string, ModelStateEntry> pair in context.ModelState)
					{
						ModelError? error = pair.Value.Errors.FirstOrDefault();
						if (error != null)
						{
							string message = error.ErrorMessage;
							if (String.IsNullOrEmpty(message))
							{
								message = error.Exception?.Message ?? "Invalid error object";
							}
							return new BadRequestObjectResult(EpicGames.Core.LogEvent.Create(LogLevel.Error, KnownLogEvents.None, error.Exception, "Invalid value for {Name}: {Message}", pair.Key, message));
						}
					}
					return new BadRequestObjectResult(context.ModelState);
				};
			});

			DirectoryReference dashboardDir = DirectoryReference.Combine(Program.AppDir, "DashboardApp");
			if (DirectoryReference.Exists(dashboardDir)) 
			{
				services.AddSpaStaticFiles(config => {config.RootPath = "DashboardApp";});
			}

			ConfigureMongoDbClient();
			ConfigureFormatters();

			OnAddHealthChecks(services);
		}

		public static void ConfigureFormatters()
		{
			LogValueFormatter.RegisterTypeAnnotation<AgentId>("AgentId");
			LogValueFormatter.RegisterTypeAnnotation<JobId>("JobId");
			LogValueFormatter.RegisterTypeAnnotation<LeaseId>("LeaseId");
			LogValueFormatter.RegisterTypeAnnotation<LogId>("LogId");
			LogValueFormatter.RegisterTypeAnnotation<UserId>("UserId");
		}

		public static void ConfigureJsonSerializer(JsonSerializerOptions options)
		{
			options.DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull;
			options.PropertyNamingPolicy = JsonNamingPolicy.CamelCase;
			options.PropertyNameCaseInsensitive = true;
			options.Converters.Add(new JsonObjectIdConverter());
			options.Converters.Add(new JsonStringEnumConverter());
			options.Converters.Add(new JsonKnownTypesConverterFactory());
			options.Converters.Add(new JsonStringIdConverterFactory());
			options.Converters.Add(new JsonDateTimeConverter());
			options.Converters.Add(new JsonTimeSpanConverter());
		}

		private static void ConfigureLogStorage(IServiceCollection services)
		{
			services.AddSingleton<ILogBuilder>(provider =>
			{
				RedisService? redisService = provider.GetService<RedisService>();
				if(redisService == null)
				{
					return new LocalLogBuilder();
				}
				else
				{
					return new RedisLogBuilder(redisService.ConnectionPool, provider.GetRequiredService<ILogger<RedisLogBuilder>>());
				}
			});

			services.AddSingleton<PersistentLogStorage>();

			services.AddSingleton<ILogStorage>(provider =>
			{
				ILogStorage storage = provider.GetRequiredService<PersistentLogStorage>();

//				IDatabase? RedisDb = Provider.GetService<IDatabase>();
//				if (RedisDb != null)
//				{
//					Storage = new RedisLogStorage(RedisDb, Provider.GetService<ILogger<RedisLogStorage>>(), Storage);
//				}

				storage = new SequencedLogStorage(storage);
				storage = new LocalLogStorage(50, storage);
				return storage;
			});
		}
		/*
				private static void ConfigureLogFileWriteCache(IServiceCollection Services, ServerSettings Settings)
				{
					bool RedisConfigured = !String.IsNullOrEmpty(Settings.RedisConnectionConfig);
					string CacheType = Settings.LogServiceWriteCacheType.ToLower(CultureInfo.CurrentCulture);

					if (CacheType == "inmemory")
					{
						Services.AddSingleton<ILogFileWriteCache2>(Sp => new InMemoryLogFileWriteCache2());
					}
					else if (CacheType == "redis" && RedisConfigured)
					{
						Services.AddSingleton<ILogFileWriteCache2>(Sp => new RedisLogFileWriteCache2(Sp.GetService<ILogger<RedisLogFileWriteCache>>(), Sp.GetService<IDatabase>()));
					}
					else if (CacheType == "redis" && !RedisConfigured)
					{
						throw new Exception("Redis must be configured to use the Redis-backed log write cache");
					}
					else
					{
						throw new Exception("Unknown value set for LogServiceWriteCacheType in config: " + Settings.LogServiceWriteCacheType);
					}
				}
		*/

		public sealed class BlobIdBsonSerializer : SerializerBase<BlobId>
		{
			/// <inheritdoc/>
			public override BlobId Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
			{
				return new BlobId(context.Reader.ReadString());
			}

			/// <inheritdoc/>
			public override void Serialize(BsonSerializationContext context, BsonSerializationArgs args, BlobId value)
			{
				context.Writer.WriteString(value.ToString());
			}
		}

		public sealed class RefIdBsonSerializer : SerializerBase<RefId>
		{
			/// <inheritdoc/>
			public override RefId Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
			{
				return new RefId(IoHash.Parse(context.Reader.ReadString()));
			}

			/// <inheritdoc/>
			public override void Serialize(BsonSerializationContext context, BsonSerializationArgs args, RefId value)
			{
				context.Writer.WriteString(value.ToString());
			}
		}

		public sealed class RefNameBsonSerializer : SerializerBase<RefName>
		{
			/// <inheritdoc/>
			public override RefName Deserialize(BsonDeserializationContext context, BsonDeserializationArgs args)
			{
				return new RefName(context.Reader.ReadString());
			}

			/// <inheritdoc/>
			public override void Serialize(BsonSerializationContext context, BsonSerializationArgs args, RefName value)
			{
				context.Writer.WriteString(value.ToString());
			}
		}

		static int s_haveConfiguredMongoDb = 0;

		public static void ConfigureMongoDbClient()
		{
			if (Interlocked.CompareExchange(ref s_haveConfiguredMongoDb, 1, 0) == 0)
			{
				// Ignore extra elements on deserialized documents
				ConventionPack conventionPack = new ConventionPack();
				conventionPack.Add(new IgnoreExtraElementsConvention(true));
				conventionPack.Add(new EnumRepresentationConvention(BsonType.String));
				ConventionRegistry.Register("Horde", conventionPack, type => true);

				// Register the custom serializers
				BsonSerializer.RegisterSerializer(new BlobIdBsonSerializer());
				BsonSerializer.RegisterSerializer(new RefIdBsonSerializer());
				BsonSerializer.RegisterSerializer(new RefNameBsonSerializer());
				BsonSerializer.RegisterSerializer(new ConditionSerializer());
				BsonSerializer.RegisterSerializationProvider(new BsonSerializationProvider());
				BsonSerializer.RegisterSerializationProvider(new StringIdSerializationProvider());
				BsonSerializer.RegisterSerializationProvider(new ObjectIdSerializationProvider());

                // Register all the custom class maps
                BsonClassMap.RegisterClassMap<AclV2>(AclV2.ConfigureClassMap);
			}
		}

		private static void OnAddHealthChecks(IServiceCollection services)
		{
			IHealthChecksBuilder healthChecks = services.AddHealthChecks().AddCheck("self", () => HealthCheckResult.Healthy(), tags: new[] { "self" });
		}

		// This method gets called by the runtime. Use this method to configure the HTTP request pipeline.
		public static void Configure(IApplicationBuilder app, IWebHostEnvironment env, Microsoft.Extensions.Hosting.IHostApplicationLifetime lifetime, IOptions<ServerSettings> settings)
		{
			app.UseForwardedHeaders();
			app.UseExceptionHandler("/api/v1/error");

			// Used for allowing auth cookies in combination with OpenID Connect auth (for example, Google Auth did not work with these unset)
			app.UseCookiePolicy(new CookiePolicyOptions()
			{
				MinimumSameSitePolicy = SameSiteMode.None,
				CheckConsentNeeded = _ => true
			});

			if (settings.Value.CorsEnabled)
			{
				app.UseCors("CorsPolicy");
			}

			// Enable middleware to serve generated Swagger as a JSON endpoint.
			app.UseSwagger();

			// Enable serilog request logging
			app.UseSerilogRequestLogging(options =>
			{
				options.GetLevel = GetRequestLoggingLevel;
				options.EnrichDiagnosticContext = (diagnosticContext, httpContext) =>
				{
					diagnosticContext.Set("RemoteIP", httpContext.Connection.RemoteIpAddress);
				};
			});

			// Enable middleware to serve swagger-ui (HTML, JS, CSS, etc.),
			// specifying the Swagger JSON endpoint.
			app.UseSwaggerUI(c =>
			{
				c.SwaggerEndpoint("/swagger/v1/swagger.json", "Horde Server API");
				c.RoutePrefix = "swagger";
			});

			if (!env.IsDevelopment())
			{
				app.UseMiddleware<RequestTrackerMiddleware>();	
			}

			app.UseDefaultFiles();
			app.UseStaticFiles();

			DirectoryReference dashboardDir = DirectoryReference.Combine(Program.AppDir, "DashboardApp");

			if (DirectoryReference.Exists(dashboardDir)) 
			{
				app.UseSpaStaticFiles();
			}
						
			app.UseRouting();

			app.UseAuthentication();
			app.UseAuthorization();

			app.UseEndpoints(endpoints =>
			{
				endpoints.MapGrpcService<HealthService>();
				endpoints.MapGrpcService<RpcService>();

				endpoints.MapGrpcReflectionService();

				endpoints.MapControllers();
			});

			if (DirectoryReference.Exists(dashboardDir)) 
			{
				app.UseSpa(spa =>
				{ 
					spa.Options.SourcePath = "DashboardApp";        
				});
			}

			if (settings.Value.OpenBrowser && RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				lifetime.ApplicationStarted.Register(() => LaunchBrowser(app));
			}
		}

		static void LaunchBrowser(IApplicationBuilder app)
		{
			IServerAddressesFeature? feature = app.ServerFeatures.Get<IServerAddressesFeature>();
			if (feature != null && feature.Addresses.Count > 0)
			{
				// with a development cert, host will be set by default to localhost, otherwise there will be no host in address
				string address = feature.Addresses.First().Replace("[::]", System.Net.Dns.GetHostName(), StringComparison.OrdinalIgnoreCase);
				Process.Start(new ProcessStartInfo { FileName = address, UseShellExecute = true });
			}
		}

		static LogEventLevel GetRequestLoggingLevel(HttpContext context, double elapsedMs, Exception ex)
		{
			if (context.Request != null && context.Request.Path.HasValue)
			{
				string requestPath = context.Request.Path;
				if (requestPath.Equals("/Horde.HordeRpc/QueryServerStateV2", StringComparison.OrdinalIgnoreCase))
				{
					return LogEventLevel.Verbose;
				}
				if (requestPath.Equals("/Horde.HordeRpc/UpdateSession", StringComparison.OrdinalIgnoreCase))
				{
					return LogEventLevel.Verbose;
				}
				if (requestPath.Equals("/Horde.HordeRpc/CreateEvents", StringComparison.OrdinalIgnoreCase))
				{
					return LogEventLevel.Verbose;
				}
				if (requestPath.Equals("/Horde.HordeRpc/WriteOutput", StringComparison.OrdinalIgnoreCase))
				{
					return LogEventLevel.Verbose;
				}
			}
			return LogEventLevel.Information;
		}

		public static void AddServices(IServiceCollection serviceCollection, IConfiguration configuration)
		{
			Startup startup = new Startup(configuration);
			startup.ConfigureServices(serviceCollection);
		}

		public static IServiceProvider CreateServiceProvider(IConfiguration configuration)
		{
			IServiceCollection serviceCollection = new ServiceCollection();
			AddServices(serviceCollection, configuration);
			return serviceCollection.BuildServiceProvider();
		}

		public static ServiceProvider CreateServiceProvider(IConfiguration configuration, ILoggerProvider loggerProvider)
		{
			IServiceCollection services = new ServiceCollection();
			services.AddSingleton(configuration);
			services.AddSingleton(loggerProvider);
			AddServices(services, configuration);
			return services.BuildServiceProvider();
		}
	}
}
