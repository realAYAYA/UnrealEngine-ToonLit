// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Build.Server
{
	/// <summary>
	/// Object containing settings for the server
	/// </summary>
	public class AdminSettings
	{
		/// <summary>
		/// The default perforce server
		/// </summary>
		public string? DefaultServerAndPort { get; set; }

		/// <summary>
		/// The default perforce username
		/// </summary>
		public string? DefaultUserName { get; set; }

		/// <summary>
		/// The default perforce password
		/// </summary>
		public string? DefaultPassword { get; set; }
	}

	/// <summary>
	/// The conform limit value
	/// </summary>
	public class ConformSettings
	{
		/// <summary>
		/// Maximum number of conforms allowed at once
		/// </summary>
		public int MaxCount { get; set; }
	}

	/// <summary>
	/// Controller managing account status
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class AdminController : HordeControllerBase
	{
		readonly AclService _aclService;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		readonly IOptions<ServerSettings> _serverSettings;

		/// <summary>
		/// Constructor
		/// </summary>
		public AdminController(AclService aclService, IOptionsSnapshot<GlobalConfig> globalConfig, IOptions<ServerSettings> serverSettings)
		{
			_aclService = aclService;
			_globalConfig = globalConfig;
			_serverSettings = serverSettings;
		}

		/// <summary>
		/// Issues a token for the given roles. Issues a token for the current user if not specified.
		/// </summary>
		/// <returns>Administrative settings for the server</returns>
		[HttpGet]
		[Route("/api/v1/admin/token")]
		public async Task<ActionResult<string>> GetTokenAsync()
		{
			if (!_globalConfig.Value.Authorize(AclAction.IssueBearerToken, User))
			{
				return Forbid(AclAction.IssueBearerToken);
			}

			return await _aclService.IssueBearerTokenAsync(User.Claims, GetDefaultExpiryTime());
		}

		/// <summary>
		/// Returns the fully parsed config object.
		/// </summary>
		[HttpGet]
		[Route("/api/v1/admin/config")]
		public ActionResult<object> GetConfig()
		{
			if (!_globalConfig.Value.Authorize(AclAction.AdminRead, User))
			{
				return Forbid(AclAction.AdminRead);
			}

			return _globalConfig.Value;
		}

		/// <summary>
		/// Issues a token for the given roles. Issues a token for the current user if not specified.
		/// </summary>
		/// <param name="roles">Roles for the new token</param>
		/// <returns>Administrative settings for the server</returns>
		[HttpGet]
		[Route("/api/v1/admin/roletoken")]
		public async Task<ActionResult<string>> GetRoleTokenAsync([FromQuery] string roles)
		{
			if (!_globalConfig.Value.Authorize(AclAction.AdminWrite, User))
			{
				return Forbid(AclAction.AdminWrite);
			}

			List<Claim> claims = new List<Claim>();
			claims.AddRange(roles.Split('+').Select(x => new Claim(ClaimTypes.Role, x)));

			return await _aclService.IssueBearerTokenAsync(claims, GetDefaultExpiryTime());
		}

		/// <summary>
		/// Issues a token for the given roles. Issues a token for the current user if not specified.
		/// </summary>
		/// <returns>Administrative settings for the server</returns>
		[HttpGet]
		[Route("/api/v1/admin/registrationtoken")]
		public async Task<ActionResult<string>> GetRegistrationTokenAsync()
		{
			if (!_globalConfig.Value.Authorize(AclAction.AdminWrite, User))
			{
				return Forbid(AclAction.AdminWrite);
			}

			List<AclClaimConfig> claims = new List<AclClaimConfig>();
			claims.Add(new AclClaimConfig(ClaimTypes.Name, User.Identity?.Name ?? "Unknown"));
			claims.Add(HordeClaims.AgentRegistrationClaim);

			return await _aclService.IssueBearerTokenAsync(claims, null);
		}

		/// <summary>
		/// Issues a token valid to upload new versions of the agent software.
		/// </summary>
		/// <returns>Administrative settings for the server</returns>
		[HttpGet]
		[Route("/api/v1/admin/softwaretoken")]
		public async Task<ActionResult<string>> GetSoftwareTokenAsync()
		{
			if (!_globalConfig.Value.Authorize(AclAction.AdminWrite, User))
			{
				return Forbid(AclAction.AdminWrite);
			}

			List<AclClaimConfig> claims = new List<AclClaimConfig>();
			claims.Add(new AclClaimConfig(ClaimTypes.Name, User.Identity?.Name ?? "Unknown"));
			claims.Add(HordeClaims.UploadSoftwareClaim);

			return await _aclService.IssueBearerTokenAsync(claims, null);
		}

		/// <summary>
		/// Issues a token valid to download new versions of the agent software.
		/// </summary>
		/// <returns>Administrative settings for the server</returns>
		[HttpGet]
		[Route("/api/v1/admin/softwaredownloadtoken")]
		public async Task<ActionResult<string>> GetSoftwareDownloadTokenAsync()
		{
			if (!_globalConfig.Value.Authorize(AclAction.AdminRead, User))
			{
				return Forbid(AclAction.AdminRead);
			}

			List<AclClaimConfig> claims = new List<AclClaimConfig>();
			claims.Add(new AclClaimConfig(ClaimTypes.Name, User.Identity?.Name ?? "Unknown"));
			claims.Add(HordeClaims.DownloadSoftwareClaim);

			return await _aclService.IssueBearerTokenAsync(claims, null);
		}

		/// <summary>
		/// Issues a token valid to configure streams and projects
		/// </summary>
		/// <returns>Administrative settings for the server</returns>
		[HttpGet]
		[Route("/api/v1/admin/configtoken")]
		public async Task<ActionResult<string>> GetConfigToken()
		{
			if (!_globalConfig.Value.Authorize(AclAction.AdminRead, User))
			{
				return Forbid(AclAction.AdminRead);
			}

			List<AclClaimConfig> claims = new List<AclClaimConfig>();
			claims.Add(new AclClaimConfig(ClaimTypes.Name, User.Identity?.Name ?? "Unknown"));
			claims.Add(HordeClaims.ConfigureProjectsClaim);

			return await _aclService.IssueBearerTokenAsync(claims, null);
		}

		/// <summary>
		/// Issues a token valid to start chained jobs
		/// </summary>
		/// <returns>Administrative settings for the server</returns>
		[HttpGet]
		[Route("/api/v1/admin/chainedjobtoken")]
		public async Task<ActionResult<string>> GetChainedJobToken()
		{
			if (!_globalConfig.Value.Authorize(AclAction.AdminRead, User))
			{
				return Forbid(AclAction.AdminRead);
			}

			List<AclClaimConfig> claims = new List<AclClaimConfig>();
			//Claims.Add(new AclClaim(ClaimTypes.Name, User.Identity.Name ?? "Unknown"));
			claims.Add(HordeClaims.StartChainedJobClaim);

			return await _aclService.IssueBearerTokenAsync(claims, null);
		}

		/// <summary>
		/// Gets the default expiry time for a token
		/// </summary>
		/// <returns></returns>
		private TimeSpan? GetDefaultExpiryTime()
		{
			ServerSettings serverSettings = _serverSettings.Value;

			TimeSpan? expiryTime = null;
			if (serverSettings.JwtExpiryTimeHours != -1)
			{
				expiryTime = TimeSpan.FromHours(serverSettings.JwtExpiryTimeHours);
			}
			return expiryTime;
		}
	}
}
