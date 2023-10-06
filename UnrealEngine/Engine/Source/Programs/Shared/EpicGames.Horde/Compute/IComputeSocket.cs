// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Socket for sending and reciving data using a "push" model. The application can attach multiple writers to accept received data.
	/// </summary>
	public interface IComputeSocket : IAsyncDisposable
	{
		/// <summary>
		/// Attaches a buffer to receive data.
		/// </summary>
		/// <param name="channelId">Channel to receive data on</param>
		/// <param name="recvBufferWriter">Writer for the buffer to store received data</param>
		void AttachRecvBuffer(int channelId, IComputeBufferWriter recvBufferWriter);

		/// <summary>
		/// Attaches a buffer to send data.
		/// </summary>
		/// <param name="channelId">Channel to receive data on</param>
		/// <param name="sendBufferReader">Reader for the buffer to send data from</param>
		void AttachSendBuffer(int channelId, IComputeBufferReader sendBufferReader);
	}
}
