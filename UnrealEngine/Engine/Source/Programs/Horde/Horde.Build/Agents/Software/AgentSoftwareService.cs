// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Build.Commands;
using Horde.Build.Server;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Agents.Software
{
	using AgentSoftwareChannelName = StringId<AgentSoftwareChannels>;

	/// <summary>
	/// Information about a channel
	/// </summary>
	public class AgentSoftwareChannel : IAgentSoftwareChannel
	{
		/// <summary>
		/// The channel id
		/// </summary>
		public AgentSoftwareChannelName Name { get; set; }

		/// <summary>
		/// Name of the user that made the last modification
		/// </summary>
		public string? ModifiedBy { get; set; }

		/// <summary>
		/// Last modification time
		/// </summary>
		public DateTime ModifiedTime { get; set; }

		/// <summary>
		/// The software revision number
		/// </summary>
		public string Version { get; set; } = String.Empty;

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		[BsonConstructor]
		private AgentSoftwareChannel()
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name of the channel</param>
		public AgentSoftwareChannel(AgentSoftwareChannelName name)
		{
			Name = name;
		}
	}

	/// <summary>
	/// Singleton document used to track different versions of the agent software
	/// </summary>
	[SingletonDocument("5f455039d97900f2b6c735a9")]
	public class AgentSoftwareChannels : SingletonBase
	{
		/// <summary>
		/// The next version number
		/// </summary>
		public int NextVersion { get; set; } = 1;

		/// <summary>
		/// List of channels
		/// </summary>
		public List<AgentSoftwareChannel> Channels { get; set; } = new List<AgentSoftwareChannel>();

		/// <summary>
		/// Finds an existing channel by the given name, or adds a new one
		/// </summary>
		/// <param name="name"></param>
		/// <returns></returns>
		public AgentSoftwareChannel FindOrAddChannel(AgentSoftwareChannelName name)
		{
			AgentSoftwareChannel? channel = Channels.FirstOrDefault(x => x.Name == name);
			if (channel == null)
			{
				channel = new AgentSoftwareChannel(name);
				Channels.Add(channel);
			}
			return channel;
		}
	}

	/// <summary>
	/// Wrapper for a collection of software revisions. 
	/// </summary>
	public class AgentSoftwareService
	{
		/// <summary>
		/// Name of the default channel
		/// </summary>
		public static AgentSoftwareChannelName DefaultChannelName { get; } = new AgentSoftwareChannelName("default");

		/// <summary>
		/// Collection of software documents
		/// </summary>
		readonly IAgentSoftwareCollection _collection;

		/// <summary>
		/// Channels singleton
		/// </summary>
		readonly ISingletonDocument<AgentSoftwareChannels> _singleton;

		/// <summary>
		/// Cached copy of the channels singleton
		/// </summary>
		readonly LazyCachedValue<Task<AgentSoftwareChannels>> _channelsDocument;

		/// <summary>
		/// The server settings
		/// </summary>
		readonly IOptionsMonitor<ServerSettings> _settings;

		/// <summary>
		///  Logger for controller
		/// </summary>
		private readonly ILogger<AgentSoftwareService> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="collection">The software collection</param>
		/// <param name="singleton">The channels singleton</param>		
		/// <param name="settings">The settings monitor</param>		
		/// <param name="logger">The logger instance</param>
		public AgentSoftwareService(IAgentSoftwareCollection collection, ISingletonDocument<AgentSoftwareChannels> singleton, IOptionsMonitor<ServerSettings> settings, ILogger<AgentSoftwareService> logger)
		{
			_collection = collection;
			_singleton = singleton;
			_settings = settings;
			_logger = logger;

			_channelsDocument = new LazyCachedValue<Task<AgentSoftwareChannels>>(() => singleton.GetAsync(), TimeSpan.FromSeconds(20.0));

			Task.Run(() => RegisterDefaultAgent(5000));
		}

		/// <summary>
		/// Finds all software archives matching a set of criteria
		/// </summary>
		public async Task<List<IAgentSoftwareChannel>> FindChannelsAsync()
		{
			AgentSoftwareChannels current = await _channelsDocument.GetLatest();
			return current.Channels.ConvertAll<IAgentSoftwareChannel>(x => x);
		}

		/// <summary>
		/// Gets a single software archive
		/// </summary>
		/// <param name="name">Unique id of the channel to find software for</param>
		public async Task<IAgentSoftwareChannel?> GetChannelAsync(AgentSoftwareChannelName name)
		{
			AgentSoftwareChannels current = await _channelsDocument.GetLatest();
			return current.Channels.FirstOrDefault(x => x.Name == name);
		}

		/// <summary>
		/// Gets a cached channel name
		/// </summary>
		/// <param name="name">The cached channel name</param>
		/// <returns></returns>
		public async Task<IAgentSoftwareChannel?> GetCachedChannelAsync(AgentSoftwareChannelName name)
		{
			AgentSoftwareChannels document = await _channelsDocument.GetCached();
			return document.Channels.FirstOrDefault(x => x.Name == name);
		}

		/// <summary>
		/// Removes a channel
		/// </summary>
		/// <param name="name">The channel id</param>
		/// <returns>Async task</returns>
		public async Task DeleteChannelAsync(AgentSoftwareChannelName name)
		{
			for (; ; )
			{
				AgentSoftwareChannels current = await _singleton.GetAsync();

				int channelIdx = current.Channels.FindIndex(x => x.Name == name);
				if (channelIdx == -1)
				{
					break;
				}

				string version = current.Channels[channelIdx].Version;
				current.Channels.RemoveAt(channelIdx);

				if (await _singleton.TryUpdateAsync(current))
				{
					if (!current.Channels.Any(x => x.Version == version))
					{
						await _collection.RemoveAsync(version);
					}
					break;
				}
			}
		}

		/// <summary>
		/// Updates a new software revision
		/// </summary>
		/// <param name="name">Name of the channel</param>
		/// <param name="author">Name of the user uploading this file</param>
		/// <param name="data">The input data stream. This should be a zip archive containing the HordeAgent executable.</param>
		/// <returns>Unique id for the file</returns>
		public async Task<string> SetArchiveAsync(AgentSoftwareChannelName name, string? author, byte[] data)
		{
			// Upload the software
			string version = AgentUtilities.ReadVersion(data);
			await _collection.AddAsync(version, data);

			// Update the channel
			for(; ;)
			{
				AgentSoftwareChannels instance = await _singleton.GetAsync();

				AgentSoftwareChannel channel = instance.FindOrAddChannel(name);
				channel.ModifiedBy = author;
				channel.ModifiedTime = DateTime.UtcNow;
				channel.Version = version;

				if (await _singleton.TryUpdateAsync(instance))
				{
					break;
				}
			}
			return version;
		}

		/// <summary>
		/// Gets the zip file for a given channel
		/// </summary>
		/// <param name="name">The channel name</param>
		/// <returns>Data for the given archive</returns>
		public async Task<byte[]?> GetArchiveAsync(AgentSoftwareChannelName name)
		{
			AgentSoftwareChannels instance = await _singleton.GetAsync();

			AgentSoftwareChannel? channel = instance.Channels.FirstOrDefault(x => x.Name == name);
			if (channel == null)
			{
				return null;
			}
			else
			{
				return await _collection.GetAsync(channel.Version);
			}
		}

		/// <summary>
		/// Gets the zip file with a given version number
		/// </summary>
		/// <param name="version">The version</param>
		/// <returns>Data for the given archive</returns>
		public Task<byte[]?> GetArchiveAsync(string version)
		{
			return _collection.GetAsync(version);
		}
		async Task RegisterDefaultAgent(int delayMs)
		{
			await Task.Delay(delayMs);

			// Check whether we have an installed agent zip
			FileReference agentZip = FileReference.Combine(Program.AppDir, "DefaultAgent/Agent.zip");
			if (!_settings.CurrentValue.SingleInstance || !FileReference.Exists(agentZip))
			{
				return;
			}

			_logger.LogInformation("Checking for default agent software update");

			string agentHash = ContentHash.MD5(agentZip).ToString();

			FileReference agentHashFile = FileReference.Combine(Program.DataDir, "Agent/DefaultAgentHash");

			try
			{

				if (FileReference.Exists(agentHashFile) && FileReference.ReadAllText(agentHashFile).Trim() == agentHash)
				{
					_logger.LogInformation("Default agent software is up to date");
					return;
				}
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Error checking default agent software, {Message}", ex.Message);
			}

            byte[] bytes = await File.ReadAllBytesAsync(agentZip.ToString());

			using X509Certificate2? grpcCertificate = ServerCommand.ReadGrpcCertificate(_settings.CurrentValue);

			if (grpcCertificate == null)
			{
				throw new Exception("Unable to register default agent without valid grpc certicate");
			}

			// construct agent app settings
			
			string profileName = "Default";

			string serverUrl = $"https://{System.Net.Dns.GetHostName()}:{_settings.CurrentValue.HttpsPort}";			

			Dictionary<string, object> defaultProfile = new Dictionary<string, object>() { { "Name", profileName }, { "Environment", "prod" }, { "Thumbprint", grpcCertificate.Thumbprint }, { "Url", serverUrl } };

			List<object> serverProfiles = new List<object>() { defaultProfile };

			Dictionary<string, object> agentSettings = new Dictionary<string, object>() { { "ServerProfiles", serverProfiles }, { "Server", profileName }};

			bytes = AgentUtilities.UpdateAppSettings(bytes, agentSettings);

			string version = await SetArchiveAsync(new AgentSoftwareChannelName("default"), null, bytes);

			if (!DirectoryReference.Exists(agentHashFile.Directory))
			{
				DirectoryReference.CreateDirectory(agentHashFile.Directory);
			}

			await FileReference.WriteAllTextAsync(agentHashFile, agentHash);

			_logger.LogInformation("Updated default agent software to {AgentHash}", agentHash);

		}
	}
}
