// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO.Pipelines;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Transports;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Horde.Tests
{
	[TestClass]
	public class ComputeSocketTests
	{
		class TestComputeSocket : ComputeSocket, IDisposable
		{
			public Dictionary<int, ComputeBufferWriter> RecvBufferWriters { get; } = new Dictionary<int, ComputeBufferWriter>();
			public Dictionary<int, ComputeBufferReader> SendBufferReaders { get; } = new Dictionary<int, ComputeBufferReader>();

			public override ComputeProtocol Protocol => ComputeProtocol.Latest;
			public override ILogger Logger => NullLogger.Instance;

			public void Dispose()
			{
				foreach (ComputeBufferWriter writer in RecvBufferWriters.Values)
				{
					writer.Dispose();
				}
				foreach (ComputeBufferReader reader in SendBufferReaders.Values)
				{
					reader.Dispose();
				}
			}

			public override void AttachRecvBuffer(int channelId, ComputeBuffer recvBuffer)
			{
				RecvBufferWriters.Add(channelId, recvBuffer.CreateWriter());
			}

			public override void AttachSendBuffer(int channelId, ComputeBuffer sendBuffer)
			{
				SendBufferReaders.Add(channelId, sendBuffer.CreateReader());
			}
		}

		class TestLogger : ILogger
		{
			public IDisposable BeginScope<TState>(TState state) => null!;

			public bool IsEnabled(LogLevel logLevel) => true;

			public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
			{
				Console.WriteLine($"{logLevel}: {formatter(state, exception)}");
				Assert.IsFalse(logLevel == LogLevel.Error);
				Assert.IsFalse(logLevel == LogLevel.Warning);
			}
		}

		[TestMethod]
		public async Task TestAgentMessageLoopPipeAsync()
		{
			Pipe recvPipe = new Pipe();
			Pipe sendPipe = new Pipe();
			await using PipeTransport localTransport = new(sendPipe.Reader, recvPipe.Writer);
			await using PipeTransport agentTransport = new(recvPipe.Reader, sendPipe.Writer);
			await using RemoteComputeSocket localSocket = new(localTransport, ComputeProtocol.Latest, new TestLogger());
			await using RemoteComputeSocket agentSocket = new(agentTransport, ComputeProtocol.Latest, new TestLogger());

			await RunAgentTestsAsync(localSocket, agentSocket);
		}

		[TestMethod]
		public async Task TestAgentMessageLoopTcpAsync()
		{
			using CancellationTokenSource cts = new(5000);
			(Socket clientSocket, Socket serverSocket) = await CreateSocketsAsync(cts.Token);

			await using TcpTransport clientTransport = new(clientSocket);
			await using TcpTransport serverTransport = new(serverSocket);
			await using RemoteComputeSocket localSocket = new(clientTransport, ComputeProtocol.Latest, new TestLogger());
			await using RemoteComputeSocket agentSocket = new(serverTransport, ComputeProtocol.Latest, new TestLogger());

			await RunAgentTestsAsync(localSocket, agentSocket, cts.Token);
		}

		[TestMethod]
		[DataRow(Encryption.Ssl)]
		[DataRow(Encryption.SslEcdsaP256)]
		public async Task TestAgentMessageLoopTcpSslAsync(Encryption encryption)
		{
			using CancellationTokenSource cts = new(5000);
			(Socket clientSocket, Socket serverSocket) = await CreateSocketsAsync(cts.Token);

			byte[] certData = TcpSslTransport.GenerateCert(encryption);
			await using TcpSslTransport clientTransport = new(clientSocket, certData, false);
			await using TcpSslTransport serverTransport = new(serverSocket, certData, true);

			Task t1 = clientTransport.AuthenticateAsync(cts.Token);
			Task t2 = serverTransport.AuthenticateAsync(cts.Token);
			await t2;
			await t1;

			await using RemoteComputeSocket localSocket = new(clientTransport, ComputeProtocol.Latest, new TestLogger());
			await using RemoteComputeSocket agentSocket = new(serverTransport, ComputeProtocol.Latest, new TestLogger());
			await RunAgentTestsAsync(localSocket, agentSocket, cts.Token);
		}

		private static async Task<(Socket client, Socket server)> CreateSocketsAsync(CancellationToken cancellationToken)
		{
			int port = GetAvailablePort();
#if NET8_0_OR_GREATER
			using TcpListener listener = new (IPAddress.Loopback, port);
#else
			TcpListener listener = new(IPAddress.Loopback, port);
#endif
			listener.Start();
			Socket clientSocket = new(SocketType.Stream, ProtocolType.Tcp);
			Task clientConnectTask = clientSocket.ConnectAsync(IPAddress.Loopback, port, cancellationToken).AsTask();
			Socket serverSocket = await listener.AcceptSocketAsync(cancellationToken);
			await clientConnectTask;
			return (clientSocket, serverSocket);
		}

		static async Task RunAgentTestsAsync(RemoteComputeSocket localSocket, RemoteComputeSocket agentSocket, CancellationToken cancellationToken = default)
		{
			DirectoryReference tempDir = new DirectoryReference("test-temp");
			await using (BackgroundTask agentTask = BackgroundTask.StartNew(ctx => RunAgentAsync(agentSocket, tempDir, ctx)))
			{
				const int PrimaryChannelId = 0;
				using (AgentMessageChannel channel = localSocket.CreateAgentMessageChannel(PrimaryChannelId, 4 * 1024 * 1024))
				{
					await channel.WaitForAttachAsync(cancellationToken);

					await channel.PingAsync(cancellationToken);
					using (AgentMessage message = await channel.ReceiveAsync(cancellationToken))
					{
						Assert.AreEqual(AgentMessageType.Ping, message.Type);
						Assert.IsTrue(message.Data.Span.SequenceEqual(ReadOnlySpan<byte>.Empty));
					}

					await channel.SendXorRequestAsync(new byte[] { 1, 2, 3 }, 44, cancellationToken);
					using (AgentMessage message = await channel.ReceiveAsync(cancellationToken))
					{
						Assert.AreEqual(AgentMessageType.XorResponse, message.Type);
						Assert.IsTrue(message.Data.Span.SequenceEqual(new byte[] { 1 ^ 44, 2 ^ 44, 3 ^ 44 }));
					}

					const int SecondaryChannelId = 1;
					using (AgentMessageChannel channel2 = localSocket.CreateAgentMessageChannel(SecondaryChannelId, 4 * 1024 * 1024))
					{
						await channel.ForkAsync(SecondaryChannelId, 4 * 1024 * 1024, cancellationToken);

						await channel2.WaitForAttachAsync(cancellationToken);

						await channel2.SendXorRequestAsync(new byte[] { 1, 2, 3 }, 44, cancellationToken);
						using (AgentMessage message = await channel2.ReceiveAsync(cancellationToken))
						{
							Assert.AreEqual(AgentMessageType.XorResponse, message.Type);
							Assert.IsTrue(message.Data.Span.SequenceEqual(new byte[] { 1 ^ 44, 2 ^ 44, 3 ^ 44 }));
						}

						await channel2.CloseAsync(cancellationToken);
					}

					using BundleStorageClient storage = BundleStorageClient.CreateInMemory(NullLogger.Instance);
					await using (IBlobWriter blobWriter = storage.CreateBlobWriter())
					{
						FileReference file = FileReference.Combine(tempDir, "subdir/hello.txt");
						if (FileReference.Exists(file))
						{
							FileReference.Delete(file);
						}
						Assert.IsFalse(FileReference.Exists(file));

						byte[] data = Encoding.UTF8.GetBytes("Hello world");

						using ChunkedDataWriter writer = new ChunkedDataWriter(blobWriter, new ChunkingOptions());
						ChunkedData chunkedData = await writer.CreateAsync(data, cancellationToken);

						DirectoryNode directory = new DirectoryNode();
						directory.AddFile("hello.txt", FileEntryFlags.None, data.Length, chunkedData);

						IBlobRef<DirectoryNode> directoryRef = await blobWriter.WriteBlobAsync(directory, cancellationToken: cancellationToken);

						DirectoryNode root = new DirectoryNode();
						root.AddDirectory(new DirectoryEntry("subdir", directory.Length, directoryRef));

						IBlobRef<DirectoryNode> handle = await blobWriter.WriteBlobAsync(root, cancellationToken: cancellationToken);
						await blobWriter.FlushAsync(cancellationToken);

						await channel.UploadFilesAsync("", handle.GetLocator(), storage.Backend, cancellationToken);

						Assert.IsTrue(FileReference.Exists(file));
						byte[] readData = await FileReference.ReadAllBytesAsync(file, cancellationToken);
						Assert.IsTrue(readData.SequenceEqual(data));

						await channel.DeleteFilesAsync(new[] { "subdir/hello.txt" }, cancellationToken);

						await channel.PingAsync(cancellationToken);
						using (AgentMessage message = await channel.ReceiveAsync(cancellationToken))
						{
							Assert.AreEqual(AgentMessageType.Ping, message.Type);
							Assert.IsTrue(message.Data.Span.SequenceEqual(ReadOnlySpan<byte>.Empty));
						}

						Assert.IsFalse(FileReference.Exists(file));
					}
				}
			}

			await localSocket.CloseAsync(CancellationToken.None);
			await agentSocket.CloseAsync(CancellationToken.None);
		}

		static int GetAvailablePort()
		{
#if NET8_0_OR_GREATER
			using TcpListener listener = new(IPAddress.Loopback, 0);
#else
			TcpListener listener = new(IPAddress.Loopback, 0);
#endif
			listener.Start();
			int port = ((IPEndPoint)listener.LocalEndpoint).Port;
			listener.Stop();
			return port;
		}

		static async Task RunAgentAsync(ComputeSocket socket, DirectoryReference tempDir, CancellationToken cancellationToken)
		{
			try
			{
				AgentMessageHandler handler = new AgentMessageHandler(tempDir, null, true, null, null, NullLogger.Instance);
				await handler.RunAsync(socket, cancellationToken);
			}
			catch (Exception e)
			{
				Console.WriteLine("Exception when running agent:\n" + e);
				throw;
			}
		}
	}
}
