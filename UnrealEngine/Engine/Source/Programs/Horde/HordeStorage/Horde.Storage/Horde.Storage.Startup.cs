// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Security;
using System.Runtime.InteropServices;
using System.Security.Authentication;
using Amazon.DAX;
using Amazon.DynamoDBv2;
using Amazon.Extensions.NETCore.Setup;
using Amazon.Runtime;
using Amazon.S3;
using Amazon.SecretsManager;
using Cassandra;
using Horde.Storage.Controllers;
using Horde.Storage.Implementation;
using Horde.Storage.Implementation.Blob;
using Horde.Storage.Implementation.LeaderElection;
using Jupiter;
using Jupiter.Common.Implementation;
using Jupiter.Utils;
using Microsoft.AspNetCore.Authorization;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using StatsdClient;

namespace Horde.Storage
{
    // ReSharper disable once ClassNeverInstantiated.Global
    public class HordeStorageStartup : BaseStartup
    {
        public HordeStorageStartup(IConfiguration configuration) : base(configuration)
        {
            string? ddAgentHost = System.Environment.GetEnvironmentVariable("DD_AGENT_HOST");
            if (!string.IsNullOrEmpty(ddAgentHost))
            {
                Logger.Information("Initializing Dogstatsd to connect to: {DatadogAgentHost}", ddAgentHost);
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
                string GetPlatformName()
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
                string? hordeStorageVersion = versionFile?.VersionString ?? "unknown";
                string platformName = GetPlatformName();
                client.DefaultRequestHeaders.TryAddWithoutValidation("UserAgent", $"UnrealEngine/{engineVersion} ({platformName}) HordeStorage/{hordeStorageVersion}");
            });

            services.AddOptions<HordeStorageSettings>().Bind(Configuration.GetSection("Horde_Storage")).ValidateDataAnnotations();
            services.AddOptions<MongoSettings>().Bind(Configuration.GetSection("Mongo")).ValidateDataAnnotations();
            services.AddOptions<CosmosSettings>().Bind(Configuration.GetSection("Cosmos")).ValidateDataAnnotations();
            services.AddOptions<DynamoDbSettings>().Bind(Configuration.GetSection("DynamoDb")).ValidateDataAnnotations();

            services.AddOptions<CallistoTransactionLogSettings>().Bind(Configuration.GetSection("Callisto")).ValidateDataAnnotations();

            services.AddOptions<MemoryCacheBlobSettings>().Bind(Configuration.GetSection("Cache.Blob")).ValidateDataAnnotations();
            services.AddOptions<MemoryCacheRefSettings>().Bind(Configuration.GetSection("Cache.Db")).ValidateDataAnnotations();

            services.AddOptions<S3Settings>().Bind(Configuration.GetSection("S3")).ValidateDataAnnotations();
            services.AddOptions<AzureSettings>().Bind(Configuration.GetSection("Azure")).ValidateDataAnnotations();
            services.AddOptions<FilesystemSettings>().Bind(Configuration.GetSection("Filesystem")).ValidateDataAnnotations();

            services.AddOptions<ConsistencyCheckSettings>().Bind(Configuration.GetSection("ConsistencyCheck")).ValidateDataAnnotations();

            services.AddOptions<UpstreamRelaySettings>().Configure(o => Configuration.GetSection("Upstream").Bind(o)).ValidateDataAnnotations();
            services.AddOptions<ClusterSettings>().Configure(o => Configuration.GetSection("Cluster").Bind(o)).ValidateDataAnnotations();

            services.AddOptions<GCSettings>().Configure(o => Configuration.GetSection("GC").Bind(o)).ValidateDataAnnotations();
            services.AddOptions<ReplicationSettings>().Configure(o => Configuration.GetSection("Replication").Bind(o)).ValidateDataAnnotations();
            services.AddOptions<ServiceCredentialSettings>().Configure(o => Configuration.GetSection("ServiceCredentials").Bind(o)).ValidateDataAnnotations();
            services.AddOptions<SnapshotSettings>().Configure(o => Configuration.GetSection("Snapshot").Bind(o)).ValidateDataAnnotations();

            services.AddOptions<ScyllaSettings>().Configure(o => Configuration.GetSection("Scylla").Bind(o)).ValidateDataAnnotations();

            services.AddOptions<KubernetesLeaderElectionSettings>().Configure(o => Configuration.GetSection("Kubernetes").Bind(o)).ValidateDataAnnotations();

            services.AddSingleton(typeof(OodleCompressor), CreateOodleCompressor);
            services.AddSingleton(typeof(CompressedBufferUtils), CreateCompressedBufferUtils);

            services.AddSingleton<AWSCredentials>(provider =>
            {
                AWSCredentialsSettings awsSettings = provider.GetService<IOptionsMonitor<AWSCredentialsSettings>>()!.CurrentValue;

                return AWSCredentialsHelper.GetCredentials(awsSettings, "Horde.Storage");
            });
            services.AddSingleton(typeof(IAmazonDynamoDB), AddDynamo);

            services.AddSingleton<BufferedPayloadFactory>();

            services.AddSingleton<BlobCleanupService>();
            services.AddHostedService<BlobCleanupService>(p => p.GetService<BlobCleanupService>()!);

            services.AddSingleton(serviceType: typeof(IDDCRefService), typeof(DDCRefService));

            services.AddSingleton(serviceType: typeof(IObjectService), typeof(ObjectService));
            services.AddSingleton(serviceType: typeof(IReferencesStore), ObjectStoreFactory);
            services.AddSingleton(serviceType: typeof(IReferenceResolver), typeof(ReferenceResolver));
            services.AddSingleton(serviceType: typeof(IContentIdStore), ContentIdStoreFactory);

            services.AddSingleton(serviceType: typeof(ITransactionLogWriter), TransactionLogWriterFactory);

            services.AddSingleton(serviceType: typeof(IBlobIndex), BlobIndexFactory);
            services.AddSingleton(serviceType: typeof(IAmazonS3), CreateS3);

            services.AddSingleton<AmazonS3Store>();
            services.AddSingleton<FileSystemStore>();
            services.AddSingleton<AzureBlobStore>();
            services.AddSingleton<MemoryCacheBlobStore>();
            services.AddSingleton<MemoryBlobStore>();
            services.AddSingleton<RelayBlobStore>();

            services.AddSingleton<OrphanBlobCleanup>();
            services.AddSingleton<OrphanBlobCleanupRefs>();

            services.AddSingleton(typeof(IBlobService), typeof(BlobService));

            services.AddSingleton(serviceType: typeof(IScyllaSessionManager), ScyllaFactory);

            services.AddSingleton(serviceType: typeof(IReplicationLog), ReplicationLogWriterFactory);
            
            services.AddSingleton<LastAccessTrackerRefRecord>();
            services.AddSingleton(serviceType: typeof(ILastAccessCache<RefRecord>), p => p.GetService<LastAccessTrackerRefRecord>()!);
            services.AddSingleton(serviceType: typeof(ILastAccessTracker<RefRecord>), p => p.GetService<LastAccessTrackerRefRecord>()!);

            services.AddSingleton<LastAccessTrackerReference>();
            services.AddSingleton(serviceType: typeof(ILastAccessCache<LastAccessRecord>), p => p.GetService<LastAccessTrackerReference>()!);
            services.AddSingleton(serviceType: typeof(ILastAccessTracker<LastAccessRecord>), p => p.GetService<LastAccessTrackerReference>()!);

            services.AddSingleton(serviceType: typeof(ILeaderElection), CreateLeaderElection);

            services.AddTransient<RequestHelper>();

            services.AddSingleton(Configuration);
            services.AddSingleton<DynamoNamespaceStore>();
            services.AddSingleton(RefStoreFactory);

            services.AddSingleton<FormatResolver>();

            services.AddSingleton(serviceType: typeof(ISecretResolver), typeof(SecretResolver));
            services.AddSingleton(typeof(IAmazonSecretsManager), CreateAWSSecretsManager);

            services.AddSingleton<LastAccessService>();
            services.AddHostedService<LastAccessService>(p => p.GetService<LastAccessService>()!);

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

            services.AddSingleton(typeof(IPeerStatusService), typeof(PeerStatusService));
            services.AddHostedService<PeerStatusService>(p => (PeerStatusService)p.GetService<IPeerStatusService>()!);
        
            services.AddTransient(typeof(IRefCleanup), typeof(RefCleanup));

            services.AddTransient(typeof(VersionFile), typeof(VersionFile));

            services.AddSingleton<RefCleanupService>();
            services.AddHostedService<RefCleanupService>(p => p.GetService<RefCleanupService>()!);

            // This will technically create a new settings object, but as we do not dynamically update it the settings will reflect what we actually want
            HordeStorageSettings settings = services.BuildServiceProvider().GetService<IOptionsMonitor<HordeStorageSettings>>()!.CurrentValue!;

            if (settings.LeaderElectionImplementation == HordeStorageSettings.LeaderElectionImplementations.Kubernetes)
            {
                // add the kubernetes leader instance under its actual type as well to make it easier to find
                services.AddSingleton<KubernetesLeaderElection>(p => (KubernetesLeaderElection)p.GetService<ILeaderElection>()!);
                services.AddHostedService<KubernetesLeaderElection>(p => p.GetService<KubernetesLeaderElection>()!);
            }
        }

        private IBlobIndex BlobIndexFactory(IServiceProvider provider)
        {
            HordeStorageSettings settings = provider.GetService<IOptionsMonitor<HordeStorageSettings>>()!.CurrentValue!;
            switch (settings.BlobIndexImplementation)
            {
                case HordeStorageSettings.BlobIndexImplementations.Memory:
                    return ActivatorUtilities.CreateInstance<MemoryBlobIndex>(provider);
                case HordeStorageSettings.BlobIndexImplementations.Scylla:
                    return ActivatorUtilities.CreateInstance<ScyllaBlobIndex>(provider);
                case HordeStorageSettings.BlobIndexImplementations.Mongo:
                    return ActivatorUtilities.CreateInstance<MongoBlobIndex>(provider);
                case HordeStorageSettings.BlobIndexImplementations.Cache:
                    return ActivatorUtilities.CreateInstance<CachedBlobIndex>(provider);
                default:
                    throw new NotImplementedException($"Unknown blob index implementation: {settings.BlobIndexImplementation}");
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

        private object CreateOodleCompressor(IServiceProvider provider)
        {
            OodleCompressor compressor = new OodleCompressor();
            compressor.InitializeOodle();
            return compressor;
        }

        private object ContentIdStoreFactory(IServiceProvider provider)
        {
            HordeStorageSettings settings = provider.GetService<IOptionsMonitor<HordeStorageSettings>>()!.CurrentValue!;
            IContentIdStore store = settings.ContentIdStoreImplementation switch
            {
                HordeStorageSettings.ContentIdStoreImplementations.Memory => ActivatorUtilities
                    .CreateInstance<MemoryContentIdStore>(provider),
                HordeStorageSettings.ContentIdStoreImplementations.Scylla => ActivatorUtilities
                    .CreateInstance<ScyllaContentIdStore>(provider),
                HordeStorageSettings.ContentIdStoreImplementations.Mongo => ActivatorUtilities
                    .CreateInstance<MongoContentIdStore>(provider),
                HordeStorageSettings.ContentIdStoreImplementations.Cache => ActivatorUtilities
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
            const string DefaultKeyspaceName = "jupiter";

            string? connectionString = secretResolver.Resolve(settings.ConnectionString);
            if (string.IsNullOrEmpty(connectionString))
            {
                throw new Exception("Connection string has to be specified");
            }

            // Configure the builder with your cluster's contact points
            Builder clusterBuilder = Cluster.Builder()
                .WithConnectionString(connectionString)
                .WithDefaultKeyspace(DefaultKeyspaceName)
                .WithLoadBalancingPolicy(new DefaultLoadBalancingPolicy(settings.LocalDatacenterName))
                .WithPoolingOptions(PoolingOptions.Create().SetMaxConnectionsPerHost(HostDistance.Local, settings.MaxConnectionForLocalHost))
                .WithExecutionProfiles(options =>
                    options.WithProfile("default", builder => builder.WithConsistencyLevel(ConsistencyLevel.LocalOne)));

            if (settings.UseAzureCosmosDB)
            {
                CassandraConnectionStringBuilder connectionStringBuilder = new CassandraConnectionStringBuilder(connectionString);
                connectionStringBuilder.DefaultKeyspace = DefaultKeyspaceName;
                string[] contactPoints = connectionStringBuilder.ContactPoints;

                // Connect to cassandra cluster using TLSv1.2.
                SSLOptions sslOptions = new SSLOptions(SslProtocols.None, false,
                    (sender, certificate, chain, sslPolicyErrors) =>
                    {
                        if (sslPolicyErrors == SslPolicyErrors.None)
                        {
                            return true;
                        }

                        Logger.Error("Certificate error: {0}", sslPolicyErrors);
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
            Cluster cluster = clusterBuilder.Build();

            Dictionary<string, string> replicationStrategy = ReplicationStrategies.CreateSimpleStrategyReplicationProperty(2);
            if (settings.KeyspaceReplicationStrategy != null)
            {
                replicationStrategy = settings.KeyspaceReplicationStrategy;
                Logger.Information("Scylla Replication strategy for replicated keyspace is set to {@ReplicationStrategy}", replicationStrategy);
            }
            ISession replicatedSession = cluster.ConnectAndCreateDefaultKeyspaceIfNotExists(replicationStrategy);

            replicatedSession.Execute(new SimpleStatement("CREATE TYPE IF NOT EXISTS blob_identifier (hash blob)"));
            replicatedSession.UserDefinedTypes.Define(UdtMap.For<ScyllaBlobIdentifier>("blob_identifier", DefaultKeyspaceName));

            replicatedSession.Execute(new SimpleStatement("CREATE TYPE IF NOT EXISTS object_reference (bucket text, key text)"));
            replicatedSession.UserDefinedTypes.Define(UdtMap.For<ScyllaObjectReference>("object_reference", DefaultKeyspaceName));

            string localKeyspaceName = $"jupiter_local_{settings.LocalKeyspaceSuffix}";

            Dictionary<string, string> replicationStrategyLocal = ReplicationStrategies.CreateSimpleStrategyReplicationProperty(2);
            if (settings.LocalKeyspaceReplicationStrategy != null)
            {
                replicationStrategyLocal = settings.LocalKeyspaceReplicationStrategy;
                Logger.Information("Scylla Replication strategy for local keyspace is set to {@ReplicationStrategy}", replicationStrategyLocal);
            }

            // the local keyspace is never replicated so we do not support controlling how the replication strategy is setup
            replicatedSession.CreateKeyspaceIfNotExists(localKeyspaceName, replicationStrategyLocal);
            ISession localSession = cluster.Connect(localKeyspaceName);

            localSession.Execute(new SimpleStatement("CREATE TYPE IF NOT EXISTS blob_identifier (hash blob)"));
            localSession.UserDefinedTypes.Define(UdtMap.For<ScyllaBlobIdentifier>("blob_identifier", localKeyspaceName));

            bool isScylla = !settings.UseAzureCosmosDB;
            return new ScyllaSessionManager(replicatedSession, localSession, isScylla, !isScylla);
        }

        private IReferencesStore ObjectStoreFactory(IServiceProvider provider)
        {
            HordeStorageSettings settings = provider.GetService<IOptionsMonitor<HordeStorageSettings>>()!.CurrentValue!;
            switch (settings.ReferencesDbImplementation)
            {
                case HordeStorageSettings.ReferencesDbImplementations.Memory:
                    return ActivatorUtilities.CreateInstance<MemoryReferencesStore>(provider);
                case HordeStorageSettings.ReferencesDbImplementations.Scylla:
                    return ActivatorUtilities.CreateInstance<ScyllaReferencesStore>(provider);
                case HordeStorageSettings.ReferencesDbImplementations.Mongo:
                    return ActivatorUtilities.CreateInstance<MongoReferencesStore>(provider);
                case HordeStorageSettings.ReferencesDbImplementations.Cache:
                    return ActivatorUtilities.CreateInstance<CacheReferencesStore>(provider);
                default:
                    throw new NotImplementedException($"Unknown references db implementation: {settings.ReferencesDbImplementation}");
            }
        }
        
        private ILeaderElection CreateLeaderElection(IServiceProvider provider)
        {
            HordeStorageSettings settings = provider.GetService<IOptionsMonitor<HordeStorageSettings>>()!.CurrentValue!;
            if (settings.LeaderElectionImplementation == HordeStorageSettings.LeaderElectionImplementations.Kubernetes)
            {
                return ActivatorUtilities.CreateInstance<KubernetesLeaderElection>(provider);
            }
            else
            {
                // hard coded leader election that assumes it is always the leader
                return ActivatorUtilities.CreateInstance<LeaderElectionStub>(provider, true);
            }
        }

        private IAmazonS3 CreateS3(IServiceProvider provider)
        {
            HordeStorageSettings settings = provider.GetService<IOptionsMonitor<HordeStorageSettings>>()!.CurrentValue!;

            if (settings.StorageImplementations == null)
            {
                throw new ArgumentException("No storage implementation set");
            }
            
            bool isS3InUse = settings.StorageImplementations.Any(x =>
                string.Equals(x, HordeStorageSettings.StorageBackendImplementations.S3.ToString(), StringComparison.OrdinalIgnoreCase));

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
                }
                else
                {
                    throw new NotImplementedException();
                }

                return serviceClient;
            }

            return null!;
        }

        private IAmazonDynamoDB AddDynamo(IServiceProvider provider)
        {
            IOptionsMonitor<HordeStorageSettings> settings = provider.GetService<IOptionsMonitor<HordeStorageSettings>>()!;

            bool hasDynamo = settings.CurrentValue.RefDbImplementation == HordeStorageSettings.RefDbImplementations.DynamoDb ||
                             settings.CurrentValue.TreeRootStoreImplementation == HordeStorageSettings.TreeRootStoreImplementations.DynamoDb ||
                             settings.CurrentValue.TreeStoreImplementation == HordeStorageSettings.TreeStoreImplementations.DynamoDb;
            if (!hasDynamo)
            {
                return null!;
            }

            AWSCredentials awsCredentials = provider.GetService<AWSCredentials>()!;

            DynamoDbSettings dynamoDbSettings = provider.GetService<IOptionsMonitor<DynamoDbSettings>>()!.CurrentValue;

            AWSOptions awsOptions = Configuration.GetAWSOptions();
            if (dynamoDbSettings.ConnectionString.ToUpper() != "AWS")
            {
                awsOptions.DefaultClientConfig.ServiceURL = dynamoDbSettings.ConnectionString;
            }

            awsOptions.Credentials = awsCredentials;

            IAmazonDynamoDB serviceClient = awsOptions.CreateServiceClient<IAmazonDynamoDB>();

            if (!string.IsNullOrEmpty(dynamoDbSettings.DaxEndpoint))
            {
                (string daxHost, int daxPort) = DynamoDbSettings.ParseDaxEndpointAsHostPort(dynamoDbSettings.DaxEndpoint);
                DaxClientConfig daxConfig = new (daxHost, daxPort)
                {
                    AwsCredentials = awsOptions.Credentials
                };
                
                serviceClient = new ClusterDaxClient(daxConfig);
            }

            return serviceClient;
        }

        private ITransactionLogWriter TransactionLogWriterFactory(IServiceProvider provider)
        {
            HordeStorageSettings settings = provider.GetService<IOptionsMonitor<HordeStorageSettings>>()!.CurrentValue;
            switch (settings.TransactionLogWriterImplementation)
            {
                case HordeStorageSettings.TransactionLogWriterImplementations.Callisto:
                    return ActivatorUtilities.CreateInstance<CallistoTransactionLogWriter>(provider);
                case HordeStorageSettings.TransactionLogWriterImplementations.Memory:
                    return ActivatorUtilities.CreateInstance<MemoryTransactionLogWriter>(provider);
                default:
                    throw new NotImplementedException();
            }
        }

        private IReplicationLog ReplicationLogWriterFactory(IServiceProvider provider)
        {
            HordeStorageSettings settings = provider.GetService<IOptionsMonitor<HordeStorageSettings>>()!.CurrentValue;
            switch (settings.ReplicationLogWriterImplementation)
            {
                case HordeStorageSettings.ReplicationLogWriterImplementations.Scylla:
                    return ActivatorUtilities.CreateInstance<ScyllaReplicationLog>(provider);
                case HordeStorageSettings.ReplicationLogWriterImplementations.Memory:
                    return ActivatorUtilities.CreateInstance<MemoryReplicationLog>(provider);
                default:
                    throw new NotImplementedException();
            }
        }

        private static IRefsStore RefStoreFactory(IServiceProvider provider)
        {
            HordeStorageSettings settings = provider.GetService<IOptionsMonitor<HordeStorageSettings>>()!.CurrentValue;

            IRefsStore refsStore = settings.RefDbImplementation switch
            {
                HordeStorageSettings.RefDbImplementations.Memory => ActivatorUtilities.CreateInstance<MemoryRefsStore>(provider),
                HordeStorageSettings.RefDbImplementations.DynamoDb => ActivatorUtilities.CreateInstance<DynamoDbRefsStore>(provider),
                HordeStorageSettings.RefDbImplementations.Mongo => ActivatorUtilities.CreateInstance<MongoRefsStore>(provider),
                HordeStorageSettings.RefDbImplementations.Cosmos => ActivatorUtilities.CreateInstance<CosmosRefsStore>(provider),
                _ => throw new NotImplementedException()
            };

            MemoryCacheRefSettings memoryCacheSettings = provider.GetService<IOptionsMonitor<MemoryCacheRefSettings>>()!.CurrentValue;

            if (memoryCacheSettings.Enabled)
            {
                refsStore = ActivatorUtilities.CreateInstance<CachedRefStore>(provider, refsStore);
            }

            return refsStore;
        }

        protected override void OnAddHealthChecks(IServiceCollection services, IHealthChecksBuilder healthChecks)
        {
            ServiceProvider provider = services.BuildServiceProvider();
            HordeStorageSettings settings = provider.GetService<IOptionsMonitor<HordeStorageSettings>>()!.CurrentValue;

            switch (settings.RefDbImplementation)
            {
                case HordeStorageSettings.RefDbImplementations.Memory:
                    break;
                case HordeStorageSettings.RefDbImplementations.Mongo:
                case HordeStorageSettings.RefDbImplementations.Cosmos:
                    MongoSettings mongoSettings = provider.GetService<IOptionsMonitor<MongoSettings>>()!.CurrentValue;
                    healthChecks.AddMongoDb(mongoSettings.ConnectionString, tags: new[] {"services"});
                    break;
                case HordeStorageSettings.RefDbImplementations.DynamoDb:
                    // dynamo health tests disabled for now as they do not support specifying credentials that are automatically renewed
                    break;
                    /*
                    AWSCredentials awsCredentials = provider.GetService<AWSCredentials>()!;
                    IAmazonDynamoDB amazonDynamoDb = provider.GetService<IAmazonDynamoDB>()!;

                    // if the region endpoint is null, e.g. we are using a local dynamo then we do not create health checks as they do not support this
                    if (amazonDynamoDb.Config.RegionEndpoint != null)
                    {
                        healthChecks.AddDynamoDb(options =>
                        {
                            ImmutableCredentials credentials = awsCredentials.GetCredentials();
                            options.RegionEndpoint = amazonDynamoDb.Config.RegionEndpoint;
                            options.AccessKey = credentials.AccessKey;
                            options.SecretKey = credentials.SecretKey;
                        }, tags: new[] {"services"});
                    }
                    break;*/
                default:
                    throw new NotImplementedException($"Unknown ref db implementation: {settings.RefDbImplementation} when adding health checks");
            }

            foreach (HordeStorageSettings.StorageBackendImplementations impl in settings.GetStorageImplementations())
            {
                switch (impl)
                {
                    case HordeStorageSettings.StorageBackendImplementations.S3:

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
                    case HordeStorageSettings.StorageBackendImplementations.Azure:
                        AzureSettings azureSettings = provider.GetService<IOptionsMonitor<AzureSettings>>()!.CurrentValue;
                        healthChecks.AddAzureBlobStorage(AzureBlobStore.GetConnectionString(azureSettings, provider), tags: new[] {"services"});
                        break;

                    case HordeStorageSettings.StorageBackendImplementations.FileSystem:
                        healthChecks.AddDiskStorageHealthCheck(options =>
                        {
                            FilesystemSettings filesystemSettings = provider.GetService<IOptionsMonitor<FilesystemSettings>>()!.CurrentValue;
                            string? driveRoot = Path.GetPathRoot(PathUtil.ResolvePath(filesystemSettings.RootDir));
                            options.AddDrive(driveRoot);
                        });
                        break;
                    case HordeStorageSettings.StorageBackendImplementations.Memory:
                    case HordeStorageSettings.StorageBackendImplementations.MemoryBlobStore:
                    case HordeStorageSettings.StorageBackendImplementations.Relay:
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

            if (settings.LeaderElectionImplementation == HordeStorageSettings.LeaderElectionImplementations.Kubernetes)
            {
                healthChecks.AddCheck<KubernetesLeaderServiceCheck>("KubernetesLeaderService", tags: new[] {"services"});
            }
        }
    }
}
