// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Jobs;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace Horde.Build.Users
{
	using JobId = ObjectId<IJob>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Controller for the /api/v1/user endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class UserController : ControllerBase
	{
		/// <summary>
		/// The user collection instance
		/// </summary>
		IUserCollection UserCollection { get; set; }

		/// <summary>
		/// The avatar service
		/// </summary>
		IAvatarService? AvatarService { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="userCollection"></param>
		/// <param name="avatarService"></param>
		public UserController(IUserCollection userCollection, IAvatarService? avatarService)
		{
			UserCollection = userCollection;
			AvatarService = avatarService;
		}

		/// <summary>
		/// Gets information about the logged in user
		/// </summary>
		/// <returns>Http result code</returns>
		[HttpGet]
		[Route("/api/v1/user")]
		[ProducesResponseType(typeof(List<GetUserResponse>), 200)]
		public async Task<ActionResult<object>> GetUserAsync([FromQuery] PropertyFilter? filter = null)
		{
			IUser? internalUser = await UserCollection.GetUserAsync(User);
			if (internalUser == null)
			{
				return NotFound();
			}

			IAvatar? avatar = (AvatarService == null)? (IAvatar?)null : await AvatarService.GetAvatarAsync(internalUser);
			IUserClaims claims = await UserCollection.GetClaimsAsync(internalUser.Id);
			IUserSettings settings = await UserCollection.GetSettingsAsync(internalUser.Id);
			return PropertyFilter.Apply(new GetUserResponse(internalUser, avatar, claims, settings), filter);
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
			await UserCollection.UpdateSettingsAsync(userId.Value, request.EnableExperimentalFeatures, request.DashboardSettings?.ToBsonValue(), request.AddPinnedJobIds?.Select(x => new JobId(x)), request.RemovePinnedJobIds?.Select(x => new JobId(x)));
			return Ok();
		}
	}
}
