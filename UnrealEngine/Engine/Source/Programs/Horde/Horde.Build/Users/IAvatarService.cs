// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;

namespace Horde.Build.Users
{
	/// <summary>
	/// Provides avatar images
	/// </summary>
	public interface IAvatarService
	{
		/// <summary>
		/// Gets a users's avatar
		/// </summary>
		/// <param name="user"></param>
		/// <returns></returns>
		Task<IAvatar?> GetAvatarAsync(IUser user);
	}
}

