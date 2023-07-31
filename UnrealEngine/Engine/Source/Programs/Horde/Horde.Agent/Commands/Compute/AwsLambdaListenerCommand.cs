// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Amazon.SimpleSystemsManagement;
using Amazon.SimpleSystemsManagement.Model;
using EpicGames.Core;
using EpicGames.Horde.Auth;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Impl;
using Google.Protobuf;
using Grpc.Core;
using Horde.Agent.Services;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Serilog;
using Serilog.Events;
using Command = EpicGames.Core.Command;
using ILogger = Microsoft.Extensions.Logging.ILogger;

namespace Horde.Agent.Commands.Compute
{
	/// <summary>
	/// Run WorkerService inside an AWS Lambda function invocation
	/// Will be run in "one-shot mode" where either one lease will be handled, or max wait time is encountered.
	/// Since the running time of a single Lambda function invocation is limited to 15 minutes,
	/// we want to handle only one lease before returning.
	/// </summary>
	class WorkerServiceLambda : IAwsLambdaFunction
	{
		private readonly Func<string, WorkerService> _createWorkerService;
		
		public WorkerServiceLambda(Func<string, WorkerService> createWorkerService)
		{
			_createWorkerService = createWorkerService;
		}

		/// <inheritdoc />
		public async Task<ReadOnlyMemory<byte>> OnLambdaInvokeAsync(string requestId, ReadOnlyMemory<byte> requestData, CancellationToken cancellationToken)
		{
			bool isProtobufCompatibleJson = requestData.Length >= 1 && requestData.Span[0] == '{';
			AwsLambdaListenRequest request = isProtobufCompatibleJson ?
				AwsLambdaListenRequest.Parser.ParseJson(Encoding.UTF8.GetString(requestData.Span)) :
				AwsLambdaListenRequest.Parser.ParseFrom(requestData.Span);

			TimeSpan maxWaitTime = TimeSpan.FromMilliseconds(request.MaxWaitTimeForLeaseMs);
			using WorkerService service = _createWorkerService(requestId);
			bool isLeaseActive = false;
			service.OnLeaseActive += lease => isLeaseActive = true;
			using CancellationTokenSource maxWaitTimeCts = new();

			Task _ = Task.Run(async () =>
			{
				await Task.Delay(maxWaitTime, cancellationToken);
				if (!isLeaseActive)
				{
					maxWaitTimeCts.Cancel();
				}
			}, cancellationToken);

			// Combine tokens so either max wait time or original token can cancel
			using CancellationTokenSource linkedCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, maxWaitTimeCts.Token);
			try
			{
				await service.HandleSessionAsync(true, false, linkedCts.Token);
			}
			catch (RpcException e)
			{
				// Don't bubble exceptions for cancellations as they can happen naturally from maxWaitTime parameter above
				if (e.Status.StatusCode != StatusCode.Cancelled)
				{
					throw;
				}
			}
			
			AwsLambdaListenResponse response = new () { DidAcceptLease = service.NumLeasesCompleted > 0 };
			byte[] responseData = isProtobufCompatibleJson ?
				Encoding.UTF8.GetBytes(JsonFormatter.Default.Format(response)) :
				response.ToByteArray();

			return responseData;
		}
	}

	/// <summary>
	/// Run agent inside an AWS Lambda function.
	/// </summary>
	[Command("Compute", "AwsLambda", "Listen for AWS Lambda invocations")]
	class AwsLambdaListenerCommand : Command
	{
		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			LogLevel logLevel;
			string logLevelStr = Environment.GetEnvironmentVariable("UE_HORDE_LOG_LEVEL") ?? "debug";

			if (Enum.TryParse(logLevelStr, true, out LogLevel logEventLevel))
			{
				logLevel = logEventLevel;
			}
			else
			{
				logger.LogError("Unable to parse log level: {LogLevelStr}", logLevelStr);
				return 1;
			}

			Serilog.Core.Logger serilogLogger = new LoggerConfiguration()
				.MinimumLevel.Debug()
				.MinimumLevel.Override("Microsoft", LogEventLevel.Warning)
				.Enrich.FromLogContext()
				.WriteTo.Console()
				.CreateLogger();
			
			using ILoggerFactory loggerFactory = LoggerFactory.Create(logging => { logging.AddSerilog(serilogLogger); });
			ILogger<WorkerService> workerServiceLogger = loggerFactory.CreateLogger<WorkerService>();
			ILogger<GrpcService> grpcServiceLogger = loggerFactory.CreateLogger<GrpcService>();
			ILogger<AwsLambdaClient> lambdaClientLogger = loggerFactory.CreateLogger<AwsLambdaClient>();
			
			AwsLambdaClient lambdaClient = AwsLambdaClient.InitFromEnv(lambdaClientLogger);
			
			using CancellationTokenSource cts = new ();
			try
			{
				logger.LogInformation("Initializing AWS Lambda executor...");

				HttpServiceClientOptions clientOptions = await CreateHordeStorageOptions(cts.Token);
				using HttpClient httpClient = new ();
				OAuthHandlerFactory oAuthHandlerFactory = new (httpClient);
				OAuthHandler<HttpStorageClient> oAuthHandler = oAuthHandlerFactory.Create<HttpStorageClient>(clientOptions);
				using HttpClient client = new (oAuthHandler);
				client.BaseAddress = clientOptions.Url;
				IStorageClient storageClient = new HttpStorageClient(client);

				AgentSettings agentSettings = await CreateAgentSettingsAsync(cts.Token);
				agentSettings.WorkingDir = Environment.GetEnvironmentVariable("UE_HORDE_WORKING_DIR") ?? "/tmp";
				agentSettings.PerforceExecutor.RunConform = false;
				
				OptionsWrapper<AgentSettings> agentSettingsOptions = new (agentSettings);
				GrpcService grpcService = new (agentSettingsOptions, grpcServiceLogger);

				AppDomain.CurrentDomain.ProcessExit += (s, e) => 
				{
					logger.LogInformation("ProcessExit triggered. Shutting down...");
					cts.Cancel();
				};

				WorkerServiceLambda lambdaFunction = new ((requestId) =>
				{
					agentSettingsOptions.Value.Name = "lambda-" + requestId;
					return new WorkerService(workerServiceLogger, agentSettingsOptions, grpcService, storageClient);
				});
				bool success = await lambdaClient.ListenForInvocationsAsync(lambdaFunction, cts.Token);
				return success ? 1 : 0;
			}
			catch (Exception e)
			{
				logger.LogError(e, "Error initializing");
				try
				{
					List<string>? stacktraceLines = e.StackTrace?.Split("\n").ToList();
					await lambdaClient.SendInitErrorAsync("general", $"{e.GetType()}: {e.Message}", stacktraceLines);
				}
				catch (AwsLambdaClientException initException)
				{
					logger.LogError(initException, "Bad response when sending init error. isFatal={IsFatal}", initException.IsFatal);
				}
				
				return 1;
			}
		}

		private static async Task<string> GetSsmParameterSecretAsync(string name, CancellationToken cancellationToken)
		{
			using AmazonSimpleSystemsManagementClient ssmClient = new ();
			GetParameterRequest request = new () { Name = name, WithDecryption = true };
			GetParameterResponse response = await ssmClient.GetParameterAsync(request, cancellationToken);
			return response.Parameter.Value;
		}

		private static async Task<HttpServiceClientOptions> CreateHordeStorageOptions(CancellationToken cancellationToken)
		{
			return new HttpServiceClientOptions
			{
				Url = new Uri(GetEnvVar("UE_HORDE_STORAGE_URL")),
				AuthUrl = new Uri(GetEnvVar("UE_HORDE_STORAGE_OAUTH_URL")),
				GrantType = GetEnvVar("UE_HORDE_STORAGE_OAUTH_GRANT_TYPE"),
				ClientId = GetEnvVar("UE_HORDE_STORAGE_OAUTH_CLIENT_ID"),
				ClientSecret = await GetSsmParameterSecretAsync(GetEnvVar("UE_HORDE_STORAGE_OAUTH_CLIENT_SECRET_ARN"), cancellationToken),
				Scope = GetEnvVar("UE_HORDE_STORAGE_OAUTH_SCOPE"),
			};
		}
		
		private static async Task<AgentSettings> CreateAgentSettingsAsync(CancellationToken cancellationToken)
		{
			AgentSettings settings = new ();
			settings.Server = "AwsLambdaDefault"; 
			settings.ServerProfiles.Add(new ServerProfile
			{
				Name = settings.Server,
				Url = new Uri(GetEnvVar("UE_HORDE_BUILD_URL")),
				Token = await GetSsmParameterSecretAsync(GetEnvVar("UE_HORDE_BUILD_TOKEN"), cancellationToken)
			});
			return settings;
		}

		private static string GetEnvVar(string name)
		{
			string? value = Environment.GetEnvironmentVariable(name);
			if (value == null)
			{
				throw new ArgumentException($"Missing env var {name}");
			}
			return value;
		}
	}
}
