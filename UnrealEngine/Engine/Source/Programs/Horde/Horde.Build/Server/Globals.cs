// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Security.Cryptography;
using EpicGames.Horde.Common;
using EpicGames.Perforce;
using Horde.Build.Acls;
using Horde.Build.Server;
using Horde.Build.Utilities;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;

namespace Horde.Build.Server
{
	/// <summary>
	/// Base class for singleton documents
	/// </summary>
	public abstract class SingletonBase
	{
		/// <summary>
		/// Unique id for this singleton
		/// </summary>
		[BsonId]
		public ObjectId Id { get; set; }

		/// <summary>
		/// The revision index of this document
		/// </summary>
		public int Revision { get; set; }

		/// <summary>
		/// Callback to allow the singleton to fix up itself after being read
		/// </summary>
		public virtual void PostLoad()
		{
		}
	}

	/// <summary>
	/// How frequently the maintence window repeats
	/// </summary>
	public enum ScheduledDowntimeFrequency
	{
		/// <summary>
		/// Once
		/// </summary>
		Once,

		/// <summary>
		/// Every day
		/// </summary>
		Daily,

		/// <summary>
		/// Every week
		/// </summary>
		Weekly,
	}

	/// <summary>
	/// Settings for the maintenance window
	/// </summary>
	public class ScheduledDowntime
	{
		/// <summary>
		/// Start time
		/// </summary>
		public DateTimeOffset StartTime { get; set; }

		/// <summary>
		/// Finish time
		/// </summary>
		public DateTimeOffset FinishTime { get; set; }

		/// <summary>
		/// Frequency that the window repeats
		/// </summary>
		public ScheduledDowntimeFrequency Frequency { get; set; } = ScheduledDowntimeFrequency.Once;

		/// <summary>
		/// Gets the next scheduled downtime
		/// </summary>
		/// <param name="now">The current time</param>
		/// <returns>Start and finish time</returns>
		public (DateTimeOffset StartTime, DateTimeOffset FinishTime) GetNext(DateTimeOffset now)
		{
			TimeSpan offset = TimeSpan.Zero;
			if (Frequency == ScheduledDowntimeFrequency.Daily)
			{
				double days = (now - StartTime).TotalDays;
				if (days >= 1.0)
				{
					days -= days % 1.0;
				}
				offset = TimeSpan.FromDays(days);
			}
			else if (Frequency == ScheduledDowntimeFrequency.Weekly)
			{
				double days = (now - StartTime).TotalDays;
				if (days >= 7.0)
				{
					days -= days % 7.0;
				}
				offset = TimeSpan.FromDays(days);
			}
			return (StartTime + offset, FinishTime + offset);
		}

		/// <summary>
		/// Determines if this schedule is active
		/// </summary>
		/// <param name="now">The current time</param>
		/// <returns>True if downtime is active</returns>
		public bool IsActive(DateTimeOffset now)
		{
			if (Frequency == ScheduledDowntimeFrequency.Once)
			{
				return now >= StartTime && now < FinishTime;
			}
			else if (Frequency == ScheduledDowntimeFrequency.Daily)
			{
				double days = (now - StartTime).TotalDays;
				if (days < 0.0)
				{
					return false;
				}
				else
				{
					return (days % 1.0) < (FinishTime - StartTime).TotalDays;
				}
			}
			else if (Frequency == ScheduledDowntimeFrequency.Weekly)
			{
				double days = (now - StartTime).TotalDays;
				if(days < 0.0)
				{
					return false;
				}
				else
				{
					return (days % 7.0) < (FinishTime - StartTime).TotalDays;
				}
			}
			else
			{
				return false;
			}
		}
	}

	/// <summary>
	/// Path to a platform and stream to use for syncing AutoSDK
	/// </summary>
	public class AutoSdkWorkspace
	{
		/// <summary>
		/// Name of this workspace
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// The agent properties to check (eg. "OSFamily=Windows")
		/// </summary>
		public List<string> Properties { get; set; } = new List<string>();

		/// <summary>
		/// Username for logging in to the server
		/// </summary>
		public string? UserName { get; set; }

		/// <summary>
		/// Stream to use
		/// </summary>
		[Required]
		public string? Stream { get; set; }
	}

	/// <summary>
	/// Information about an individual Perforce server
	/// </summary>
	public class PerforceServer
	{
		/// <summary>
		/// The server and port. The server may be a DNS entry with multiple records, in which case it will be actively load balanced.
		/// </summary>
		public string ServerAndPort { get; set; } = "perforce:1666";

		/// <summary>
		/// Whether to query the healthcheck address under each server
		/// </summary>
		public bool HealthCheck { get; set; }

		/// <summary>
		/// Whether to resolve the DNS entries and load balance between different hosts
		/// </summary>
		public bool ResolveDns { get; set; }

		/// <summary>
		/// Maximum number of simultaneous conforms on this server
		/// </summary>
		public int MaxConformCount { get; set; }

		/// <summary>
		/// Optional condition for a machine to be eligable to use this server
		/// </summary>
		[BsonIgnoreIfNull]
		public Condition? Condition { get; set; }

		/// <summary>
		/// List of properties for an agent to be eligable to use this server
		/// </summary>
		[BsonIgnoreIfNull]
		public List<string>? Properties { get; set; }
	}

	/// <summary>
	/// Credentials for a Perforce user
	/// </summary>
	public class PerforceCredentials
	{
		/// <summary>
		/// The username
		/// </summary>
		public string UserName { get; set; } = String.Empty;

		/// <summary>
		/// Password for the user
		/// </summary>
		public string Password { get; set; } = String.Empty;
	}

	/// <summary>
	/// Information about a cluster of Perforce servers. 
	/// </summary>
	public class PerforceCluster
	{
		/// <summary>
		/// The default cluster name
		/// </summary>
		public const string DefaultName = "Default";

		/// <summary>
		/// Name of the cluster
		/// </summary>
		[Required]
		public string Name { get; set; } = null!;

		/// <summary>
		/// Username for Horde to log in to this server. Will use the default user if not set.
		/// </summary>
		public string? ServiceAccount { get; set; }

		/// <summary>
		/// Whether the service account can impersonate other users
		/// </summary>
		public bool CanImpersonate { get; set; } = true;

		/// <summary>
		/// List of servers
		/// </summary>
		public List<PerforceServer> Servers { get; set; } = new List<PerforceServer>();

		/// <summary>
		/// List of server credentials
		/// </summary>
		public List<PerforceCredentials> Credentials { get; set; } = new List<PerforceCredentials>();

		/// <summary>
		/// List of autosdk streams
		/// </summary>
		public List<AutoSdkWorkspace> AutoSdk { get; set; } = new List<AutoSdkWorkspace>();
	}

	/// <summary>
	/// Global server settings
	/// </summary>
	[SingletonDocument("5e3981cb28b8ec59cd07184a")]
	public class Globals : SingletonBase
	{
		/// <summary>
		/// The unique id for all globals objects
		/// </summary>
		public static ObjectId StaticId { get; } = new ObjectId("5e3981cb28b8ec59cd07184a");

		/// <summary>
		/// Unique instance id of this database
		/// </summary>
		public ObjectId InstanceId { get; set; }

		/// <summary>
		/// The config revision
		/// </summary>
		public string? ConfigRevision { get; set; }

		/// <summary>
		/// List of Perforce clusters
		/// </summary>
		public List<PerforceCluster> PerforceClusters { get; set; } = new List<PerforceCluster>();

		/// <summary>
		/// Maximum number of simultaneous conforms
		/// </summary>
		public int MaxConformCount { get; set; }

		/// <summary>
		/// The root ACL
		/// </summary>
		public Acl? RootAcl { get; set; }

		/// <summary>
		/// Randomly generated JWT signing key
		/// </summary>
		public byte[]? JwtSigningKey { get; set; }

		/// <summary>
		/// The current schema version
		/// </summary>
		public int? SchemaVersion { get; set; }

		/// <summary>
		/// The scheduled downtime
		/// </summary>
		public List<ScheduledDowntime> ScheduledDowntime { get; set; } = new List<ScheduledDowntime>();

		/// <summary>
		/// List of compute profiles
		/// </summary>
		public List<ComputeClusterConfig> ComputeClusters { get; set; } = new List<ComputeClusterConfig>();

		/// <summary>
		/// Private constructor for serialization
		/// </summary>
		public Globals()
		{
			InstanceId = ObjectId.GenerateNewId();
		}

		/// <summary>
		/// Rotate the signing key
		/// </summary>
		public void RotateSigningKey()
		{
			JwtSigningKey = RandomNumberGenerator.GetBytes(128);
		}

		/// <summary>
		/// Finds a perforce cluster with the given name or that contains the provided server
		/// </summary>
		/// <param name="name">Name of the cluster</param>
		/// <param name="serverAndPort">Find cluster which contains server</param>
		/// <returns></returns>
		public PerforceCluster? FindPerforceCluster(string? name, string? serverAndPort = null)
		{
			List<PerforceCluster> clusters = PerforceClusters;

			if (serverAndPort != null)
			{
				return clusters.FirstOrDefault(x => x.Servers.FirstOrDefault( server => String.Equals(server.ServerAndPort, serverAndPort, StringComparison.OrdinalIgnoreCase)) != null);
			}

			if (clusters.Count == 0)
			{
				clusters = DefaultClusters;
			}

			if (name == null)
			{
				return clusters.FirstOrDefault();
			}
			else
			{
				return clusters.FirstOrDefault(x => String.Equals(x.Name, name, StringComparison.OrdinalIgnoreCase));
			}
		}

		static List<PerforceCluster> DefaultClusters { get; } = GetDefaultClusters();

		static List<PerforceCluster> GetDefaultClusters()
		{
			PerforceServer server = new PerforceServer();
			server.ServerAndPort = PerforceSettings.Default.ServerAndPort;

			PerforceCluster cluster = new PerforceCluster();
			cluster.Name = "Default";
			cluster.CanImpersonate = false;
			cluster.Servers.Add(server);

			return new List<PerforceCluster> { cluster };
		}
	}
}
