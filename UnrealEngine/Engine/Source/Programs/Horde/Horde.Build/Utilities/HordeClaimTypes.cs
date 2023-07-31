// Copyright Epic Games, Inc. All Rights Reserved.

using System.Linq;
using System.Security.Claims;
using Horde.Build.Users;

namespace Horde.Build.Utilities
{
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Claim types that are specific to horde
	/// </summary>
	public static class HordeClaimTypes
	{
		/// <summary>
		/// Base URI for all Horde claims.
		/// </summary>
		const string Prefix = "http://epicgames.com/ue/horde/";

		/// <summary>
		/// Claim for a particular role.
		/// </summary>
		public const string Role = Prefix + "role";

		/// <summary>
		/// Claim for a well known internal role (eg. admin)
		/// </summary>
		public const string InternalRole = Prefix + "internal-role";

		/// <summary>
		/// Claim for a particular session
		/// </summary>
		public const string AgentId = Prefix + "agent";

		/// <summary>
		/// Claim for a particular session
		/// </summary>
		public const string AgentSessionId = Prefix + "session";

		/// <summary>
		/// User name for horde
		/// </summary>
		public const string User = Prefix + "user";

		/// <summary>
		/// Unique id of the horde user (see <see cref="IUserCollection"/>)
		/// </summary>
		public const string UserId = Prefix + "user-id-v3";

		/// <summary>
		/// Claim for downloading artifacts from a particular job
		/// </summary>
		public const string JobArtifacts = Prefix + "job-artifacts";

		/// <summary>
		/// Claim for the Perforce username
		/// </summary>
		public const string PerforceUser = Prefix + "perforce-user";

		/// <summary>
		/// Claim for the external issue username
		/// </summary>
		public const string ExternalIssueUser = Prefix + "external-issue-user";

	}

	/// <summary>
	/// Extension methods for getting Horde claims from a principal
	/// </summary>
	public static class HordeClaimExtensions
	{
		/// <summary>
		/// Gets the Horde user id from a principal
		/// </summary>
		/// <param name="principal"></param>
		/// <returns></returns>
		public static UserId? GetUserId(this ClaimsPrincipal principal)
		{
			string? idValue = principal.FindFirstValue(HordeClaimTypes.UserId);
			if(idValue == null)
			{
				return null;
			}
			else
			{
				return new UserId(idValue);
			}
		}

		/// <summary>
		/// Gets the Horde user name from a principal
		/// </summary>
		/// <param name="principal"></param>
		/// <returns></returns>
		public static string? GetUserName(this ClaimsPrincipal principal)
		{
			return principal.FindFirstValue(HordeClaimTypes.User) ?? principal.FindFirstValue(ClaimTypes.Name);
		}

		/// <summary>
		/// Get the email for the given user
		/// </summary>
		/// <param name="user">The user to query the email for</param>
		/// <returns>The user's email address or null if not found</returns>
		public static string? GetEmail(this ClaimsPrincipal user)
		{
			return user.Claims.FirstOrDefault(x => x.Type == ClaimTypes.Email)?.Value;
		}

		/// <summary>
		/// Gets the perforce username for the given principal
		/// </summary>
		/// <param name="user">The principal to get the Perforce user for</param>
		/// <returns>Perforce user name</returns>
		public static string? GetPerforceUser(this ClaimsPrincipal user)
		{
			Claim? claim = user.FindFirst(HordeClaimTypes.PerforceUser);
			if (claim == null)
			{
				return null;
			}
			return claim.Value;
		}

		/// <summary>
		/// Gets the external issue username for the given principal
		/// </summary>
		/// <param name="user">The principal to get the external issue user for</param>
		/// <returns>Jira user name</returns>
		public static string? GetExternalIssueUser(this ClaimsPrincipal user)
		{
			Claim? claim = user.FindFirst(HordeClaimTypes.ExternalIssueUser);
			if (claim == null)
			{
				return null;
			}
			return claim.Value;
		}

	}
}
