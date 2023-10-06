// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;

namespace Horde.Server.Users
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

	/// <summary>
	/// Default implementation of <see cref="IAvatarService"/>
	/// </summary>
	public class NullAvatarService : IAvatarService
	{
		/// <inheritdoc/>
		public Task<IAvatar?> GetAvatarAsync(IUser user) => Task.FromResult<IAvatar?>(null);
	}
}

