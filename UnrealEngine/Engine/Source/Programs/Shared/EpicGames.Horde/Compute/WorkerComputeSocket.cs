// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute.Buffers;

namespace EpicGames.Horde.Compute
{
	internal enum IpcMessage
	{
		AttachRecvBuffer = 0,
		AttachSendBuffer = 1,
	}

	/// <summary>
	/// Provides functionality for attaching buffers for compute workers 
	/// </summary>
	public sealed class WorkerComputeSocket : IComputeSocket, IDisposable
	{
		/// <summary>
		/// Name of the environment variable for passing the name of the compute channel
		/// </summary>
		public const string IpcEnvVar = "UE_HORDE_COMPUTE_IPC";

		readonly SharedMemoryBuffer _commandBuffer;

		/// <summary>
		/// Creates a socket for a worker
		/// </summary>
		private WorkerComputeSocket(SharedMemoryBuffer commandBuffer)
		{
			_commandBuffer = commandBuffer;
		}

		/// <summary>
		/// Opens a socket which allows a worker to communicate with the Horde Agent
		/// </summary>
		public static WorkerComputeSocket Open()
		{
			string? baseName = Environment.GetEnvironmentVariable(IpcEnvVar);
			if (baseName == null)
			{
				throw new InvalidOperationException($"Environment variable {IpcEnvVar} is not defined; cannot connect as worker.");
			}

			SharedMemoryBuffer commandBuffer = SharedMemoryBuffer.OpenExisting(baseName);
			return new WorkerComputeSocket(commandBuffer);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_commandBuffer.Dispose();
		}

		/// <inheritdoc/>
		public ValueTask DisposeAsync()
		{
			Dispose();
			return new ValueTask();
		}

		/// <inheritdoc/>
		public void AttachRecvBuffer(int channelId, IComputeBufferWriter writer)
		{
			string bufferName = SharedMemoryBuffer.GetName(writer);
			AttachBuffer(IpcMessage.AttachRecvBuffer, channelId, bufferName);
		}

		/// <inheritdoc/>
		public void AttachSendBuffer(int channelId, IComputeBufferReader reader)
		{
			string bufferName = SharedMemoryBuffer.GetName(reader);
			AttachBuffer(IpcMessage.AttachSendBuffer, channelId, bufferName);
		}

		void AttachBuffer(IpcMessage message, int channelId, string bufferName)
		{
			MemoryWriter writer = new MemoryWriter(_commandBuffer.Writer.GetWriteBuffer());
			writer.WriteUnsignedVarInt((int)message);
			writer.WriteUnsignedVarInt(channelId);
			writer.WriteString(bufferName);
			_commandBuffer.Writer.AdvanceWritePosition(writer.Length);
		}
	}
}
