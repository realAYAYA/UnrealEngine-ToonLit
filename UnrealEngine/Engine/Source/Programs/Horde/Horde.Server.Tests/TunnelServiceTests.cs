// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Binary;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Clients;
using Horde.Server.Compute;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	[TestClass]
	public sealed class TunnelServiceTests : IDisposable
	{
		readonly ServiceProvider _serviceProvider;

		public TunnelServiceTests()
		{
			ServiceCollection services = new ServiceCollection();
			services.AddLogging();
			services.Configure<ServerSettings>(x =>
			{
				x.ComputeInitiatorPort = 7000;
				x.ComputeRemotePort = 7002;
			});
			services.AddSingleton<TunnelService>();

			_serviceProvider = services.BuildServiceProvider();
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_serviceProvider.Dispose();
		}

		[TestMethod]
		public async Task SendRecvTest()
		{
			CancellationToken cancellationToken = CancellationToken.None;
			byte[] nonce = new byte[ServerComputeClient.NonceLength];

			TunnelService tunnelService = _serviceProvider.GetRequiredService<TunnelService>();
			tunnelService.Start(IPAddress.Loopback);

			IOptions<ServerSettings> settings = _serviceProvider.GetRequiredService<IOptions<ServerSettings>>();

			using (Socket initiatorSocket = await ConnectAsync(settings.Value.ComputeInitiatorPort, nonce, cancellationToken))
			{
				byte[] remotePortBuffer = new byte[2];
				await initiatorSocket.ReceiveAsync(remotePortBuffer, SocketFlags.None, cancellationToken);
				ushort remotePort = BinaryPrimitives.ReadUInt16LittleEndian(remotePortBuffer);

				using (Socket remoteSocket = await ConnectAsync(remotePort, nonce, cancellationToken))
				{
					byte[] sendInitiatorData = new byte[] { 1, 2, 3, 4, 5 };
					byte[] sendRemoteData = new byte[] { 6, 7, 8, 9 };

					Task<byte[]> initiatorTask = SendRecvAsync(initiatorSocket, sendInitiatorData, sendRemoteData.Length, cancellationToken);
					Task<byte[]> remoteTask = SendRecvAsync(remoteSocket, sendRemoteData, sendInitiatorData.Length, cancellationToken);

					byte[] recvInitiatorData = await initiatorTask.ConfigureAwait(false);
					byte[] recvRemoteData = await remoteTask.ConfigureAwait(false);

					Assert.IsTrue(recvRemoteData.SequenceEqual(sendInitiatorData));
					Assert.IsTrue(recvInitiatorData.SequenceEqual(sendRemoteData));
				}
			}

			await tunnelService.StopAsync(CancellationToken.None);
		}

		static async Task<Socket> ConnectAsync(int port, byte[] nonce, CancellationToken cancellationToken)
		{
			Socket socket = new Socket(SocketType.Stream, ProtocolType.Tcp);
			await socket.ConnectAsync(IPAddress.Loopback, port, cancellationToken);
			await socket.SendAsync(nonce, SocketFlags.None);
			return socket;
		}

		static async Task<byte[]> SendRecvAsync(Socket socket, byte[] dataToSend, int lengthToRecv, CancellationToken cancellationToken)
		{
			Task<byte[]> recv = RecvAsync(socket, lengthToRecv, cancellationToken);
			for (int offset = 0; offset < dataToSend.Length;)
			{
				int sent = await socket.SendAsync(dataToSend.AsMemory(offset), SocketFlags.None, cancellationToken);
				if (sent == 0)
				{
					throw new NotImplementedException();
				}
				offset += sent;
			}
			return await recv;
		}

		static async Task<byte[]> RecvAsync(Socket socket, int length, CancellationToken cancellationToken)
		{
			byte[] buffer = new byte[length];
			for (int offset = 0; offset < length;)
			{
				int read = await socket.ReceiveAsync(buffer.AsMemory(offset), SocketFlags.None, cancellationToken);
				if (read == 0)
				{
					throw new NotImplementedException();
				}
				offset += read;
			}
			return buffer;
		}
	}
}
