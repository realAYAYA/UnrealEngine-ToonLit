// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using System;
using System.IO;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;
using System.Linq;
using EpicGames.OIDC;

namespace OidcToken
{
	class Program
	{
		static async Task Main(string[] args)
		{
			if (args.Any(s => s.Equals("--help") || s.Equals("-help")) || args.Length == 0)
			{
				// print help
				Console.WriteLine("Usage: OidcToken --Service <serviceName> [options]");
				Console.WriteLine("Service is a required parameter to indicate which OIDC service you intend to connect to. The connection details of the service is configured in appsettings.json");
				Console.WriteLine();
				Console.WriteLine("Options: ");
				Console.WriteLine(" --Mode [Query/GetToken] - Switch mode to allow you to preview operation without triggering user interaction (result can be used to determine if user interaction is required)");
				Console.WriteLine(" --OutFile <path> - Path to create json file of result");
				Console.WriteLine(" --ResultToConsole [true/false] - If true the resulting json file is output to stdout (and logs are not created)");
				Console.WriteLine(" --Unattended [true/false] - If true we assume no user is present and thus can not rely on their input");
				Console.WriteLine(" --Zen [true/false] - If true the resulting refresh token is posted to Zens token endpoints");
				Console.WriteLine(" --Project <path> - Project can be used to tell oidc token which game its working in to allow us to read game specific settings");

				return;
			}

			ConfigurationBuilder configBuilder = new();
			configBuilder.SetBasePath(AppContext.BaseDirectory)
				.AddJsonFile("appsettings.json", false, false)
				.AddCommandLine(args);

			IConfiguration config = configBuilder.Build();

			TokenServiceOptions options = new();
			config.Bind(options);

			// guess where the engine directory is based on the assumption that we are running out of Engine\Binaries\DotNET\OidcToken\<platform>
			DirectoryInfo engineDir = new DirectoryInfo(Path.Combine(AppContext.BaseDirectory, "../../../../../Engine"));
			if (!engineDir.Exists)
			{
				// try to see if engine dir can be found from the current code path Engine\Source\Programs\OidcToken\bin\<Configuration>\<.net-version>
				engineDir = new DirectoryInfo(Path.Combine(AppContext.BaseDirectory, "../../../../../../../Engine"));

				if (!engineDir.Exists)
				{
					throw new Exception($"Unable to guess engine directory so unable to continue running. Starting directory was: {AppContext.BaseDirectory}");
				}
			}

			await Host.CreateDefaultBuilder(args)
				.ConfigureAppConfiguration(builder =>
				{
					builder.AddConfiguration(config);
				})
				.ConfigureLogging(loggingBuilder =>
				{
					loggingBuilder.ClearProviders();

					if (!options.ResultToConsole)
					{
						loggingBuilder.AddConsole();
					}
				})
				.ConfigureServices(
				(content, services) =>
				{
					IConfiguration configuration = content.Configuration;
					services.AddOptions<TokenServiceOptions>().Bind(configuration).ValidateDataAnnotations();
					services.AddOptions<OidcTokenOptions>().Bind(ProviderConfigurationFactory.ReadConfiguration(engineDir, !string.IsNullOrEmpty(options.Project) ? new DirectoryInfo(options.Project) : null)).ValidateDataAnnotations();

					services.AddSingleton<OidcTokenManager>();
					services.AddTransient<ITokenStore>(TokenStoreFactory.CreateTokenStore);

					services.AddHostedService<TokenService>();
				})
				.RunConsoleAsync();
		}
	}
}