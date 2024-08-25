// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.ObjectStores;
using EpicGames.Redis;
using EpicGames.Redis.Utility;
using Horde.Server.Server;
using Horde.Server.Utilities;
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
using OpenTelemetry.Trace;
using StackExchange.Redis;

namespace Horde.Server.Storage
{
	/// <summary>
	/// Functionality related to the storage service
	/// </summary>
	public sealed class StorageService : IHostedService, IStorageClientFactory, IAsyncDisposable
	{
		class RefCount
		{
			int _value;

			public RefCount() => _value = 1;
			public void AddRef() => Interlocked.Increment(ref _value);
			public int Release() => Interlocked.Decrement(ref _value);
		}

		sealed class StorageBackendImpl : IStorageBackend
		{
			readonly StorageService _outer;
			readonly IObjectStore _store;
			readonly NamespaceConfig _config;

			public NamespaceConfig Config => _config;
			public NamespaceId NamespaceId => _config.Id;

			/// <inheritdoc/>
			public bool SupportsRedirects { get; }

			public StorageBackendImpl(StorageService outer, NamespaceConfig config, IObjectStore store)
			{
				_outer = outer;
				_store = store;
				_config = config;

				SupportsRedirects = store.SupportsRedirects && !config.EnableAliases;
			}

			#region Blobs

			public Task<Stream> OpenBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default)
				=> _store.OpenAsync(GetObjectKey(locator), offset, length, cancellationToken);

			/// <inheritdoc/>
			public Task<IReadOnlyMemoryOwner<byte>> ReadBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default)
				=> _store.ReadAsync(GetObjectKey(locator), offset, length, cancellationToken);

			/// <inheritdoc/>
			public async Task<BlobLocator> WriteBlobAsync(Stream stream, string? basePath = null, CancellationToken cancellationToken = default)
			{
				BlobLocator locator = StorageHelpers.CreateUniqueLocator(basePath);

				await _store.WriteAsync(GetObjectKey(locator), stream, cancellationToken);
				await _outer.AddBlobAsync(NamespaceId, locator, null, cancellationToken);

				return locator;
			}

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetBlobReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default)
			{
				return _store.TryGetReadRedirectAsync(GetObjectKey(locator), cancellationToken);
			}

			/// <inheritdoc/>
			public async ValueTask<(BlobLocator, Uri)?> TryGetBlobWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default)
			{
				if (!_store.SupportsRedirects)
				{
					return null;
				}

				BlobLocator locator = StorageHelpers.CreateUniqueLocator(prefix);

				Uri? url = await _store.TryGetWriteRedirectAsync(GetObjectKey(locator), cancellationToken);
				if (url == null)
				{
					return null;
				}

				await _outer.AddBlobAsync(NamespaceId, locator, null, cancellationToken);

				return (locator, url);
			}

			#endregion

			#region Aliases

			/// <inheritdoc/>
			public Task AddAliasAsync(string name, BlobLocator locator, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
				=> _outer.AddAliasAsync(NamespaceId, name, locator, rank, data, cancellationToken);

			/// <inheritdoc/>
			public Task RemoveAliasAsync(string name, BlobLocator locator, CancellationToken cancellationToken = default)
				=> _outer.RemoveAliasAsync(NamespaceId, name, locator, cancellationToken);

			/// <inheritdoc/>
			public async Task<BlobAliasLocator[]> FindAliasesAsync(string alias, int? maxResults, CancellationToken cancellationToken = default)
			{
				List<(BlobLocator, AliasInfo)> aliases = await _outer.FindAliasesAsync(NamespaceId, alias, cancellationToken);
				if (maxResults != null && maxResults.Value < aliases.Count)
				{
					aliases.RemoveRange(maxResults.Value, aliases.Count - maxResults.Value);
				}
				return aliases.Select(x => new BlobAliasLocator(x.Item1, x.Item2.Rank, x.Item2.Data)).ToArray();
			}

			#endregion

			#region Refs

			/// <inheritdoc/>
			public async Task<BlobRefValue?> TryReadRefAsync(RefName name, RefCacheTime cacheTime, CancellationToken cancellationToken)
			{
				RefInfo? refInfo = await _outer.TryReadRefAsync(NamespaceId, name, cacheTime, cancellationToken);
				if (refInfo == null)
				{
					return null;
				}
				return new BlobRefValue(refInfo.Hash, refInfo.Target);
			}

			/// <inheritdoc/>
			public Task WriteRefAsync(RefName name, BlobRefValue value, RefOptions? options = null, CancellationToken cancellationToken = default)
				=> _outer.WriteRefAsync(NamespaceId, name, value, options, cancellationToken);

			/// <inheritdoc/>
			public Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
				=> _outer.DeleteRefAsync(NamespaceId, name, cancellationToken);

			#endregion

			/// <inheritdoc/>
			public void GetStats(StorageStats stats) => _store.GetStats(stats);
		}

		class State
		{
			public GlobalConfig Config { get; }
			public Dictionary<NamespaceId, NamespaceInfo> Namespaces { get; } = new Dictionary<NamespaceId, NamespaceInfo>();

			public State(GlobalConfig config)
			{
				Config = config;
			}

			public IStorageBackend? TryCreateBackend(NamespaceId namespaceId)
			{
				NamespaceInfo? namespaceInfo;
				if (!Namespaces.TryGetValue(namespaceId, out namespaceInfo))
				{
					return null;
				}
				return namespaceInfo.Backend;
			}
		}

		class NamespaceInfo
		{
			public NamespaceId Id => Config.Id;
			public NamespaceConfig Config { get; }
			public IObjectStore Store { get; }
			public StorageBackendImpl Backend { get; }

			public NamespaceInfo(NamespaceConfig config, IObjectStore store, StorageBackendImpl backend)
			{
				Config = config;
				Store = store;
				Backend = backend;
			}
		}

		class AliasInfo
		{
			[BsonElement("nam")]
			public string Name { get; set; }

			[BsonElement("frg")]
			public string Fragment { get; set; }

			[BsonElement("rnk"), BsonIgnoreIfDefault]
			public int Rank { get; set; }

			[BsonElement("dat"), BsonIgnoreIfNull]
			public byte[]? Data { get; set; }

			[BsonConstructor]
			public AliasInfo()
			{
				Name = String.Empty;
				Fragment = String.Empty;
			}

			public AliasInfo(string name, string fragment, byte[]? data, int rank)
			{
				Name = name;
				Fragment = fragment;
				Rank = rank;
				Data = (data == null || data.Length == 0) ? null : data;
			}
		}

		class BlobInfo
		{
			public ObjectId Id { get; set; }

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; }

			[BsonElement("blob")]
			public string Path { get; set; }

			[BsonElement("imp"), BsonIgnoreIfNull]
			public List<ObjectId>? Imports { get; set; }

			[BsonElement("ali"), BsonIgnoreIfNull]
			public List<AliasInfo>? Aliases { get; set; }

			[BsonIgnore]
			public BlobLocator Locator => new BlobLocator(Path);

			public BlobInfo()
			{
				Path = String.Empty;
			}

			public BlobInfo(ObjectId id, NamespaceId namespaceId, BlobLocator locator)
			{
				Id = id;
				NamespaceId = namespaceId;
				Path = locator.ToString();
			}
		}

		class RefInfo : ISupportInitialize
		{
			[BsonIgnoreIfDefault]
			public ObjectId Id { get; set; }

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; }

			[BsonElement("name")]
			public RefName Name { get; set; }

			[BsonElement("hash")]
			public IoHash Hash { get; set; }

			[BsonElement("tgt")]
			public BlobLocator Target { get; set; }

			[BsonElement("binf")]
			public ObjectId TargetBlobId { get; set; }

			[BsonElement("xa"), BsonIgnoreIfDefault]
			public DateTime? ExpiresAtUtc { get; set; }

			[BsonElement("xt"), BsonIgnoreIfDefault]
			public TimeSpan? Lifetime { get; set; }

#pragma warning disable IDE0051 // Remove unused private members
			[BsonExtraElements]
			BsonDocument? ExtraElements { get; set; }
#pragma warning restore IDE0051 // Remove unused private members

			[BsonConstructor]
			public RefInfo()
			{
				Name = RefName.Empty;
			}

			public RefInfo(NamespaceId namespaceId, RefName name, IoHash hash, BlobLocator target, ObjectId targetBlobId)
			{
				NamespaceId = namespaceId;
				Name = name;
				Hash = hash;
				Target = target;
				TargetBlobId = targetBlobId;
			}

			public bool HasExpired(DateTime utcNow) => ExpiresAtUtc.HasValue && utcNow >= ExpiresAtUtc.Value;

			public bool RequiresTouch(DateTime utcNow) => ExpiresAtUtc.HasValue && Lifetime.HasValue && utcNow >= ExpiresAtUtc.Value - new TimeSpan(Lifetime.Value.Ticks / 4);

			void ISupportInitialize.BeginInit() { }

			void ISupportInitialize.EndInit()
			{
				if (ExtraElements != null)
				{
					if (ExtraElements.TryGetValue("blob", out BsonValue blob) && ExtraElements.TryGetValue("idx", out BsonValue idx))
					{
						Target = new BlobLocator($"{blob.AsString}#{idx.AsInt32}");
					}
					ExtraElements = null;
				}
			}
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

		readonly RedisService _redisService;
		readonly IClock _clock;
		readonly BundleCache _bundleCache;
		readonly IMemoryCache _memoryCache;
		readonly IObjectStoreFactory _objectStoreFactory;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly Tracer _tracer;
		readonly ILogger _logger;

		readonly IMongoCollection<BlobInfo> _blobCollection;
		readonly IMongoCollection<RefInfo> _refCollection;

		readonly ITicker _blobTicker;
		readonly ITicker _refTicker;

		readonly SingletonDocument<GcState> _gcState;
		readonly ITicker _gcTicker;

		readonly object _lockObject = new object();

		string? _lastConfigRevision;
		State? _lastState;

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageService(MongoService mongoService, RedisService redisService, IClock clock, BundleCache bundleCache, IMemoryCache memoryCache, IObjectStoreFactory objectStoreFactory, IOptionsMonitor<GlobalConfig> globalConfig, Tracer tracer, ILogger<StorageService> logger)
		{
			_redisService = redisService;
			_clock = clock;
			_bundleCache = bundleCache;
			_memoryCache = memoryCache;
			_objectStoreFactory = objectStoreFactory;
			_globalConfig = globalConfig;
			_tracer = tracer;
			_logger = logger;

			List<MongoIndex<BlobInfo>> blobIndexes = new List<MongoIndex<BlobInfo>>();
			blobIndexes.Add(keys => keys.Ascending(x => x.Imports));
			blobIndexes.Add(keys => keys.Ascending(x => x.NamespaceId).Ascending(x => x.Path), unique: true);
			blobIndexes.Add(keys => keys.Ascending(x => x.NamespaceId).Ascending($"{nameof(BlobInfo.Aliases)}.{nameof(AliasInfo.Name)}"));
			_blobCollection = mongoService.GetCollection<BlobInfo>("Storage.Blobs", blobIndexes);

			List<MongoIndex<RefInfo>> refIndexes = new List<MongoIndex<RefInfo>>();
			refIndexes.Add(keys => keys.Ascending(x => x.NamespaceId).Ascending(x => x.Name), unique: true);
			refIndexes.Add(keys => keys.Ascending(x => x.TargetBlobId));
			_refCollection = mongoService.GetCollection<RefInfo>("Storage.Refs", refIndexes);

			_blobTicker = clock.AddSharedTicker("Storage:Blobs", TimeSpan.FromMinutes(5.0), TickBlobsAsync, _logger);
			_refTicker = clock.AddSharedTicker("Storage:Refs", TimeSpan.FromMinutes(5.0), TickRefsAsync, _logger);

			_gcState = new SingletonDocument<GcState>(mongoService);
			_gcTicker = clock.AddTicker("Storage:GC", TimeSpan.FromMinutes(5.0), TickGcAsync, logger);
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await _blobTicker.DisposeAsync();
			await _refTicker.DisposeAsync();
			await _gcTicker.DisposeAsync();
		}

		internal static ObjectKey GetObjectKey(BlobLocator locator) => new ObjectKey($"{locator.Path}.blob");

		class StorageClientFactory : IStorageClientFactory
		{
			readonly StorageService _storageService;
			readonly GlobalConfig _globalConfig;

			public StorageClientFactory(StorageService storageService, GlobalConfig globalConfig)
			{
				_storageService = storageService;
				_globalConfig = globalConfig;
			}

			public IStorageClient? TryCreateClient(NamespaceId namespaceId)
				=> _storageService.TryCreateClient(_globalConfig, namespaceId);
		}

		/// <summary>
		/// Creates a new storage client factory using the current global config value
		/// </summary>
		public IStorageClientFactory CreateStorageClientFactory(GlobalConfig globalConfig)
			=> new StorageClientFactory(this, globalConfig);

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

		/// <inheritdoc/>
		public IStorageBackend CreateBackend(NamespaceId namespaceId)
		{
			return TryCreateBackend(namespaceId) ?? throw new StorageException($"Namespace '{namespaceId}' not found");
		}

		/// <inheritdoc/>
		public IStorageBackend? TryCreateBackend(NamespaceId namespaceId)
			=> TryCreateBackend(_globalConfig.CurrentValue, namespaceId);

		/// <inheritdoc/>
		public IStorageBackend? TryCreateBackend(GlobalConfig globalConfig, NamespaceId namespaceId)
		{
			State snapshot = CreateState(globalConfig);
			return snapshot.TryCreateBackend(namespaceId);
		}

		/// <inheritdoc/>
		public IStorageClient? TryCreateClient(NamespaceId namespaceId)
			=> TryCreateClient(namespaceId, null);

		/// <inheritdoc/>
		public IStorageClient? TryCreateClient(NamespaceId namespaceId, BundleOptions? bundleOptions = null)
			=> TryCreateClient(_globalConfig.CurrentValue, namespaceId, bundleOptions);

		/// <inheritdoc/>
		public IStorageClient? TryCreateClient(GlobalConfig globalConfig, NamespaceId namespaceId, BundleOptions? bundleOptions = null)
		{
#pragma warning disable CA2000 // Call dispose on backend; will be disposed by BundleStorageClient
			IStorageBackend? backend = TryCreateBackend(globalConfig, namespaceId);
#pragma warning restore CA2000
			if (backend == null)
			{
				return null;
			}
			else
			{
				return new BundleStorageClient(backend, _bundleCache, bundleOptions, _logger);
			}
		}

		#region Config

		State CreateState(GlobalConfig globalConfig)
		{
			lock (_lockObject)
			{
				if (_lastState == null || !String.Equals(_lastConfigRevision, globalConfig.Revision, StringComparison.Ordinal))
				{
					_logger.LogDebug("Updating storage providers for config {Revision}", globalConfig.Revision);

					State nextState = new State(globalConfig);

					StorageConfig storageConfig = globalConfig.Storage;
					foreach (NamespaceConfig namespaceConfig in storageConfig.Namespaces)
					{
						NamespaceId namespaceId = namespaceConfig.Id;
						try
						{
							IObjectStore objectStore = _objectStoreFactory.CreateObjectStore(namespaceConfig.BackendConfig);

							if (!String.IsNullOrEmpty(namespaceConfig.Prefix))
							{
								objectStore = new PrefixedObjectStore(namespaceConfig.Prefix, objectStore);
							}

							StorageBackendImpl backend = new StorageBackendImpl(this, namespaceConfig, objectStore);

							NamespaceInfo namespaceInfo = new NamespaceInfo(namespaceConfig, objectStore, backend);
							nextState.Namespaces.Add(namespaceId, namespaceInfo);
						}
						catch (Exception ex)
						{
							_logger.LogError(ex, "Unable to create storage backend for {NamespaceId}: ", namespaceId);
						}
					}

					_lastState = nextState;
					_lastConfigRevision = globalConfig.Revision;
				}
				return _lastState;
			}
		}

		#endregion

		#region Blobs

		/// <inheritdoc/>
		async Task AddBlobAsync(NamespaceId namespaceId, BlobLocator locator, List<AliasInfo>? exports = null, CancellationToken cancellationToken = default)
		{
			ObjectId id = ObjectId.GenerateNewId(_clock.UtcNow);
			BlobInfo blobInfo = new BlobInfo(id, namespaceId, locator);
			blobInfo.Aliases = exports;
			await _blobCollection.InsertOneAsync(blobInfo, new InsertOneOptions { }, cancellationToken);
		}

		/// <inheritdoc/>
		async Task<bool> IsBlobReferencedAsync(ObjectId blobInfoId, CancellationToken cancellationToken = default)
		{
			FilterDefinition<BlobInfo> blobFilter = Builders<BlobInfo>.Filter.AnyEq(x => x.Imports, blobInfoId);
			if (await _blobCollection.Find(blobFilter).Limit(1).CountDocumentsAsync(cancellationToken) > 0)
			{
				return true;
			}

			FilterDefinition<RefInfo> refFilter = Builders<RefInfo>.Filter.Eq(x => x.TargetBlobId, blobInfoId);
			if (await _refCollection.Find(refFilter).Limit(1).CountDocumentsAsync(cancellationToken) > 0)
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
			GcState gcState = await _gcState.GetAsync(cancellationToken);
			DateTime utcNow = _clock.UtcNow;

			// Get the current state of the storage system
			State state = CreateState(_globalConfig.CurrentValue);

			Dictionary<NamespaceId, BundleStorageClient> cachedClients = new();
			try
			{
				// Compute missing import info, by searching for blobs with an ObjectId timestamp after the last import compute cycle
				ObjectId latestInfoId = ObjectId.GenerateNewId(utcNow - TimeSpan.FromMinutes(30.0));
				using (IAsyncCursor<BlobInfo> cursor = await _blobCollection.Find(x => x.Id >= gcState.LastImportBlobInfoId && x.Id < latestInfoId).ToCursorAsync(cancellationToken))
				{
					while (await cursor.MoveNextAsync(cancellationToken))
					{
						// Find imports, and add a check record for each new blob
						foreach (BlobInfo blobInfo in cursor.Current)
						{
							NamespaceInfo? namespaceInfo;
							if (state.Namespaces.TryGetValue(blobInfo.NamespaceId, out namespaceInfo))
							{
								BundleStorageClient? storageClient;
								if (!cachedClients.TryGetValue(namespaceInfo.Id, out storageClient))
								{
									storageClient = new BundleStorageClient(namespaceInfo.Backend, _bundleCache, null, _logger);
									cachedClients.Add(namespaceInfo.Id, storageClient);
								}

								try
								{
									await TickBlobAsync(storageClient, blobInfo, cancellationToken);
								}
								catch (ObjectNotFoundException ex)
								{
									_logger.LogInformation(ex, "Unable to read references for {NamespaceId} blob {BlobId}: {Message}", blobInfo.NamespaceId, blobInfo.Id, ex.Message);
								}
								catch (Exception ex)
								{
									_logger.LogWarning(ex, "Unable to read references for {NamespaceId} blob {BlobId} (key: {ObjectKey}): {Message}", blobInfo.NamespaceId, blobInfo.Id, GetObjectKey(blobInfo.Locator), ex.Message);
								}
							}
						}

						// Update the last imported blob id
						await _gcState.UpdateAsync(state => state.LastImportBlobInfoId = latestInfoId, cancellationToken);
					}
				}
			}
			finally
			{
				foreach (BundleStorageClient client in cachedClients.Values)
				{
					client.Dispose();
				}
			}
		}

		async Task TickBlobAsync(BundleStorageClient storageClient, BlobInfo blobInfo, CancellationToken cancellationToken)
		{
			List<ObjectId> importInfoIds = new List<ObjectId>();

			IEnumerable<BlobLocator> importLocators = await storageClient.ReadBundleReferencesAsync(blobInfo.Locator, cancellationToken);
			foreach (BlobLocator importLocator in importLocators)
			{
				string importPath = importLocator.BaseLocator.ToString();

				FilterDefinition<BlobInfo> filter = Builders<BlobInfo>.Filter.Expr(x => x.NamespaceId == blobInfo.NamespaceId && x.Path == importPath);
				UpdateDefinition<BlobInfo> update = Builders<BlobInfo>.Update.SetOnInsert(x => x.Imports, null);
				BlobInfo blobInfoDoc = await _blobCollection.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<BlobInfo> { IsUpsert = true, ReturnDocument = ReturnDocument.After }, cancellationToken);

				importInfoIds.Add(blobInfoDoc.Id);
			}
			await _blobCollection.UpdateOneAsync(x => x.Id == blobInfo.Id, Builders<BlobInfo>.Update.Set(x => x.Imports, importInfoIds), null, cancellationToken);

			AddGcCheckRecord(blobInfo.NamespaceId, blobInfo.Id);
			_logger.LogDebug("Added {Count} imports for {NamespaceId} blob {BlobId}", importInfoIds.Count, blobInfo.NamespaceId, blobInfo.Id);
		}

		#endregion

		#region Nodes

		/// <summary>
		/// Adds a node alias
		/// </summary>
		/// <param name="namespaceId">Namespace to search</param>
		/// <param name="name">Alias for the node</param>
		/// <param name="target">Target node for the alias</param>
		/// <param name="rank">Rank for the alias. Higher ranked aliases are preferred by default.</param>
		/// <param name="data">Inline data to store with this alias</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of thandles</returns>
		async Task AddAliasAsync(NamespaceId namespaceId, string name, BlobLocator target, int rank, ReadOnlyMemory<byte> data, CancellationToken cancellationToken = default)
		{
			string blobPath = target.BaseLocator.ToString();
			string blobFragment = target.Fragment.ToString();

			BlobInfo? blobInfo = await _blobCollection.Find(x => x.NamespaceId == namespaceId && x.Path == blobPath).FirstOrDefaultAsync(cancellationToken);
			if (blobInfo == null)
			{
				throw new KeyNotFoundException($"Missing blob {blobPath}");
			}
			if (blobInfo.Aliases != null && blobInfo.Aliases.Any(x => x.Name.Equals(name, StringComparison.Ordinal) && x.Fragment.Equals(blobFragment, StringComparison.Ordinal)))
			{
				return;
			}

			FilterDefinition<BlobInfo> filter = Builders<BlobInfo>.Filter.Expr(x => x.NamespaceId == blobInfo.NamespaceId && x.Path == blobPath);
			UpdateDefinition<BlobInfo> update = Builders<BlobInfo>.Update.Push(x => x.Aliases, new AliasInfo(name, blobFragment, data.ToArray(), rank));
			await _blobCollection.UpdateOneAsync(filter, update, cancellationToken: cancellationToken);
		}

		/// <summary>
		/// Removes a node alias
		/// </summary>
		/// <param name="namespaceId">Namespace to search</param>
		/// <param name="name">Alias for the node</param>
		/// <param name="target">Target node for the alias</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of thandles</returns>
		async Task RemoveAliasAsync(NamespaceId namespaceId, string name, BlobLocator target, CancellationToken cancellationToken = default)
		{
			string blobPath = target.BaseLocator.ToString();
			string blobFragment = target.Fragment.ToString();

			FilterDefinition<BlobInfo> filter = Builders<BlobInfo>.Filter.Expr(x => x.NamespaceId == namespaceId && x.Path == blobPath);
			UpdateDefinition<BlobInfo> update = Builders<BlobInfo>.Update.PullFilter(x => x.Aliases, Builders<AliasInfo>.Filter.Expr(x => x.Name == name && x.Fragment == blobFragment));
			await _blobCollection.UpdateOneAsync(filter, update, cancellationToken: cancellationToken);
		}

		/// <summary>
		/// Finds nodes with the given type and hash
		/// </summary>
		/// <param name="namespaceId">Namespace to search</param>
		/// <param name="name">Alias for the node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of thandles</returns>
		async Task<List<(BlobLocator, AliasInfo)>> FindAliasesAsync(NamespaceId namespaceId, string name, CancellationToken cancellationToken = default)
		{
			List<(BlobLocator, AliasInfo)> results = new List<(BlobLocator, AliasInfo)>();
			await foreach (BlobInfo blobInfo in _blobCollection.Find(x => x.NamespaceId == namespaceId && x.Aliases!.Any(y => y.Name == name)).ToAsyncEnumerable(cancellationToken))
			{
				if (blobInfo.Aliases != null)
				{
					foreach (AliasInfo aliasInfo in blobInfo.Aliases)
					{
						if (String.Equals(aliasInfo.Name, name, StringComparison.Ordinal))
						{
							BlobLocator locator = new BlobLocator(blobInfo.Locator, aliasInfo.Fragment);
							results.Add((locator, aliasInfo));
						}
					}
				}
			}
			return results.OrderByDescending(x => x.Item2.Rank).ToList();
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
			using (ICacheEntry newEntry = _memoryCache.CreateEntry(new RefCacheKey(namespaceId, name)))
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
						AddGcCheckRecord(refInfo.NamespaceId, refInfo.TargetBlobId);
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
		async Task<bool> DeleteRefAsync(NamespaceId namespaceId, RefName name, CancellationToken cancellationToken = default)
		{
			FilterDefinition<RefInfo> filter = Builders<RefInfo>.Filter.Expr(x => x.NamespaceId == namespaceId && x.Name == name);
			return await DeleteRefInternalAsync(namespaceId, name, filter, cancellationToken);
		}

		/// <summary>
		/// Deletes a ref document that has reached its expiry time
		/// </summary>
		async Task DeleteExpiredRefAsync(RefInfo refDocument, CancellationToken cancellationToken = default)
		{
			FilterDefinition<RefInfo> filter = Builders<RefInfo>.Filter.Expr(x => x.Id == refDocument.Id && x.ExpiresAtUtc == refDocument.ExpiresAtUtc);
			await DeleteRefInternalAsync(refDocument.NamespaceId, refDocument.Name, filter, cancellationToken);
		}

		async Task<bool> DeleteRefInternalAsync(NamespaceId namespaceId, RefName name, FilterDefinition<RefInfo> filter, CancellationToken cancellationToken = default)
		{
			RefInfo? oldRefInfo = await _refCollection.FindOneAndDeleteAsync<RefInfo>(filter, cancellationToken: cancellationToken);
			AddRefToCache(namespaceId, name, default);

			if (oldRefInfo != null)
			{
				_logger.LogInformation("Deleted ref {NamespaceId}:{RefName}", namespaceId, name);
				AddGcCheckRecord(namespaceId, oldRefInfo.TargetBlobId);
				return true;
			}

			return false;
		}

		/// <inheritdoc/>
		async Task<RefInfo?> TryReadRefAsync(NamespaceId namespaceId, RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			RefCacheValue? entry;
			if (!_memoryCache.TryGetValue(name, out entry) || entry == null || RefCacheTime.IsStaleCacheEntry(entry.Time, cacheTime))
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

			return entry.Value;
		}

		/// <inheritdoc/>
		async Task WriteRefAsync(NamespaceId namespaceId, RefName name, BlobRefValue value, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			string path = value.Locator.BaseLocator.ToString();

			BlobInfo? newBlobInfo = await _blobCollection.Find(x => x.NamespaceId == namespaceId && x.Path == path).FirstOrDefaultAsync(cancellationToken);
			if (newBlobInfo == null)
			{
				throw new Exception($"Invalid/unknown blob identifier '{path}' in namespace {namespaceId}");
			}

			RefInfo newRefInfo = new RefInfo(namespaceId, name, value.Hash, value.Locator, newBlobInfo.Id);

			if (options != null && options.Lifetime.HasValue)
			{
				newRefInfo.ExpiresAtUtc = _clock.UtcNow + options.Lifetime.Value;
				if (options.Extend ?? true)
				{
					newRefInfo.Lifetime = options.Lifetime;
				}
			}

			RefInfo? oldRefInfo = await _refCollection.FindOneAndReplaceAsync<RefInfo>(x => x.NamespaceId == namespaceId && x.Name == name, newRefInfo, new FindOneAndReplaceOptions<RefInfo> { IsUpsert = true }, cancellationToken);
			if (oldRefInfo != null)
			{
				AddGcCheckRecord(namespaceId, oldRefInfo.TargetBlobId);
			}

			_logger.LogInformation("Updated ref {NamespaceId}:{RefName}", namespaceId, name);
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
				GlobalConfig globalConfig = _globalConfig.CurrentValue;

				State state = CreateState(globalConfig);

				StorageConfig storageConfig = state.Config.Storage;
				if (!storageConfig.EnableGC)
				{
					break;
				}

				// Synchronize the list of configured namespaces with the GC state object
				GcState gcState = await _gcState.GetAsync(cancellationToken);
				if (!Enumerable.SequenceEqual(storageConfig.Namespaces.Select(x => x.Id.Text.Text).OrderBy(x => x), gcState.Namespaces.Select(x => x.Id.Text.Text).OrderBy(x => x)))
				{
					gcState = await _gcState.UpdateAsync(s => SyncNamespaceList(s, storageConfig.Namespaces), cancellationToken);
				}

				// Find all the namespaces that need to have GC run on them
				List<(DateTime, GcNamespaceState)> pending = new List<(DateTime, GcNamespaceState)>();
				foreach (GcNamespaceState namespaceState in gcState.Namespaces)
				{
					if (!ranNamespaceIds.Contains(namespaceState.Id))
					{
						NamespaceConfig? namespaceConfig;
						if (storageConfig.TryGetNamespace(namespaceState.Id, out namespaceConfig))
						{
							DateTime time = namespaceState.LastTime + TimeSpan.FromHours(namespaceConfig.GcFrequencyHrs);
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
#pragma warning disable CA2000 // Dispose objects before losing scope
						using (RedisLock namespaceLock = new RedisLock(_redisService.GetDatabase(), key))
						{
							if (await namespaceLock.AcquireAsync(TimeSpan.FromMinutes(20.0)))
							{
								try
								{
									await TickGcForNamespaceAsync(state.Namespaces[namespaceId], gcState.LastImportBlobInfoId, utcNow, cancellationToken);
								}
								catch (Exception ex)
								{
									_logger.LogError(ex, "Exception while running garbage collection: {Message}", ex.Message);
								}
								break;
							}
						}
#pragma warning restore CA2000 // Dispose objects before losing scope
					}
				}
			}
		}

		async Task TickGcForNamespaceAsync(NamespaceInfo namespaceInfo, ObjectId lastImportBlobInfoId, DateTime utcNow, CancellationToken cancellationToken)
		{
			using IStorageClient client = this.CreateClient(namespaceInfo.Id);

			Stopwatch timer = Stopwatch.StartNew();
			_logger.LogInformation("Running garbage collection for namespace {NamespaceId}...", namespaceInfo.Id);
			int numItemsRemoved = 0;

			double score = GetGcTimestamp(utcNow);

			RedisSortedSetKey<RedisValue> checkSet = GetGcCheckSet(namespaceInfo.Id);
			while (_globalConfig.CurrentValue.Storage.EnableGC)
			{
				long length = await _redisService.GetDatabase().SortedSetLengthAsync(checkSet);
				int batchSize = (int)Math.Min(length, 1024);
				_logger.LogInformation("Garbage collection queue for namespace {NamespaceId} ({QueueName}) has {Length} entries; taking {Count}", namespaceInfo.Id, checkSet.Inner, length, batchSize);

				if (length == 0)
				{
					await _gcState.UpdateAsync(state => state.FindOrAddNamespace(namespaceInfo.Id).LastTime = utcNow, cancellationToken);
					_logger.LogInformation("Finished garbage collection for namespace {NamespaceId} in {TimeSecs}s ({NumItems} removed)", namespaceInfo.Id, timer.Elapsed.TotalSeconds, numItemsRemoved);
					break;
				}

				RedisValue[] values = await _redisService.GetDatabase().SortedSetRangeByRankAsync(checkSet, 0, batchSize);
				foreach (RedisValue value in values)
				{
					ObjectId blobInfoId = new ObjectId(((byte[]?)value)!);

					using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(StorageService)}.{nameof(TickGcForNamespaceAsync)}");
					span.SetAttribute("BlobId", blobInfoId.ToString());

					if (blobInfoId < lastImportBlobInfoId && !await IsBlobReferencedAsync(blobInfoId, cancellationToken))
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

							ObjectKey objectKey = GetObjectKey(new BlobLocator(info.Path));
							_logger.LogDebug("Deleting {NamespaceId} blob {BlobId}, key: {ObjectKey} ({ImportCount} imports)", namespaceInfo.Id, blobInfoId, objectKey, info.Imports?.Count ?? 0);
							await namespaceInfo.Store.DeleteAsync(objectKey, cancellationToken);
							numItemsRemoved++;
						}
					}
					_ = _redisService.GetDatabase().SortedSetRemoveAsync(checkSet, value, CommandFlags.FireAndForget);
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

			state.Namespaces.SortBy(x => x.Id.Text.Text);
		}

		void AddGcCheckRecord(NamespaceId namespaceId, ObjectId id)
		{
			double score = GetGcTimestamp();
			_ = _redisService.GetDatabase().SortedSetAddAsync(GetGcCheckSet(namespaceId), id.ToByteArray(), score, flags: CommandFlags.FireAndForget);
		}

		#endregion
	}
}
