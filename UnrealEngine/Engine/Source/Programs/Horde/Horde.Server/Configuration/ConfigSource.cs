// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Perforce;
using Horde.Server.Users;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Configuration
{
	/// <summary>
	/// Source for reading config files
	/// </summary>
	public interface IConfigSource
	{
		/// <summary>
		/// URI scheme for this config source
		/// </summary>
		string Scheme { get; }

		/// <summary>
		/// Update interval for this source
		/// </summary>
		TimeSpan UpdateInterval { get; }

		/// <summary>
		/// Reads a config file from this source
		/// </summary>
		/// <param name="uris">Locations of the config files to query</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Config file data</returns>
		Task<IConfigFile[]> GetAsync(Uri[] uris, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Extension methods for <see cref="IConfigSource"/>
	/// </summary>
	public static class ConfigSource
	{
		/// <summary>
		/// Gets a single config file from a source
		/// </summary>
		/// <param name="source">Source to query</param>
		/// <param name="uri">Location of the config file to query</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Config file data</returns>
		public static async Task<IConfigFile> GetAsync(this IConfigSource source, Uri uri, CancellationToken cancellationToken)
		{
			IConfigFile[] result = await source.GetAsync(new[] { uri }, cancellationToken);
			return result[0];
		}
	}

	/// <summary>
	/// In-memory config file source
	/// </summary>
	public sealed class InMemoryConfigSource : IConfigSource
	{
		class ConfigFileRevisionImpl : IConfigFile
		{
			public Uri Uri { get; }
			public string Revision { get; }
			public ReadOnlyMemory<byte> Data { get; }
			public IUser? Author => null;

			public ConfigFileRevisionImpl(Uri uri, string version, ReadOnlyMemory<byte> data)
			{
				Uri = uri;
				Revision = version;
				Data = data;
			}

			public ValueTask<ReadOnlyMemory<byte>> ReadAsync(CancellationToken cancellationToken) => new ValueTask<ReadOnlyMemory<byte>>(Data);
		}

		readonly Dictionary<Uri, ConfigFileRevisionImpl> _files = new Dictionary<Uri, ConfigFileRevisionImpl>();

		/// <summary>
		/// Name of the scheme for this source
		/// </summary>
		public const string Scheme = "memory";

		/// <inheritdoc/>
		string IConfigSource.Scheme => Scheme;

		/// <inheritdoc/>
		public TimeSpan UpdateInterval => TimeSpan.FromSeconds(1.0);

		/// <summary>
		/// Manually adds a new config file
		/// </summary>
		/// <param name="path">Path to the config file</param>
		/// <param name="data">Config file data</param>
		public void Add(Uri path, ReadOnlyMemory<byte> data)
		{
			_files.Add(path, new ConfigFileRevisionImpl(path, "v1", data));
		}

		/// <inheritdoc/>
		public Task<IConfigFile[]> GetAsync(Uri[] uris, CancellationToken cancellationToken)
		{
			IConfigFile[] result = new IConfigFile[uris.Length];
			for (int idx = 0; idx < uris.Length; idx++)
			{
				ConfigFileRevisionImpl? configFile;
				if (!_files.TryGetValue(uris[idx], out configFile))
				{
					throw new FileNotFoundException($"Config file {uris[idx]} not found.");
				}
				result[idx] = configFile;
			}
			return Task.FromResult(result);
		}
	}

	/// <summary>
	/// Config file source which reads from the filesystem
	/// </summary>
	public sealed class FileConfigSource : IConfigSource
	{
		class ConfigFileImpl : IConfigFile
		{
			public Uri Uri { get; }
			public string Revision { get; }
			public DateTime LastWriteTimeUtc { get; }
			public ReadOnlyMemory<byte> Data { get; }
			public IUser? Author => null;

			public ConfigFileImpl(Uri uri, DateTime lastWriteTimeUtc, ReadOnlyMemory<byte> data)
			{
				Uri = uri;
				Revision = $"timestamp={lastWriteTimeUtc.Ticks}";
				LastWriteTimeUtc = lastWriteTimeUtc;
				Data = data;
			}

			public ValueTask<ReadOnlyMemory<byte>> ReadAsync(CancellationToken cancellationToken) => new ValueTask<ReadOnlyMemory<byte>>(Data);
		}

		/// <summary>
		/// Name of the scheme for this source
		/// </summary>
		public const string Scheme = "file";

		/// <inheritdoc/>
		string IConfigSource.Scheme => Scheme;

		/// <inheritdoc/>
		public TimeSpan UpdateInterval => TimeSpan.FromSeconds(5.0);

		readonly DirectoryReference _baseDir;
		readonly ConcurrentDictionary<FileReference, ConfigFileImpl> _files = new ConcurrentDictionary<FileReference, ConfigFileImpl>();

		/// <summary>
		/// Constructor
		/// </summary>
		public FileConfigSource()
			: this(DirectoryReference.GetCurrentDirectory())
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="baseDir">Base directory for resolving relative paths</param>
		public FileConfigSource(DirectoryReference baseDir)
		{
			_baseDir = baseDir;
		}

		/// <inheritdoc/>
		public async Task<IConfigFile[]> GetAsync(Uri[] uris, CancellationToken cancellationToken)
		{
			IConfigFile[] files = new IConfigFile[uris.Length];
			for (int idx = 0; idx < uris.Length; idx++)
			{
				Uri uri = uris[idx];
				FileReference localPath = FileReference.Combine(_baseDir, uri.LocalPath);

				ConfigFileImpl? file;
				for (; ; )
				{
					if (_files.TryGetValue(localPath, out file))
					{
						if (FileReference.GetLastWriteTimeUtc(localPath) == file.LastWriteTimeUtc)
						{
							break;
						}
						else
						{
							_files.TryRemove(new KeyValuePair<FileReference, ConfigFileImpl>(localPath, file));
						}
					}

					using (FileStream stream = FileReference.Open(localPath, FileMode.Open, FileAccess.Read, FileShare.Read))
					{
						using MemoryStream memoryStream = new MemoryStream();
						await stream.CopyToAsync(memoryStream, cancellationToken);
						DateTime lastWriteTime = FileReference.GetLastWriteTimeUtc(localPath);
						file = new ConfigFileImpl(uri, lastWriteTime, memoryStream.ToArray());
					}

					if (_files.TryAdd(localPath, file))
					{
						break;
					}
				}

				files[idx] = file;
			}
			return files;
		}
	}

	/// <summary>
	/// Perforce cluster config file source
	/// </summary>
	public sealed class PerforceConfigSource : IConfigSource
	{
		class ConfigFileImpl : IConfigFile
		{
			public Uri Uri { get; }
			public int Change { get; }
			public string Revision { get; }
			public IUser? Author { get; }

			readonly PerforceConfigSource _owner;

			public ConfigFileImpl(Uri uri, int change, IUser? author, PerforceConfigSource owner)
			{
				Uri = uri;
				Change = change;
				Revision = $"{change}";
				Author = author;
				_owner = owner;
			}

			public ValueTask<ReadOnlyMemory<byte>> ReadAsync(CancellationToken cancellationToken) => _owner.ReadAsync(Uri, Change, cancellationToken);
		}

		/// <summary>
		/// Name of the scheme for this source
		/// </summary>
		public const string Scheme = "perforce";

		/// <inheritdoc/>
		string IConfigSource.Scheme => Scheme;

		/// <inheritdoc/>
		public TimeSpan UpdateInterval => TimeSpan.FromMinutes(1.0);

		readonly IOptionsMonitor<ServerSettings> _settings;
		readonly IUserCollection _userCollection;
		readonly IMemoryCache _cache;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public PerforceConfigSource(IOptionsMonitor<ServerSettings> settings, IUserCollection userCollection, IMemoryCache cache, ILogger<PerforceConfigSource> logger)
		{
			_settings = settings;
			_userCollection = userCollection;
			_cache = cache;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task<IConfigFile[]> GetAsync(Uri[] uris, CancellationToken cancellationToken)
		{
			Dictionary<Uri, IConfigFile> results = new Dictionary<Uri, IConfigFile>();
			foreach (IGrouping<string, Uri> group in uris.GroupBy(x => x.Host))
			{
				using (IPerforceConnection perforce = await ConnectAsync(group.Key, cancellationToken))
				{
					FileSpecList fileSpec = group.Select(x => x.AbsolutePath).Distinct(StringComparer.OrdinalIgnoreCase).ToList();

					List<FStatRecord> records = await perforce.FStatAsync(FStatOptions.ShortenOutput, fileSpec, cancellationToken).ToListAsync(cancellationToken);
					records.RemoveAll(x => x.HeadAction == FileAction.Delete || x.HeadAction == FileAction.MoveDelete);

					Dictionary<string, FStatRecord> absolutePathToRecord = records.ToDictionary(x => x.DepotFile ?? String.Empty, x => x, StringComparer.OrdinalIgnoreCase);
					foreach (Uri uri in group)
					{
						FStatRecord? record;
						if (!absolutePathToRecord.TryGetValue(uri.AbsolutePath, out record))
						{
							throw new FileNotFoundException($"Unable to read {uri}. No matching files found.");
						}

						IUser? author = await GetAuthorAsync(perforce, group.Key, record.HeadChange, cancellationToken);
						results[uri] = new ConfigFileImpl(uri, record.HeadChange, author, this);
					}
				}
			}
			return uris.ConvertAll(x => results[x]);
		}

		async ValueTask<IUser?> GetAuthorAsync(IPerforceConnection perforce, string host, int change, CancellationToken cancellationToken)
		{
			string cacheKey = $"{nameof(PerforceConfigSource)}:author:{host}@{change}";
			if (!_cache.TryGetValue(cacheKey, out string? author))
			{
				ChangeRecord record = await perforce.GetChangeAsync(GetChangeOptions.None, change, cancellationToken);
				using (ICacheEntry entry = _cache.CreateEntry(cacheKey))
				{
					entry.SetSlidingExpiration(TimeSpan.FromHours(1.0));
					entry.SetSize(256);
					entry.SetValue(record.User);
				}
			}
			return (author != null) ? await _userCollection.FindUserByLoginAsync(author, cancellationToken) : null;
		}

		async ValueTask<ReadOnlyMemory<byte>> ReadAsync(Uri uri, int change, CancellationToken cancellationToken)
		{
			string cacheKey = $"{nameof(PerforceConfigSource)}:data:{uri}@{change}";
			if (_cache.TryGetValue(cacheKey, out ReadOnlyMemory<byte> data))
			{
				_logger.LogInformation("Read {Uri}@{Change} from cache ({Key})", uri, change, cacheKey);
			}
			else
			{
				_logger.LogInformation("Reading {Uri} at CL {Change} from Perforce", uri, change);
				using (IPerforceConnection perforce = await ConnectAsync(uri.Host, cancellationToken))
				{
					PerforceResponse<PrintRecord<byte[]>> response = await perforce.TryPrintDataAsync($"{uri.AbsolutePath}@{change}", cancellationToken);
					response.EnsureSuccess();
					data = response.Data.Contents!;
				}
				using (ICacheEntry entry = _cache.CreateEntry(cacheKey))
				{
					entry.SetSlidingExpiration(TimeSpan.FromHours(1.0));
					entry.SetSize(data.Length);
					entry.SetValue(data);
				}
			}
			return data;
		}

		async Task<IPerforceConnection> ConnectAsync(string host, CancellationToken cancellationToken)
		{
			_ = cancellationToken;

			ServerSettings settings = _settings.CurrentValue;

			PerforceConnectionId connectionId = new PerforceConnectionId();
			if (!String.IsNullOrEmpty(host))
			{
				connectionId = new PerforceConnectionId(host);
			}

			PerforceConnectionSettings? connectionSettings = settings.Perforce.FirstOrDefault(x => x.Id == connectionId);
			if (connectionSettings == null)
			{
				if (connectionId == PerforceConnectionSettings.Default)
				{
					connectionSettings = new PerforceConnectionSettings();
				}
				else
				{
					throw new InvalidOperationException($"No Perforce connection settings defined for '{connectionId}'.");
				}
			}

			IPerforceConnection connection = await PerforceConnection.CreateAsync(connectionSettings.ToPerforceSettings(), _logger);
			return connection;
		}
	}
}
