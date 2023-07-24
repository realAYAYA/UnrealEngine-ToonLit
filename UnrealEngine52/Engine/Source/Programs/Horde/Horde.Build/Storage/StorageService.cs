// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Buffers.Binary;
using System.Collections;
using System.Collections.Generic;
using System.Composition;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Redis;
using EpicGames.Redis.Utility;
using Horde.Build.Acls;
using Horde.Build.Server;
using Horde.Build.Storage.Backends;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using StackExchange.Redis;

namespace Horde.Build.Storage
{
	using BackendId = StringId<IStorageBackend>;

	/// <summary>
	/// Exception thrown by the <see cref="StorageService"/>
	/// </summary>
	public sealed class StorageException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public StorageException(string message, Exception? inner = null)
			: base(message, inner)
		{
		}
	}

	/// <summary>
	/// Interface for storage clients which includes a backend implementation. Some functionality is exposed through the backend which is not part of the regular storage API (eg. enumerating).
	/// </summary>
	public interface IStorageClientImpl : IStorageClient
	{
		/// <summary>
		/// Configuration for this namespace
		/// </summary>
		NamespaceConfig Config { get; }

		/// <summary>
		/// The storage backend
		/// </summary>
		IStorageBackend Backend { get; }

		/// <summary>
		/// Whether the backend supports redirects
		/// </summary>
		bool SupportsRedirects { get; }

		/// <summary>
		/// Gets a redirect for a read request
		/// </summary>
		/// <param name="locator">Locator for the blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Path to upload the data to</returns>
		ValueTask<Uri?> GetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a redirect for a write request
		/// </summary>
		/// <param name="prefix">Prefix for the new blob locator</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Locator and path to upload the data to</returns>
		ValueTask<(BlobLocator, Uri)?> GetWriteRedirectAsync(Utf8String prefix = default, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Functionality related to the storage service
	/// </summary>
	public sealed class StorageService : IHostedService, IDisposable, IStorageClientFactory
	{
		sealed class StorageClient : StorageClientBase, IStorageClientImpl, IDisposable
		{
			readonly StorageService _outer;
			readonly string _prefix;

			public NamespaceConfig Config { get; }

			public IStorageBackend Backend { get; }

			NamespaceId NamespaceId => Config.Id;

			public bool SupportsRedirects { get; }

			public StorageClient(StorageService outer, NamespaceConfig config, IStorageBackend backend)
			{
				_outer = outer;
				Config = config;
				Backend = backend;
				SupportsRedirects = (backend is IStorageBackendWithRedirects) && !config.EnableAliases;

				_prefix = config.Prefix;
				if (_prefix.Length > 0 && !_prefix.EndsWith("/", StringComparison.Ordinal))
				{
					_prefix += "/";
				}
			}

			public void Dispose()
			{
				if (Backend is IDisposable disposable)
				{
					disposable.Dispose();
				}
			}

			string GetBlobPath(BlobId blobId) => $"{_prefix}{blobId}.blob";

			#region Blobs

			/// <inheritdoc/>
			public override async Task<Stream> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default)
			{
				string path = GetBlobPath(locator.BlobId);

				Stream? stream = await Backend.TryReadAsync(path, cancellationToken);
				if (stream == null)
				{
					throw new StorageException($"Unable to read data from {path}");
				}

				return stream;
			}

			/// <inheritdoc/>
			public ValueTask<Uri?> GetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default)
			{
				IStorageBackendWithRedirects? redirectBackend = Backend as IStorageBackendWithRedirects;
				if (redirectBackend == null)
				{
					return new ValueTask<Uri?>((Uri?)null);
				}
				return redirectBackend.GetReadRedirectAsync(GetBlobPath(locator.BlobId), cancellationToken);
			}

			/// <inheritdoc/>
			public override async Task<Stream> ReadBlobRangeAsync(BlobLocator locator, int offset, int length, CancellationToken cancellationToken = default)
			{
				string path = GetBlobPath(locator.BlobId);

				Stream? stream = await Backend.TryReadAsync(path, offset, length, cancellationToken);
				if (stream == null)
				{
					throw new StorageException($"Unable to read data from {path}");
				}

				return stream;
			}

			/// <inheritdoc/>
			public override async Task<BlobLocator> WriteBlobAsync(Stream stream, Utf8String prefix = default, CancellationToken cancellationToken = default)
			{
				BlobLocator locator;
				if (Config.EnableAliases)
				{
					using (MemoryStream memoryStream = new MemoryStream())
					{
						// Read the blob into memory
						await stream.CopyToAsync(memoryStream, cancellationToken);

						// Reset the position back to zero and read the header
						memoryStream.Position = 0;
						BundleHeader header = await BundleHeader.FromStreamAsync(memoryStream, cancellationToken);

						List<ExportInfo> exports = new List<ExportInfo>();
						for (int idx = 0; idx < header.Exports.Count; idx++)
						{
							BundleExport export = header.Exports[idx];
							if (!export.Alias.IsEmpty)
							{
								exports.Add(new ExportInfo(export.Alias.ToString(), export.Hash, idx));
							}
						}

						// Add the blob record
						locator = await _outer.AddBlobAsync(NamespaceId, prefix, exports, cancellationToken);

						// Write it to the backend
						string path = GetBlobPath(locator.BlobId);
						memoryStream.Position = 0;
						await Backend.WriteAsync(path, memoryStream, cancellationToken);
					}
				}
				else
				{
					// Add the blob record
					locator = await _outer.AddBlobAsync(NamespaceId, prefix, null, cancellationToken);

					// Write it to the backend
					string path = GetBlobPath(locator.BlobId);
					await Backend.WriteAsync(path, stream, cancellationToken);
				}
				return locator;
			}

			/// <inheritdoc/>
			public async ValueTask<(BlobLocator, Uri)?> GetWriteRedirectAsync(Utf8String prefix = default, CancellationToken cancellationToken = default)
			{
				IStorageBackendWithRedirects? redirectBackend = Backend as IStorageBackendWithRedirects;
				if (redirectBackend == null)
				{
					return null;
				}

				BlobLocator locator = await _outer.AddBlobAsync(NamespaceId, prefix, null, cancellationToken);
				string path = GetBlobPath(locator.BlobId);

				Uri? url = await redirectBackend.GetWriteRedirectAsync(path, cancellationToken);
				if (url == null)
				{
					return null;
				}

				return (locator, url);
			}

			public async Task DeleteBlobAsync(BlobId blobId, CancellationToken cancellationToken = default)
			{
				string path = GetBlobPath(blobId);
				await Backend.DeleteAsync(path, cancellationToken);
			}

			#endregion

			#region Nodes

			/// <inheritdoc/>
			public override IAsyncEnumerable<NodeHandle> FindNodesAsync(Utf8String alias, CancellationToken cancellationToken = default) => _outer.FindNodesAsync(NamespaceId, alias, cancellationToken);

			#endregion

			#region Refs

			/// <inheritdoc/>
			public override Task<NodeHandle?> TryReadRefTargetAsync(RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default) => _outer.TryReadRefTargetAsync(NamespaceId, name, cacheTime, cancellationToken);

			/// <inheritdoc/>
			public override Task WriteRefTargetAsync(RefName name, NodeHandle target, RefOptions? options = null, CancellationToken cancellationToken = default) => _outer.WriteRefTargetAsync(NamespaceId, name, target, options, cancellationToken);

			/// <inheritdoc/>
			public override Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default) => _outer.DeleteRefAsync(NamespaceId, name, cancellationToken);

			#endregion
		}

		class NamespaceInfo : IDisposable
		{
			public NamespaceConfig Config { get; }
			public StorageClient Client { get; }

			public NamespaceInfo(NamespaceConfig config, StorageClient client)
			{
				Config = config;
				Client = client;
			}

			public void Dispose() => (Client as IDisposable)?.Dispose();
		}

		class State : IDisposable
		{
			public StorageConfig Config { get; }
			public Dictionary<NamespaceId, NamespaceInfo> Namespaces { get; } = new Dictionary<NamespaceId, NamespaceInfo>();

			public State(StorageConfig config)
			{
				Config = config;
			}

			public void Dispose()
			{
				foreach (NamespaceInfo namespaceInfo in Namespaces.Values)
				{
					namespaceInfo.Dispose();
				}
			}
		}

		class ExportInfo
		{
			[BsonElement("alias")]
			public string Alias { get; set; } = String.Empty;

			[BsonElement("hash")]
			public IoHash Hash { get; set; }

			[BsonElement("idx")]
			public int Index { get; set; }

			public ExportInfo()
			{
			}

			public ExportInfo(string alias, IoHash hash, int index)
			{
				Alias = alias;
				Hash = hash;
				Index = index;
			}
		}

		class BlobInfo
		{
			public ObjectId Id { get; set; }

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; }

			[BsonElement("host")]
			public HostId HostId { get; set; }

			[BsonElement("blob")]
			public BlobId BlobId { get; set; }

			[BsonElement("imp"), BsonIgnoreIfNull]
			public List<ObjectId>? Imports { get; set; }

			[BsonElement("exp"), BsonIgnoreIfNull]
			public List<ExportInfo>? Exports { get; set; }

			[BsonIgnore]
			public BlobLocator Locator => new BlobLocator(HostId, BlobId);

			public BlobInfo()
			{
			}

			public BlobInfo(ObjectId id, NamespaceId namespaceId, HostId hostId, BlobId blobId)
			{
				Id = id;
				NamespaceId = namespaceId;
				HostId = hostId;
				BlobId = blobId;
			}
		}

		class RefInfo
		{
			[BsonIgnoreIfDefault]
			public ObjectId Id { get; set; }

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; }

			[BsonElement("name")]
			public RefName Name { get; set; }

			[BsonElement("hash")]
			public IoHash Hash { get; set; }

			[BsonElement("blob")]
			public BlobLocator BlobLocator { get; set; }

			[BsonElement("binf")]
			public ObjectId BlobInfoId { get; set; }

			[BsonElement("idx")]
			public int ExportIdx { get; set; }

			[BsonElement("xa"), BsonIgnoreIfDefault]
			public DateTime? ExpiresAtUtc { get; set; }

			[BsonElement("xt"), BsonIgnoreIfDefault]
			public TimeSpan? Lifetime { get; set; }

			[BsonIgnore]
			public NodeHandle Target => new NodeHandle(Hash, BlobLocator, ExportIdx);

			public RefInfo()
			{
				Name = RefName.Empty;
				BlobLocator = BlobLocator.Empty;
			}

			public RefInfo(NamespaceId namespaceId, RefName name, NodeHandle target, ObjectId blobInfoId)
			{
				NamespaceId = namespaceId;
				Name = name;
				Hash = target.Hash;
				BlobLocator = target.Locator.Blob;
				BlobInfoId = blobInfoId;
				ExportIdx = target.Locator.ExportIdx;
			}

			public bool HasExpired(DateTime utcNow) => ExpiresAtUtc.HasValue && utcNow >= ExpiresAtUtc.Value;

			public bool RequiresTouch(DateTime utcNow) => ExpiresAtUtc.HasValue && Lifetime.HasValue && utcNow >= ExpiresAtUtc.Value - new TimeSpan(Lifetime.Value.Ticks / 4);
		}

		[SingletonDocument("gc-state")]
		class GcState : SingletonBase
		{
			public ObjectId LastImportBlobInfoId { get; set; }
			public List<GcNamespaceState> Namespaces { get; set; } = new List<GcNamespaceState>();

			public GcNamespaceState FindOrAddNamespace(NamespaceId namespaceId)
			{
				GcNamespaceState? namespaceState = Namespaces.FirstOrDefault(x => x.Id == namespaceId);
				if (namespaceState == null)
				{
					namespaceState = new GcNamespaceState { Id = namespaceId, LastTime = DateTime.UtcNow };
					Namespaces.Add(namespaceState);
				}
				return namespaceState;
			}
		}

		class GcNamespaceState
		{
			public NamespaceId Id { get; set; }
			public DateTime LastTime { get; set; }
		}

		readonly GlobalsService _globalsService;
		readonly RedisService _redisService;
		readonly AclService _aclService;
		readonly IClock _clock;
		readonly IMemoryCache _cache;
		readonly IServiceProvider _serviceProvider;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly ILogger _logger;

		readonly IMongoCollection<BlobInfo> _blobCollection;
		readonly IMongoCollection<RefInfo> _refCollection;

		readonly ITicker _blobTicker;
		readonly ITicker _refTicker;

		readonly SingletonDocument<GcState> _gcState;
		readonly ITicker _gcTicker;

		State? _lastState;
		string? _lastConfigRevision;

		readonly AsyncCachedValue<State> _cachedState;

		static readonly BackendConfig s_defaultBackendConfig = new BackendConfig();

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageService(GlobalsService globalsService, MongoService mongoService, RedisService redisService, AclService aclService, IClock clock, IMemoryCache cache, IServiceProvider serviceProvider, IOptionsMonitor<GlobalConfig> globalConfig, ILogger<StorageService> logger)
		{
			_globalsService = globalsService;
			_redisService = redisService;
			_aclService = aclService;
			_clock = clock;
			_cache = cache;
			_serviceProvider = serviceProvider;
			_cachedState = new AsyncCachedValue<State>(() => Task.FromResult(GetNextState()), TimeSpan.FromMinutes(1.0));
			_globalConfig = globalConfig;
			_logger = logger;

			List<MongoIndex<BlobInfo>> blobIndexes = new List<MongoIndex<BlobInfo>>();
			blobIndexes.Add(keys => keys.Ascending(x => x.Imports));
			blobIndexes.Add(keys => keys.Ascending(x => x.NamespaceId).Ascending(x => x.BlobId), unique: true);
			blobIndexes.Add(keys => keys.Ascending(x => x.NamespaceId).Ascending($"{nameof(BlobInfo.Exports)}.{nameof(ExportInfo.Alias)}"));
			_blobCollection = mongoService.GetCollection<BlobInfo>("Storage.Blobs", blobIndexes);

			List<MongoIndex<RefInfo>> refIndexes = new List<MongoIndex<RefInfo>>();
			refIndexes.Add(keys => keys.Ascending(x => x.NamespaceId).Ascending(x => x.Name), unique: true);
			refIndexes.Add(keys => keys.Ascending(x => x.BlobInfoId));
			_refCollection = mongoService.GetCollection<RefInfo>("Storage.Refs", refIndexes);

			_blobTicker = clock.AddSharedTicker("Storage:Blobs", TimeSpan.FromMinutes(5.0), TickBlobsAsync, _logger);
			_refTicker = clock.AddSharedTicker("Storage:Refs", TimeSpan.FromMinutes(5.0), TickRefsAsync, _logger);

			_gcState = new SingletonDocument<GcState>(mongoService);
			_gcTicker = clock.AddTicker("Storage:GC", TimeSpan.FromMinutes(5.0), TickGcAsync, logger);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			if (_lastState != null)
			{
				_lastState.Dispose();
				_lastState = null;
			}

			_blobTicker.Dispose();
			_refTicker.Dispose();
			_gcTicker.Dispose();
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _blobTicker.StartAsync();
			await _refTicker.StartAsync();
			await _gcTicker.StartAsync();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _gcTicker.StopAsync();
			await _refTicker.StopAsync();
			await _blobTicker.StopAsync();
		}

		async ValueTask<NamespaceInfo?> TryGetNamespaceInfoAsync(NamespaceId namespaceId, CancellationToken cancellationToken)
		{
			State state = await _cachedState.GetAsync(cancellationToken);
			state.Namespaces.TryGetValue(namespaceId, out NamespaceInfo? namespaceInfo);
			return namespaceInfo;
		}

		async ValueTask<NamespaceInfo> GetNamespaceInfoAsync(NamespaceId namespaceId, CancellationToken cancellationToken)
		{
			NamespaceInfo? namespaceInfo = await TryGetNamespaceInfoAsync(namespaceId, cancellationToken);
			if (namespaceInfo == null)
			{
				throw new StorageException($"No namespace '{namespaceId}' is configured.");
			}
			return namespaceInfo;
		}

		/// <summary>
		/// Finds the configuration for all current namespaces
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of namespace configurations</returns>
		public async Task<List<NamespaceConfig>> GetNamespacesAsync(CancellationToken cancellationToken)
		{
			State state = await _cachedState.GetAsync(cancellationToken);
			return state.Namespaces.Select(x => x.Value.Config).ToList();
		}

		/// <summary>
		/// Gets a storage client for the given namespace
		/// </summary>
		/// <param name="namespaceId">Namespace identifier</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async ValueTask<IStorageClientImpl> GetClientAsync(NamespaceId namespaceId, CancellationToken cancellationToken)
		{
			NamespaceInfo namespaceInfo = await GetNamespaceInfoAsync(namespaceId, cancellationToken);
			return namespaceInfo.Client;
		}

		/// <inheritdoc/>
		async ValueTask<IStorageClient> IStorageClientFactory.GetClientAsync(NamespaceId namespaceId, CancellationToken cancellationToken) => await GetClientAsync(namespaceId, cancellationToken);

		#region Config

		State GetNextState()
		{
			GlobalConfig globalConfig = _globalConfig.CurrentValue;
			if (_lastState == null || !String.Equals(_lastConfigRevision, globalConfig.Revision, StringComparison.Ordinal))
			{
				StorageConfig storageConfig = globalConfig.Storage;

				// Create a lookup for backend config objects by their id
				Dictionary<BackendId, BackendConfig> backendIdToConfig = new Dictionary<BackendId, BackendConfig>();
				foreach (BackendConfig backendConfig in storageConfig.Backends)
				{
					backendIdToConfig.Add(backendConfig.Id, backendConfig);
				}

				// Create a lookup of namespace id to config, to ensure there are no duplicates
				Dictionary<NamespaceId, NamespaceConfig> namespaceIdToConfig = new Dictionary<NamespaceId, NamespaceConfig>();
				foreach (NamespaceConfig namespaceConfig in storageConfig.Namespaces)
				{
					namespaceIdToConfig.Add(namespaceConfig.Id, namespaceConfig);
				}

				// Configure the new clients
				State nextState = new State(storageConfig);
				try
				{
					Dictionary<BackendId, BackendConfig> mergedIdToConfig = new Dictionary<BackendId, BackendConfig>();
					foreach (NamespaceConfig namespaceConfig in storageConfig.Namespaces)
					{
						if (namespaceConfig.Backend.IsEmpty)
						{
							throw new StorageException($"No backend configured for namespace {namespaceConfig.Id}");
						}

						BackendConfig backendConfig = GetBackendConfig(namespaceConfig.Backend, backendIdToConfig, mergedIdToConfig);
						IStorageBackend backend = CreateStorageBackend(backendConfig);

#pragma warning disable CA2000 // Dispose objects before losing scope
						StorageClient client = new StorageClient(this, namespaceConfig, backend);
						nextState.Namespaces.Add(namespaceConfig.Id, new NamespaceInfo(namespaceConfig, client));
#pragma warning restore CA2000 // Dispose objects before losing scope
					}
				}
				catch
				{
					nextState.Dispose();
					throw;
				}

				_lastState?.Dispose();
				_lastState = nextState;

				_lastConfigRevision = globalConfig.Revision;
			}
			return _lastState;
		}

		/// <summary>
		/// Creates a storage backend with the given configuration
		/// </summary>
		/// <param name="config">Configuration for the backend</param>
		/// <returns>New storage backend instance</returns>
		IStorageBackend CreateStorageBackend(BackendConfig config)
		{
			switch (config.Type ?? StorageBackendType.FileSystem)
			{
				case StorageBackendType.FileSystem:
					return new FileSystemStorageBackend(config);
				case StorageBackendType.Aws:
					return new AwsStorageBackend(_serviceProvider.GetRequiredService<IConfiguration>(), config, _serviceProvider.GetRequiredService<ILogger<AwsStorageBackend>>());
				case StorageBackendType.Memory:
					return new MemoryStorageBackend();
				default:
					throw new NotImplementedException();
			}
		}

		/// <summary>
		/// Gets the configuration for a particular backend
		/// </summary>
		/// <param name="backendId">Identifier for the backend</param>
		/// <param name="baseIdToConfig">Lookup for all backend configuration data</param>
		/// <param name="mergedIdToConfig">Lookup for computed hierarchical config objects</param>
		/// <returns>Merged config for the given backend</returns>
		BackendConfig GetBackendConfig(BackendId backendId, Dictionary<BackendId, BackendConfig> baseIdToConfig, Dictionary<BackendId, BackendConfig> mergedIdToConfig)
		{
			BackendConfig? config;
			if (mergedIdToConfig.TryGetValue(backendId, out config))
			{
				if (config == null)
				{
					throw new StorageException($"Configuration for storage backend '{backendId}' is recursive.");
				}
			}
			else
			{
				mergedIdToConfig.Add(backendId, null!);

				if (!baseIdToConfig.TryGetValue(backendId, out config))
				{
					throw new StorageException($"Unable to find storage backend '{backendId}'"); 
				}

				if (config.Base != BackendId.Empty)
				{
					BackendConfig baseConfig = GetBackendConfig(config.Base, baseIdToConfig, mergedIdToConfig);
					MergeConfigs(baseConfig, config, s_defaultBackendConfig);
				}

				mergedIdToConfig[backendId] = config;
			}
			return config;
		}

		/// <summary>
		/// Allows one configuration object to override another
		/// </summary>
		static void MergeConfigs<T>(T sourceObject, T targetObject, T defaultObject)
		{
			PropertyInfo[] properties = typeof(T).GetProperties(BindingFlags.Public | BindingFlags.Instance);
			foreach (PropertyInfo property in properties)
			{
				object? targetValue = property.GetValue(targetObject);
				object? defaultValue = property.GetValue(defaultObject);

				if (ValueEquals(targetValue, defaultValue))
				{
					object? sourceValue = property.GetValue(sourceObject);
					property.SetValue(targetObject, sourceValue);
				}
			}
		}

		/// <summary>
		/// Tests two objects for value equality
		/// </summary>
		static bool ValueEquals(object? source, object? target)
		{
			if (ReferenceEquals(source, null) || ReferenceEquals(target, null))
			{
				return ReferenceEquals(source, target);
			}

			if (source is IEnumerable _)
			{
				IEnumerator sourceEnumerator = ((IEnumerable)source).GetEnumerator();
				IEnumerator targetEnumerator = ((IEnumerable)target).GetEnumerator();

				for (; ; )
				{
					if (!sourceEnumerator.MoveNext())
					{
						return !targetEnumerator.MoveNext();
					}
					if (!targetEnumerator.MoveNext() || !ValueEquals(sourceEnumerator.Current, targetEnumerator.Current))
					{
						return false;
					}
				}
			}

			return source.Equals(target);
		}

		#endregion

		#region Blobs

		/// <inheritdoc/>
		async Task<BlobLocator> AddBlobAsync(NamespaceId namespaceId, Utf8String prefix = default, List<ExportInfo>? exports = null, CancellationToken cancellationToken = default)
		{
			HostId hostId = HostId.Empty;

			ObjectId id = ObjectId.GenerateNewId(_clock.UtcNow);
			BlobId blobId = (prefix.Length > 0) ? new BlobId($"{prefix}/{id}") : new BlobId(id.ToString());

			BlobInfo blobInfo = new BlobInfo(id, namespaceId, hostId, blobId);
			blobInfo.Exports = exports;
			await _blobCollection.InsertOneAsync(blobInfo, new InsertOneOptions { }, cancellationToken);

			return new BlobLocator(hostId, blobId);
		}

		/// <inheritdoc/>
		async Task<bool> IsBlobReferenced(ObjectId blobInfoId, CancellationToken cancellationToken = default)
		{
			FilterDefinition<BlobInfo> blobFilter = Builders<BlobInfo>.Filter.AnyEq(x => x.Imports, blobInfoId);
			if (await _blobCollection.Find(blobFilter).AnyAsync(cancellationToken))
			{
				return true;
			}

			FilterDefinition<RefInfo> refFilter = Builders<RefInfo>.Filter.Eq(x => x.BlobInfoId, blobInfoId);
			if (await _refCollection.Find(refFilter).AnyAsync(cancellationToken))
			{
				return true;
			}

			return false;
		}

		/// <summary>
		/// Finds blobs at least 30 minutes old and computes import metadata for them. Done with a delay to allow write redirects.
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async ValueTask TickBlobsAsync(CancellationToken cancellationToken)
		{
			GcState state = await _gcState.GetAsync();
			DateTime utcNow = _clock.UtcNow;

			// Cached storage clients for each namespace
			Dictionary<NamespaceId, IStorageClient?> namespaceIdToClient = new Dictionary<NamespaceId, IStorageClient?>();

			// Compute missing import info, by searching for blobs with an ObjectId timestamp after the last import compute cycle
			ObjectId latestInfoId = ObjectId.GenerateNewId(utcNow - TimeSpan.FromMinutes(30.0));
			using (IAsyncCursor<BlobInfo> cursor = await _blobCollection.Find(x => x.Id >= state.LastImportBlobInfoId && x.Id < latestInfoId).ToCursorAsync(cancellationToken))
			{
				while (await cursor.MoveNextAsync(cancellationToken))
				{
					// Find imports, and add a check record for each new blob
					foreach (BlobInfo blobInfo in cursor.Current)
					{
						IStorageClient? client;
						if (!namespaceIdToClient.TryGetValue(blobInfo.NamespaceId, out client))
						{
							NamespaceInfo? namespaceInfo = await TryGetNamespaceInfoAsync(blobInfo.NamespaceId, cancellationToken);
							client = namespaceInfo?.Client;
							namespaceIdToClient.Add(blobInfo.NamespaceId, client);
						}

						if (client != null)
						{
							BundleHeader? header = await ReadHeaderAsync(client, blobInfo.Locator, cancellationToken);
							if (header != null)
							{
								List<ObjectId> importInfoIds = new List<ObjectId>();
								foreach (BundleImport import in header.Imports)
								{
									FilterDefinition<BlobInfo> filter = Builders<BlobInfo>.Filter.Expr(x => x.NamespaceId == blobInfo.NamespaceId && x.BlobId == import.Locator.BlobId);
									UpdateDefinition<BlobInfo> update = Builders<BlobInfo>.Update.SetOnInsert(x => x.HostId, import.Locator.HostId);
									BlobInfo blobInfoDoc = await _blobCollection.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<BlobInfo> { IsUpsert = true, ReturnDocument = ReturnDocument.After }, cancellationToken);
									importInfoIds.Add(blobInfoDoc.Id);
								}
								await _blobCollection.UpdateOneAsync(x => x.Id == blobInfo.Id, Builders<BlobInfo>.Update.Set(x => x.Imports, importInfoIds), null, cancellationToken);
							}
							AddGcCheckRecord(blobInfo.NamespaceId, blobInfo.Id);
						}
					}

					// Update the last imported blob id
					await _gcState.UpdateAsync(state => state.LastImportBlobInfoId = latestInfoId);
				}
			}
		}

		#endregion

		#region Nodes

		/// <summary>
		/// Finds nodes with the given type and hash
		/// </summary>
		/// <param name="namespaceId">Namespace to search</param>
		/// <param name="alias">Alias for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of thandles</returns>
		async IAsyncEnumerable<NodeHandle> FindNodesAsync(NamespaceId namespaceId, Utf8String alias, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			await foreach (BlobInfo blobInfo in _blobCollection.Find(x => x.NamespaceId == namespaceId && x.Exports!.Any(y => y.Alias == alias)).ToAsyncEnumerable(cancellationToken))
			{
				if (blobInfo.Exports != null)
				{
					foreach (ExportInfo exportInfo in blobInfo.Exports)
					{
						if (exportInfo.Alias == alias)
						{
							yield return new NodeHandle(exportInfo.Hash, blobInfo.Locator, exportInfo.Index);
						}
					}
				}
			}
		}

		#endregion

		#region Refs

		record RefCacheKey(NamespaceId NamespaceId, RefName Name);
		record RefCacheValue(RefInfo? Value, DateTime Time);

		/// <summary>
		/// Adds a ref value to the cache
		/// </summary>
		/// <param name="namespaceId">Namespace containing the ref</param>
		/// <param name="name">Name of the ref</param>
		/// <param name="value">New target for the ref</param>
		/// <returns>The cached value</returns>
		RefCacheValue AddRefToCache(NamespaceId namespaceId, RefName name, RefInfo? value)
		{
			RefCacheValue cacheValue = new RefCacheValue(value, DateTime.UtcNow);
			using (ICacheEntry newEntry = _cache.CreateEntry(new RefCacheKey(namespaceId, name)))
			{
				newEntry.Value = cacheValue;
				newEntry.SetSize(name.Text.Length);
				newEntry.SetSlidingExpiration(TimeSpan.FromMinutes(5.0));
			}
			return cacheValue;
		}

		/// <summary>
		/// Expires any refs that are no longer valid
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		async ValueTask TickRefsAsync(CancellationToken cancellationToken)
		{
			DateTime utcNow = DateTime.UtcNow;
			using (IAsyncCursor<RefInfo> cursor = await _refCollection.Find(x => x.ExpiresAtUtc < utcNow).ToCursorAsync(cancellationToken))
			{
				List<DeleteOneModel<RefInfo>> requests = new List<DeleteOneModel<RefInfo>>();
				while (await cursor.MoveNextAsync(cancellationToken))
				{
					requests.Clear();

					foreach (RefInfo refInfo in cursor.Current)
					{
						_logger.LogInformation("Expired ref {NamespaceId}:{RefName}", refInfo.NamespaceId, refInfo.Name);
						FilterDefinition<RefInfo> filter = Builders<RefInfo>.Filter.Expr(x => x.Id == refInfo.Id && x.ExpiresAtUtc == refInfo.ExpiresAtUtc);
						requests.Add(new DeleteOneModel<RefInfo>(filter));
						AddGcCheckRecord(refInfo.NamespaceId, refInfo.BlobInfoId);
						AddRefToCache(refInfo.NamespaceId, refInfo.Name, default);
					}

					if (requests.Count > 0)
					{
						await _refCollection.BulkWriteAsync(requests, cancellationToken: cancellationToken);
					}
				}
			}
		}

		/// <inheritdoc/>
		async Task DeleteRefAsync(NamespaceId namespaceId, RefName name, CancellationToken cancellationToken = default)
		{
			FilterDefinition<RefInfo> filter = Builders<RefInfo>.Filter.Expr(x => x.NamespaceId == namespaceId && x.Name == name);
			await DeleteRefInternalAsync(namespaceId, name, filter, cancellationToken);
		}

		/// <summary>
		/// Deletes a ref document that has reached its expiry time
		/// </summary>
		async Task DeleteExpiredRefAsync(RefInfo refDocument, CancellationToken cancellationToken = default)
		{
			FilterDefinition<RefInfo> filter = Builders<RefInfo>.Filter.Expr(x => x.Id == refDocument.Id && x.ExpiresAtUtc == refDocument.ExpiresAtUtc);
			await DeleteRefInternalAsync(refDocument.NamespaceId, refDocument.Name, filter, cancellationToken);
		}

		async Task DeleteRefInternalAsync(NamespaceId namespaceId, RefName name, FilterDefinition<RefInfo> filter, CancellationToken cancellationToken = default)
		{
			RefInfo? oldRefInfo = await _refCollection.FindOneAndDeleteAsync<RefInfo>(filter, cancellationToken: cancellationToken);
			if (oldRefInfo != null)
			{
				_logger.LogInformation("Deleted ref {NamespaceId}:{RefName}", namespaceId, name);
				AddGcCheckRecord(namespaceId, oldRefInfo.BlobInfoId);
			}
			AddRefToCache(namespaceId, name, default);
		}

		/// <inheritdoc/>
		async Task<NodeHandle?> TryReadRefTargetAsync(NamespaceId namespaceId, RefName name, DateTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			RefCacheValue entry;
			if (!_cache.TryGetValue(name, out entry) || entry.Time < cacheTime)
			{
				RefInfo? refDocument = await _refCollection.Find(x => x.NamespaceId == namespaceId && x.Name == name).FirstOrDefaultAsync(cancellationToken);
				entry = AddRefToCache(namespaceId, name, refDocument);
			}

			if (entry.Value == null)
			{
				return null;
			}

			if (entry.Value.ExpiresAtUtc != null)
			{
				DateTime utcNow = _clock.UtcNow;
				if (entry.Value.HasExpired(utcNow))
				{
					await DeleteExpiredRefAsync(entry.Value, cancellationToken);
					return default;
				}
				if (entry.Value.RequiresTouch(utcNow))
				{
					await _refCollection.UpdateOneAsync(x => x.Id == entry.Value.Id, Builders<RefInfo>.Update.Set(x => x.ExpiresAtUtc, utcNow + entry.Value.Lifetime!.Value), cancellationToken: cancellationToken);
				}
			}

			return entry.Value.Target;
		}

		/// <inheritdoc/>
		async Task WriteRefTargetAsync(NamespaceId namespaceId, RefName name, NodeHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			BlobInfo? newBlobInfo = await _blobCollection.Find(x => x.NamespaceId == namespaceId && x.BlobId == target.Locator.Blob.BlobId).FirstOrDefaultAsync(cancellationToken);
			if (newBlobInfo == null)
			{
				throw new Exception($"Invalid/unknown blob identifier '{target.Locator.Blob.BlobId}' in namespace {namespaceId}");
			}

			RefInfo newRefInfo = new RefInfo(namespaceId, name, target, newBlobInfo.Id);

			if (options != null && options.Lifetime.HasValue)
			{
				newRefInfo.ExpiresAtUtc = _clock.UtcNow + options.Lifetime.Value;
				if (options.Extend ?? true)
				{
					newRefInfo.Lifetime = options.Lifetime;
				}
			}

			RefInfo? oldRefInfo = await _refCollection.FindOneAndReplaceAsync<RefInfo>(x => x.NamespaceId == namespaceId && x.Name == name, newRefInfo, new FindOneAndReplaceOptions<RefInfo> { IsUpsert = true }, cancellationToken);
			if (oldRefInfo != null && oldRefInfo.BlobInfoId != newRefInfo.BlobInfoId)
			{
				AddGcCheckRecord(namespaceId, oldRefInfo.BlobInfoId);
			}

			if (oldRefInfo == null)
			{
				_logger.LogInformation("Inserted ref {NamespaceId}:{RefName} to {Target}", namespaceId, name, target);
			}
			else if (oldRefInfo.Target != newRefInfo.Target)
			{
				_logger.LogInformation("Updated ref {NamespaceId}:{RefName} to {Target} (was {OldTarget})", namespaceId, name, newRefInfo.Target, oldRefInfo.Target);
			}
			else
			{
				_logger.LogInformation("Updated ref {NamespaceId}:{RefName} to {Target} (no-op)", namespaceId, name, target);
			}

			AddRefToCache(namespaceId, name, newRefInfo);
		}

		#endregion

		#region GC

		uint GetGcTimestamp() => GetGcTimestamp(_clock.UtcNow);
		static uint GetGcTimestamp(DateTime utcTime) => (uint)((utcTime - DateTime.UnixEpoch).Ticks / TimeSpan.TicksPerMinute);

		static RedisKey GetRedisKey(string suffix) => $"storage:{suffix}";
		static RedisKey GetRedisKey(NamespaceId namespaceId, string suffix) => GetRedisKey($"{namespaceId}:{suffix}");

		static RedisSortedSetKey<RedisValue> GetGcCheckSet(NamespaceId namespaceId) => new RedisSortedSetKey<RedisValue>(GetRedisKey(namespaceId, "check"));

		/// <summary>
		/// Find the next namespace to run GC on
		/// </summary>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		async ValueTask TickGcAsync(CancellationToken cancellationToken)
		{
			HashSet<NamespaceId> ranNamespaceIds = new HashSet<NamespaceId>();

			DateTime utcNow = _clock.UtcNow;
			for (; ; )
			{
				// Synchronize the list of configured namespaces with the GC state object
				List<NamespaceConfig> namespaces = await GetNamespacesAsync(cancellationToken);

				GcState state = await _gcState.GetAsync();
				if (!Enumerable.SequenceEqual(namespaces.Select(x => x.Id).OrderBy(x => x), state.Namespaces.Select(x => x.Id).OrderBy(x => x)))
				{
					state = await _gcState.UpdateAsync(s => SyncNamespaceList(s, namespaces));
				}

				// Find all the namespaces that need to have GC run on them
				List<(DateTime, GcNamespaceState)> pending = new List<(DateTime, GcNamespaceState)>();
				foreach (GcNamespaceState namespaceState in state.Namespaces)
				{
					if (!ranNamespaceIds.Contains(namespaceState.Id))
					{
						NamespaceConfig? config = namespaces.FirstOrDefault(x => x.Id == namespaceState.Id);
						if (config != null)
						{
							DateTime time = namespaceState.LastTime + TimeSpan.FromHours(config.GcFrequencyHrs);
							if (time < utcNow)
							{
								pending.Add((time, namespaceState));
							}
						}
					}
				}
				pending.SortBy(x => x.Item1);

				// If there's nothing left to GC, bail out
				if (pending.Count == 0)
				{
					break;
				}

				// Update the first one we can acquire a lock for
				foreach ((_, GcNamespaceState namespaceState) in pending)
				{
					NamespaceId namespaceId = namespaceState.Id;
					if (ranNamespaceIds.Add(namespaceId))
					{
						RedisKey key = GetRedisKey(namespaceId, "lock");
						using (RedisLock namespaceLock = new RedisLock(_redisService.GetDatabase(), key))
						{
							if (await namespaceLock.AcquireAsync(TimeSpan.FromMinutes(20.0)))
							{
								try
								{
									await TickGcForNamespaceAsync(namespaceId, state.LastImportBlobInfoId, utcNow, cancellationToken);
								}
								catch (Exception ex)
								{
									_logger.LogError(ex, "Exception while running garbage collection: {Message}", ex.Message);
								}
								break;
							}
						}
					}
				}
			}
		}

		async Task TickGcForNamespaceAsync(NamespaceId namespaceId, ObjectId lastImportBlobInfoId, DateTime utcNow, CancellationToken cancellationToken)
		{
			NamespaceInfo namespaceInfo = await GetNamespaceInfoAsync(namespaceId, cancellationToken);

			double score = GetGcTimestamp(utcNow);

			RedisSortedSetKey<RedisValue> checkSet = GetGcCheckSet(namespaceId);
			for (; ; )
			{
				RedisValue[] values = await _redisService.GetDatabase().SortedSetRangeByRankAsync(checkSet, 0, 0);
				if (values.Length == 0)
				{
					break;
				}

				ObjectId blobInfoId = new ObjectId(((byte[]?)values[0])!);
				if (blobInfoId < lastImportBlobInfoId && !await IsBlobReferenced(blobInfoId, cancellationToken))
				{
					BlobInfo? info = await _blobCollection.FindOneAndDeleteAsync(x => x.Id == blobInfoId, cancellationToken: cancellationToken);
					if (info != null)
					{
						if (info.Imports != null)
						{
							SortedSetEntry<RedisValue>[] entries = info.Imports.Select(x => new SortedSetEntry<RedisValue>(x.ToByteArray(), score)).ToArray();
							_ = _redisService.GetDatabase().SortedSetAddAsync(checkSet, entries, flags: CommandFlags.FireAndForget);
							score = Math.BitIncrement(score);
						}
						await namespaceInfo.Client.DeleteBlobAsync(info.BlobId, cancellationToken);
					}
				}
				_ = _redisService.GetDatabase().SortedSetRemoveAsync(checkSet, values[0], CommandFlags.FireAndForget);
			}

			await _gcState.UpdateAsync(state => state.FindOrAddNamespace(namespaceId).LastTime = utcNow);
		}

		async Task<BundleHeader?> ReadHeaderAsync(IStorageClient store, BlobLocator locator, CancellationToken cancellationToken)
		{
			int fetchSize = 64 * 1024;
			for (; ; )
			{
				using (IMemoryOwner<byte> owner = MemoryPool<byte>.Shared.Rent(fetchSize))
				{
					using (Stream? stream = await store.ReadBlobRangeAsync(locator, 0, fetchSize, cancellationToken))
					{
						if (stream == null)
						{
							_logger.LogError("Unable to read blob {Blob}", locator);
							return null;
						}

						// Read the start of the blob
						Memory<byte> memory = owner.Memory.Slice(0, fetchSize);

						int length = await stream.ReadGreedyAsync(memory, cancellationToken);
						if (length < BundleHeader.PreludeLength)
						{
							_logger.LogError("Blob {Blob} does not have a valid prelude", locator);
							return null;
						}

						memory = memory.Slice(0, length);

						// Make sure it's large enough to hold the header
						int headerSize = BundleHeader.ReadPrelude(memory);
						if (headerSize <= fetchSize)
						{
							return new BundleHeader(new MemoryReader(memory));
						}

						// Increase the fetch size and retry
						fetchSize = headerSize;
					}
				}
			}
		}

		static void SyncNamespaceList(GcState state, List<NamespaceConfig> namespaces)
		{
			HashSet<NamespaceId> validNamespaceIds = new HashSet<NamespaceId>(namespaces.Select(x => x.Id));
			state.Namespaces.RemoveAll(x => !validNamespaceIds.Contains(x.Id));

			HashSet<NamespaceId> currentNamespaceIds = new HashSet<NamespaceId>(state.Namespaces.Select(x => x.Id));
			foreach (NamespaceConfig config in namespaces)
			{
				if (!currentNamespaceIds.Contains(config.Id))
				{
					state.Namespaces.Add(new GcNamespaceState { Id = config.Id });
				}
			}

			state.Namespaces.SortBy(x => x.Id);
		}

		void AddGcCheckRecord(NamespaceId namespaceId, ObjectId blobInfoId)
		{
			double score = GetGcTimestamp();
			_ = _redisService.GetDatabase().SortedSetAddAsync(GetGcCheckSet(namespaceId), blobInfoId.ToByteArray(), score, flags: CommandFlags.FireAndForget);
		}

		#endregion
	}
}
