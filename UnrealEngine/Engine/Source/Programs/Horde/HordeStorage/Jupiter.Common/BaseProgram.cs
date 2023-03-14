// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Hosting;
using Serilog;

namespace Jupiter
{
    // ReSharper disable once UnusedMember.Global
    public static class BaseProgram<T> where T : BaseStartup
    {
        private static IConfiguration Configuration { get; } = GetConfigurationBuilder();

        private static IConfiguration GetConfigurationBuilder()
        {
            string env = Environment.GetEnvironmentVariable("ASPNETCORE_ENVIRONMENT") ?? "Production";
            string mode = Environment.GetEnvironmentVariable("HORDESTORAGE_MODE") ?? "DefaultMode";
            string configRoot = "/config";
            // set the config root to config under the current directory for windows
            if (OperatingSystem.IsWindows())
            {
                configRoot = Path.Combine(Directory.GetCurrentDirectory(), "config");
            }
            return new ConfigurationBuilder()
                .SetBasePath(Directory.GetCurrentDirectory())
                .AddJsonFile("appsettings.json", false, false)
                .AddJsonFile(
                    path:
                    $"appsettings.{env}.json",
                    true)
                .AddYamlFile(
                    path:
                    $"appsettings.{mode}.yaml",
                    true)
                .AddYamlFile(Path.Combine(configRoot, "appsettings.Local.yaml"), optional: true, reloadOnChange: true)
                .AddEnvironmentVariables()
                .Build();

        }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1000:Do not declare static members on generic types", Justification = "Keep logic for the base program together")]
        public static int BaseMain(string[] args)
        {
            Log.Logger = new LoggerConfiguration()
                .ReadFrom.Configuration(Configuration)
                .CreateLogger();

            try
            {
                Log.Information("Creating ASPNET Host");
                CreateHostBuilder(args).Build().Run();
                return 0;
            }
            catch (Exception ex)
            {
                Log.Fatal(ex, "Host terminated unexpectedly");
                return 1;
            }
            finally
            {
                Log.CloseAndFlush();
            }
        }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1000:Do not declare static members on generic types", Justification = "Keep logic for the base program together")]
        public static IHostBuilder CreateHostBuilder(string[] args)
        {
            return Host.CreateDefaultBuilder(args)
                .ConfigureWebHostDefaults(webBuilder =>
                {
                    webBuilder.UseStartup<T>();
                    webBuilder.UseConfiguration(Configuration);
                    webBuilder.UseSerilog();
                    // configure microsoft.extensions.logging to configure log4net to allow us to set it in our appsettings
                    // Disabled forwarding of log4net logs into serilog, as the AWS sdk is very spammy with its output producing multiple errors for a 404 (which isn't even an error in the first place)
                    // This can be enabled if you need to investigate some more complicated AWS sdk issue
                    /*webBuilder.ConfigureLogging((hostingContext, logging) =>
                    {
                        // configure log4net (used by aws sdk) to write to serilog so we get the logs in the system we want it in
                        Log4net.Appender.Serilog.Configuration.Configure();
                    });*/
                    // remove the server header from kestrel
                    webBuilder.ConfigureKestrel(options =>
                    {
                        options.AddServerHeader = false;
                    });
                });
        }
    }
}
