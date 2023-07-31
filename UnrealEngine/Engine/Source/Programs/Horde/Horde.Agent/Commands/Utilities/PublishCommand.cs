// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.IO.Compression;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Threading.Tasks;
using EpicGames.Core;
using Grpc.Net.Client;
using Horde.Agent.Utility;
using HordeCommon.Rpc;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Utilities
{
	/// <summary>
	/// Publishes the contents of this application directory to the server
	/// </summary>
	[Command("Publish", "Publishes this application to the server")]
	class PublishCommand : Command
	{
		/// <summary>
		/// The server to upload the software to
		/// </summary>
		[CommandLine("-Server=", Required = true)]
		public string Server { get; set; } = null!;

		/// <summary>
		/// Access token used to authenticate with the server
		/// </summary>
		[CommandLine("-Token=", Required = true)]
		public string Token { get; set; } = null!;

		/// <summary>
		/// The input directory
		/// </summary>
		[CommandLine("-InputDir=", Required = true)]
		public DirectoryReference InputDir { get; set; } = null!;

		/// <summary>
		/// The channel to post to
		/// </summary>
		[CommandLine("-Channel=", Required = true)]
		public string Channel { get; set; } = null!;

		/// <summary>
		/// Main entry point for this command
		/// </summary>
		/// <param name="logger"></param>
		/// <returns>Async task</returns>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			// Create the zip archive
			logger.LogInformation("Creating archive...");
			byte[] archiveData;
			using (MemoryStream outputStream = new MemoryStream())
			{
				using (ZipArchive outputArchive = new ZipArchive(outputStream, ZipArchiveMode.Create, true))
				{
					foreach (FileReference inputFile in DirectoryReference.EnumerateFiles(InputDir, "*", SearchOption.AllDirectories))
					{
						string relativePath = inputFile.MakeRelativeTo(InputDir).Replace(Path.DirectorySeparatorChar, '/');
						if (!relativePath.Equals("Log.json", StringComparison.OrdinalIgnoreCase) && !relativePath.Equals("Log.txt", StringComparison.OrdinalIgnoreCase))
						{
							logger.LogInformation("  Adding {RelativePath}", relativePath);

							ZipArchiveEntry outputEntry = outputArchive.CreateEntry(relativePath, CompressionLevel.Optimal);

							using Stream inputEntryStream = FileReference.Open(inputFile, FileMode.Open, FileAccess.Read, FileShare.Read);
							using Stream outputEntryStream = outputEntry.Open();

							await inputEntryStream.CopyToAsync(outputEntryStream);
						}
					}
				}
				archiveData = outputStream.ToArray();
			}
			logger.LogInformation("Complete ({Size:n0} bytes)", archiveData.Length);

			// Read the application settings
			IConfigurationRoot config = new ConfigurationBuilder()
				.SetBasePath(Program.AppDir.FullName)
				.AddJsonFile("appsettings.json", true)
				.AddJsonFile("appsettings.Production.json", true)
				.Build();

			AgentSettings settings = new AgentSettings();
			config.GetSection("Horde").Bind(settings);

			// Get the server we're using
			ServerProfile serverProfile = settings.GetServerProfile(Server);

			// Create the http message handler
			using HttpClientHandler handler = new HttpClientHandler();
			handler.CheckCertificateRevocationList = true;
			handler.ServerCertificateCustomValidationCallback += (sender, cert, chain, errors) => CertificateHelper.CertificateValidationCallBack(logger, sender, cert, chain, errors, serverProfile);

			// Create a new http client for uploading
			using HttpClient httpClient = new HttpClient(handler);
			httpClient.BaseAddress = serverProfile.Url;
			httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", Token);

			GrpcChannelOptions channelOptions = new GrpcChannelOptions
			{
				HttpClient = httpClient,
				MaxReceiveMessageSize = 100 * 1024 * 1024, // 100 MB
				MaxSendMessageSize = 100 * 1024 * 1024 // 100 MB
			};
			using (GrpcChannel rpcChannel = GrpcChannel.ForAddress(serverProfile.Url, channelOptions))
			{
				HordeRpc.HordeRpcClient rpcClient = new HordeRpc.HordeRpcClient(rpcChannel);

				UploadSoftwareRequest uploadRequest = new UploadSoftwareRequest();
				uploadRequest.Channel = Channel;
				uploadRequest.Data = Google.Protobuf.ByteString.CopyFrom(archiveData);

				UploadSoftwareResponse uploadResponse = await rpcClient.UploadSoftwareAsync(uploadRequest);
				logger.LogInformation("Created software (version={SoftwareId} channel={Default})", uploadResponse.Version, uploadRequest.Channel);
			}

			return 0;
		}
	}
}
