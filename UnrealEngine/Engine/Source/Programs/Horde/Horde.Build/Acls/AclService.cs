// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IdentityModel.Tokens.Jwt;
using System.Linq;
using System.Security.Claims;
using System.Threading.Tasks;
using Horde.Build.Agents;
using Horde.Build.Agents.Sessions;
using Horde.Build.Server;
using Horde.Build.Users;
using Horde.Build.Utilities;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.Tokens;
using MongoDB.Driver;

namespace Horde.Build.Acls
{
	using SessionId = ObjectId<ISession>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Cache of global ACL
	/// </summary>
	public class GlobalPermissionsCache
	{
		/// <summary>
		/// The root acl
		/// </summary>
		public Acl? RootAcl { get; set; }
	}

	/// <summary>
	/// Wraps functionality for manipulating permissions
	/// </summary>
	public class AclService
	{
		/// <summary>
		/// Name of the role that can be used to administer agents
		/// </summary>
		public static AclClaim AgentRegistrationClaim { get; } = new AclClaim(HordeClaimTypes.Role, "agent-registration");

		/// <summary>
		/// Name of the role used to upload software
		/// </summary>
		public static AclClaim DownloadSoftwareClaim { get; } = new AclClaim(HordeClaimTypes.Role, "download-software");

		/// <summary>
		/// Name of the role used to upload software
		/// </summary>
		public static AclClaim UploadSoftwareClaim { get; } = new AclClaim(HordeClaimTypes.Role, "upload-software");

		/// <summary>
		/// Name of the role used to upload software
		/// </summary>
		public static AclClaim ConfigureProjectsClaim { get; } = new AclClaim(HordeClaimTypes.Role, "configure-projects");

		/// <summary>
		/// Name of the role used to upload software
		/// </summary>
		public static AclClaim StartChainedJobClaim { get; } = new AclClaim(HordeClaimTypes.Role, "start-chained-job");

		/// <summary>
		/// Role for all agents
		/// </summary>
		public static AclClaim AgentRoleClaim { get; } = new AclClaim(HordeClaimTypes.Role, "agent");

		private readonly Acl _defaultAcl = new();
		private readonly MongoService _mongoService;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service instance</param>
		/// <param name="settings">The settings object</param>
		public AclService(MongoService mongoService, IOptionsMonitor<ServerSettings> settings)
		{
			_mongoService = mongoService;

            List<AclAction> adminActions = new();
			foreach (AclAction? action in Enum.GetValues(typeof(AclAction)))
			{
				adminActions.Add(action!.Value);
			}

			_defaultAcl = new();
			_defaultAcl.Entries.Add(new AclEntry(new AclClaim(ClaimTypes.Role, "internal:AgentRegistration"), new[] { AclAction.CreateAgent, AclAction.CreateSession }));
			_defaultAcl.Entries.Add(new AclEntry(AgentRegistrationClaim, new[] { AclAction.CreateAgent, AclAction.CreateSession, AclAction.UpdateAgent, AclAction.DownloadSoftware, AclAction.CreatePool, AclAction.UpdatePool, AclAction.ViewPool, AclAction.DeletePool, AclAction.ListPools, AclAction.ViewStream, AclAction.ViewProject, AclAction.ViewJob, AclAction.ViewCosts }));
			_defaultAcl.Entries.Add(new AclEntry(AgentRoleClaim, new[] { AclAction.ViewProject, AclAction.ViewStream, AclAction.CreateEvent, AclAction.DownloadSoftware }));
			_defaultAcl.Entries.Add(new AclEntry(DownloadSoftwareClaim, new[] { AclAction.DownloadSoftware }));
			_defaultAcl.Entries.Add(new AclEntry(UploadSoftwareClaim, new[] { AclAction.UploadSoftware }));
			_defaultAcl.Entries.Add(new AclEntry(ConfigureProjectsClaim, new[] { AclAction.CreateProject, AclAction.UpdateProject, AclAction.ViewProject, AclAction.CreateStream, AclAction.UpdateStream, AclAction.ViewStream, AclAction.ChangePermissions }));
			_defaultAcl.Entries.Add(new AclEntry(StartChainedJobClaim, new[] { AclAction.CreateJob, AclAction.ExecuteJob, AclAction.UpdateJob, AclAction.ViewJob, AclAction.ViewTemplate, AclAction.ViewStream }));

			ServerSettings settingsValue = settings.CurrentValue;
			if (settingsValue.AdminClaimType != null && settingsValue.AdminClaimValue != null)
			{
				_defaultAcl.Entries.Add(new AclEntry(new AclClaim(settingsValue.AdminClaimType, settingsValue.AdminClaimValue), adminActions.ToArray()));
			}
		}

		/// <summary>
		/// Gets the root ACL scope table
		/// </summary>
		/// <returns>Scopes instance</returns>
		public async Task<Acl> GetRootAcl()
		{
			Globals globals = await _mongoService.GetGlobalsAsync();
			return globals.RootAcl ?? new Acl();
		}

		/// <summary>
		/// Authorizes a user against a given scope
		/// </summary>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to validate</param>
		/// <param name="cache">The ACL scope cache</param>
		/// <returns>Async task</returns>
		public async Task<bool> AuthorizeAsync(AclAction action, ClaimsPrincipal user, GlobalPermissionsCache? cache = null)
		{
			Acl? rootAcl;
			if(cache == null)
			{
				rootAcl = await GetRootAcl();
			}
			else if(cache.RootAcl == null)
			{
				rootAcl = cache.RootAcl = await GetRootAcl();
			}
			else
			{
				rootAcl = cache.RootAcl;
			}
			return rootAcl.Authorize(action, user) ?? _defaultAcl.Authorize(action, user) ?? false;
		}

		/// <summary>
		/// Issues a bearer token with the given roles
		/// </summary>
		/// <param name="claims">List of claims to include</param>
		/// <param name="expiry">Time that the token expires</param>
		/// <returns>JWT security token with a claim for creating new agents</returns>
		public string IssueBearerToken(IEnumerable<AclClaim> claims, TimeSpan? expiry)
		{
			return IssueBearerToken(claims.Select(x => new Claim(x.Type, x.Value)), expiry);
		}

		/// <summary>
		/// Issues a bearer token with the given claims
		/// </summary>
		/// <param name="claims">List of claims to include</param>
		/// <param name="expiry">Time that the token expires</param>
		/// <returns>JWT security token with a claim for creating new agents</returns>
		public string IssueBearerToken(IEnumerable<Claim> claims, TimeSpan? expiry)
		{
			SigningCredentials signingCredentials = new(_mongoService.JwtSigningKey, SecurityAlgorithms.HmacSha256);

			JwtSecurityToken token = new(_mongoService.JwtIssuer, null, claims, null, DateTime.UtcNow + expiry, signingCredentials);
			return new JwtSecurityTokenHandler().WriteToken(token);
		}

		/// <summary>
		/// Get the roles for the given user
		/// </summary>
		/// <param name="user">The user to query roles for</param>
		/// <returns>Collection of roles</returns>
		public static HashSet<string> GetRoles(ClaimsPrincipal user)
		{
			return new HashSet<string>(user.Claims.Where(x => x.Type == ClaimTypes.Role).Select(x => x.Value));
		}

		/// <summary>
		/// Gets the user name from the given principal
		/// </summary>
		/// <param name="user">The principal to check</param>
		/// <returns></returns>
		public static string? GetUserName(ClaimsPrincipal user)
		{
			return (user.Claims.FirstOrDefault(x => x.Type == HordeClaimTypes.User) ?? user.Claims.FirstOrDefault(x => x.Type == ClaimTypes.Name))?.Value ?? "Anonymous";
		}

		/// <summary>
		/// Gets the agent id associated with a particular user
		/// </summary>
		/// <param name="user"></param>
		/// <returns></returns>
		public static AgentId? GetAgentId(ClaimsPrincipal user)
		{
			Claim? claim = user.Claims.FirstOrDefault(x => x.Type == HordeClaimTypes.AgentId);
			if (claim == null)
			{
				return null;
			}
			else
			{
				return new AgentId(claim.Value);
			}
		}

		/// <summary>
		/// Gets the role for a specific agent
		/// </summary>
		/// <param name="agentId">The session id</param>
		/// <returns>New claim instance</returns>
		public static AclClaim GetAgentClaim(AgentId agentId)
		{
			return new AclClaim(HordeClaimTypes.AgentId, agentId.ToString());
		}

		/// <summary>
		/// Gets the role for a specific agent session
		/// </summary>
		/// <param name="sessionId">The session id</param>
		/// <returns>New claim instance</returns>
		public static AclClaim GetSessionClaim(SessionId sessionId)
		{
			return new AclClaim(HordeClaimTypes.AgentSessionId, sessionId.ToString());
		}

		/// <summary>
		/// Determines whether the given user can masquerade as a given user
		/// </summary>
		/// <param name="user"></param>
		/// <param name="userId"></param>
		/// <returns></returns>
		public Task<bool> AuthorizeAsUserAsync(ClaimsPrincipal user, UserId userId)
		{
			UserId? currentUserId = user.GetUserId();
			if (currentUserId != null && currentUserId.Value == userId)
			{
				return Task.FromResult(true);
			}
			else
			{
				return AuthorizeAsync(AclAction.Impersonate, user);
			}
		}
	}

    internal static class ClaimExtensions
	{
		public static bool HasSessionClaim(this ClaimsPrincipal user, SessionId sessionId)
		{
			return user.HasClaim(HordeClaimTypes.AgentSessionId, sessionId.ToString());
		}

		public static SessionId? GetSessionClaim(this ClaimsPrincipal user)
		{
			Claim? claim = user.FindFirst(HordeClaimTypes.AgentSessionId);
			if (claim == null || !SessionId.TryParse(claim.Value, out SessionId sessionIdValue))
			{
				return null;
			}
			else
			{
				return sessionIdValue;
			}
		}
		
		public static string GetSessionClaimsAsString(this ClaimsPrincipal user)
		{
			return String.Join(",", user.Claims
				.Where(c => c.Type == HordeClaimTypes.AgentSessionId)
				.Select(c => c.Value));
		}
	}
}
