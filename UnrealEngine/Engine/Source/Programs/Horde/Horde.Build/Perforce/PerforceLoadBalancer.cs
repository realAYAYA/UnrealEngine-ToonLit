// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Common;
using Google.Protobuf.WellKnownTypes;
using Horde.Build.Agents;
using Horde.Build.Agents.Leases;
using Horde.Build.Server;
using Horde.Build.Utilities;
using HordeCommon;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Perforce
{
	/// <summary>
	/// Health of a particular server
	/// </summary>
	public enum PerforceServerStatus
	{
		/// <summary>
		/// Server could not be reached
		/// </summary>
		Unknown,

		/// <summary>
		/// Bad. Do not use.
		/// </summary>
		Unhealthy,

		/// <summary>
		/// Degraded, but functioning
		/// </summary>
		Degraded,

		/// <summary>
		/// Good
		/// </summary>
		Healthy,
	}

	/// <summary>
	/// Perforce server infomation
	/// </summary>
	public interface IPerforceServer
	{
		/// <summary>
		/// The server and port
		/// </summary>
		public string ServerAndPort { get; }

		/// <summary>
		/// The server and port before doing DNS resolution
		/// </summary>
		public string BaseServerAndPort { get; }

		/// <summary>
		/// The cluster this server belongs to
		/// </summary>
		public string Cluster { get; }

		/// <summary>
		/// Current status
		/// </summary>
		public PerforceServerStatus Status { get; }

		/// <summary>
		/// Number of leases using this server
		/// </summary>
		public int NumLeases { get; }

		/// <summary>
		/// Error message related to this server
		/// </summary>
		public string? Detail { get; }

		/// <summary>
		/// Last update time for this server
		/// </summary>
		public DateTime? LastUpdateTime { get; }
	}

	/// <summary>
	/// Load balancer for Perforce edge servers
	/// </summary>
	public sealed class PerforceLoadBalancer : IHostedService, IDisposable
	{
		/// <summary>
		/// Information about a resolved Perforce server
		/// </summary>
		class PerforceServerEntry : IPerforceServer
		{
			public string ServerAndPort { get; set; } = String.Empty;
			public string BaseServerAndPort { get; set; } = String.Empty;
			public string? HealthCheckUrl { get; set; }
			public string Cluster { get; set; } = String.Empty;
			public PerforceServerStatus Status { get; set; }
			public string? Detail { get; set; }
			public int NumLeases { get; set; }
			public DateTime? LastUpdateTime { get; set; }

			[BsonConstructor]
			private PerforceServerEntry()
			{
			}

			public PerforceServerEntry(string serverAndPort, string baseServerAndPort, string? healthCheckUrl, string cluster, PerforceServerStatus status, string? detail, DateTime? lastUpdateTime)
			{
				ServerAndPort = serverAndPort;
				BaseServerAndPort = baseServerAndPort;
				HealthCheckUrl = healthCheckUrl;
				Cluster = cluster;
				Status = status;
				Detail = detail;
				LastUpdateTime = lastUpdateTime;
			}
		}

		/// <summary>
		/// 
		/// </summary>
		[SingletonDocument("6046aec374a9283100967ee7")]
		class PerforceServerList : SingletonBase
		{
			public List<PerforceServerEntry> Servers { get; set; } = new List<PerforceServerEntry>();
		}

		readonly MongoService _mongoService;
		readonly ILeaseCollection _leaseCollection;
		readonly SingletonDocument<PerforceServerList> _serverListSingleton;
		readonly Random _random = new Random();
		readonly ILogger _logger;
		readonly ITicker _ticker;

		/// <summary>
		/// Constructor
		/// </summary>
		public PerforceLoadBalancer(MongoService mongoService, ILeaseCollection leaseCollection, IClock clock, ILogger<PerforceLoadBalancer> logger)
		{
			_mongoService = mongoService;
			_leaseCollection = leaseCollection;
			_serverListSingleton = new SingletonDocument<PerforceServerList>(mongoService);
			_logger = logger;
			if (mongoService.ReadOnlyMode)
			{
				_ticker = new NullTicker();
			}
			else
			{
				_ticker = clock.AddSharedTicker<PerforceLoadBalancer>(TimeSpan.FromMinutes(1.0), TickInternalAsync, logger);
			}
		}

		/// <inheritdoc/>
		public Task StartAsync(CancellationToken cancellationToken) => _ticker.StartAsync();

		/// <inheritdoc/>
		public Task StopAsync(CancellationToken cancellationToken) => _ticker.StopAsync();

		/// <inheritdoc/>
		public void Dispose() => _ticker.Dispose();

		/// <summary>
		/// Get the current server list
		/// </summary>
		/// <returns></returns>
		async Task<PerforceServerList> GetServerListAsync()
		{
			PerforceServerList serverList = await _serverListSingleton.GetAsync();

			DateTime minLastUpdateTime = DateTime.UtcNow - TimeSpan.FromMinutes(2.5);
			foreach (PerforceServerEntry server in serverList.Servers)
			{
				if (server.Status == PerforceServerStatus.Healthy && server.LastUpdateTime != null && server.LastUpdateTime.Value < minLastUpdateTime)
				{
					server.Status = PerforceServerStatus.Degraded;
					server.Detail = "Server has not responded to health check";
				}
			}

			return serverList;
		}

		/// <summary>
		/// Get the perforce servers
		/// </summary>
		/// <returns></returns>
		public async Task<List<IPerforceServer>> GetServersAsync()
		{
			PerforceServerList serverList = await GetServerListAsync();
			return serverList.Servers.ConvertAll<IPerforceServer>(x => x);
		}

		/// <summary>
		/// Allocates a server for use by a lease
		/// </summary>
		/// <returns>The server to use. Null if there is no healthy server available.</returns>
		public async Task<IPerforceServer?> GetServer(string cluster)
		{
			PerforceServerList serverList = await GetServerListAsync();

			List<PerforceServerEntry> candidates = serverList.Servers.Where(x => x.Cluster == cluster && x.Status >= PerforceServerStatus.Healthy).ToList();
			if(candidates.Count == 0)
			{
				int idx = _random.Next(0, candidates.Count);
				return candidates[idx];
			}
			return null;
		}

		/// <summary>
		/// Select a Perforce server to use by the Horde server
		/// </summary>
		/// <param name="cluster"></param>
		/// <returns></returns>
		public Task<IPerforceServer?> SelectServerAsync(PerforceCluster cluster)
		{
			List<string> properties = new List<string>{ "HordeServer=1" };
			return SelectServerAsync(cluster, properties);
		}

		/// <summary>
		/// Select a Perforce server to use
		/// </summary>
		/// <param name="cluster"></param>
		/// <param name="agent"></param>
		/// <returns></returns>
		public Task<IPerforceServer?> SelectServerAsync(PerforceCluster cluster, IAgent agent)
		{
			return SelectServerAsync(cluster, agent.Properties);
		}

		/// <summary>
		/// Evaluate a server condition using standard Horde syntax
		/// </summary>
		static bool EvaluateCondition(Condition? condition, IReadOnlyList<string> properties)
		{
			if (condition == null)
			{
				return false;
			}
			if (condition.IsEmpty())
			{
				return false;
			}
			if (properties == null)
			{
				throw new ArgumentException(nameof(properties));
			}
			return condition.Evaluate(x => GetPropertyValues(properties, x));
		}

		/// <summary>
		/// Get property values from a list starting with the given key
		/// </summary>
		static IEnumerable<string> GetPropertyValues(IReadOnlyList<string> properties, string name)
		{
			foreach (string property in properties)
			{
				if (property.Length > name.Length && property[name.Length] == '=' && property.StartsWith(name, StringComparison.OrdinalIgnoreCase))
				{
					yield return property.Substring(name.Length + 1);
				}
			}
		}

		/// <summary>
		/// Select a Perforce server to use
		/// </summary>
		/// <param name="cluster"></param>
		/// <param name="properties"></param>
		/// <returns></returns>
		public async Task<IPerforceServer?> SelectServerAsync(PerforceCluster cluster, IReadOnlyList<string> properties)
		{
			// Find all the valid servers for this agent
			List<PerforceServer> validServers = new List<PerforceServer>();
			if (properties != null)
			{
				validServers.AddRange(cluster.Servers.Where(x => (x.Properties != null && x.Properties.Count > 0 && x.Properties.All(y => properties.Contains(y))) || EvaluateCondition(x.Condition, properties)));
			}
			if (validServers.Count == 0)
			{
				validServers.AddRange(cluster.Servers.Where(x => (x.Properties == null || x.Properties.Count == 0) && (x.Condition == null || x.Condition.IsEmpty())));
			}

			HashSet<string> validServerNames = new HashSet<string>(validServers.Select(x => x.ServerAndPort), StringComparer.OrdinalIgnoreCase);

			// Find all the matching servers.
			PerforceServerList serverList = await GetServerListAsync();

			List<PerforceServerEntry> candidates = serverList.Servers.Where(x => x.Cluster == cluster.Name && validServerNames.Contains(x.BaseServerAndPort)).ToList();
			if (candidates.Count == 0)
			{
				foreach (PerforceServer validServer in validServers)
				{
					_logger.LogDebug("Fetching server info for {ServerAndPort}", validServer.ServerAndPort);
					await UpdateServerAsync(cluster, validServer, candidates);
				}
				if (candidates.Count == 0)
				{
					_logger.LogWarning("Unable to resolve any Perforce servers from valid list");
					return null; 
				}
			}

			// Remove any servers that are unhealthy
			if (candidates.Any(x => x.Status == PerforceServerStatus.Healthy))
			{
				candidates.RemoveAll(x => x.Status != PerforceServerStatus.Healthy);
			}
			else
			{
				candidates.RemoveAll(x => x.Status != PerforceServerStatus.Unknown);
			}

			if (candidates.Count == 0)
			{
				_logger.LogWarning("Unable to find any healthy Perforce server in cluster {ClusterName}", cluster.Name);
				return null;
			}

			// Select which server to use with a weighted average of the number of active leases
			int index = 0;
			if (candidates.Count > 1)
			{
				const int BaseWeight = 20;

				int totalLeases = candidates.Sum(x => x.NumLeases);
				int totalWeight = candidates.Sum(x => (totalLeases + BaseWeight) - x.NumLeases);

				int weight = _random.Next(totalWeight);
				for (; index + 1 < candidates.Count; index++)
				{
					weight -= (totalLeases + BaseWeight) - candidates[index].NumLeases;
					if(weight < 0)
					{
						break;
					}
				}
			}

			return candidates[index];
		}

		/// <inheritdoc/>
		async ValueTask TickInternalAsync(CancellationToken cancellationToken)
		{
			Globals globals = await _mongoService.GetGlobalsAsync();

			// Set of new server entries
			List<PerforceServerEntry> newServers = new List<PerforceServerEntry>();

			// Update the state of all the valid servers
			foreach (PerforceCluster cluster in globals.PerforceClusters)
			{
				foreach (PerforceServer server in cluster.Servers)
				{
					try
					{
						await UpdateServerAsync(cluster, server, newServers);
					}
					catch (Exception ex)
					{
						_logger.LogError(ex, "Exception while updating Perforce server status");
					}
				}
			}

			// Update the number of leases for each entry
			List<PerforceServerEntry> newEntries = newServers.OrderBy(x => x.Cluster).ThenBy(x => x.BaseServerAndPort).ThenBy(x => x.ServerAndPort).ToList();
			await UpdateLeaseCounts(newEntries);
			PerforceServerList list = await _serverListSingleton.UpdateAsync(list => MergeServerList(list, newEntries));

			// Now update the health of each entry
			List<Task> tasks = new List<Task>();
			foreach (PerforceServerEntry entry in list.Servers)
			{
				tasks.Add(Task.Run(() => UpdateHealthAsync(entry), CancellationToken.None));
			}
			await Task.WhenAll(tasks);
		}

		static void MergeServerList(PerforceServerList serverList, List<PerforceServerEntry> newEntries)
		{
			Dictionary<string, PerforceServerEntry> existingEntries = new Dictionary<string, PerforceServerEntry>(StringComparer.OrdinalIgnoreCase);
			foreach (PerforceServerEntry existingEntry in serverList.Servers)
			{
				existingEntries[existingEntry.ServerAndPort] = existingEntry;
			}
			foreach (PerforceServerEntry newEntry in newEntries)
			{
				PerforceServerEntry? existingEntry;
				if (existingEntries.TryGetValue(newEntry.ServerAndPort, out existingEntry))
				{
					newEntry.Status = existingEntry.Status;
					newEntry.Detail = existingEntry.Detail;
					newEntry.LastUpdateTime = existingEntry.LastUpdateTime;
				}
			}
			serverList.Servers = newEntries;
		}

		async Task UpdateLeaseCounts(IEnumerable<PerforceServerEntry> newServerEntries)
		{
			Dictionary<string, Dictionary<string, PerforceServerEntry>> newServerLookup = new Dictionary<string, Dictionary<string, PerforceServerEntry>>();
			foreach (PerforceServerEntry newServerEntry in newServerEntries)
			{
				if (newServerEntry.Cluster != null)
				{
					Dictionary<string, PerforceServerEntry>? clusterServers;
					if (!newServerLookup.TryGetValue(newServerEntry.Cluster, out clusterServers))
					{
						clusterServers = new Dictionary<string, PerforceServerEntry>();
						newServerLookup.Add(newServerEntry.Cluster, clusterServers);
					}
					clusterServers[newServerEntry.ServerAndPort] = newServerEntry;
				}
			}

			List<ILease> leases = await _leaseCollection.FindActiveLeasesAsync();
			foreach (ILease lease in leases)
			{
				Any any = Any.Parser.ParseFrom(lease.Payload.ToArray());

				HashSet<(string, string)> servers = new HashSet<(string, string)>();
				if (any.TryUnpack(out ConformTask conformTask))
				{
					foreach (HordeCommon.Rpc.Messages.AgentWorkspace workspace in conformTask.Workspaces)
					{
						servers.Add((workspace.Cluster, workspace.ServerAndPort));
					}
				}
				if (any.TryUnpack(out ExecuteJobTask executeJobTask))
				{
					if (executeJobTask.AutoSdkWorkspace != null)
					{
						servers.Add((executeJobTask.AutoSdkWorkspace.Cluster, executeJobTask.AutoSdkWorkspace.ServerAndPort));
					}
					if (executeJobTask.Workspace != null)
					{
						servers.Add((executeJobTask.Workspace.Cluster, executeJobTask.Workspace.ServerAndPort));
					}
				}

				foreach ((string cluster, string serverAndPort) in servers)
				{
					IncrementLeaseCount(cluster, serverAndPort, newServerLookup);
				}
			}
		}

		static void IncrementLeaseCount(string cluster, string serverAndPort, Dictionary<string, Dictionary<string, PerforceServerEntry>> newServers)
		{
			Dictionary<string, PerforceServerEntry>? clusterServers;
			if (cluster != null && newServers.TryGetValue(cluster, out clusterServers))
			{
				PerforceServerEntry? entry;
				if (clusterServers.TryGetValue(serverAndPort, out entry))
				{
					entry.NumLeases++;
				}
			}
		}

		async Task UpdateServerAsync(PerforceCluster cluster, PerforceServer server, List<PerforceServerEntry> newServers)
		{
			string initialHostName = server.ServerAndPort;
			int port = 1666;

			int portIdx = initialHostName.LastIndexOf(':');
			if (portIdx != -1)
			{
				port = Int32.Parse(initialHostName.Substring(portIdx + 1), NumberStyles.Integer, CultureInfo.InvariantCulture);
				initialHostName = initialHostName.Substring(0, portIdx);
			}

			List<string> hostNames = new List<string>();
			if (!server.ResolveDns || IPAddress.TryParse(initialHostName, out IPAddress? _))
			{
				hostNames.Add(initialHostName);
			}
			else
			{
				await ResolveServersAsync(initialHostName, hostNames);
			}

			foreach (string hostName in hostNames)
			{
				string? healthCheckUrl = null;
				if (server.HealthCheck)
				{
					healthCheckUrl = $"http://{hostName}:5000/healthcheck";
				}
				newServers.Add(new PerforceServerEntry($"{hostName}:{port}", server.ServerAndPort, healthCheckUrl, cluster.Name, PerforceServerStatus.Unknown, "", DateTime.UtcNow));
			}
		}

		async Task ResolveServersAsync(string hostName, List<string> hostNames)
		{
			// Find all the addresses of the hosts
			IPHostEntry entry = await Dns.GetHostEntryAsync(hostName);
			foreach (IPAddress address in entry.AddressList)
			{
				try
				{
					hostNames.Add(address.ToString());
				}
				catch (Exception ex)
				{
					_logger.LogDebug(ex, "Unable to resolve host name for Perforce server {Address}", address);
				}
			}
		}

		/// <summary>
		/// Updates 
		/// </summary>
		/// <param name="entry"></param>
		/// <returns></returns>
		async Task UpdateHealthAsync(PerforceServerEntry entry)
		{
			DateTime? updateTime = null;
			string detail = "Health check disabled";

			// Get the health of the server
			PerforceServerStatus health = PerforceServerStatus.Healthy;
			if (entry.HealthCheckUrl != null)
			{
				updateTime = DateTime.UtcNow;
				Uri healthCheckUrl = new Uri(entry.HealthCheckUrl);
				try
				{
					(health, detail) = await GetServerHealthAsync(healthCheckUrl);
				}
				catch
				{
					(health, detail) = (PerforceServerStatus.Unhealthy, $"Failed to query status at {healthCheckUrl}");
				}
			}

			// Update the server record
			if (health != entry.Status || detail != entry.Detail || updateTime != entry.LastUpdateTime)
			{
				await _serverListSingleton.UpdateAsync(x => UpdateHealth(x, entry.ServerAndPort, health, detail, updateTime));
			}
		}

		static void UpdateHealth(PerforceServerList serverList, string serverAndPort, PerforceServerStatus status, string detail, DateTime? updateTime)
		{
			PerforceServerEntry? entry = serverList.Servers.FirstOrDefault(x => x.ServerAndPort.Equals(serverAndPort, StringComparison.Ordinal));
			if (entry != null)
			{
				if (entry.LastUpdateTime == null || updateTime == null || entry.LastUpdateTime.Value < updateTime.Value)
				{
					entry.Status = status;
					entry.Detail = detail;
					entry.LastUpdateTime = updateTime;
				}
			}
		}

		static async Task<(PerforceServerStatus, string)> GetServerHealthAsync(Uri healthCheckUrl)
		{
			using HttpClient client = new HttpClient();
			HttpResponseMessage response = await client.GetAsync(healthCheckUrl);

			byte[] data = await response.Content.ReadAsByteArrayAsync();
			JsonDocument document = JsonDocument.Parse(data);

			foreach (JsonElement element in document.RootElement.GetProperty("results").EnumerateArray())
			{
				if (element.TryGetProperty("checker", out JsonElement checker) && checker.ValueEquals("edge_traffic_lights"))
				{
					if (element.TryGetProperty("output", out JsonElement output))
					{
						string status = output.GetString() ?? String.Empty;
						switch(status)
						{
							case "green":
								return (PerforceServerStatus.Healthy, "Server is healthy");
							case "yellow":
								return (PerforceServerStatus.Degraded, "Degraded service");
							case "red":
								return (PerforceServerStatus.Unhealthy, "Server is being drained");
							default:
								return (PerforceServerStatus.Unknown, $"Expected state for health check ({status})");
						}
					}
				}
			}

			return (PerforceServerStatus.Unknown, "Unable to parse health check output");
		}
	}
}
