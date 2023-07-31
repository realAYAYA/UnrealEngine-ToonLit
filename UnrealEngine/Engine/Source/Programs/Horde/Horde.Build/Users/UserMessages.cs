// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using EpicGames.Core;
using MongoDB.Bson;

namespace Horde.Build.Users
{
	/// <summary>
	/// Response describing the current user
	/// </summary>
	public class GetUserResponse
	{
		/// <summary>
		/// Id of the user
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Name of the user
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Avatar image URL (24px)
		/// </summary>
		public string? Image24 { get; set; }

		/// <summary>
		/// Avatar image URL (32px)
		/// </summary>
		public string? Image32 { get; set; }

		/// <summary>
		/// Avatar image URL (48px)
		/// </summary>
		public string? Image48 { get; set; }

		/// <summary>
		/// Avatar image URL (72px)
		/// </summary>
		public string? Image72 { get; set; }

		/// <summary>
		/// Email of the user
		/// </summary>
		public string? Email { get; set; }

		/// <summary>
		/// Claims for the user
		/// </summary>
		public List<UserClaim>? Claims { get; set; }

		/// <summary>
		/// Whether to enable experimental features for this user
		/// </summary>
		public bool? EnableExperimentalFeatures { get; set; }

		/// <summary>
		/// Settings for the dashboard
		/// </summary>
		public object? DashboardSettings { get; set; }

		/// <summary>
		/// List of pinned job ids
		/// </summary>
		public List<string>? PinnedJobIds { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetUserResponse(IUser user, IAvatar? avatar, IUserClaims? claims, IUserSettings? settings)
		{
			Id = user.Id.ToString();
			Name = user.Name;
			Email = user.Email;

			Image24 = avatar?.Image24;
			Image32 = avatar?.Image32;
			Image48 = avatar?.Image48;
			Image72 = avatar?.Image72;
						
			Claims = claims?.Claims.Select(x => new UserClaim(x)).ToList();

			if (settings != null)
			{
				EnableExperimentalFeatures = settings.EnableExperimentalFeatures;
				
				DashboardSettings = BsonTypeMapper.MapToDotNetValue(settings.DashboardSettings);
				PinnedJobIds = settings.PinnedJobIds.ConvertAll(x => x.ToString());
			}
		}
	}

	/// <summary>
	/// Basic information about a user. May be embedded in other responses.
	/// </summary>
	public class GetThinUserInfoResponse
	{
		/// <summary>
		/// Id of the user
		/// </summary>
		public string Id { get; set; }

		/// <summary>
		/// Name of the user
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// The user's email address
		/// </summary>
		public string? Email { get; set; }

		/// <summary>
		/// The user login [DEPRECATED]
		/// </summary>
		internal string? Login { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="user"></param>
		public GetThinUserInfoResponse(IUser? user)
		{
			if (user == null)
			{
				Id = String.Empty;
				Name = "(Unknown)";
				Email = null;
				Login = null;
			}
			else
			{
				Id = user.Id.ToString();
				Name = user.Name;
				Email = user.Email;
				Login = user.Login;
			}
		}
	}

	/// <summary>
	/// Request to update settings for a user
	/// </summary>
	public class UpdateUserRequest
	{
		/// <summary>
		/// Whether to enable experimental features for this user
		/// </summary>
		public bool? EnableExperimentalFeatures { get; set; }

		/// <summary>
		/// New dashboard settings
		/// </summary>
		public JsonElement? DashboardSettings { get; set; }

		/// <summary>
		/// Job ids to add to the pinned list
		/// </summary>
		public List<string>? AddPinnedJobIds { get; set; }

		/// <summary>
		/// Jobs ids to remove from the pinned list
		/// </summary>
		public List<string>? RemovePinnedJobIds { get; set; }
	}
}
