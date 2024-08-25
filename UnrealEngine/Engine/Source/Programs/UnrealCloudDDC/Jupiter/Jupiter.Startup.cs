// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Security;
using System.Runtime.InteropServices;
using System.Security.Authentication;
using System.Threading;
using Amazon.Extensions.NETCore.Setup;
using Amazon.Runtime;
using Amazon.S3;
using Amazon.SecretsManager;
using Cassandra;
using Jupiter.Controllers;
using Jupiter.Implementation;
using Jupiter.Implementation.Blob;
using Jupiter.Implementation.LeaderElection;
using Jupiter.Common.Implementation;
using Jupiter.Implementation.Objects;
using Jupiter.Implementation.TransactionLog;
using Microsoft.AspNetCore.Authorization;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using StatsdClient;
using Microsoft.Extensions.Logging;

namespace Jupiter
{
	// ReSharper disable once ClassNeverInstantiated.Global
	public class JupiterStartup : BaseStartup
	{
		public JupiterStartup(IConfiguration configuration) : base(configuration, CreateLogger<JupiterStartup>())
		{
			string? ddAgentHost = System.Environment.GetEnvironmentVariable("DD_AGENT_HOST");
			if (!string.IsNullOrEmpty(ddAgentHost))
			{
				Logger.LogInformation("Initializing Dogstatsd to connect to: {DatadogAgentHost}", ddAgentHost);
				StatsdConfig dogstatsdConfig = new StatsdConfig
				{
					StatsdServerName = ddAgentHost,
					StatsdPort = 8125,
				};

				DogStatsd.Configure(dogstatsdConfig);
			}
		}

		protected override void OnAddAuthorization(AuthorizationOptions authorizationOptions, List<string> defaultSchemes)
		{
  
		}

		protected override void OnAddService(IServiceCollection services)
		{
			services.AddHttpClient(Options.DefaultName, (provider, client) =>
			{
				static string GetPlatformName()
				{
					if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
					{
						return "Linux";
					} 
					else if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
					{
						return "Windows";
					} 
					else if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
					{
						return "OSX";
					} 
					else
					{
						throw new NotSupportedException("Unknown OS when formatting platform name");
					}
				}
				IVersionFile? versionFile = provider.GetService<IVersionFile>();
				string engineVersion = "5.1"; // TODO: we should read this from the version file
				string? version = versionFile?.VersionString ?? "unknown";
				string platformName = GetPlatformName();
				client.DefaultRequestHeaders.TryAddWithoutValidation("UserAgent", $"UnrealEngine/{engineVersion} ({platformName}) Jupiter/{version}");
			});

			services.AddOptions<UnrealCloudDDCSettings>().Bind(Configuration.GetSection("UnrealCloudDDC")).ValidateDataAnnotations();
			services.AddOptions<MongoSettings>().Bind(Configuration.GetSection("Mongo")).ValidateDataAnnotations();

			services.AddOptions<S3Settings>().Bind(Configuration.GetSection("S3")).ValidateDataAnnotations();
			services.AddOptions<AzureSettings>().Bind(Configuration.GetSection("Azure")).ValidateDataAnnotations();
			services.AddOptions<FilesystemSettings>().Bind(Configuration.GetSection("Filesystem")).ValidateDataAnnotations();

			services.AddOptions<NginxSettings>().Bind(Configuration.GetSection("Nginx")).ValidateDataAnnotations();

			services.AddOptions<ConsistencyCheckSettings>().Bind(Configuration.GetSection("ConsistencyCheck")).ValidateDataAnnotations();

			services.AddOptions<BufferedPayloadOptions>().Bind(Configuration.GetSection("PayloadBuffering")).ValidateDataAnnotations();

			services.AddOptions<UpstreamRelaySettings>().Configure(o => Configuration.GetSection("Upstream").Bind(o)).ValidateDataAnnotations();
			services.AddOptions<ClusterSettings>().Configure(o => Configuration.GetSection("Cluster").Bind(o)).ValidateDataAnnotations();

			services.AddOptions<GCSettings>().Configure(o => Configuration.GetSection("GC").Bind(o)).ValidateDataAnnotations();
			services.AddOptions<ReplicationSettings>().Configure(o => Configuration.GetSection("Replication").Bind(o)).ValidateDataAnnotations();
			services.AddOptions<ServiceCredentialSettings>().Configure(o => Configuration.GetSection("ServiceCredentials").Bind(o)).ValidateDataAnnotations();
			services.AddOptions<SnapshotSettings>().Configure(o => Configuration.GetSection("Snapshot").Bind(o)).ValidateDataAnnotations();
			services.AddOptions<MetricsServiceSettings>().Configure(o => Configuration.GetSection("Metrics").Bind(o)).ValidateDataAnnotations();

			services.AddOptions<ScyllaSettings>().Configure(o => Configuration.GetSection("Scylla").Bind(o)).ValidateDataAnnotations();

			services.AddOptions<KubernetesLeaderElectionSettings>().Configure(o => Configuration.GetSection("Kubernetes").Bind(o)).ValidateDataAnnotations();
			services.AddOptions<StaticPeerServiceDiscoverySettings>().Configure(o => Configuration.GetSection("ServiceDiscovery").Bind(o)).ValidateDataAnnotations();

			services.AddOptions<MemoryCacheReferencesSettings>().Configure(o => Configuration.GetSection("CacheRef").Bind(o)).ValidateDataAnnotations();
			services.AddOptions<MemoryCacheContentIdSettings>().Configure(o => Configuration.GetSection("CacheContentId").Bind(o)).ValidateDataAnnotations();

			services.AddSingleton(typeof(CompressedBufferUtils), CreateCompressedBufferUtils);

			services.AddSingleton<AWSCredentials>(provider =>
			{
				AWSCredentialsSettings awsSettings = provider.GetService<IOptionsMonitor<AWSCredentialsSettings>>()!.CurrentValue;

				return AWSCredentialsHelper.GetCredentials(awsSettings, "Jupiter");
			});
			services.AddSingleton<BufferedPayloadFactory>();
			services.AddSingleton<ReplicationLogFactory>();

			services.AddSingleton<BlobCleanupService>();
			services.AddHostedService<BlobCleanupService>(p => p.GetService<BlobCleanupService>()!);

			services.AddSingleton(serviceType: typeof(IRefService), typeof(ObjectService));
			services.AddSingleton(serviceType: typeof(IReferencesStore), ObjectStoreFactory);
			services.AddSingleton(serviceType: typeof(IReferenceResolver), typeof(ReferenceResolver));
			services.AddSingleton(serviceType: typeof(IContentIdStore), ContentIdStoreFactory);

			services.AddSingleton(serviceType: typeof(IBlobIndex), BlobIndexFactory);
			services.AddSingleton(serviceType: typeof(IAmazonS3), CreateS3);

			services.AddSingleton<AmazonS3Store>();
			services.AddSingleton<FileSystemStore>();
			services.AddSingleton<AzureBlobStore>();
			services.AddSingleton<PeerBlobStore>();
			services.AddSingleton<MemoryBlobStore>();
			services.AddSingleton<RelayBlobStore>();

			services.AddSingleton<OrphanBlobCleanupRefs>();

			services.AddSingleton(typeof(IBlobService), typeof(BlobService));
			services.AddSingleton(serviceType: typeof(IScyllaSessionManager), ScyllaFactory);

			services.AddSingleton(serviceType: typeof(IReplicationLog), ReplicationLogWriterFactory);
			
			services.AddSingleton<LastAccessTrackerReference>();
			services.AddSingleton(serviceType: typeof(ILastAccessCache<LastAccessRecord>), p => p.GetService<LastAccessTrackerReference>()!);
			services.AddSingleton(serviceType: typeof(ILastAccessTracker<LastAccessRecord>), p => p.GetService<LastAccessTrackerReference>()!);

			services.AddSingleton(serviceType: typeof(ILeaderElection), CreateLeaderElection);

			services.AddTransient<IRequestHelper, RequestHelper>();
			services.AddSingleton<BufferedPayloadFactory>();
			services.AddSingleton(Configuration);

			services.AddSingleton<FormatResolver>();
			services.AddSingleton<NginxRedirectHelper>();

			services.AddSingleton(serviceType: typeof(ISecretResolver), typeof(SecretResolver));
			services.AddSingleton(typeof(IAmazonSecretsManager), CreateAWSSecretsManager);

			services.AddSingleton<LastAccessServiceReferences>();
			services.AddHostedService<LastAccessServiceReferences>(p => p.GetService<LastAccessServiceReferences>()!);

			services.AddSingleton<IServiceCredentials, ServiceCredentials>(p => ActivatorUtilities.CreateInstance<ServiceCredentials>(p));
			
			services.AddSingleton<ReplicationService>();
			services.AddHostedService<ReplicationService>(p => p.GetService<ReplicationService>()!);

			services.AddSingleton<ReplicationSnapshotService>();
			services.AddHostedService<ReplicationSnapshotService>(p => p.GetService<ReplicationSnapshotService>()!);

			services.AddSingleton<BlobStoreConsistencyCheckService>();
			services.AddHostedService<BlobStoreConsistencyCheckService>(p => p.GetService<BlobStoreConsistencyCheckService>()!);

			services.AddSingleton<BlobIndexConsistencyCheckService>();
			services.AddHostedService<BlobIndexConsistencyCheckService>(p => p.GetService<BlobIndexConsistencyCheckService>()!);

			services.AddSingleton<RefStoreConsistencyCheckService>();
			services.AddHostedService<RefStoreConsistencyCheckService>(p => p.GetService<RefStoreConsistencyCheckService>()!);

			services.AddSingleton<MetricsService>();
			services.AddHostedService<MetricsService>(p => p.GetService<MetricsService>()!);

			services.AddSingleton(typeof(IPeerStatusService), typeof(PeerStatusService));
			services.AddHostedService<PeerStatusService>(p => (PeerStatusService)p.GetService<IPeerStatusService>()!);
		
			services.AddTransient(typeof(IRefCleanup), typeof(RefLastAccessCleanup));

			services.AddTransient(typeof(VersionFile), typeof(VersionFile));

			services.AddSingleton<RefCleanupService>();
			services.AddHostedService<RefCleanupService>(p => p.GetService<RefCleanupService>()!);

			// This will technically create a new settings object, but as we do not dynamically update it the settings will reflect what we actually want
			UnrealCloudDDCSettings settings = services.BuildServiceProvider().GetService<IOptionsMonitor<UnrealCloudDDCSettings>>()!.CurrentValue!;

			if (settings.LeaderElectionImplementation == UnrealCloudDDCSettings.LeaderElectionImplementations.Kubernetes)
			{
				// add the kubernetes leader instance under its actual type as well to make it easier to find
				services.AddSingleton<KubernetesLeaderElection>(p => (KubernetesLeaderElection)p.GetService<ILeaderElection>()!);
				services.AddHostedService<KubernetesLeaderElection>(p => p.GetService<KubernetesLeaderElection>()!);
			}

			services.AddSingleton<BlobStoreConsistencyCheckService>();
			services.AddSingleton(typeof(IPeerServiceDiscovery), ServiceDiscoveryFactory);
		}

		private IBlobIndex BlobIndexFactory(IServiceProvider provider)
		{
			UnrealCloudDDCSettings settings = provider.GetService<IOptionsMonitor<UnrealCloudDDCSettings>>()!.CurrentValue!;
			switch (settings.BlobIndexImplementation)
			{
				case UnrealCloudDDCSettings.BlobIndexImplementations.Memory:
					return ActivatorUtilities.CreateInstance<MemoryBlobIndex>(provider);
				case UnrealCloudDDCSettings.BlobIndexImplementations.Scylla:
					return ActivatorUtilities.CreateInstance<ScyllaBlobIndex>(provider);
				case UnrealCloudDDCSettings.BlobIndexImplementations.Mongo:
					return ActivatorUtilities.CreateInstance<MongoBlobIndex>(provider);
				case UnrealCloudDDCSettings.BlobIndexImplementations.Cache:
					return ActivatorUtilities.CreateInstance<CachedBlobIndex>(provider);
				default:
					throw new NotImplementedException($"Unknown blob index implementation: {settings.BlobIndexImplementation}");
			}
		}

		private IPeerServiceDiscovery ServiceDiscoveryFactory(IServiceProvider provider)
		{
			UnrealCloudDDCSettings settings = provider.GetService<IOptionsMonitor<UnrealCloudDDCSettings>>()!.CurrentValue!;
			switch (settings.ServiceDiscoveryImplementation)
			{
				case UnrealCloudDDCSettings.ServiceDiscoveryImplementations.Kubernetes:
					return ActivatorUtilities.CreateInstance<KubernetesPeerServiceDiscovery>(provider);
				case UnrealCloudDDCSettings.ServiceDiscoveryImplementations.Static:
					return ActivatorUtilities.CreateInstance<StaticPeerServiceDiscovery>(provider);
				default:
					throw new NotImplementedException($"Unknown service discovery implementation: {settings.ServiceDiscoveryImplementation}");
			}
		}

		private IAmazonSecretsManager CreateAWSSecretsManager(IServiceProvider provider)
		{
			AWSCredentials awsCredentials = provider.GetService<AWSCredentials>()!;
			AWSOptions awsOptions = Configuration.GetAWSOptions();
			awsOptions.Credentials = awsCredentials;

			IAmazonSecretsManager serviceClient = awsOptions.CreateServiceClient<IAmazonSecretsManager>();
			return serviceClient;
		}

		private CompressedBufferUtils CreateCompressedBufferUtils(IServiceProvider provider)
		{
			return ActivatorUtilities.CreateInstance<CompressedBufferUtils>(provider);
		}

		private object ContentIdStoreFactory(IServiceProvider provider)
		{
			UnrealCloudDDCSettings settings = provider.GetService<IOptionsMonitor<UnrealCloudDDCSettings>>()!.CurrentValue!;
			IContentIdStore store = settings.ContentIdStoreImplementation switch
			{
				UnrealCloudDDCSettings.ContentIdStoreImplementations.Memory => ActivatorUtilities
					.CreateInstance<MemoryContentIdStore>(provider),
				UnrealCloudDDCSettings.ContentIdStoreImplementations.Scylla => ActivatorUtilities
					.CreateInstance<ScyllaContentIdStore>(provider),
				UnrealCloudDDCSettings.ContentIdStoreImplementations.Mongo => ActivatorUtilities
					.CreateInstance<MongoContentIdStore>(provider),
				UnrealCloudDDCSettings.ContentIdStoreImplementations.Cache => ActivatorUtilities
					.CreateInstance<CacheContentIdStore>(provider),
				_ => throw new NotImplementedException(
					$"Unknown content id store implementation: {settings.ContentIdStoreImplementation}")
			};

			MemoryCacheContentIdSettings memoryCacheSettings = provider.GetService<IOptionsMonitor<MemoryCacheContentIdSettings>>()!.CurrentValue;

			if (memoryCacheSettings.Enabled)
			{
				store = ActivatorUtilities.CreateInstance<MemoryCachedContentIdStore>(provider, store);
			}
			return store;
		}

		private IScyllaSessionManager ScyllaFactory(IServiceProvider provider)
		{
			ScyllaSettings settings = provider.GetService<IOptionsMonitor<ScyllaSettings>>()!.CurrentValue!;
			ISecretResolver secretResolver = provider.GetService<ISecretResolver>()!;

			using Serilog.Extensions.Logging.SerilogLoggerProvider serilogLoggerProvider = new();
			Diagnostics.AddLoggerProvider(serilogLoggerProvider);

			string? connectionString = secretResolver.Resolve(settings.ConnectionString);
			if (string.IsNullOrEmpty(connectionString))
			{
				throw new Exception("Connection string has to be specified");
			}

			CassandraConnectionStringBuilder cassandraConnectionStringBuilder = new CassandraConnectionStringBuilder(connectionString);
			if (string.IsNullOrEmpty(cassandraConnectionStringBuilder.DefaultKeyspace))
			{
				throw new Exception("Default Keyspace has to be specified");
			}

			// Configure the builder with your cluster's contact points
			Builder clusterBuilder = Cluster.Builder()
				.WithConnectionString(connectionString)
				.WithLoadBalancingPolicy(Policies.NewDefaultLoadBalancingPolicy(settings.LocalDatacenterName))
				.WithPoolingOptions(PoolingOptions.Create().SetMaxConnectionsPerHost(HostDistance.Local, settings.MaxConnectionForLocalHost).SetMaxRequestsPerConnection(settings.MaxRequestsPerConnection))
				.WithExecutionProfiles(options =>
					options.WithProfile("default", builder => builder.WithConsistencyLevel(ConsistencyLevel.LocalOne)));

			if (settings.ReadTimeout != -1)
			{
				clusterBuilder.SocketOptions.SetReadTimeoutMillis(settings.ReadTimeout);
			}
			if (settings.UseAzureCosmosDB)
			{
				CassandraConnectionStringBuilder connectionStringBuilder = new CassandraConnectionStringBuilder(connectionString);
				string[] contactPoints = connectionStringBuilder.ContactPoints;

				// Connect to cassandra cluster using TLSv1.2.
				if (settings.UseSSL)
				{
					SSLOptions sslOptions = new SSLOptions(SslProtocols.None, false,
						(sender, certificate, chain, sslPolicyErrors) =>
						{
							if (sslPolicyErrors == SslPolicyErrors.None)
							{
								return true;
							}

							Logger.LogError("Certificate error: {Errors}", sslPolicyErrors);
							// Do not allow this client to communicate with unauthenticated servers.
							return false;
						});

					// Prepare a map to resolve the host name from the IP address.
					Dictionary<IPAddress, string> hostNameByIp = new Dictionary<IPAddress, string>();
					foreach (string contactPoint in contactPoints)
					{
						IPAddress[] resolvedIps = Dns.GetHostAddresses(contactPoint);
						foreach (IPAddress resolvedIp in resolvedIps)
						{
							hostNameByIp[resolvedIp] = contactPoint;
						}
					}

					sslOptions.SetHostNameResolver((ipAddress) =>
					{
						if (hostNameByIp.TryGetValue(ipAddress, out string? resolvedName))
						{
							return resolvedName;
						}
						IPHostEntry hostEntry = Dns.GetHostEntry(ipAddress.ToString());
						return hostEntry.HostName;
					});

					clusterBuilder = clusterBuilder.WithSSL(sslOptions);
				}
			}
			Cluster cluster = clusterBuilder.Build();

			Dictionary<string, string> replicationStrategy = ReplicationStrategies.CreateSimpleStrategyReplicationProperty(2);
			if (settings.KeyspaceReplicationStrategy != null)
			{
				replicationStrategy = settings.KeyspaceReplicationStrategy;
				Logger.LogInformation("Scylla Replication strategy for replicated keyspace is set to {@ReplicationStrategy}", replicationStrategy);
			}

			ISession replicatedSession;
			const int MaxRetryAttempts = 100;
			int countOfAttempts = 0;
			while(true)
			{
				try
				{
					countOfAttempts += 1;
					replicatedSession = cluster.ConnectAndCreateDefaultKeyspaceIfNotExists(replicationStrategy);
					break;
				}
				catch (NoHostAvailableException)
				{
					Logger.LogWarning("Failed to connect to scylla, waiting a while and will then retry. Attempt {AttemptNum} of {NumAttempts}", countOfAttempts, MaxRetryAttempts);
					// wait for a few seconds before attempt to connect again
					Thread.Sleep(5000);
					if (countOfAttempts >= MaxRetryAttempts)
					{
						Logger.LogError("Failed to connect to Scylla after {NumAttempts} attempts, giving up.", countOfAttempts);
						throw;
					}
				}
			}

			string keyspace = replicatedSession.Keyspace;
			if (!settings.AvoidSchemaChanges)
			{
				replicatedSession.Execute(new SimpleStatement("CREATE TYPE IF NOT EXISTS blob_identifier (hash blob)"));
				replicatedSession.Execute(new SimpleStatement("CREATE TYPE IF NOT EXISTS object_reference (bucket text, key text)"));
			}

			replicatedSession.UserDefinedTypes.Define(UdtMap.For<ScyllaBlobIdentifier>("blob_identifier", keyspace));
			replicatedSession.UserDefinedTypes.Define(UdtMap.For<ScyllaObjectReference>("object_reference", keyspace));

			string localKeyspaceName = $"{keyspace}_local_{settings.LocalKeyspaceSuffix}";

			Dictionary<string, string> replicationStrategyLocal = ReplicationStrategies.CreateSimpleStrategyReplicationProperty(2);
			if (settings.LocalKeyspaceReplicationStrategy != null)
			{
				replicationStrategyLocal = settings.LocalKeyspaceReplicationStrategy;
				Logger.LogInformation("Scylla Replication strategy for local keyspace is set to {@ReplicationStrategy}", replicationStrategyLocal);
			}

			// the local keyspace is never replicated so we do not support controlling how the replication strategy is setup
			replicatedSession.CreateKeyspaceIfNotExists(localKeyspaceName, replicationStrategyLocal);
			ISession localSession = cluster.Connect(localKeyspaceName);

			if (!settings.AvoidSchemaChanges)
			{
				localSession.Execute(new SimpleStatement("CREATE TYPE IF NOT EXISTS blob_identifier (hash blob)"));
			}
			localSession.UserDefinedTypes.Define(UdtMap.For<ScyllaBlobIdentifier>("blob_identifier", localKeyspaceName));

			bool isScylla = !settings.UseAzureCosmosDB;
			return new ScyllaSessionManager(replicatedSession, localSession, isScylla, !isScylla);
		}

		private IReferencesStore ObjectStoreFactory(IServiceProvider provider)
		{
			UnrealCloudDDCSettings settings = provider.GetService<IOptionsMonitor<UnrealCloudDDCSettings>>()!.CurrentValue!;
			IReferencesStore store = settings.ReferencesDbImplementation switch
			{
				UnrealCloudDDCSettings.ReferencesDbImplementations.Memory => ActivatorUtilities.CreateInstance<MemoryReferencesStore>(provider),
				UnrealCloudDDCSettings.ReferencesDbImplementations.Scylla => ActivatorUtilities.CreateInstance<ScyllaReferencesStore>(provider),
				UnrealCloudDDCSettings.ReferencesDbImplementations.Mongo => ActivatorUtilities.CreateInstance<MongoReferencesStore>(provider),
				UnrealCloudDDCSettings.ReferencesDbImplementations.Cache => ActivatorUtilities.CreateInstance<CacheReferencesStore>(provider),
				_ => throw new NotImplementedException(
					$"Unknown references db implementation: {settings.ReferencesDbImplementation}")
			};

			MemoryCacheReferencesSettings memoryCacheSettings = provider.GetService<IOptionsMonitor<MemoryCacheReferencesSettings>>()!.CurrentValue;

			if (memoryCacheSettings.Enabled)
			{
				store = ActivatorUtilities.CreateInstance<MemoryCachedReferencesStore>(provider, store);
			}

			return store;
		}
		
		private ILeaderElection CreateLeaderElection(IServiceProvider provider)
		{
			UnrealCloudDDCSettings settings = provider.GetService<IOptionsMonitor<UnrealCloudDDCSettings>>()!.CurrentValue!;
			if (settings.LeaderElectionImplementation == UnrealCloudDDCSettings.LeaderElectionImplementations.Kubernetes)
			{
				return ActivatorUtilities.CreateInstance<KubernetesLeaderElection>(provider);
			}
			else if (settings.LeaderElectionImplementation == UnrealCloudDDCSettings.LeaderElectionImplementations.Static)
			{
				// hard coded leader election that assumes it is always the leader
				return ActivatorUtilities.CreateInstance<LeaderElectionStub>(provider, true);
			}
			else if (settings.LeaderElectionImplementation == UnrealCloudDDCSettings.LeaderElectionImplementations.Disabled)
			{
				// disabled leader election means we are never the leader
				return ActivatorUtilities.CreateInstance<LeaderElectionStub>(provider, false);
			}
			else
			{
				throw new NotImplementedException($"Unknown leader election set {settings.LeaderElectionImplementation}");
			}
		}

		private IAmazonS3 CreateS3(IServiceProvider provider)
		{
			UnrealCloudDDCSettings settings = provider.GetService<IOptionsMonitor<UnrealCloudDDCSettings>>()!.CurrentValue!;

			if (settings.StorageImplementations == null)
			{
				throw new ArgumentException("No storage implementation set");
			}
			
			bool isS3InUse = settings.StorageImplementations.Any(x =>
				string.Equals(x, UnrealCloudDDCSettings.StorageBackendImplementations.S3.ToString(), StringComparison.OrdinalIgnoreCase));

			if (isS3InUse)
			{
				S3Settings s3Settings = provider.GetService<IOptionsMonitor<S3Settings>>()!.CurrentValue!;
				AWSCredentials awsCredentials = provider.GetService<AWSCredentials>()!;
				AWSOptions awsOptions = Configuration.GetAWSOptions();
				if (s3Settings.ConnectionString.ToUpper() != "AWS")
				{
					awsOptions.DefaultClientConfig.ServiceURL = s3Settings.ConnectionString;
				}

				awsOptions.Credentials = awsCredentials;

				IAmazonS3 serviceClient = awsOptions.CreateServiceClient<IAmazonS3>();

				if (serviceClient.Config is AmazonS3Config c)
				{
					if (s3Settings.ForceAWSPathStyle)
					{
						c.ForcePathStyle = true;
					}

					if (s3Settings.UseArnRegion.HasValue)
					{
						c.UseArnRegion = s3Settings.UseArnRegion.Value;
					}
				}
				else
				{
					throw new NotImplementedException();
				}

				return serviceClient;
			}

			return null!;
		}

		private IReplicationLog ReplicationLogWriterFactory(IServiceProvider provider)
		{
			UnrealCloudDDCSettings settings = provider.GetService<IOptionsMonitor<UnrealCloudDDCSettings>>()!.CurrentValue;
			switch (settings.ReplicationLogWriterImplementation)
			{
				case UnrealCloudDDCSettings.ReplicationLogWriterImplementations.Scylla:
					return ActivatorUtilities.CreateInstance<ScyllaReplicationLog>(provider);
				case UnrealCloudDDCSettings.ReplicationLogWriterImplementations.Memory:
					return ActivatorUtilities.CreateInstance<MemoryReplicationLog>(provider);
				default:
					throw new NotImplementedException();
			}
		}

		protected override void OnAddHealthChecks(IServiceCollection services, IHealthChecksBuilder healthChecks)
		{
			ServiceProvider provider = services.BuildServiceProvider();
			UnrealCloudDDCSettings settings = provider.GetService<IOptionsMonitor<UnrealCloudDDCSettings>>()!.CurrentValue;
			
			foreach (UnrealCloudDDCSettings.StorageBackendImplementations impl in settings.GetStorageImplementations())
			{
				switch (impl)
				{
					case UnrealCloudDDCSettings.StorageBackendImplementations.S3:

						// The S3 health checks are disabled as they do not support dynamic credentials similar to the issue with dynamo
						/*AWSCredentials awsCredentials = provider.GetService<AWSCredentials>();
						S3Settings s3Settings = provider.GetService<IOptionsMonitor<S3Settings>>()!.CurrentValue;
						healthChecks.AddS3(options =>
						{
							options.BucketName = s3Settings.BucketName;
							options.S3Config = provider.GetService<IAmazonS3>().Config as AmazonS3Config;
							options.Credentials = awsCredentials;
						}, tags: new[] {"services"});*/
						break;
					case UnrealCloudDDCSettings.StorageBackendImplementations.Azure:
						// Health checks for Azure are disabled as the connection string will vary based on the namespace used
						/*AzureSettings azureSettings = provider.GetService<IOptionsMonitor<AzureSettings>>()!.CurrentValue;
						healthChecks.AddAzureBlobStorage(AzureBlobStore.GetConnectionString(azureSettings, provider), tags: new[] {"services"});*/
						
						break;

					case UnrealCloudDDCSettings.StorageBackendImplementations.FileSystem:
						healthChecks.AddDiskStorageHealthCheck(options =>
						{
							FilesystemSettings filesystemSettings = provider.GetService<IOptionsMonitor<FilesystemSettings>>()!.CurrentValue;
							string? driveRoot = Path.GetPathRoot(PathUtil.ResolvePath(filesystemSettings.RootDir));
							if (!string.IsNullOrEmpty(driveRoot))
							{
							options.AddDrive(driveRoot);
							}
						});
						break;
					case UnrealCloudDDCSettings.StorageBackendImplementations.Memory:
					case UnrealCloudDDCSettings.StorageBackendImplementations.Relay:
					case UnrealCloudDDCSettings.StorageBackendImplementations.Peer:
						break;
					default:
						throw new ArgumentOutOfRangeException("Unhandled storage impl " + impl);
				}
			}

			healthChecks.AddCheck<LastAccessServiceCheck>("LastAccessServiceCheck", tags: new[] {"services"});

			healthChecks.AddCheck<ReplicatorServiceCheck>("ReplicatorServiceCheck", tags: new[] { "services" });
			healthChecks.AddCheck<ReplicationSnapshotServiceCheck>("ReplicationSnapshotServiceCheck", tags: new[] { "services" });

			GCSettings gcSettings = provider.GetService<IOptionsMonitor<GCSettings>>()!.CurrentValue;

			if (gcSettings.CleanOldRefRecords)
			{
				healthChecks.AddCheck<RefCleanupServiceCheck>("RefCleanupCheck", tags: new[] {"services"});
			}

			if (gcSettings.BlobCleanupServiceEnabled)
			{
				healthChecks.AddCheck<BlobCleanupServiceCheck>("BlobStoreCheck", tags: new[] {"services"});
			}

			if (settings.LeaderElectionImplementation == UnrealCloudDDCSettings.LeaderElectionImplementations.Kubernetes)
			{
				healthChecks.AddCheck<KubernetesLeaderServiceCheck>("KubernetesLeaderService", tags: new[] {"services"});
			}
		}
	}
}
