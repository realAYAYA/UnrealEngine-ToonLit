// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IdentityModel.Tokens.Jwt;
using System.Linq;
using System.Security.Claims;
using System.Threading.Tasks;
using Horde.Server.Agents;
using Horde.Server.Agents.Leases;
using Horde.Server.Agents.Sessions;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.IdentityModel.Tokens;
using MongoDB.Driver;

namespace Horde.Server.Acls
{
	/// <summary>
	/// Wraps functionality for manipulating permissions
	/// </summary>
	public class AclService
	{
		private readonly GlobalsService _globalsService;

		/// <summary>
		/// Constructor
		/// </summary>
		public AclService(GlobalsService globalsService)
		{
			_globalsService = globalsService;
		}

		/// <summary>
		/// Issues a bearer token with the given roles
		/// </summary>
		/// <param name="claims">List of claims to include</param>
		/// <param name="expiry">Time that the token expires</param>
		/// <returns>JWT security token with a claim for creating new agents</returns>
		public async ValueTask<string> IssueBearerTokenAsync(IEnumerable<AclClaimConfig> claims, TimeSpan? expiry)
		{
			return await IssueBearerTokenAsync(claims.Select(x => new Claim(x.Type, x.Value)), expiry);
		}

		/// <summary>
		/// Issues a bearer token with the given claims
		/// </summary>
		/// <param name="claims">List of claims to include</param>
		/// <param name="expiry">Time that the token expires</param>
		/// <returns>JWT security token with a claim for creating new agents</returns>
		public async ValueTask<string> IssueBearerTokenAsync(IEnumerable<Claim> claims, TimeSpan? expiry)
		{
			IGlobals globals = await _globalsService.GetAsync();
			SigningCredentials signingCredentials = new(globals.JwtSigningKey, SecurityAlgorithms.HmacSha256);

			JwtSecurityToken token = new(globals.JwtIssuer, null, claims, null, DateTime.UtcNow + expiry, signingCredentials);
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
			Claim? claim = user.Claims.FirstOrDefault(x => x.Type == HordeClaimTypes.Agent);
			if (claim == null)
			{
				return null;
			}
			else
			{
				return new AgentId(claim.Value);
			}
		}
	}

    internal static class ClaimExtensions
	{
		public static bool HasAdminClaim(this ClaimsPrincipal user)
		{
			return user.HasClaim(HordeClaims.AdminClaim.Type, HordeClaims.AdminClaim.Value);
		}

		public static bool HasAgentClaim(this ClaimsPrincipal user, AgentId agentId)
		{
			return user.HasClaim(HordeClaimTypes.Agent, agentId.ToString());
		}

		public static bool HasLeaseClaim(this ClaimsPrincipal user, LeaseId leaseId)
		{
			return user.HasClaim(HordeClaimTypes.Lease, leaseId.ToString());
		}

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
