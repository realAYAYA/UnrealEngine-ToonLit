// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Server.Jobs;
using Horde.Server.Server;
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

		/// <summary>
		/// Constructor
		/// </summary>
		public UserController(IUserCollection userCollection, IAvatarService? avatarService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_userCollection = userCollection;
			_avatarService = avatarService;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Gets information about the logged in user
		/// </summary>
		/// <returns>Http result code</returns>
		[HttpGet]
		[Route("/api/v1/user")]
		[ProducesResponseType(typeof(GetUserResponse), 200)]
		public async Task<ActionResult<object>> GetUserAsync([FromQuery] PropertyFilter? filter = null)
		{
			IUser? internalUser = await _userCollection.GetUserAsync(User);
			if (internalUser == null)
			{
				return NotFound();
			}

			IAvatar? avatar = (_avatarService == null)? (IAvatar?)null : await _avatarService.GetAvatarAsync(internalUser);
			IUserClaims claims = await _userCollection.GetClaimsAsync(internalUser.Id);
			IUserSettings settings = await _userCollection.GetSettingsAsync(internalUser.Id);

			GetUserResponse response = new GetUserResponse(internalUser, avatar, claims, settings);
			response.DashboardFeatures = new GetDashboardFeaturesResponse(_globalConfig.Value, User);

			return PropertyFilter.Apply(response, filter);
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
			if(userId == null)
			{
				return BadRequest("Current user does not have a registered profile");
			}

			await _userCollection.UpdateSettingsAsync(userId.Value, request.EnableExperimentalFeatures, request.DashboardSettings?.ToBsonValue(), request.AddPinnedJobIds?.Select(x => JobId.Parse(x)), request.RemovePinnedJobIds?.Select(x => JobId.Parse(x)));
			return Ok();
		}
	}
}
