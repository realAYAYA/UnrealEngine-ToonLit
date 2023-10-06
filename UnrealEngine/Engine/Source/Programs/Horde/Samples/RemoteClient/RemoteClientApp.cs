// Copyright Epic Games, Inc. All Rights Reserved.

using System.Buffers.Binary;
using System.Net.Http.Headers;
using System.Reflection;
using EpicGames.Core;
using EpicGames.Horde.Common;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Compute.Buffers;
using EpicGames.Horde.Compute.Clients;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.OIDC;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;

namespace RemoteClient
{
	class ClientAppOptions
	{
		[CommandLine("-Server=")]
		public string? Server { get; set; }

		[CommandLine("-Oidc=")]
		public string? OidcProvider { get; set; }

		[CommandLine("-Condition=")]
		public string? Condition { get; set; }

		[CommandLine("-Cpp")]
		public bool UseCppWorker { get; set; }
	}

	class ClientApp
	{
		static FileReference CurrentAssemblyFile { get; } = new FileReference(Assembly.GetExecutingAssembly().Location);
		static DirectoryReference ClientSourceDir { get; } = DirectoryReference.Combine(CurrentAssemblyFile.Directory, "../../..");

		static async Task Main(string[] args)
		{
			ILogger logger = new DefaultConsoleLogger(LogLevel.Trace);

			// Parse the command line arguments
			ClientAppOptions options = new ClientAppOptions();
			CommandLineArguments arguments = new CommandLineArguments(args);
			arguments.ApplyTo(options);
			arguments.CheckAllArgumentsUsed(logger);

			// Create the client to handle our requests
			await using IComputeClient client = await CreateClientAsync(options, logger);

			// Allocate a worker
			Requirements? requirements = null;
			if (options.Condition != null)
			{
				requirements = new Requirements(Condition.Parse(options.Condition));
			}

			await using IComputeLease? lease = await client.TryAssignWorkerAsync(new ClusterId("default"), requirements);
			if (lease == null)
			{
				logger.LogInformation("Unable to connect to remote");
				return;
			}

			// Run the worker
			if (options.UseCppWorker)
			{
				FileReference remoteServerFile = FileReference.Combine(ClientSourceDir, "../RemoteWorkerCpp/bin/RemoteServerCpp.exe");
				await RunRemoteAsync(lease, remoteServerFile.Directory, "RemoteWorkerCpp.exe", new List<string>(), logger);
			}
			else
			{
				FileReference remoteServerFile = FileReference.Combine(ClientSourceDir, "../RemoteWorker", CurrentAssemblyFile.Directory.MakeRelativeTo(ClientSourceDir), "RemoteWorker.dll");
				await RunRemoteAsync(lease, remoteServerFile.Directory, @"C:\Program Files\dotnet\dotnet.exe", new List<string> { remoteServerFile.GetFileName() }, logger);
			}
		}

		const int PrimaryChannelId = 0;
		const int BackgroundChannelId = 1;
		const int ChildProcessChannelId = 100;

		static async Task RunRemoteAsync(IComputeLease lease, DirectoryReference uploadDir, string executable, List<string> arguments, ILogger logger)
		{
			// Create a message channel on channel id 0. The Horde Agent always listens on this channel for requests.
			using (AgentMessageChannel channel = lease.Socket.CreateAgentMessageChannel(PrimaryChannelId, 4 * 1024 * 1024, logger))
			{
				await channel.WaitForAttachAsync();

				// Fork another message loop. We'll use this to run an XOR task in the background.
				using AgentMessageChannel backgroundChannel = lease.Socket.CreateAgentMessageChannel(BackgroundChannelId, 4 * 1024 * 1024, logger);
				await using BackgroundTask otherChannelTask = BackgroundTask.StartNew(ctx => RunBackgroundXorAsync(backgroundChannel));
				await channel.ForkAsync(BackgroundChannelId, 4 * 1024 * 1024, default);

				// Upload the sandbox to the primary channel.
				MemoryStorageClient storage = new MemoryStorageClient();
				await using (IStorageWriter treeWriter = storage.CreateWriter())
				{
					DirectoryNode sandbox = new DirectoryNode();
					await sandbox.CopyFromDirectoryAsync(uploadDir.ToDirectoryInfo(), new ChunkingOptions(), treeWriter, null);
					BlobHandle handle = await treeWriter.FlushAsync(sandbox);
					await channel.UploadFilesAsync("", handle.GetLocator(), storage);
				}

				// Run the task remotely in the background and echo the output to the console
				await using (AgentManagedProcess process = await channel.ExecuteAsync(executable, arguments, null, null))
				{
					await using BackgroundTask tickTask = BackgroundTask.StartNew(ctx => WriteNumbersAsync(lease.Socket, logger, ctx));

					string? line;
					while ((line = await process.ReadLineAsync()) != null)
					{
						logger.LogInformation("[REMOTE] {Line}", line);
					}
				}
			}
			await lease.CloseAsync();
		}
		
		static async Task WriteNumbersAsync(ComputeSocket socket, ILogger logger, CancellationToken cancellationToken)
		{
			// Wait until the remote sends a message indicating that it's ready
			using (PooledBuffer recvBuffer = new PooledBuffer(1, 20))
			{
				socket.AttachRecvBuffer(ChildProcessChannelId, recvBuffer.Writer);
				await recvBuffer.Reader.WaitToReadAsync(1, cancellationToken);
			}

			// Write data to the child process channel. The remote server will echo them back to us as it receives them, then exit when the channel is complete/closed.
			byte[] buffer = new byte[4];
			for (int idx = 0; idx < 3; idx++)
			{
				cancellationToken.ThrowIfCancellationRequested();
				logger.LogInformation("Writing value: {Value}", idx);
				BinaryPrimitives.WriteInt32LittleEndian(buffer, idx);
				await socket.SendAsync(ChildProcessChannelId, buffer, cancellationToken);
				await Task.Delay(1000, cancellationToken);
			}

			await socket.MarkCompleteAsync(ChildProcessChannelId, cancellationToken);
		}

		static async Task RunBackgroundXorAsync(AgentMessageChannel channel)
		{
			await channel.WaitForAttachAsync();

			byte[] dataToXor = new byte[] { 1, 2, 3, 4, 5 };
			await channel.SendXorRequestAsync(dataToXor, 123);

			using AgentMessage response = await channel.ReceiveAsync(AgentMessageType.XorResponse);
			for (int idx = 0; idx < dataToXor.Length; idx++)
			{
				if (response.Data.Span[idx] != (byte)(dataToXor[idx] ^ 123))
				{
					throw new InvalidOperationException();
				}
			}

			await channel.CloseAsync();
		}

		static async Task<IComputeClient> CreateClientAsync(ClientAppOptions options, ILogger logger)
		{
			if (options.Server == null)
			{
				DirectoryReference sandboxDir = DirectoryReference.Combine(new FileReference(Assembly.GetExecutingAssembly().Location).Directory, "Sandbox");
				return new LocalComputeClient(2000, sandboxDir, logger);
			}
			else
			{
				AuthenticationHeaderValue? authHeader = await GetAuthHeaderAsync(options, logger);
				return new ServerComputeClient(new Uri(options.Server), authHeader, logger);
			}
		}

		static async Task<AuthenticationHeaderValue?> GetAuthHeaderAsync(ClientAppOptions options, ILogger logger)
		{
			if (options.OidcProvider == null)
			{
				return null;
			}

			for (DirectoryReference? currentDir = CurrentAssemblyFile.Directory; currentDir != null; currentDir = currentDir.ParentDirectory)
			{
				FileReference buildVersionFile = FileReference.Combine(currentDir, "Build/Build.version");
				if (FileReference.Exists(buildVersionFile))
				{
					string bearerToken = await GetOidcBearerTokenAsync(currentDir, null, options.OidcProvider, logger);
					return new AuthenticationHeaderValue("Bearer", bearerToken);
				}
			}

			throw new Exception($"Unable to find engine directory above {CurrentAssemblyFile}");
		}

		static async Task<string> GetOidcBearerTokenAsync(DirectoryReference engineDir, DirectoryReference? projectDir, string oidcProvider, ILogger logger)
		{
			logger.LogInformation("Performing OIDC token refresh...");

			using ITokenStore tokenStore = TokenStoreFactory.CreateTokenStore();
			IConfiguration providerConfiguration = ProviderConfigurationFactory.ReadConfiguration(engineDir.ToDirectoryInfo(), projectDir?.ToDirectoryInfo());
			OidcTokenManager oidcTokenManager = OidcTokenManager.CreateTokenManager(providerConfiguration, tokenStore, new List<string>() { oidcProvider });

			OidcTokenInfo result;
			try
			{
				result = await oidcTokenManager.GetAccessToken(oidcProvider);
			}
			catch (NotLoggedInException)
			{
				result = await oidcTokenManager.Login(oidcProvider);
			}

			if (result.AccessToken == null)
			{
				throw new Exception($"Unable to get access token for {oidcProvider}");
			}

			logger.LogInformation("Received bearer token for {OidcProvider}", oidcProvider);
			return result.AccessToken;
		}
	}
}