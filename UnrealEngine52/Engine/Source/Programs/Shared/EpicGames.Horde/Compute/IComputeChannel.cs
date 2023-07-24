// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Full-duplex channel for sending and reciving messages
	/// </summary>
	public interface IComputeChannel
	{
		/// <summary>
		/// Receives a message from the remote
		/// </summary>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		Task<object> ReadAsync(CancellationToken cancellationToken);

		/// <summary>
		/// Sends a message to the remote
		/// </summary>
		/// <param name="message">Message to be sent</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns></returns>
		Task WriteAsync(object message, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Extension methods for compute channels
	/// </summary>
	public static class ComputeChannelExtensions
	{
		/// <summary>
		/// Receives a message from the remote
		/// </summary>
		/// <param name="channel">Channel to read from</param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async Task<T> ReadAsync<T>(this IComputeChannel channel, CancellationToken cancellationToken)
		{
			return (T)await channel.ReadAsync(cancellationToken);
		}
	}
}
