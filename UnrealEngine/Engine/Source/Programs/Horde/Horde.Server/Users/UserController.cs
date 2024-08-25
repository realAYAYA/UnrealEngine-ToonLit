// Copyright Epic Games, Inc. All Rights Reserved.

using System.Linq;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Bisect;
using EpicGames.Horde.Users;
using Horde.Server.Accounts;
using Horde.Server.Agents;
using Horde.Server.Agents.Pools;
using Horde.Server.Server;
using Horde.Server.Server.Notices;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Server.Users
{
	/// <summary>
	/// Controller for the /api/v1/user endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class UserController : ControllerBase
	{
		readonly IUserCollection _userCollection;
		readonly IAvatarService? _avatarService;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		readonly IOptionsMonitor<ServerSettings> _settings;

		/// <summary>
		/// Constructor
		/// </summary>
		public UserController(IUserCollection userCollection, IAvatarService? avatarService, IOptionsSnapshot<GlobalConfig> globalConfig, IOptionsMonitor<ServerSettings> settings)
		{
			_userCollection = userCollection;
			_avatarService = avatarService;
			_globalConfig = globalConfig;
			_settings = settings;
		}

		/// <summary>
		/// Gets information about the logged in user
		/// </summary>
		/// <returns>Http result code</returns>
		[HttpGet]
		[Route("/api/v1/user")]
		[ProducesResponseType(typeof(GetUserResponse), 200)]
		public async Task<ActionResult<object>> GetUserAsync([FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)
		{
			IUser? internalUser = await _userCollection.GetUserAsync(User, cancellationToken);
			if (internalUser == null)
			{
				return NotFound();
			}

			IAvatar? avatar = (_avatarService == null) ? (IAvatar?)null : await _avatarService.GetAvatarAsync(internalUser, cancellationToken);
			IUserClaims claims = await _userCollection.GetClaimsAsync(internalUser.Id, cancellationToken);
			IUserSettings settings = await _userCollection.GetSettingsAsync(internalUser.Id, cancellationToken);

			GetUserResponse response = internalUser.ToApiResponse(avatar, claims, settings);
			response.DashboardFeatures = GetDashboardFeatures(_globalConfig.Value, _settings.CurrentValue, User);

			return PropertyFilter.Apply(response, filter);
		}

		static GetDashboardFeaturesResponse GetDashboardFeatures(GlobalConfig globalConfig, ServerSettings settings, ClaimsPrincipal principal)
		{
			GetDashboardFeaturesResponse response = new GetDashboardFeaturesResponse();
			response.ShowLandingPage = globalConfig.Dashboard.ShowLandingPage;
			response.ShowCI = globalConfig.Dashboard.ShowCI;
			response.ShowAgents = globalConfig.Dashboard.ShowAgents;
			response.ShowAgentRegistration = globalConfig.Dashboard.ShowAgentRegistration;
			response.ShowPerforceServers = globalConfig.Dashboard.ShowPerforceServers;
			response.ShowDeviceManager = globalConfig.Dashboard.ShowDeviceManager;
			response.ShowTests = globalConfig.Dashboard.ShowTests;
			response.ShowNoticeEditor = globalConfig.Authorize(NoticeAclAction.CreateNotice, principal) || globalConfig.Authorize(NoticeAclAction.UpdateNotice, principal);
			response.ShowPoolEditor = globalConfig.VersionEnum < GlobalVersion.PoolsInConfigFiles && (globalConfig.Authorize(PoolAclAction.CreatePool, principal) || globalConfig.Authorize(PoolAclAction.UpdatePool, principal));
			response.ShowRemoteDesktop = globalConfig.Authorize(AgentAclAction.UpdateAgent, principal);
			response.ShowAccounts = settings.AuthMethod == EpicGames.Horde.Server.AuthMethod.Horde && globalConfig.Authorize(AccountAclAction.UpdateAccount, principal);
			return response;
		}

		/// <summary>
		/// Updates the logged in user
		/// </summary>
		/// <returns>Http result code</returns>
		[HttpPut]
		[Route("/api/v1/user")]
		public async Task<ActionResult> UpdateUserAsync(UpdateUserRequest request)
		{
			UserId? userId = User.GetUserId();
			if (userId == null)
			{
				return BadRequest("Current user does not have a registered profile");
			}

			await _userCollection.UpdateSettingsAsync(userId.Value, request.EnableExperimentalFeatures, request.AlwaysTagPreflightCL, request.DashboardSettings?.ToBsonValue(), request.AddPinnedJobIds?.Select(x => JobId.Parse(x)), request.RemovePinnedJobIds?.Select(x => JobId.Parse(x)), null, request.AddPinnedBisectTaskIds?.Select(x => BisectTaskId.Parse(x)), request.RemovePinnedBisectTaskIds?.Select(x => BisectTaskId.Parse(x)));
			return Ok();
		}

		/// <summary>
		/// Gets claims for the current user
		/// </summary>
		[HttpGet]
		[Route("/api/v1/user/claims")]
		public ActionResult<object[]> GetUserClaims()
		{
			return User.Claims.Select(x => new { x.Type, x.Value }).ToArray();
		}
	}
}
