// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Streams;
using EpicGames.Horde.Users;
using EpicGames.Perforce;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Users;
using Horde.Server.Utilities;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Horde.Server.Perforce
{
	/// <summary>
	/// P4API implementation of the Perforce service
	/// </summary>
	class PerforceService : IPerforceService, IAsyncDisposable
	{
		protected sealed class PooledConnection : IDisposable
		{
			public IPerforceConnection Perforce { get; }
			public IPerforceSettings Settings => Perforce.Settings;
			public Credentials Credentials { get; }
			public ClientRecord? Client { get; }
			public DateTime ExpiresAt { get; }

			InfoRecord? _info;

			public PooledConnection(IPerforceConnection perforce, Credentials credentials, ClientRecord? client, DateTime expiresAt)
			{
				Perforce = perforce;
				Credentials = credentials;
				Client = client;
				ExpiresAt = expiresAt;
			}

			public void Dispose()
			{
				Perforce.Dispose();
			}

			public async ValueTask<InfoRecord> GetInfoAsync(CancellationToken cancellationToken)
			{
				_info ??= await Perforce.GetInfoAsync(InfoOptions.ShortOutput, cancellationToken);
				return _info;
			}
		}

		protected class PooledConnectionHandle : IPooledPerforceConnection
		{
			readonly PerforceService _owner;
			PooledConnection _inner;

			public ClientRecord? Client => _inner.Client;
			public Credentials Credentials => _inner.Credentials;

			public PooledConnectionHandle(PerforceService owner, PooledConnection inner)
			{
				_owner = owner;
				_inner = inner;
			}

			/// <inheritdoc/>
			public void Dispose()
			{
				if (_inner != null)
				{
					_owner.ReleasePooledConnection(_inner);
					_inner = null!;
				}
			}

			/// <inheritdoc/>
			public IPerforceSettings Settings => _inner.Perforce.Settings;

			/// <inheritdoc/>
			public ILogger Logger => _inner.Perforce.Logger;

			/// <inheritdoc/>
			public IPerforceOutput Command(string command, IReadOnlyList<string> arguments, IReadOnlyList<string>? fileArguments, byte[]? inputData, string? promptResponse, bool interceptIo)
			{
				return _inner.Perforce.Command(command, arguments, fileArguments, inputData, promptResponse, interceptIo);
			}

			/// <inheritdoc/>
			public PerforceRecord CreateRecord(List<KeyValuePair<string, object>> fields)
			{
				return _inner.Perforce.CreateRecord(fields);
			}

			/// <inheritdoc/>
			public ValueTask<InfoRecord> GetInfoAsync(CancellationToken cancellationToken) => _inner.GetInfoAsync(cancellationToken);

			/// <inheritdoc/>
			public async ValueTask<PerforceViewMap> GetCachedStreamViewAsync(StreamConfig streamConfig, CancellationToken cancellationToken)
			{
				PerforceViewMap? viewMap;
				if (!_owner._streamCache.TryGetValue(streamConfig.Id, out viewMap))
				{
					StreamRecord streamRecord = await _inner.Perforce.GetStreamAsync(streamConfig.Name, true, cancellationToken);
					viewMap = PerforceViewMap.Parse(streamRecord.View);

					using (ICacheEntry entry = _owner._streamCache.CreateEntry(streamConfig.Id))
					{
						entry.AbsoluteExpirationRelativeToNow = TimeSpan.FromMinutes(2.0);
						entry.Value = viewMap;
					}
				}

				return viewMap!;
			}
		}

		protected class Credentials
		{
			public string UserName { get; }
			public string? Password { get; }
			public string? Ticket { get; }
			public DateTime? ExpiresAt { get; }

			public Credentials(string userName, string? password, string? ticket, DateTime? expiresAt)
			{
				UserName = userName;
				Password = password;
				Ticket = ticket;
				ExpiresAt = expiresAt;
			}
		}

		class Commit : ICommit
		{
			readonly PerforceService _owner;
			readonly StreamConfig _streamConfig;

			public StreamId StreamId => _streamConfig.Id;
			public int Number { get; }
			public int OriginalChange { get; }
			public UserId AuthorId { get; }
			public UserId OwnerId { get; }
			public string Description { get; }
			public string BasePath { get; }
			public DateTime DateUtc { get; }

			IReadOnlyList<string> _files = Array.Empty<string>();
			bool _hasAllFiles;

			public Commit(PerforceService owner, StreamConfig streamConfig, int number, int originalChange, UserId authorId, UserId ownerId, string description, string basePath, DateTime dateUtc)
			{
				_owner = owner;
				_streamConfig = streamConfig;

				Number = number;
				OriginalChange = originalChange;
				AuthorId = authorId;
				OwnerId = ownerId;
				Description = description;
				BasePath = basePath;
				DateUtc = dateUtc;
			}

			public void SetFiles(IReadOnlyList<string> files, int? maxFiles)
			{
				_files = files;
				_hasAllFiles = (maxFiles == null || files.Count < maxFiles.Value);
			}

			public async ValueTask<IReadOnlyList<CommitTag>> GetTagsAsync(CancellationToken cancellationToken)
			{
				List<CommitTag> commitTags = new List<CommitTag>();
				foreach (CommitTagConfig commitTagConfig in _streamConfig.GetAllCommitTags())
				{
					if (_streamConfig.TryGetCommitTagFilter(commitTagConfig.Name, out FileFilter? filter) && await MatchesFilterAsync(filter, cancellationToken))
					{
						commitTags.Add(commitTagConfig.Name);
					}
				}
				return commitTags;
			}

			public async ValueTask<bool> MatchesFilterAsync(FileFilter filter, CancellationToken cancellationToken)
			{
				int maxFiles = 1000;
				for (; ; )
				{
					// Query the files up to the current maximum
					IReadOnlyList<string> files = await GetFilesAsync(maxFiles, cancellationToken);
					if (filter.ApplyTo(files).Any())
					{
						return true;
					}

					// If we've already got the full file list for this change, also don't need to query any more
					if (_hasAllFiles)
					{
						return false;
					}

					// Query more files next time
					maxFiles = Math.Max(files.Count, maxFiles) * 10;
				}
			}

			public async ValueTask<IReadOnlyList<string>> GetFilesAsync(int maxFiles, CancellationToken cancellationToken)
			{
				if (maxFiles > _files.Count && !_hasAllFiles)
				{
					using (IPooledPerforceConnection perforce = await _owner.ConnectAsync(_streamConfig.ClusterName, null, cancellationToken))
					{
						DescribeRecord describeRecord = await perforce.DescribeAsync(DescribeOptions.None, maxFiles, Number, cancellationToken);
						_files = await perforce.GetStreamFilesAsync(_streamConfig, describeRecord, cancellationToken);
						_hasAllFiles = _files.Count < maxFiles;
					}
				}
				return _files;
			}
		}

		readonly PerforceLoadBalancer _loadBalancer;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;
		readonly Tracer _tracer;
		readonly ILogger _logger;

		// Useful overrides for local debugging with read-only data
		readonly string? _perforceServerOverride;
		readonly string? _perforceUserOverride;

		readonly ServerSettings _settings;
		readonly Dictionary<string, Dictionary<string, Credentials>> _userCredentialsByCluster = new Dictionary<string, Dictionary<string, Credentials>>(StringComparer.OrdinalIgnoreCase);
		readonly IUserCollection _userCollection;
		readonly MemoryCache _userCache = new MemoryCache(new MemoryCacheOptions { SizeLimit = 2000 });
		readonly MemoryCache _streamCache = new MemoryCache(new MemoryCacheOptions());

		readonly List<PooledConnection> _pooledConnections = new List<PooledConnection>();

		/// <summary>
		/// Constructor
		/// </summary>
		public PerforceService(PerforceLoadBalancer loadBalancer, IUserCollection userCollection, IOptions<ServerSettings> settings, IOptionsMonitor<GlobalConfig> globalConfig, Tracer tracer, ILogger<PerforceService> logger)
		{
			_loadBalancer = loadBalancer;
			_userCollection = userCollection;
			_settings = settings.Value;
			_globalConfig = globalConfig;
			_tracer = tracer;
			_logger = logger;

			if (settings.Value.UseLocalPerforceEnv)
			{
				IPerforceSettings perforceSettings = PerforceSettings.Default;
				_perforceServerOverride = perforceSettings.ServerAndPort;
				_perforceUserOverride = perforceSettings.UserName;
			}
		}

		public virtual ValueTask DisposeAsync()
		{
			foreach (PooledConnection pooledConnection in _pooledConnections)
			{
				pooledConnection.Dispose();
			}
			_pooledConnections.Clear();

			_userCache.Dispose();
			_streamCache.Dispose();
			return new ValueTask();
		}

		async Task<PooledConnectionHandle> CreatePooledConnectionAsync(string serverAndPort, Credentials credentials, ClientRecord? clientRecord, CancellationToken cancellationToken)
		{
			_ = cancellationToken;

			PerforceSettings settings = new PerforceSettings(serverAndPort, credentials.UserName);
			settings.AppName = "Horde.Server";
			settings.Password = String.IsNullOrEmpty(credentials.Ticket) ? credentials.Password : credentials.Ticket;
			settings.HostName = clientRecord?.Host;
			settings.ClientName = clientRecord?.Name ?? "__DOES_NOT_EXIST__";
			settings.PreferNativeClient = true;

			DateTime expiresAt = DateTime.UtcNow + TimeSpan.FromHours(1.0);
			if (credentials.ExpiresAt.HasValue && credentials.ExpiresAt.Value < expiresAt)
			{
				expiresAt = credentials.ExpiresAt.Value;
			}

			IPerforceConnection connection = await PerforceConnection.CreateAsync(settings, _logger);
			PooledConnection pooledConnection = new PooledConnection(connection, credentials, clientRecord, expiresAt);
			return new PooledConnectionHandle(this, pooledConnection);
		}

		PooledConnectionHandle? GetPooledConnection(Predicate<PooledConnection> predicate)
		{
			lock (_pooledConnections)
			{
				ReleaseExpiredConnections();
				for (int idx = 0; idx < _pooledConnections.Count; idx++)
				{
					PooledConnection connection = _pooledConnections[idx];
					if (predicate(connection))
					{
						_pooledConnections.RemoveAt(idx);
						return new PooledConnectionHandle(this, connection);
					}
				}
				return null;
			}
		}

		PooledConnectionHandle? GetPooledConnectionForUser(PerforceCluster cluster, string? userName)
		{
			string? poolUserName = userName ?? GetServiceUserCredentials(cluster).UserName;
			return GetPooledConnection(x => IsValidServer(x, cluster) && String.Equals(x.Settings.UserName, poolUserName, StringComparison.Ordinal) && x.Client == null);
		}

		PooledConnectionHandle? GetPooledConnectionForClient(PerforceCluster cluster, string? userName, string clientName)
		{
			string? poolUserName = userName ?? GetServiceUserCredentials(cluster).UserName;
			return GetPooledConnection(x => IsValidServer(x, cluster) && String.Equals(x.Settings.UserName, poolUserName, StringComparison.Ordinal) && String.Equals(x.Client?.Name, clientName, StringComparison.Ordinal));
		}

		PooledConnectionHandle? GetPooledConnectionForStream(PerforceCluster cluster, string? userName, string streamName)
		{
			string? poolUserName = userName ?? GetServiceUserCredentials(cluster).UserName;
			return GetPooledConnection(x => IsValidServer(x, cluster) && String.Equals(x.Settings.UserName, poolUserName, StringComparison.Ordinal) && x.Client != null && String.Equals(x.Client.Stream, streamName, StringComparison.Ordinal));
		}

		bool IsValidServer(PooledConnection connection, PerforceCluster cluster)
		{
			string serverAndPort = connection.Settings.ServerAndPort;
			if (_perforceServerOverride != null)
			{
				return String.Equals(serverAndPort, _perforceServerOverride, StringComparison.Ordinal);
			}
			else
			{
				return cluster.Servers.Any(x => x.ServerAndPort.Equals(serverAndPort, StringComparison.Ordinal));
			}
		}

		void ReleaseExpiredConnections()
		{
			DateTime utcNow = DateTime.UtcNow;
			for (int idx = _pooledConnections.Count - 1; idx >= 0; idx--)
			{
				PooledConnection connection = _pooledConnections[idx];
				if (connection.ExpiresAt < utcNow)
				{
					connection.Dispose();
					_pooledConnections.RemoveAt(idx);
				}
			}
		}

		void ReleasePooledConnection(PooledConnection connection)
		{
			lock (_pooledConnections)
			{
				ReleaseExpiredConnections();
				if (_pooledConnections.Count >= _settings.PerforceConnectionPoolSize)
				{
					_pooledConnections[0].Dispose();
					_pooledConnections.RemoveAt(0);
				}
				_pooledConnections.Add(connection);
			}
		}

		Credentials GetServiceUserCredentials(PerforceCluster cluster)
		{
			if (_perforceUserOverride != null)
			{
				return new Credentials(_perforceUserOverride, null, null, null);
			}
			else if (cluster.ServiceAccount != null)
			{
				PerforceCredentials? credentials = cluster.Credentials.FirstOrDefault(x => x.UserName.Equals(cluster.ServiceAccount, StringComparison.OrdinalIgnoreCase));
				if (credentials == null)
				{
					throw new Exception($"No credentials defined for {cluster.ServiceAccount} on {cluster.Name}");
				}
				return new Credentials(credentials.UserName, credentials.Password, credentials.Ticket, null);
			}
			else
			{
				PerforceCredentials? credentials = cluster.Credentials.FirstOrDefault();
				if (credentials != null)
				{
					return new Credentials(credentials.UserName, credentials.Password, credentials.Ticket, null);
				}
				else
				{
					return new Credentials(PerforceSettings.Default.UserName, null, null, null);
				}
			}
		}

		async Task<Credentials> GetCredentialsAsync(PerforceCluster cluster, string? userName, CancellationToken cancellationToken)
		{
			if (_perforceUserOverride != null)
			{
				return new Credentials(_perforceUserOverride, null, null, null);
			}
			else if (userName != null)
			{
				if (cluster.CanImpersonate && cluster.ServiceAccount != null)
				{
					return await GetTicketAsync(cluster, userName, cancellationToken);
				}
				else
				{
					return new Credentials(userName, null, null, null);
				}
			}
			else
			{
				return GetServiceUserCredentials(cluster);
			}
		}

		public async Task<IPooledPerforceConnection> ConnectAsync(string clusterName, string? userName = null, CancellationToken cancellationToken = default)
		{
			PerforceCluster cluster = _globalConfig.CurrentValue.GetPerforceCluster(clusterName);
			return await ConnectAsync(cluster, userName, cancellationToken);
		}

		async Task<PooledConnectionHandle> ConnectAsync(PerforceCluster cluster, string? userName, CancellationToken cancellationToken)
		{
			PooledConnectionHandle? handle = GetPooledConnectionForUser(cluster, userName);
			if (handle == null)
			{
				IPerforceServer server = await GetServerAsync(cluster, cancellationToken);
				Credentials credentials = await GetCredentialsAsync(cluster, userName, cancellationToken);
				handle = await CreatePooledConnectionAsync(server.ServerAndPort, credentials, null, cancellationToken);

				if (!String.IsNullOrEmpty(credentials.Password) && String.IsNullOrEmpty(credentials.Ticket))
				{
					await handle.LoginAsync(credentials.Password, cancellationToken);
				}
			}
			return handle;
		}

		async Task<PooledConnectionHandle> ConnectAsChangeOwnerAsync(PerforceCluster cluster, int change, CancellationToken cancellationToken)
		{
			using (IPerforceConnection perforce = await ConnectAsync(cluster, null, cancellationToken))
			{
				// Get the change information
				ChangeRecord changeRecord = await perforce.GetChangeAsync(GetChangeOptions.None, change, cancellationToken);
				if (changeRecord.Client == null)
				{
					throw new PerforceException("Client not specified on changelist {Change}", changeRecord.Number);
				}
				if (changeRecord.User == null)
				{
					throw new PerforceException("User not specified on changelist {Change}", changeRecord.Number);
				}

				// Check if there's an existing connection for this client and user. We can forgo any server checks if the client matches, because we know it'll be on the same edge server.
				PooledConnectionHandle? handle = GetPooledConnectionForClient(cluster, changeRecord.User, changeRecord.Client);
				if (handle != null)
				{
					return handle;
				}

				// Get the client that owns this change
				ClientRecord clientRecord = await perforce.GetClientAsync(changeRecord.Client, cancellationToken);

				// Get the server address to connect as, if it's locked to an edge server
				string? serverAndPort = null;
				if (!String.IsNullOrEmpty(clientRecord.ServerId))
				{
					ServerRecord serverRecord = await perforce.GetServerAsync(clientRecord.ServerId, cancellationToken);
					if (!String.IsNullOrEmpty(serverRecord.Address))
					{
						serverAndPort = serverRecord.Address;
					}
				}

				// Otherwise connect to the default
				if (serverAndPort == null)
				{
					IPerforceServer server = await GetServerAsync(cluster, cancellationToken);
					serverAndPort = server.ServerAndPort;
				}

				// Create a new connection
				Credentials credentials = await GetCredentialsAsync(cluster, changeRecord.User, cancellationToken);
				return await CreatePooledConnectionAsync(serverAndPort, credentials, clientRecord, cancellationToken);
			}
		}

		async Task<PooledConnectionHandle> ConnectWithStreamClientAsync(string clusterName, string? userName, string streamName, CancellationToken cancellationToken)
		{
			PerforceCluster cluster = _globalConfig.CurrentValue.GetPerforceCluster(clusterName);

			PooledConnectionHandle? handle = GetPooledConnectionForStream(cluster, userName, streamName);
			if (handle != null)
			{
				return handle;
			}

			_logger.LogDebug("Unable to allocate existing pooled connection (total: {Total}, cluster: {Cluster}, user: {UserName}, stream: {StreamName})", _pooledConnections.Count, clusterName, userName, streamName);

			using (PooledConnectionHandle perforce = await ConnectAsync(cluster, userName, cancellationToken))
			{
				InfoRecord info = await perforce.GetInfoAsync(cancellationToken);
				string clientName = GetClientName(info.UserName, info.ServerId ?? "unknown", streamName, readOnly: true);

				string root = RuntimeInformation.IsOSPlatform(OSPlatform.Linux) ? $"/tmp/{clientName}/" : $"{Path.GetTempPath()}{clientName}\\";

				ClientRecord client = new ClientRecord(clientName, info.UserName ?? String.Empty, root);
				client.Stream = streamName;
				await perforce.CreateClientAsync(client, cancellationToken);

				_logger.LogDebug("Created client {ClientName} (user: {User}, root: {Root}, stream: {Stream}, server: {ServerId}, serverAndPort: {ServerAndPort})", client.Name, client.Owner, client.Root, client.Stream, info.ServerId ?? "unknown", perforce.Settings.ServerAndPort);
				return await CreatePooledConnectionAsync(perforce.Settings.ServerAndPort, perforce.Credentials, client, cancellationToken);
			}
		}

		Task<PooledConnectionHandle> ConnectWithStreamClientAsync(StreamConfig streamConfig, string? userName, CancellationToken cancellationToken)
		{
			return ConnectWithStreamClientAsync(streamConfig.ClusterName, userName, streamConfig.Name, cancellationToken);
		}

		async Task<Credentials> GetTicketAsync(PerforceCluster cluster, string userName, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PerforceService)}.{nameof(GetTicketAsync)}");
			span.SetAttribute("clusterName", cluster.Name);
			span.SetAttribute("userName", userName);

			Credentials? ticketInfo = null;

			Dictionary<string, Credentials>? userTickets;
			lock (_userCredentialsByCluster)
			{
				// Check if we have a ticket
				if (!_userCredentialsByCluster.TryGetValue(cluster.Name, out userTickets))
				{
					userTickets = new Dictionary<string, Credentials>(StringComparer.OrdinalIgnoreCase);
					_userCredentialsByCluster[cluster.Name] = userTickets;
				}
				if (userTickets.TryGetValue(userName, out ticketInfo))
				{
					// if the credential expires within the next 15 minutes, refresh
					if (DateTime.UtcNow > ticketInfo.ExpiresAt)
					{
						userTickets.Remove(userName);
						ticketInfo = null;
					}
				}
			}

			if (ticketInfo == null)
			{
				PerforceResponse<LoginRecord> response;
				using (IPerforceConnection connection = await ConnectAsync(cluster, null, cancellationToken: cancellationToken))
				{
					response = await connection.TryLoginAsync(LoginOptions.AllHosts | LoginOptions.PrintTicket, userName, null, null, cancellationToken);
				}
				if (!response.Succeeded || response.Data.Ticket == null)
				{
					throw new PerforceException($"Unable to get impersonation credentials for {userName} for cluster {cluster.Name}");
				}

				DateTime expiresAt = DateTime.UtcNow + new TimeSpan(response.Data.TicketExpiration * TimeSpan.TicksPerSecond) - TimeSpan.FromMinutes(15.0);
				ticketInfo = new Credentials(userName, response.Data.Ticket, response.Data.Ticket, expiresAt);

				lock (_userCredentialsByCluster)
				{
					userTickets[userName] = ticketInfo;
				}
			}

			return ticketInfo;
		}

		public async ValueTask<IUser> FindOrAddUserAsync(string clusterName, string userName, CancellationToken cancellationToken = default)
		{
			PerforceCluster cluster = _globalConfig.CurrentValue.GetPerforceCluster(clusterName);
			return await FindOrAddUserAsync(cluster, userName, cancellationToken);
		}

		public async ValueTask<IUser> FindOrAddUserAsync(PerforceCluster cluster, string userName, CancellationToken cancellationToken = default)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PerforceService)}.{nameof(FindOrAddUserAsync)}");
			span.SetAttribute("clusterName", cluster.Name);
			span.SetAttribute("userName", userName);

			IUser? user;
			if (!_userCache.TryGetValue((cluster.Name, userName), out user))
			{
				user = await _userCollection.FindUserByLoginAsync(userName, cancellationToken);
				if (user == null)
				{
					UserRecord? userRecord = null;
					using (IPerforceConnection perforce = await ConnectAsync(cluster, null, cancellationToken))
					{
						PerforceResponseList<UserRecord> responses = await perforce.TryGetUserAsync(userName, cancellationToken);
						if (responses.Count > 0 && responses[0].Succeeded)
						{
							userRecord = responses[0].Data;
						}
						else
						{
							_logger.LogWarning("Unable to find user {UserName} on cluster {ClusterName}", userName, cluster.Name);
						}
					}
					user = await _userCollection.FindOrAddUserByLoginAsync(userRecord?.UserName ?? userName, userRecord?.FullName, userRecord?.Email, cancellationToken);
				}

				using (ICacheEntry entry = _userCache.CreateEntry((cluster.Name, userName)))
				{
					entry.SetValue(user);
					entry.SetSize(1);
					entry.SetAbsoluteExpiration(TimeSpan.FromDays(1.0));
				}
			}
			return user!;
		}

		async Task<IPerforceServer> GetServerAsync(PerforceCluster cluster, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PerforceService)}.{nameof(GetServerAsync)}");
			span.SetAttribute("clusterName", cluster.Name);

			IPerforceServer? server = await _loadBalancer.SelectServerAsync(cluster, cancellationToken);
			if (server == null)
			{
				throw new Exception($"Unable to select server from '{cluster.Name}'");
			}
			return server;
		}

		static string GetClientName(string? serviceUserName, string serverId, string stream, bool readOnly)
		{
			string clientName = $"zzt-horde-p4bridge-{serverId}-{Dns.GetHostName()}-{serviceUserName ?? "default"}-{stream.Replace("/", "+", StringComparison.OrdinalIgnoreCase)}";

			if (!readOnly)
			{
				clientName += "-write";
			}

			return SanitizeClientName(clientName);
		}

		static string SanitizeClientName(string clientName)
		{
			string result = Regex.Replace(clientName, @"%%|\.\.\.|[#/\\@ ]", "_");
			result = Regex.Replace(result, "_+", "_");
			result = Regex.Replace(result, "^_", "");
			result = Regex.Replace(result, "_$", "");
			return result;
		}

		async ValueTask<Commit> CreateCommitInternalAsync(StreamConfig streamConfig, int number, string author, string description, string? basePath, DateTime dateUtc, CancellationToken cancellationToken)
		{
			// Robomerge submits changes as its own user, then modifies the commit with the real author afterwards. Parse it out of the changelist description to handle these cases.
			if (String.Equals(author, "robomerge", StringComparison.OrdinalIgnoreCase))
			{
				Match match = Regex.Match(description, @"^#ROBOMERGE-AUTHOR:\s*([^\s]+)", RegexOptions.Multiline);
				if (match.Success)
				{
					string newAuthor = match.Groups[1].Value;
					_logger.LogDebug("Changing author of CL {Change} from {Author} to {NewAuthor}", number, author, newAuthor);
					author = newAuthor;
				}
			}

			IUser authorUser = await FindOrAddUserAsync(streamConfig.ClusterName, author, cancellationToken);
			IUser ownerUser = authorUser;

			int originalChange = ParseRobomergeSource(description) ?? number;

			string? owner = ParseRobomergeOwner(description);
			if (owner != null)
			{
				ownerUser = await FindOrAddUserAsync(streamConfig.ClusterName, owner, cancellationToken);
			}

			return new Commit(this, streamConfig, number, originalChange, authorUser.Id, ownerUser.Id, description, basePath ?? String.Empty, dateUtc);
		}

		protected async ValueTask<ICommit> CreateCommitAsync(IPooledPerforceConnection perforce, StreamConfig streamConfig, DescribeRecord describeRecord, int maxFiles, InfoRecord serverInfo, CancellationToken cancellationToken)
		{
			DateTime timeUtc = new DateTime(describeRecord.Time.Ticks - serverInfo.TimeZoneOffsetSecs * TimeSpan.TicksPerSecond, DateTimeKind.Utc);

			Commit commit = await CreateCommitInternalAsync(streamConfig, describeRecord.Number, describeRecord.User, describeRecord.Description, describeRecord.Path, timeUtc, cancellationToken);

			List<string> files = await perforce.GetStreamFilesAsync(streamConfig, describeRecord, cancellationToken);
			if (files.Count == 0 && describeRecord.Files.Count > 0)
			{
				throw new PerforceException($"Changelist {commit.Number} does not contain any files in {streamConfig.Id}");
			}
			commit.SetFiles(files, maxFiles);

			return commit;
		}

		async ValueTask<Commit> CreateCommitAsync(StreamConfig streamConfig, ChangesRecord changesRecord, InfoRecord serverInfo, CancellationToken cancellationToken)
		{
			DateTime timeUtc = new DateTime(changesRecord.Time.Ticks - serverInfo.TimeZoneOffsetSecs * TimeSpan.TicksPerSecond, DateTimeKind.Utc);

			Commit commit = await CreateCommitInternalAsync(streamConfig, changesRecord.Number, changesRecord.User, changesRecord.Description, changesRecord.Path, timeUtc, cancellationToken);

			return commit;
		}

		/// <summary>
		/// Attempts to parse the Robomerge source from this commit information
		/// </summary>
		/// <param name="description">Description text to parse</param>
		/// <returns>The parsed source changelist, or null if no #ROBOMERGE-SOURCE tag was present</returns>
		static int? ParseRobomergeSource(string description)
		{
			// #ROBOMERGE-SOURCE: CL 13232051 in //Fortnite/Release-12.60/... via CL 13232062 via CL 13242953
			Match match = Regex.Match(description, @"^#ROBOMERGE-SOURCE: CL (\d+)", RegexOptions.Multiline);
			if (match.Success)
			{
				return Int32.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture);
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Attempts to parse the Robomerge owner from this commit information
		/// </summary>
		/// <param name="description">Description text to parse</param>
		/// <returns>The Robomerge owner, or null if no #ROBOMERGE-OWNER tag was present</returns>
		static string? ParseRobomergeOwner(string description)
		{
			// #ROBOMERGE-OWNER: ben.marsh
			Match match = Regex.Match(description, @"^#ROBOMERGE-OWNER:\s*([^@\s]+)", RegexOptions.Multiline);
			if (match.Success)
			{
				return match.Groups[1].Value;
			}
			else
			{
				return null;
			}
		}

		/// <inheritdoc/>
		public async Task<ICommit> GetChangeDetailsAsync(StreamConfig streamConfig, int changeNumber, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PerforceService)}.{nameof(GetChangeDetailsAsync)}");
			span.SetAttribute("clusterName", streamConfig.ClusterName);
			span.SetAttribute("streamName", streamConfig.Name);
			span.SetAttribute("changeNumber", changeNumber);

			PerforceCluster cluster = _globalConfig.CurrentValue.GetPerforceCluster(streamConfig.ClusterName);

			using (PooledConnectionHandle perforce = await ConnectAsync(cluster, null, cancellationToken))
			{
				const int MaxFiles = 1000;
				DescribeRecord describeRecord = await perforce.DescribeAsync(DescribeOptions.None, MaxFiles, changeNumber, cancellationToken);

				InfoRecord info = await perforce.GetInfoAsync(cancellationToken);

				DateTime timeUtc = new DateTime(describeRecord.Time.Ticks - info.TimeZoneOffsetSecs * TimeSpan.TicksPerSecond, DateTimeKind.Utc);

				ICommit commit = await CreateCommitAsync(perforce, streamConfig, describeRecord, MaxFiles, info, cancellationToken);
				return commit;
			}
		}

		/// <inheritdoc/>
		public async Task<(CheckShelfResult, ShelfInfo?)> CheckShelfAsync(StreamConfig streamConfig, int changeNumber, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PerforceService)}.{nameof(CheckShelfAsync)}");
			span.SetAttribute("clusterName", streamConfig.ClusterName);
			span.SetAttribute("streamName", streamConfig.Name);
			span.SetAttribute("changeNumber", changeNumber);

			PerforceCluster cluster = _globalConfig.CurrentValue.GetPerforceCluster(streamConfig.ClusterName);

			using (IPooledPerforceConnection perforce = await ConnectAsync(cluster, null, cancellationToken))
			{
				PerforceResponse<DescribeRecord> response = await perforce.TryDescribeAsync(DescribeOptions.Shelved, -1, changeNumber, cancellationToken);
				if (response.Error != null)
				{
					if (response.Error.Generic == PerforceGenericCode.Empty)
					{
						return (CheckShelfResult.NoChange, null);
					}
					else
					{
						response.EnsureSuccess();
					}
				}

				DescribeRecord change = response.Data;
				if (change.Files.Count == 0)
				{
					return (CheckShelfResult.NoShelvedFiles, null);
				}

				PerforceViewMap viewMap = await perforce.GetCachedStreamViewAsync(streamConfig, cancellationToken);
				List<string> mappedFiles = new List<string>();

				bool hasUnmappedFile = false;
				foreach (DescribeFileRecord shelvedFile in change.Files)
				{
					string mappedFile;
					if (viewMap.TryMapFile(shelvedFile.DepotFile, StringComparison.OrdinalIgnoreCase, out mappedFile))
					{
						mappedFiles.Add(mappedFile);
					}
					else
					{
						hasUnmappedFile = true;
					}
				}

				if (hasUnmappedFile)
				{
					return ((mappedFiles.Count > 0) ? CheckShelfResult.MixedStream : CheckShelfResult.WrongStream, null);
				}

				List<CommitTag> tags = new List<CommitTag>();
				foreach (CommitTagConfig tagConfig in streamConfig.GetAllCommitTags())
				{
					FileFilter? filter;
					if (streamConfig.TryGetCommitTagFilter(tagConfig.Name, out filter) && mappedFiles.Any(x => filter.Matches(x)))
					{
						tags.Add(tagConfig.Name);
					}
				}

				return (CheckShelfResult.Ok, new ShelfInfo(change.Description, tags));
			}
		}

		/// <inheritdoc/>
		public Task<int> DuplicateShelvedChangeAsync(string clusterName, int shelvedChange, CancellationToken cancellationToken)
		{
			throw new NotImplementedException();
		}

		/// <inheritdoc/>
		public async Task DeleteShelvedChangeAsync(string clusterName, int shelvedChange, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PerforceService)}.{nameof(DeleteShelvedChangeAsync)}");
			span.SetAttribute("clusterName", clusterName);
			span.SetAttribute("shelvedChange", shelvedChange);

			PerforceCluster cluster = _globalConfig.CurrentValue.GetPerforceCluster(clusterName);

			using (IPerforceConnection perforce = await ConnectAsChangeOwnerAsync(cluster, shelvedChange, cancellationToken))
			{
				await perforce.TryDeleteShelvedFilesAsync(shelvedChange, FileSpecList.Any, cancellationToken);
				await perforce.DeleteChangeAsync(DeleteChangeOptions.None, shelvedChange, cancellationToken);
			}
		}

		/// <inheritdoc/>
		public async Task UpdateChangelistDescriptionAsync(string clusterName, int change, Func<string, string> updateFunc, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PerforceService)}.{nameof(UpdateChangelistDescriptionAsync)}");
			span.SetAttribute("clusterName", clusterName);
			span.SetAttribute("change", change);

			try
			{
				PerforceCluster cluster = _globalConfig.CurrentValue.GetPerforceCluster(clusterName);
				using (IPerforceConnection perforce = await ConnectAsChangeOwnerAsync(cluster, change, cancellationToken))
				{
					ChangeRecord record = await perforce.GetChangeAsync(GetChangeOptions.None, change, cancellationToken);

					string newDescription = updateFunc(record.Description ?? String.Empty);
					if (!String.Equals(record.Description, newDescription, StringComparison.Ordinal))
					{
						record.Description = newDescription;
						await perforce.UpdateChangeAsync(UpdateChangeOptions.None, record, cancellationToken);
					}
				}
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Unable to update Changelist for CL {Change}: {Message}", change, ex.Message);
			}
		}

		/// <inheritdoc/>
		public virtual Task RefreshCachedCommitAsync(string clusterName, int change) => Task.CompletedTask;

		static async Task ResetClientAsync(IPerforceConnection perforce, CancellationToken cancellationToken)
		{
			await perforce.RevertAsync(-1, null, RevertOptions.KeepWorkspaceFiles, FileSpecList.Any, cancellationToken);

			List<ChangesRecord> changes = await perforce.GetChangesAsync(ChangesOptions.None, perforce.Settings.ClientName, -1, ChangeStatus.Pending, null, FileSpecList.Any, cancellationToken);
			foreach (ChangesRecord change in changes)
			{
				await perforce.DeleteShelvedFilesAsync(change.Number, FileSpecList.Any, cancellationToken);
				await perforce.DeleteChangeAsync(DeleteChangeOptions.None, change.Number, cancellationToken);
			}
		}

		/// <inheritdoc/>
		public async Task<int> CreateNewChangeAsync(string clusterName, string streamName, string filePath, string description, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PerforceService)}.{nameof(CreateNewChangeAsync)}");
			span.SetAttribute("clusterName", clusterName);
			span.SetAttribute("streamName", streamName);
			span.SetAttribute("filePath", filePath);

			using (PooledConnectionHandle perforce = await ConnectWithStreamClientAsync(clusterName, null, streamName, cancellationToken))
			{
				string workspaceFilePath = $"//{perforce.Client!.Name}/{filePath.TrimStart('/')}";
				string diskFilePath = $"{perforce.Client!.Root + filePath.TrimStart('/')}";
				_logger.LogInformation("Attempting to create new changelist by submitting {WorkspaceFilePath} (local: {DiskFilePath})", workspaceFilePath, diskFilePath);

				int attempt = 0;
				const int MaxAttempts = 5;

				for (; ; )
				{
					attempt++;
					await ResetClientAsync(perforce, cancellationToken);

					string? depotPath = null;
					try
					{
						List<FStatRecord> files = await perforce.FStatAsync(workspaceFilePath, cancellationToken).ToListAsync(cancellationToken);
						if (files.Count == 0)
						{
							// File does not exist, create it
							string? directoryName = Path.GetDirectoryName(diskFilePath);

							if (String.IsNullOrEmpty(directoryName))
							{
								throw new Exception($"Invalid directory name for local client file, disk file path: {diskFilePath}");
							}

							// Create the directory
							if (!Directory.Exists(directoryName))
							{
								Directory.CreateDirectory(directoryName);

								if (!Directory.Exists(directoryName))
								{
									throw new Exception($"Unable to create directrory: {directoryName}");
								}
							}

							// Create the file
							if (!File.Exists(diskFilePath))
							{
								using (FileStream fileStream = File.OpenWrite(diskFilePath))
								{
									fileStream.Close();
								}

								if (!File.Exists(diskFilePath))
								{
									throw new Exception($"Unable to create local change file: {diskFilePath}");
								}
							}

							List<AddRecord> addFiles = await perforce.AddAsync(-1, workspaceFilePath, cancellationToken);
							if (addFiles.Count != 1)
							{
								throw new Exception($"Unable to add local change file,  local: {diskFilePath} : workspace: {workspaceFilePath}");
							}

							depotPath = addFiles[0].DepotFile;
						}
						else
						{
							List<SyncRecord> syncResults = await perforce.SyncAsync(SyncOptions.Force, -1, workspaceFilePath, cancellationToken).ToListAsync(cancellationToken);
							if (syncResults == null || syncResults.Count != 1)
							{
								throw new Exception($"Unable to sync file, workspace: {workspaceFilePath}");
							}

							List<EditRecord> editResults = await perforce.EditAsync(-1, syncResults[0].DepotFile.ToString(), cancellationToken);
							if (editResults == null || editResults.Count != 1)
							{
								throw new Exception($"Unable to edit file, workspace: {workspaceFilePath}");
							}

							depotPath = editResults[0].DepotFile;
						}

						if (String.IsNullOrEmpty(depotPath))
						{
							throw new Exception($"Unable to get depot path for: {workspaceFilePath}");
						}

						// create a new change
						ChangeRecord change = new ChangeRecord();
						change.Description = description;
						change.Files.Add(depotPath);
						change = await perforce.CreateChangeAsync(change, cancellationToken);

						PerforceResponse<SubmitRecord> submitResponse = await perforce.TrySubmitAsync(change.Number, SubmitOptions.SubmitUnchanged, cancellationToken);
						if (attempt < MaxAttempts && submitResponse.Error != null && submitResponse.Error.Generic == PerforceGenericCode.NotYet)
						{
							// File needs resolving; sync and retry.
							_logger.LogDebug("Unable to submit new changelist (file: {File}, depotPath: {DepotPath}, description: \"{Description}\", attempt: {Attempt}/{MaxAttempts}, error: {Message}", filePath, depotPath, description, attempt, MaxAttempts, submitResponse.Error);
							continue;
						}

						SubmitRecord submit = submitResponse.Data;
						_logger.LogInformation("Submitted new changelist with {DepotPath}: CL {Change}", depotPath, submit.SubmittedChangeNumber);
						return submit.SubmittedChangeNumber;
					}
					catch (OperationCanceledException)
					{
						throw;
					}
					catch (Exception ex)
					{
						if (attempt < MaxAttempts)
						{
							_logger.LogWarning(ex, "Unable to submit new changelist (file: {File}, depotPath: {DepotPath}, description: \"{Description}\", attempt: {Attempt}/{MaxAttempts}, error: {Message}", filePath, depotPath, description, attempt, MaxAttempts, ex.Message);
						}
						else
						{
							throw new PerforceException($"Unable to submit new changelist for {filePath} after {MaxAttempts} attempts: {ex.Message}");
						}
					}
					finally
					{
						if (File.Exists(diskFilePath))
						{
							try
							{
								File.SetAttributes(diskFilePath, FileAttributes.Normal);
								File.Delete(diskFilePath);
							}
							catch (Exception ex2)
							{
								_logger.LogWarning(ex2, "Unable to delete temp file {File}", diskFilePath);
							}
						}
					}
				}
			}
		}

		/// <inheritdoc/>
		public async Task<(int? Change, string Message)> SubmitShelvedChangeAsync(StreamConfig streamConfig, int change, int originalChange, CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(PerforceService)}.{nameof(SubmitShelvedChangeAsync)}");
			span.SetAttribute("stream", streamConfig.Id);
			span.SetAttribute("change", change);
			span.SetAttribute("originalChange", originalChange);

			PerforceCluster cluster = _globalConfig.CurrentValue.GetPerforceCluster(streamConfig.ClusterName);

			List<DescribeRecord> records;
			using (IPerforceConnection perforce = await ConnectAsync(cluster, null, cancellationToken))
			{
				records = await perforce.DescribeAsync(DescribeOptions.Shelved, -1, new int[] { change, originalChange }, cancellationToken);
				if (records.Count != 2)
				{
					throw new PerforceException("Unexpected response count when examining changes to submit; expected 2, got {NumRecords}", records.Count);
				}

				if (records[0].Files.Count != records[1].Files.Count)
				{
					return (null, $"Mismatched number of shelved files for change {change} and original change: {originalChange}");
				}
				if (records[0].Files.Count == 0)
				{
					return (null, $"No shelved file for change {originalChange}");
				}

				List<DescribeFileRecord> files0 = records[0].Files.OrderBy(x => x.DepotFile, StringComparer.Ordinal).ToList();
				List<DescribeFileRecord> files1 = records[1].Files.OrderBy(x => x.DepotFile, StringComparer.Ordinal).ToList();
				foreach ((DescribeFileRecord file0, DescribeFileRecord file1) in Enumerable.Zip(files0, files1))
				{
					if (!String.Equals(file0.DepotFile, file1.DepotFile, StringComparison.Ordinal) || !String.Equals(file0.Digest, file1.Digest, StringComparison.Ordinal) || file0.Action != file1.Action)
					{
						_logger.LogInformation("Mismatch in shelved files between {File0} or {File1}", file0.DepotFile, file1.DepotFile);
						return (null, $"Shelved files have been modified.");
					}
				}
			}

			using (IPerforceConnection perforce = await ConnectAsChangeOwnerAsync(cluster, change, cancellationToken))
			{
				try
				{
					SubmitRecord submit = await perforce.SubmitShelvedAsync(change, cancellationToken);
					return (submit.SubmittedChangeNumber, records[0].Description);
				}
				catch (PerforceException ex) when (ex.Error != null && ex.Error.Generic == PerforceGenericCode.NotYet)
				{
					_logger.LogInformation(ex, "Unable to submit shelved change {Change}: {Message}", change, ex.Message);
					return (null, $"Submit command failed: {ex.Message}");
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Unable to submit shelved change {Change}: {Message}", change, ex.Message);
					return (null, $"Submit command failed: {ex.Message}");
				}
			}
		}

		/// <summary>
		/// Gets the wildcard filter for a particular range of changes
		/// </summary>
		/// <param name="basePath">Base path to find files for</param>
		/// <param name="minChange">Minimum changelist number to query</param>
		/// <param name="maxChange">Maximum changelist number to query</param>
		/// <returns>Filter string</returns>
		public static string GetFilter(string basePath, int? minChange, int? maxChange)
		{
			StringBuilder filter = new StringBuilder(basePath);
			if (minChange != null && maxChange != null)
			{
				filter.Append(CultureInfo.InvariantCulture, $"@{minChange},{maxChange}");
			}
			else if (minChange != null)
			{
				filter.Append(CultureInfo.InvariantCulture, $"@>={minChange}");
			}
			else if (maxChange != null)
			{
				filter.Append(CultureInfo.InvariantCulture, $"@<={maxChange}");
			}
			return filter.ToString();
		}

		/// <summary>
		/// Gets a stream-relative path from a depot path
		/// </summary>
		/// <param name="depotFile">The depot file to check</param>
		/// <param name="streamName">Name of the stream</param>
		/// <param name="relativePath">Receives the relative path to the file</param>
		/// <returns>True if the stream-relative path was returned</returns>
		public static bool TryGetStreamRelativePath(string depotFile, string streamName, [NotNullWhen(true)] out string? relativePath)
		{
			if (depotFile.StartsWith(streamName, StringComparison.OrdinalIgnoreCase) && depotFile.Length > streamName.Length && depotFile[streamName.Length] == '/')
			{
				relativePath = depotFile.Substring(streamName.Length);
				return true;
			}

			Match match = Regex.Match(depotFile, "^//[^/]+/[^/]+(/.*)$");
			if (match.Success)
			{
				relativePath = match.Groups[1].Value;
				return true;
			}

			relativePath = null;
			return false;
		}

		/// <summary>
		/// Implementation of <see cref="ICommitCollection"/> to return commits from a particular stream
		/// </summary>
		protected class CommitSource : ICommitCollection
		{
			protected PerforceService PerforceService { get; }
			protected StreamConfig StreamConfig { get; }
			protected ILogger Logger { get; }

			public CommitSource(PerforceService perforceService, StreamConfig streamConfig, ILogger logger)
			{
				PerforceService = perforceService;
				StreamConfig = streamConfig;
				Logger = logger;
			}

			public async Task<int> CreateNewAsync(string path, string description, CancellationToken cancellationToken = default)
			{
				Match match = Regex.Match(path, @"^(//[^/]+/[^/]+)/(.+)$");
				if (match.Success)
				{
					return await PerforceService.CreateNewChangeAsync(StreamConfig.ClusterName, match.Groups[1].Value, match.Groups[2].Value, description, cancellationToken);
				}
				else
				{
					return await PerforceService.CreateNewChangeAsync(StreamConfig.ClusterName, StreamConfig.Name, path, description, cancellationToken);
				}
			}

			public virtual async IAsyncEnumerable<ICommit> FindAsync(int? minChange, int? maxChange, int? maxResults, IReadOnlyList<CommitTag>? tags, [EnumeratorCancellation] CancellationToken cancellationToken = default)
			{
				using TelemetrySpan span = PerforceService._tracer.StartActiveSpan($"{nameof(Perforce.PerforceService)}.{nameof(PerforceService.CommitSource)}.{nameof(FindAsync)}");
				span.SetAttribute("stream", StreamConfig.ClusterName);
				span.SetAttribute("minChange", minChange ?? -2);
				span.SetAttribute("maxChange", maxChange ?? -2);
				span.SetAttribute("maxResults", maxResults ?? -1);

				using (PooledConnectionHandle perforce = await PerforceService.ConnectWithStreamClientAsync(StreamConfig, null, cancellationToken))
				{
					InfoRecord info = await perforce.GetInfoAsync(cancellationToken);

					if (tags == null || tags.Count == 0)
					{
						string filter = GetFilter($"//{perforce.Settings.ClientName}/...", minChange, maxChange);

						List<ChangesRecord> changes = await perforce.GetChangesAsync(ChangesOptions.IncludeTimes | ChangesOptions.LongOutput, maxResults ?? -1, ChangeStatus.Submitted, filter, cancellationToken);
						foreach (ChangesRecord change in changes)
						{
							ICommit commit = await PerforceService.CreateCommitAsync(StreamConfig, change, info, cancellationToken);
							yield return commit;
						}
					}
					else
					{
						// Do the query in batches
						int numResults = maxResults ?? Int32.MaxValue;
						while (numResults > 0)
						{
							string filter = GetFilter($"//{perforce.Settings.ClientName}/...", minChange, maxChange);

							const int BatchSize = 20;

							List<ChangesRecord> changesRecords = await perforce.GetChangesAsync(ChangesOptions.None, null, BatchSize, ChangeStatus.Submitted, null, filter, cancellationToken);
							if (changesRecords.Count == 0)
							{
								break;
							}

							const int MaxFiles = 1000;

							List<DescribeRecord> describeRecords = await perforce.DescribeAsync(DescribeOptions.None, MaxFiles, changesRecords.Select(x => x.Number).ToArray(), cancellationToken);
							foreach (DescribeRecord describeRecord in describeRecords)
							{
								ICommit commit = await PerforceService.CreateCommitAsync(perforce, StreamConfig, describeRecord, MaxFiles, info, cancellationToken);

								IReadOnlyList<CommitTag> commitTags = await commit.GetTagsAsync(cancellationToken);
								if (commitTags.Intersect(tags).Any())
								{
									yield return commit;
									if (--numResults == 0)
									{
										yield break;
									}
								}
							}

							maxChange = changesRecords[^1].Number - 1;
						}
					}
				}
			}

			/// <inheritdoc/>
			public virtual Task<ICommit> GetAsync(int changeNumber, CancellationToken cancellationToken = default)
			{
				return PerforceService.GetChangeDetailsAsync(StreamConfig, changeNumber, cancellationToken);
			}

			/// <inheritdoc/>
			public virtual async IAsyncEnumerable<ICommit> SubscribeAsync(int minChange, IReadOnlyList<CommitTag>? tags = null, [EnumeratorCancellation] CancellationToken cancellationToken = default)
			{
				for (; ; )
				{
					await foreach (ICommit commit in FindAsync(minChange + 1, null, 10, tags, cancellationToken))
					{
						yield return commit;
						minChange = commit.Number;
					}
					await Task.Delay(TimeSpan.FromSeconds(10.0), cancellationToken);
				}
			}
		}

		/// <inheritdoc/>
		public virtual ICommitCollection GetCommits(StreamConfig streamConfig)
		{
			return new CommitSource(this, streamConfig, _logger);
		}
	}
}
