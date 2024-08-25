// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
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
		/// <param name="cancellationToken">CAncellation token for the operation</param>
		/// <returns></returns>
		Task<IAvatar?> GetAvatarAsync(IUser user, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Default implementation of <see cref="IAvatarService"/>
	/// </summary>
	public class NullAvatarService : IAvatarService
	{
		/// <inheritdoc/>
		public Task<IAvatar?> GetAvatarAsync(IUser user, CancellationToken cancellationToken) => Task.FromResult<IAvatar?>(null);
	}
}

