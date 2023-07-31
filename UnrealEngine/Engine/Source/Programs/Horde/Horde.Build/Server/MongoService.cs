// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Security;
using System.Runtime.InteropServices;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Redis.Utility;
using Horde.Build.Secrets;
using Horde.Build.Utilities;
using Horde.Build.Utiltiies;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.Tokens;
using MongoDB.Bson;
using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using MongoDB.Driver.Core.Events;
using StackExchange.Redis;

namespace Horde.Build.Server
{
	/// <summary>
	/// Constrained set of parameters for building a mongo index
	/// </summary>
	[DebuggerDisplay("{Name}")]
	public class MongoIndex : IEquatable<MongoIndex>
	{
		/// <summary>
		/// Name of the index
		/// </summary>
		[BsonElement("name"), BsonRequired]
		public string Name { get; private set; }

		/// <summary>
		/// Keys for the index
		/// </summary>
		[BsonElement("key"), BsonRequired]
		public BsonDocument KeysDocument { get; private set; }

		/// <summary>
		/// The index should be unique
		/// </summary>
		[BsonElement("unique")]
		public bool Unique { get; private set; }

		/// <summary>
		/// Constructor
		/// </summary>
		[BsonConstructor]
		private MongoIndex()
		{
			Name = String.Empty;
			KeysDocument = new BsonDocument();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="keysDocument">Keys for the index</param>
		/// <param name="unique">Whether the index should be unique</param>
		public MongoIndex(BsonDocument keysDocument, bool unique)
			: this(GetDefaultName(keysDocument), keysDocument, unique)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name of the index</param>
		/// <param name="keysDocument">Keys for the index</param>
		/// <param name="unique">Whether the index should be unique</param>
		public MongoIndex(string name, BsonDocument keysDocument, bool unique)
		{
			Name = name;
			KeysDocument = keysDocument;
			Unique = unique;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="keysFunc">Callback to configure keys for this document</param>
		/// <param name="unique">Whether the index should be unique</param>
		public static MongoIndex<T> Create<T>(Func<IndexKeysDefinitionBuilder<T>, IndexKeysDefinition<T>> keysFunc, bool unique = false)
		{
			return new MongoIndex<T>(keysFunc(Builders<T>.IndexKeys), unique);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name of the index</param>
		/// <param name="keysFunc">Callback to configure keys for this document</param>
		/// <param name="unique">Whether the index should be unique</param>
		public static MongoIndex<T> Create<T>(string name, Func<IndexKeysDefinitionBuilder<T>, IndexKeysDefinition<T>> keysFunc, bool unique = false)
		{
			return new MongoIndex<T>(name, keysFunc(Builders<T>.IndexKeys), unique);
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is MongoIndex other && Equals(other);

		/// <inheritdoc/>
		public override int GetHashCode() => Name.GetHashCode(StringComparison.Ordinal);

		/// <inheritdoc/>
		public bool Equals(MongoIndex? other) => other != null && Name.Equals(other.Name, StringComparison.Ordinal) && KeysDocument.Equals(other.KeysDocument) && Unique == other.Unique;

		/// <summary>
		/// Gets the default name for an index based on its keys
		/// </summary>
		/// <param name="keys">Keys for the index</param>
		/// <returns>Name of the index</returns>
		protected static string GetDefaultName(BsonDocument keys)
		{
			StringBuilder name = new StringBuilder();
			foreach (BsonElement element in keys.Elements)
			{
				if (name.Length > 0)
				{
					name.Append('_');
				}
				name.Append(element.Name);
				name.Append('_');
				name.Append(element.Value.ToString());
			}
			return name.ToString();
		}
	}

	/// <summary>
	/// Strongly typed index document
	/// </summary>
	/// <typeparam name="T"></typeparam>
	public class MongoIndex<T> : MongoIndex
	{
		/// <summary>
		/// Keys for the index
		/// </summary>
		public IndexKeysDefinition<T> Keys { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="keys">Keys for the index</param>
		/// <param name="unique">Whether the index should be unique</param>
		public MongoIndex(IndexKeysDefinition<T> keys, bool unique)
			: base(keys.Render(BsonSerializer.LookupSerializer<T>(), BsonSerializer.SerializerRegistry), unique)
		{
			Keys = keys;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name of the index</param>
		/// <param name="keys">Keys for the index</param>
		/// <param name="unique">Whether the index should be unique</param>
		public MongoIndex(string name, IndexKeysDefinition<T> keys, bool unique)
			: base(name, keys.Render(BsonSerializer.LookupSerializer<T>(), BsonSerializer.SerializerRegistry), unique)
		{
			Keys = keys;
		}
	}

	/// <summary>
	/// Extension methods for <see cref="MongoIndex{T}"/>
	/// </summary>
	static class MongoIndexExtensions
	{
		public static void Add<T>(this List<MongoIndex<T>> list, Func<IndexKeysDefinitionBuilder<T>, IndexKeysDefinition<T>> keyFunc, bool unique = false)
		{
			list.Add(MongoIndex.Create(keyFunc, unique));
		}

		public static void Add<T>(this List<MongoIndex<T>> list, string name, Func<IndexKeysDefinitionBuilder<T>, IndexKeysDefinition<T>> keyFunc, bool unique = false)
		{
			list.Add(MongoIndex.Create(name, keyFunc, unique));
		}
	}

	/// <summary>
	/// Singleton for accessing the database
	/// </summary>
	public sealed class MongoService : IDisposable
	{
		/// <summary>
		/// The database instance
		/// </summary>
		public IMongoDatabase Database { get; private set; }

		/// <summary>
		/// Collection of credential documents
		/// </summary>
		public IMongoCollection<Credential> Credentials { get; }

		/// <summary>
		/// Collection of singleton documents
		/// </summary>
		public IMongoCollection<BsonDocument> Singletons { get; }

		/// <summary>
		/// Settings for the application
		/// </summary>
		ServerSettings Settings { get; }

		/// <summary>
		/// Name of the JWT issuer
		/// </summary>
		public string JwtIssuer { get; }

		/// <summary>
		/// The signing key for this instance
		/// </summary>
		public SymmetricSecurityKey JwtSigningKey { get; }

		/// <summary>
		/// Logger for this instance
		/// </summary>
		readonly ILogger<MongoService> _logger;

		/// <summary>
		/// Access the database in a read-only mode (don't create indices or modify content)
		/// </summary>
		public bool ReadOnlyMode { get; }

		/// <summary>
		/// The mongo process group
		/// </summary>
		ManagedProcessGroup? _mongoProcessGroup;

		/// <summary>
		/// The mongo process
		/// </summary>
		ManagedProcess? _mongoProcess;

		/// <summary>
		/// Task to read from the mongo process
		/// </summary>
		Task? _mongoOutputTask;

		/// <summary>
		/// Factory for creating logger instances
		/// </summary>
		readonly ILoggerFactory _loggerFactory;

		/// <summary>
		/// Default port for MongoDB connections
		/// </summary>
		const int DefaultMongoPort = 27017;

		readonly HashSet<string> _collectionNames = new HashSet<string>(StringComparer.Ordinal);
		readonly Channel<Func<CancellationToken, Task>> _updateIndexesChannel = Channel.CreateUnbounded<Func<CancellationToken, Task>>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="settingsSnapshot">The settings instance</param>
		/// <param name="loggerFactory">Instance of the logger for this service</param>
		public MongoService(IOptions<ServerSettings> settingsSnapshot, ILoggerFactory loggerFactory)
		{
			Settings = settingsSnapshot.Value;
			_logger = loggerFactory.CreateLogger<MongoService>();
			_loggerFactory = loggerFactory;

			try
			{
				ReadOnlyMode = Settings.DatabaseReadOnlyMode;
				if (Settings.DatabasePublicCert != null)
				{
					X509Store localTrustStore = new X509Store(StoreName.Root);
					try
					{
						localTrustStore.Open(OpenFlags.ReadWrite);

						X509Certificate2Collection collection = ImportCertificateBundle(Settings.DatabasePublicCert);
						foreach (X509Certificate2 certificate in collection)
						{
							_logger.LogInformation("Importing certificate for {Subject}", certificate.Subject);
						}

						localTrustStore.AddRange(collection);
					}
					finally
					{
						localTrustStore.Close();
					}
				}

				string? connectionString = Settings.DatabaseConnectionString;
				if (connectionString == null)
				{
					if (IsPortInUse(DefaultMongoPort))
					{
						connectionString = "mongodb://localhost:27017";
					}
					else if (TryStartMongoServer(_logger))
					{
						connectionString = "mongodb://localhost:27017/?readPreference=primary&appname=Horde&ssl=false";
					}
					else
					{
						throw new Exception($"Unable to connect to MongoDB server. Setup a MongoDB server and set the connection string in {Program.UserConfigFile}");
					}
				}

				MongoClientSettings mongoSettings = MongoClientSettings.FromConnectionString(connectionString);
				mongoSettings.ClusterConfigurator = clusterBuilder =>
				{
					if (_logger.IsEnabled(LogLevel.Trace))
					{
						clusterBuilder.Subscribe<CommandStartedEvent>(ev => TraceMongoCommand(ev.Command));
					}
				};

				mongoSettings.SslSettings = new SslSettings();
				mongoSettings.SslSettings.ServerCertificateValidationCallback = CertificateValidationCallBack;
				mongoSettings.MaxConnectionPoolSize = 300; // Default is 100

				//TestSslConnection(MongoSettings.Server.Host, MongoSettings.Server.Port, Logger);

				MongoClient client = new MongoClient(mongoSettings);
				Database = client.GetDatabase(Settings.DatabaseName);

				Singletons = GetCollection<BsonDocument>("Singletons");

				Globals globals = GetGlobalsAsync().Result;
				while (globals.JwtSigningKey == null)
				{
					globals.RotateSigningKey();
					if (!TryUpdateSingletonAsync(globals).Result)
					{
						globals = GetGlobalsAsync().Result;
					}
				}

				JwtIssuer = Settings.JwtIssuer ?? Dns.GetHostName();
				if (String.IsNullOrEmpty(Settings.JwtSecret))
				{
					JwtSigningKey = new SymmetricSecurityKey(globals.JwtSigningKey);
				}
				else
				{
					JwtSigningKey = new SymmetricSecurityKey(Convert.FromBase64String(Settings.JwtSecret));
				}

				Credentials = GetCollection<Credential>("Credentials");
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Exception while initializing MongoService");
				throw;
			}
		}

		internal const int CtrlCEvent = 0;

		[DllImport("kernel32.dll")]
		internal static extern bool GenerateConsoleCtrlEvent(int eventId, int processGroupId);

		/// <inheritdoc/>
		public void Dispose()
		{
			if (_mongoProcess != null)
			{
				GenerateConsoleCtrlEvent(CtrlCEvent, _mongoProcess.Id);

				_mongoOutputTask?.Wait();
				_mongoOutputTask = null;

				_mongoProcess.WaitForExit();
				_mongoProcess.Dispose();
				_mongoProcess = null;
			}
			if (_mongoProcessGroup != null)
			{
				_mongoProcessGroup.Dispose();
				_mongoProcessGroup = null;
			}
		}

		/// <summary>
		/// Checks if the given port is in use
		/// </summary>
		/// <param name="portNumber"></param>
		/// <returns></returns>
		static bool IsPortInUse(int portNumber)
		{
			IPGlobalProperties ipGlobalProperties = IPGlobalProperties.GetIPGlobalProperties();

			IPEndPoint[] listeners = ipGlobalProperties.GetActiveTcpListeners();
			if (listeners.Any(x => x.Port == portNumber))
			{
				return true;
			}

			return false;
		}

		/// <summary>
		/// Attempts to start a local instance of MongoDB
		/// </summary>
		/// <param name="logger"></param>
		/// <returns></returns>
		bool TryStartMongoServer(ILogger logger)
		{
			if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				return false;
			}

			FileReference mongoExe = FileReference.Combine(Program.AppDir, "ThirdParty", "Mongo", "mongod.exe");
			if (!FileReference.Exists(mongoExe))
			{
				logger.LogWarning("Unable to find Mongo executable.");
				return false;
			}

			DirectoryReference mongoDir = DirectoryReference.Combine(Program.DataDir, "Mongo");

			DirectoryReference mongoDataDir = DirectoryReference.Combine(mongoDir, "Data");
			DirectoryReference.CreateDirectory(mongoDataDir);

			FileReference mongoLogFile = FileReference.Combine(mongoDir, "mongod.log");

			FileReference configFile = FileReference.Combine(mongoDir, "mongod.conf");
			if (!FileReference.Exists(configFile))
			{
				DirectoryReference.CreateDirectory(configFile.Directory);
				using (StreamWriter writer = new StreamWriter(configFile.FullName))
				{
					writer.WriteLine("# mongod.conf");
					writer.WriteLine();
					writer.WriteLine("# for documentation of all options, see:");
					writer.WriteLine("# http://docs.mongodb.org/manual/reference/configuration-options/");
					writer.WriteLine();
					writer.WriteLine("storage:");
					writer.WriteLine("    dbPath: {0}", mongoDataDir.FullName);
					writer.WriteLine();
					writer.WriteLine("net:");
					writer.WriteLine("    port: {0}", DefaultMongoPort);
					writer.WriteLine("    bindIp: 127.0.0.1");
				}
			}

			_mongoProcessGroup = new ManagedProcessGroup();
			try
			{
				_mongoProcess = new ManagedProcess(_mongoProcessGroup, mongoExe.FullName, $"--config \"{configFile}\"", null, null, ProcessPriorityClass.Normal);
				_mongoProcess.StdIn.Close();
				_mongoOutputTask = Task.Run(() => RelayMongoOutput());
				return true;
			}
			catch (Exception ex)
			{
				logger.LogWarning(ex, "Unable to start Mongo server process");
				return false;
			}
		}

		/// <summary>
		/// Copies output from the mongo process to the logger
		/// </summary>
		/// <returns></returns>
		async Task RelayMongoOutput()
		{
			ILogger mongoLogger = _loggerFactory.CreateLogger("MongoDB");

			Dictionary<string, ILogger> channelLoggers = new Dictionary<string, ILogger>();
			for (; ; )
			{
				string? line = await _mongoProcess!.ReadLineAsync();
				if (line == null)
				{
					break;
				}
				if (line.Length > 0)
				{
					Match match = Regex.Match(line, @"^\s*[^\s]+\s+([A-Z])\s+([^\s]+)\s+(.*)");
					if (match.Success)
					{
						ILogger? channelLogger;
						if (!channelLoggers.TryGetValue(match.Groups[2].Value, out channelLogger))
						{
							channelLogger = _loggerFactory.CreateLogger($"MongoDB.{match.Groups[2].Value}");
							channelLoggers.Add(match.Groups[2].Value, channelLogger);
						}
						channelLogger.Log(ParseMongoLogLevel(match.Groups[1].Value), "{Message}", match.Groups[3].Value.TrimEnd());
					}
					else
					{
						mongoLogger.Log(LogLevel.Information, "{Message}", line);
					}
				}
			}
			mongoLogger.LogInformation("Exit code {ExitCode}", _mongoProcess.ExitCode);
		}

		static LogLevel ParseMongoLogLevel(string text)
		{
			if (text.Equals("I", StringComparison.Ordinal))
			{
				return LogLevel.Information;
			}
			else if (text.Equals("E", StringComparison.Ordinal))
			{
				return LogLevel.Error;
			}
			else
			{
				return LogLevel.Warning;
			}
		}

		/// <summary>
		/// Logs a mongodb command, removing any fields we don't care about
		/// </summary>
		/// <param name="command">The command document</param>
		void TraceMongoCommand(BsonDocument command)
		{
			List<string> names = new List<string>();
			List<string> values = new List<string>();

			foreach (BsonElement element in command)
			{
				if (element.Value != null && !element.Name.Equals("$db", StringComparison.Ordinal) && !element.Name.Equals("lsid", StringComparison.Ordinal))
				{
					names.Add($"{element.Name}: {{{values.Count}}}");
					values.Add(element.Value.ToString()!);
				}
			}

#pragma warning disable CA2254 // Template should be a static expression
			_logger.LogTrace($"MongoDB: {String.Join(", ", names)}", values.ToArray());
#pragma warning restore CA2254 // Template should be a static expression
		}

		/// <summary>
		/// Imports one or more certificates from a single PEM file
		/// </summary>
		/// <param name="fileName">File to import</param>
		/// <returns>Collection of certificates</returns>
		static X509Certificate2Collection ImportCertificateBundle(string fileName)
		{
			X509Certificate2Collection collection = new X509Certificate2Collection();

			string text = File.ReadAllText(fileName);
			for (int offset = 0; offset < text.Length;)
			{
				int nextOffset = text.IndexOf("-----BEGIN CERTIFICATE-----", offset + 1, StringComparison.Ordinal);
				if (nextOffset == -1)
				{
					nextOffset = text.Length;
				}

				string certificateText = text.Substring(offset, nextOffset - offset);
				collection.Add(new X509Certificate2(Encoding.UTF8.GetBytes(certificateText)));

				offset = nextOffset;
			}

			return collection;
		}

		/// <summary>
		/// Provides additional diagnostic information for SSL certificate validation
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="certificate"></param>
		/// <param name="chain"></param>
		/// <param name="sslPolicyErrors"></param>
		/// <returns>True if the certificate is allowed, false otherwise</returns>
		bool CertificateValidationCallBack(object sender, X509Certificate? certificate, X509Chain? chain, SslPolicyErrors sslPolicyErrors)
		{
			// If the certificate is a valid, signed certificate, return true.
			if (sslPolicyErrors == SslPolicyErrors.None)
			{
				return true;
			}

			// Generate diagnostic information
			StringBuilder builder = new StringBuilder();
			if (sender != null)
			{
				string senderInfo = StringUtils.Indent(sender.ToString() ?? String.Empty, "    ");
				builder.Append(CultureInfo.InvariantCulture, $"\nSender:\n{senderInfo}");
			}
			if (certificate != null)
			{
				builder.Append(CultureInfo.InvariantCulture, $"\nCertificate: {certificate.Subject}");
			}
			if (chain != null)
			{
				if (chain.ChainStatus != null && chain.ChainStatus.Length > 0)
				{
					builder.Append("\nChain status:");
					foreach (X509ChainStatus status in chain.ChainStatus)
					{
						builder.Append(CultureInfo.InvariantCulture, $"\n  {status.StatusInformation}");
					}
				}
				if (chain.ChainElements != null)
				{
					builder.Append("\nChain elements:");
					for (int idx = 0; idx < chain.ChainElements.Count; idx++)
					{
						X509ChainElement element = chain.ChainElements[idx];
						builder.Append(CultureInfo.InvariantCulture, $"\n  {idx,4} - Certificate: {element.Certificate.Subject}");
						if (element.ChainElementStatus != null && element.ChainElementStatus.Length > 0)
						{
							foreach (X509ChainStatus status in element.ChainElementStatus)
							{
								builder.Append(CultureInfo.InvariantCulture, $"\n         Status: {status.StatusInformation} ({status.Status})");
							}
						}
						if (!String.IsNullOrEmpty(element.Information))
						{
							builder.Append(CultureInfo.InvariantCulture, $"\n         Info: {element.Information}");
						}
					}
				}
			}

			// Print out additional diagnostic information
			_logger.LogError("TLS certificate validation failed ({Errors}).{AdditionalInfo}", sslPolicyErrors, StringUtils.Indent(builder.ToString(), "    "));
			return false;
		}

		/// <summary>
		/// Get a MongoDB collection from database
		/// </summary>
		/// <param name="name">Name of collection</param>
		/// <typeparam name="T">A MongoDB document</typeparam>
		/// <returns></returns>
		public IMongoCollection<T> GetCollection<T>(string name)
		{
			return new MongoTracingCollection<T>(Database.GetCollection<T>(name));
		}

		/// <summary>
		/// Get a MongoDB collection from database with a single index
		/// </summary>
		/// <param name="name">Name of collection</param>
		/// <param name="keysFunc">Method to configure keys for the collection</param>
		/// <param name="unique">Whether a unique index is required</param>
		/// <typeparam name="T">A MongoDB document</typeparam>
		/// <returns></returns>
		public IMongoCollection<T> GetCollection<T>(string name, Func<IndexKeysDefinitionBuilder<T>, IndexKeysDefinition<T>> keysFunc, bool unique = false)
		{
			List<MongoIndex<T>> indexes = new List<MongoIndex<T>>();
			indexes.Add(MongoIndex.Create<T>(keysFunc, unique));
			return GetCollection(name, indexes);
		}

		/// <summary>
		/// Get a MongoDB collection from database
		/// </summary>
		/// <param name="name">Name of collection</param>
		/// <param name="indexes">Indexes for the collection</param>
		/// <typeparam name="T">A MongoDB document</typeparam>
		/// <returns></returns>
		public IMongoCollection<T> GetCollection<T>(string name, IEnumerable<MongoIndex<T>> indexes)
		{
			IMongoCollection<T> collection = GetCollection<T>(name);
			lock (_collectionNames)
			{
				if (!_collectionNames.Add(name))
				{
					throw new NotImplementedException();
				}

				MongoIndex<T>[] indexesCopy = indexes.ToArray();
				_updateIndexesChannel.Writer.TryWrite(ctx => UpdateIndexesAsync(name, collection, indexesCopy, ctx));
			}
			return collection;
		}

		/// <summary>
		/// Pops an upgrade task from the queue. This method should only be called by <see cref="MongoUpgradeService"/>
		/// </summary>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public ValueTask<Func<CancellationToken, Task>> ReadNextUpgradeTask(CancellationToken cancellationToken)
		{
			return _updateIndexesChannel.Reader.ReadAsync(cancellationToken);
		}

		private async Task UpdateIndexesAsync<T>(string collectionName, IMongoCollection<T> collection, MongoIndex<T>[] newIndexes, CancellationToken cancellationToken)
		{
			// Find all the current indexes, excluding the default
			Dictionary<string, MongoIndex> nameToExistingIndex = new Dictionary<string, MongoIndex>(StringComparer.Ordinal);
			using (IAsyncCursor<BsonDocument> cursor = await collection.Indexes.ListAsync(cancellationToken))
			{
				while (await cursor.MoveNextAsync(cancellationToken))
				{
					foreach (BsonDocument document in cursor.Current)
					{
						MongoIndex indexDocument = BsonSerializer.Deserialize<MongoIndex>(document);
						if (!indexDocument.Name.Equals("_id_", StringComparison.Ordinal))
						{
							nameToExistingIndex.Add(indexDocument.Name, indexDocument);
						}
					}
				}
			}

			// Figure out which indexes to drop
			List<MongoIndex<T>> createIndexes = new List<MongoIndex<T>>();
			HashSet<string> removeIndexNames = new HashSet<string>(nameToExistingIndex.Keys, StringComparer.Ordinal);
			foreach (MongoIndex<T> newIndex in newIndexes)
			{
				MongoIndex? existingIndex;
				if (nameToExistingIndex.TryGetValue(newIndex.Name, out existingIndex))
				{
					if (existingIndex.Equals(newIndex))
					{
						removeIndexNames.Remove(newIndex.Name);
						continue;
					}
				}
				createIndexes.Add(newIndex);
			}

			// Drop any indexes that are no longer needed
			foreach (string removeIndexName in removeIndexNames)
			{
				if (ReadOnlyMode)
				{
					_logger.LogWarning("Would drop unused index {CollectionName}.{IndexName} - skipping due to read-only setting.", collectionName, removeIndexName);
				}
				else
				{
					_logger.LogInformation("Dropping unused index {IndexName}", removeIndexName);
					await collection.Indexes.DropOneAsync(removeIndexName, cancellationToken);
				}
			}

			// Create all the new indexes
			foreach (MongoIndex<T> createIndex in createIndexes)
			{
				if (ReadOnlyMode)
				{
					_logger.LogWarning("Would create index {CollectionName}.{IndexName} - skipping due to read-only setting.", collectionName, createIndex.Name);
				}
				else
				{
					_logger.LogInformation("Creating index {CollectionName}: {IndexName}", collectionName, createIndex.Name);

					CreateIndexOptions<T> options = new CreateIndexOptions<T>();
					options.Name = createIndex.Name;
					options.Unique = createIndex.Unique;

					CreateIndexModel<T> model = new CreateIndexModel<T>(createIndex.Keys, options);
					await collection.Indexes.CreateOneAsync(model, cancellationToken: cancellationToken);
				}
			}
		}

		/// <summary>
		/// Gets a singleton document by id
		/// </summary>
		/// <returns>The globals document</returns>
		public Task<Globals> GetGlobalsAsync()
		{
			return GetSingletonAsync(Globals.StaticId, () => new Globals() { SchemaVersion = UpgradeService.LatestSchemaVersion });
		}

		/// <summary>
		/// Gets a singleton document by id
		/// </summary>
		/// <param name="id">Id of the singleton document</param>
		/// <returns>The document</returns>
		public async Task<T> GetSingletonAsync<T>(ObjectId id) where T : SingletonBase, new()
		{
			T singleton = await GetSingletonAsync(id, () => new T());
			singleton.PostLoad();
			return singleton;
		}

		/// <summary>
		/// Gets a singleton document by id
		/// </summary>
		/// <param name="id">Id of the singleton document</param>
		/// <param name="constructor">Method to use to construct a new object</param>
		/// <returns>The document</returns>
		public async Task<T> GetSingletonAsync<T>(ObjectId id, Func<T> constructor) where T : SingletonBase, new()
		{
			FilterDefinition<BsonDocument> filter = new BsonDocument(new BsonElement("_id", id));
			for (; ; )
			{
				BsonDocument? document = await Singletons.Find(filter).FirstOrDefaultAsync();
				if (document != null)
				{
					T item = BsonSerializer.Deserialize<T>(document);
					item.PostLoad();
					return item;
				}

				T newItem = constructor();
				newItem.Id = id;
				await Singletons.InsertOneAsync(newItem.ToBsonDocument());
			}
		}

		/// <summary>
		/// Updates a singleton
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="id"></param>
		/// <param name="updater"></param>
		/// <returns></returns>
		public async Task UpdateSingletonAsync<T>(ObjectId id, Action<T> updater) where T : SingletonBase, new()
		{
			for (; ; )
			{
				T document = await GetSingletonAsync(id, () => new T());
				updater(document);

				if (await TryUpdateSingletonAsync(document))
				{
					break;
				}
			}
		}

		/// <summary>
		/// Attempts to update a singleton object
		/// </summary>
		/// <param name="singletonObject">The singleton object</param>
		/// <returns>True if the singleton document was updated</returns>
		public async Task<bool> TryUpdateSingletonAsync<T>(T singletonObject) where T : SingletonBase
		{
			int prevRevision = singletonObject.Revision++;

			BsonDocument filter = new BsonDocument { new BsonElement("_id", singletonObject.Id), new BsonElement(nameof(SingletonBase.Revision), prevRevision) };
			try
			{
				ReplaceOneResult result = await Singletons.ReplaceOneAsync(filter, singletonObject.ToBsonDocument(), new ReplaceOptions { IsUpsert = true });
				return result.MatchedCount > 0;
			}
			catch (MongoWriteException ex)
			{
				// Duplicate key error occurs if filter fails to match because revision is not the same.
				if (ex.WriteError != null && ex.WriteError.Category == ServerErrorCategory.DuplicateKey)
				{
					return false;
				}
				else
				{
					throw;
				}
			}
		}
	}

	/// <summary>
	/// Stores the version number of the latest server instance to have upgraded the database schema
	/// </summary>
	[SingletonDocument("62470d8508d48eddfac7b55d")]
	public class MongoSchemaDocument : SingletonBase
	{
		/// <summary>
		/// Current version number
		/// </summary>
		public string? Version { get; set; }
	}

	/// <summary>
	/// Service which updates the database state
	/// </summary>
	public sealed class MongoUpgradeService : IDisposable, IHostedService
	{
		static readonly RedisKey s_schemaLockKey = new RedisKey("server/schema-upgrade/lock");

		readonly MongoService _mongoService;
		readonly RedisService _redisService;
		readonly ISingletonDocument<MongoSchemaDocument> _schemaDocument;
		readonly ILogger _logger;
		readonly BackgroundTask _updateIndexesTask;

		/// <summary>
		/// Constructor
		/// </summary>
		public MongoUpgradeService(MongoService mongoService, RedisService redisService, ISingletonDocument<MongoSchemaDocument> schemaDocument, ILogger<MongoUpgradeService> logger)
		{
			_mongoService = mongoService;
			_redisService = redisService;
			_schemaDocument = schemaDocument;
			_logger = logger;
			_updateIndexesTask = new BackgroundTask(UpdateAsync);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_updateIndexesTask.Dispose();
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken)
		{
			_updateIndexesTask.Start();
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _updateIndexesTask.StopAsync();
		}

		async Task UpdateAsync(CancellationToken cancellationToken)
		{
			for (; ; )
			{
				Func<CancellationToken, Task> updateIndexTask = await _mongoService.ReadNextUpgradeTask(cancellationToken);
				for (; ; )
				{
					using (RedisLock schemaLock = new (_redisService.GetDatabase(), s_schemaLockKey))
					{
						if (await schemaLock.AcquireAsync(TimeSpan.FromMinutes(5.0)) && await UpdateOneAsync(updateIndexTask, cancellationToken))
						{
							break;
						}
					}
					await Task.Delay(TimeSpan.FromMinutes(1.0), cancellationToken);
				}
			}
		}

		async ValueTask<bool> UpdateOneAsync(Func<CancellationToken, Task> taskAsync, CancellationToken cancellationToken)
		{
			try
			{
				// Check we're not downgrading the data
				for (; ; )
				{
					MongoSchemaDocument currentSchema = await _schemaDocument.GetAsync();
					if (!String.IsNullOrEmpty(currentSchema.Version))
					{
						SemVer currentVersion = SemVer.Parse(currentSchema.Version);
						if (Program.Version < currentVersion)
						{
							return false;
						}
						if (Program.Version == currentVersion)
						{
							break;
						}
					}
					currentSchema.Version = Program.Version.ToString();

					if (_mongoService.ReadOnlyMode) // Pretend like we've upgraded the document so that the task can run and log its changes; it will guard writing while this flag is set itself.
					{
						break;
					}
					if (await _schemaDocument.TryUpdateAsync(currentSchema))
					{
						break;
					}
				}

				// Perform the upgrade
				await taskAsync(cancellationToken);
				return true;
			}
			catch (MongoCommandException ex)
			{
				_logger.LogWarning(ex, "Command exception while attempting to update indexes ({Code})", ex.Code);
				return false;
			}
			catch (Exception ex)
			{
				_logger.LogWarning(ex, "Exception while attempting to update indexes");
				return false;
			}
		}
	}
}
