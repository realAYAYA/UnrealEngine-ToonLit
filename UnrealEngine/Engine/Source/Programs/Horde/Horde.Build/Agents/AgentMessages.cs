// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using Horde.Build.Acls;
using Horde.Build.Agents;
using EpicGames.Core;
using HordeCommon;
using Horde.Build.Agents.Leases;
using Horde.Build.Agents.Sessions;

namespace Horde.Build.Agents
{
	/// <summary>
	/// Parameters to register a new agent
	/// </summary>
	public class CreateAgentRequest
	{
		/// <summary>
		/// Friendly name for the agent
		/// </summary>
		[Required]
		public string Name { get; set; } = null!; // Enforced by [required] attribute

		/// <summary>
		/// Whether the agent is currently enabled
		/// </summary>
		public bool Enabled { get; set; } = true;

		/// <summary>
		/// Whether the agent is ephemeral (ie. should not be shown when inactive)
		/// </summary>
		public bool Ephemeral { get; set; }

		/// <summary>
		/// Per-agent override for the desired agent software channel
		/// </summary>
		public string? Channel { get; set; }

		/// <summary>
		/// Pools for this agent
		/// </summary>
		public List<string>? Pools { get; set; }
	}

	/// <summary>
	/// Response from creating an agent
	/// </summary>
	public class CreateAgentResponse
	{
		/// <summary>
		/// Unique id for the new agent
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="id">Unique id for this agent</param>
		public CreateAgentResponse(string id)
		{
			Id = id;
		}
	}

	/// <summary>
	/// Parameters to update an agent
	/// </summary>
	public class UpdateAgentRequest
	{
		/// <summary>
		/// Whether the agent is currently enabled
		/// </summary>
		public bool? Enabled { get; set; }

		/// <summary>
		/// Whether the agent is ephemeral
		/// </summary>
		public bool? Ephemeral { get; set; }

		/// <summary>
		/// Request a conform be performed using the current agent set
		/// </summary>
		public bool? RequestConform { get; set; }

		/// <summary>
		/// Request that a full conform be performed, removing all intermediate files
		/// </summary>
		public bool? RequestFullConform { get; set; }

		/// <summary>
		/// Request the machine be restarted
		/// </summary>
		public bool? RequestRestart { get; set; }

		/// <summary>
		/// Request the machine be shut down
		/// </summary>
		public bool? RequestShutdown { get; set; }

		/// <summary>
		/// Per-agent override for the desired agent software channel
		/// </summary>
		public string? Channel { get; set; }

		/// <summary>
		/// Pools for this agent
		/// </summary>
		public List<string>? Pools { get; set; }

		/// <summary>
		/// New ACL for this agent
		/// </summary>
		public UpdateAclRequest? Acl { get; set; }
		
		/// <summary>
		/// New comment
		/// </summary>
		public string? Comment { get; set; }
	}

	/// <summary>
	/// Response for queries to find a particular lease within an agent
	/// </summary>
	public class GetAgentLeaseResponse
	{
		/// <summary>
		/// Identifier for the lease
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// The agent id
		/// </summary>
		public string? AgentId { get; set; }

		/// <summary>
		/// Cost of this agent, per hour
		/// </summary>
		public double? AgentRate { get; set; }

		/// <summary>
		/// Name of the lease
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Log id for this lease
		/// </summary>
		public string? LogId { get; set; }

		/// <summary>
		/// Time at which the lease started (UTC)
		/// </summary>
		public DateTime StartTime { get; set; }

		/// <summary>
		/// Time at which the lease started (UTC)
		/// </summary>
		public DateTime? FinishTime { get; set; }

		/// <summary>
		/// Whether this lease has started executing on the agent yet
		/// </summary>
		public bool Executing { get; set; }

		/// <summary>
		/// Details of the payload being executed
		/// </summary>
		public Dictionary<string, string>? Details { get; set; }

		/// <summary>
		/// Outcome of the lease
		/// </summary>
		public LeaseOutcome? Outcome { get; set; }

		/// <summary>
		/// State of the lease (for AgentLeases)
		/// </summary>
		public LeaseState? State { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="lease">The lease to initialize from</param>
		/// <param name="details">The payload details</param>
		public GetAgentLeaseResponse(AgentLease lease, Dictionary<string, string>? details)
		{
			Id = lease.Id.ToString();
			Name = lease.Name;
			LogId = lease.LogId?.ToString();
			State = lease.State;
			StartTime = lease.StartTime;
			Executing = lease.Active;
			FinishTime = lease.ExpiryTime;
			Details = details;
		}

		/// <summary>
		/// Converts this lease to a response object
		/// </summary>
		/// <param name="lease">The lease to initialize from</param>
		/// <param name="details">The payload details</param>
		/// <param name="agentRate">Rate for running this agent</param>
		public GetAgentLeaseResponse(ILease lease, Dictionary<string, string>? details, double? agentRate)
		{
			Id = lease.Id.ToString();
			AgentId = lease.AgentId.ToString();
			AgentRate = agentRate;
			Name = lease.Name;
			LogId = lease.LogId?.ToString();
			StartTime = lease.StartTime;
			Executing = (lease.FinishTime == null);
			FinishTime = lease.FinishTime;
			Details = details;
			Outcome = lease.Outcome;
		}
	}

	/// <summary>
	/// Information about an agent session
	/// </summary>
	public class GetAgentSessionResponse
	{
		/// <summary>
		/// Unique id for this session
		/// </summary>
		[Required]
		public string Id { get; set; }

		/// <summary>
		/// Start time for this session
		/// </summary>
		[Required]
		public DateTime StartTime { get; set; }

		/// <summary>
		/// Finishing time for this session
		/// </summary>
		public DateTime? FinishTime { get; set; }

		/// <summary>
		/// Properties of this agent
		/// </summary>
		public List<string>? Properties { get; set; }

		/// <summary>
		/// Version of the software running during this session
		/// </summary>
		public string? Version { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="session">The session to construct from</param>
		public GetAgentSessionResponse(ISession session)
		{
			Id = session.Id.ToString();
			StartTime = session.StartTime;
			FinishTime = session.FinishTime;
			Properties = (session.Properties != null) ? new List<string>(session.Properties) : null;
			Version = session.Version?.ToString();
		}
	}

	/// <summary>
	/// Information about a workspace synced on an agent
	/// </summary>
	public class GetAgentWorkspaceResponse
	{
		/// <summary>
		/// The Perforce server and port to connect to
		/// </summary>
		public string? Cluster { get; set; }

		/// <summary>
		/// User to log into Perforce with (eg. buildmachine)
		/// </summary>
		public string? UserName { get; set; }

		/// <summary>
		/// Identifier to distinguish this workspace from other workspaces
		/// </summary>
		public string Identifier { get; set; }

		/// <summary>
		/// The stream to sync
		/// </summary>
		public string Stream { get; set; }

		/// <summary>
		/// Custom view for the workspace
		/// </summary>
		public List<string>? View { get; set; }

		/// <summary>
		/// Whether to use an incremental workspace
		/// </summary>
		public bool BIncremental { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="workspace">The workspace to construct from</param>
		public GetAgentWorkspaceResponse(AgentWorkspace workspace)
		{
			Cluster = workspace.Cluster;
			UserName = workspace.UserName;
			Identifier = workspace.Identifier;
			Stream = workspace.Stream;
			View = workspace.View;
			BIncremental = workspace.BIncremental;
		}
	}

	/// <summary>
	/// Information about an agent
	/// </summary>
	public class GetAgentResponse
	{
		/// <summary>
		/// The agent's unique ID
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Friendly name of the agent
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Whether the agent is currently enabled
		/// </summary>
		public bool Enabled { get; set; }

		/// <summary>
		/// Cost estimate per-hour for this agent
		/// </summary>
		public double? Rate { get; set; }

		/// <summary>
		/// The current session id
		/// </summary>
		public string? SessionId { get; set; }

		/// <summary>
		/// Whether the agent is ephemeral
		/// </summary>
		public bool Ephemeral { get; set; }

		/// <summary>
		/// Whether the agent is currently online
		/// </summary>
		public bool Online { get; set; }

		/// <summary>
		/// Whether this agent has expired
		/// </summary>
		public bool Deleted { get; set; }

		/// <summary>
		/// Whether a conform job is pending
		/// </summary>
		public bool PendingConform { get; set; }

		/// <summary>
		/// Whether a full conform job is pending
		/// </summary>
		public bool PendingFullConform { get; set; }

		/// <summary>
		/// Whether a restart is pending
		/// </summary>
		public bool PendingRestart { get; set; }
		
		/// <summary>
		/// Whether a shutdown is pending
		/// </summary>
		public bool PendingShutdown { get; set; }

		/// <summary>
		/// The reason for the last shutdown
		/// </summary>
		public string LastShutdownReason { get; set; }

		/// <summary>
		/// Last time a conform was attempted
		/// </summary>
		public DateTime LastConformTime { get; set; }

		/// <summary>
		/// Number of times a conform has been attempted
		/// </summary>
		public int? ConformAttemptCount { get; set; }

		/// <summary>
		/// Last time a conform was attempted
		/// </summary>
		public DateTime? NextConformTime { get; set; }

		/// <summary>
		/// The current client version
		/// </summary>
		public string? Version { get; set; }

		/// <summary>
		/// Per-agent override for the desired agent software channel
		/// </summary>
		public string? Channel { get; set; }

		/// <summary>
		/// Properties for the agent
		/// </summary>
		public List<string> Properties { get; set; }

		/// <summary>
		/// Resources for the agent
		/// </summary>
		public Dictionary<string, int> Resources { get; set; }

		/// <summary>
		/// Last update time of this agent
		/// </summary>
		public DateTime? UpdateTime { get; set; }

		/// <summary>
		/// Pools for this agent
		/// </summary>
		public List<string>? Pools { get; set; }

		/// <summary>
		/// List of workspaces currently synced to this machine
		/// </summary>
		public List<GetAgentWorkspaceResponse> Workspaces { get; set; } = new List<GetAgentWorkspaceResponse>();

		/// <summary>
		/// Capabilities of this agent
		/// </summary>
		public object? Capabilities { get; }

		/// <summary>
		/// Array of active leases.
		/// </summary>
		public List<GetAgentLeaseResponse> Leases { get; }

		/// <summary>
		/// Per-object permissions
		/// </summary>
		public GetAclResponse? Acl { get; set; }
		
		/// <summary>
		/// Comment for this agent
		/// </summary>
		public string? Comment { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="agent">The agent to construct from</param>
		/// <param name="leases">Active leases</param>
		/// <param name="rate">Rate for this agent</param>
		/// <param name="bIncludeAcl">Whether to include the ACL in the response</param>
		public GetAgentResponse(IAgent agent, List<GetAgentLeaseResponse> leases, double? rate, bool bIncludeAcl)
		{
			Id = agent.Id.ToString();
			Name = agent.Id.ToString();
			Enabled = agent.Enabled;
			Rate = rate;
			Properties = new List<string>(agent.Properties);
			Resources = new Dictionary<string, int>(agent.Resources);
			SessionId = agent.SessionId?.ToString();
			Online = agent.IsSessionValid(DateTime.UtcNow);
			Deleted = agent.Deleted;
			PendingConform = agent.RequestConform;
			PendingFullConform = agent.RequestFullConform;
			PendingRestart = agent.RequestRestart;
			PendingShutdown = agent.RequestShutdown;
			LastShutdownReason = agent.LastShutdownReason ?? "Unknown";
			LastConformTime = agent.LastConformTime;
			NextConformTime =   agent.LastConformTime;
			ConformAttemptCount = agent.ConformAttemptCount;
			Version = agent.Version?.ToString() ?? "Unknown";
			if(agent.Channel != null)
			{
				Version += $" ({agent.Channel})";
			}
			Version = agent.Version?.ToString();
			Channel = agent.Channel?.ToString();
			UpdateTime = agent.UpdateTime;
			Pools = agent.GetPools().Select(x => x.ToString()).ToList();
			Workspaces = agent.Workspaces.ConvertAll(x => new GetAgentWorkspaceResponse(x));
			Capabilities = new { Devices = new[] { new { agent.Properties, agent.Resources } } };
			Leases = leases;
			Acl = (bIncludeAcl && agent.Acl != null) ? new GetAclResponse(agent.Acl) : null;
			Comment = agent.Comment;
		}
	}
}
