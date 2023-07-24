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
using Horde.Build.Utilities;
using Microsoft.IdentityModel.Tokens;
using MongoDB.Driver;

namespace Horde.Build.Acls
{
	using SessionId = ObjectId<ISession>;

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
