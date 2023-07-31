// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Server.Kestrel.Core;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Serilog;

namespace Horde.Build.Commands
{
	using ILogger = Microsoft.Extensions.Logging.ILogger;

	[Command("server", "Runs the Horde Build server (default)")]
	class ServerCommand : Command
	{
		readonly ServerSettings _hordeSettings;
		readonly IConfiguration _config;
		string[] _args = Array.Empty<string>();

		public ServerCommand(ServerSettings settings, IConfiguration config)
		{
			_hordeSettings = settings;
			_config = config;
		}

		public override void Configure(CommandLineArguments arguments, ILogger logger)
		{
			base.Configure(arguments, logger);
			_args = arguments.GetRawArray();
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			using (X509Certificate2? grpcCertificate = ReadGrpcCertificate(_hordeSettings))
			{
				using IHost host = CreateHostBuilderWithCert(_args, _config, _hordeSettings, grpcCertificate).Build();
				await host.RunAsync();
				return 0;
			}
		}

		static IHostBuilder CreateHostBuilderWithCert(string[] args, IConfiguration config, ServerSettings serverSettings, X509Certificate2? sslCert)
		{
			return Host.CreateDefaultBuilder(args)
				.UseSerilog()
				.ConfigureAppConfiguration(builder => builder.AddConfiguration(config))
				.ConfigureWebHostDefaults(webBuilder =>
				{
					webBuilder.ConfigureKestrel(options =>
					{
						options.Limits.MaxRequestBodySize = 100 * 1024 * 1024;

						if (serverSettings.HttpPort != 0)
						{
							options.ListenAnyIP(serverSettings.HttpPort, configure => { configure.Protocols = HttpProtocols.Http1AndHttp2; });
						}

						if (serverSettings.HttpsPort != 0)
						{
							options.ListenAnyIP(serverSettings.HttpsPort, configure => 
							{ 
								if (sslCert != null) 
								{ 
									configure.UseHttps(sslCert); 
								} 
							});
						}

						// To serve HTTP/2 with gRPC *without* TLS enabled, a separate port for HTTP/2 must be used.
						// This is useful when having a load balancer in front that terminates TLS.
						if (serverSettings.Http2Port != 0)
						{
							options.ListenAnyIP(serverSettings.Http2Port, configure => { configure.Protocols = HttpProtocols.Http2; });
						}
					});
					webBuilder.UseStartup<Startup>();
				});
		}

		/// <summary>
		/// Gets the certificate to use for Grpc endpoints
		/// </summary>
		/// <returns>Custom certificate to use for Grpc endpoints, or null for the default.</returns>
		public static X509Certificate2? ReadGrpcCertificate(ServerSettings hordeSettings)
		{
			string base64Prefix = "base64:";

			if (hordeSettings.ServerPrivateCert == null)
			{
				return null;
			}
			else if (hordeSettings.ServerPrivateCert.StartsWith(base64Prefix, StringComparison.Ordinal))
			{
				byte[] certData = Convert.FromBase64String(hordeSettings.ServerPrivateCert.Replace(base64Prefix, "", StringComparison.Ordinal));
				return new X509Certificate2(certData);
			}
			else
			{
				FileReference? serverPrivateCert;
				if (!Path.IsPathRooted(hordeSettings.ServerPrivateCert))
				{
					serverPrivateCert = FileReference.Combine(Program.AppDir, hordeSettings.ServerPrivateCert);
				}
				else
				{
					serverPrivateCert = new FileReference(hordeSettings.ServerPrivateCert);
				}

				return new X509Certificate2(FileReference.ReadAllBytes(serverPrivateCert));
			}
		}

		public static IHostBuilder CreateHostBuilderForTesting(string[] args)
		{
			ServerSettings hordeSettings = new ServerSettings();
			return CreateHostBuilderWithCert(args, new ConfigurationBuilder().Build(), hordeSettings, null);
		}
}
}
