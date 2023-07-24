// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Horde.Build.Jobs;
using Horde.Build.Utilities;
using MongoDB.Bson;

namespace Horde.Build.Users
{
	using JobId = ObjectId<IJob>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Known user ids
	/// </summary>
	public static class KnownUsers
	{
		/// <summary>
		/// The system user. Used for automated processes.
		/// </summary>
		public static UserId System { get; } = new UserId("6170b423b94a2c7c2d6b6f87");
	}

	/// <summary>
	/// Document which collates information about a user, and their personal settings
	/// </summary>
	public interface IUser
	{
		/// <summary>
		/// The user id
		/// </summary>
		public UserId Id { get; }

		/// <summary>
		/// Full name of the user
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// The user's login id
		/// </summary>
		public string Login { get; }

		/// <summary>
		/// The user's email
		/// </summary>
		public string? Email { get; }
	}

	/// <summary>
	/// Claims for a particular user
	/// </summary>
	public interface IUserClaims
	{
		/// <summary>
		/// The user id
		/// </summary>
		public UserId UserId { get; }

		/// <summary>
		/// Claims for this user (on last login)
		/// </summary>
		public IReadOnlyList<IUserClaim> Claims { get; }
	}

	/// <summary>
	/// User settings document
	/// </summary>
	public interface IUserSettings
	{
		/// <summary>
		/// The user id
		/// </summary>
		public UserId UserId { get; }

		/// <summary>
		/// Whether to enable experimental features
		/// </summary>
		public bool EnableExperimentalFeatures { get; }

		/// <summary>
		/// Opaque settings dictionary for the dashboard
		/// </summary>
		public BsonValue DashboardSettings { get; }

		/// <summary>
		/// List of pinned jobs
		/// </summary>
		public IReadOnlyList<JobId> PinnedJobIds { get; }
	}
}
