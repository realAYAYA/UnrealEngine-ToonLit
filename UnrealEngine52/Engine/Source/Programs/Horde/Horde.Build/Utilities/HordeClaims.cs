﻿// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Build.Acls;
using Horde.Build.Agents;
using Horde.Build.Agents.Sessions;

namespace Horde.Build.Utilities
{
	using SessionId = ObjectId<ISession>;

	/// <summary>
	/// Predetermined claim values
	/// </summary>
	public static class HordeClaims
	{
		/// <summary>
		/// Name of the role that can be used to administer agents
		/// </summary>
		public static AclClaimConfig AgentRegistrationClaim { get; } = new AclClaimConfig(HordeClaimTypes.Role, "agent-registration");

		/// <summary>
		/// Name of the role used to upload software
		/// </summary>
		public static AclClaimConfig DownloadSoftwareClaim { get; } = new AclClaimConfig(HordeClaimTypes.Role, "download-software");

		/// <summary>
		/// Name of the role used to upload software
		/// </summary>
		public static AclClaimConfig UploadSoftwareClaim { get; } = new AclClaimConfig(HordeClaimTypes.Role, "upload-software");

		/// <summary>
		/// Name of the role used to upload software
		/// </summary>
		public static AclClaimConfig ConfigureProjectsClaim { get; } = new AclClaimConfig(HordeClaimTypes.Role, "configure-projects");

		/// <summary>
		/// Name of the role used to upload software
		/// </summary>
		public static AclClaimConfig StartChainedJobClaim { get; } = new AclClaimConfig(HordeClaimTypes.Role, "start-chained-job");

		/// <summary>
		/// Role for all agents
		/// </summary>
		public static AclClaimConfig AgentRoleClaim { get; } = new AclClaimConfig(HordeClaimTypes.Role, "agent");

		/// <summary>
		/// Gets the role for a specific agent
		/// </summary>
		/// <param name="agentId">The session id</param>
		/// <returns>New claim instance</returns>
		public static AclClaimConfig GetAgentClaim(AgentId agentId)
		{
			return new AclClaimConfig(HordeClaimTypes.AgentId, agentId.ToString());
		}

		/// <summary>
		/// Gets the role for a specific agent session
		/// </summary>
		/// <param name="sessionId">The session id</param>
		/// <returns>New claim instance</returns>
		public static AclClaimConfig GetSessionClaim(SessionId sessionId)
		{
			return new AclClaimConfig(HordeClaimTypes.AgentSessionId, sessionId.ToString());
		}
	}
}
