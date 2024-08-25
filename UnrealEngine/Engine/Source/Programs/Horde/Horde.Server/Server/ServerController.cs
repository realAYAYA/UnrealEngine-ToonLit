// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Server;
using EpicGames.Perforce;
using Horde.Server.Agents;
using Horde.Server.Agents.Pools;
using Horde.Server.Configuration;
using Horde.Server.Perforce;
using Horde.Server.Tools;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;

namespace Horde.Server.Server
{
	/// <summary>
	/// Controller managing account status
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class ServerController : HordeControllerBase
	{
		readonly IServiceProvider _serviceProvider;
		readonly IToolCollection _toolCollection;
		readonly IClock _clock;
		readonly ConfigService _configService;
		readonly IPerforceService _perforceService;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public ServerController(IServiceProvider serviceProvider, IToolCollection toolCollection, IClock clock, ConfigService configService, IPerforceService perforceService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_serviceProvider = serviceProvider;
			_toolCollection = toolCollection;
			_clock = clock;
			_configService = configService;
			_perforceService = perforceService;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Get server version
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/version")]
		public ActionResult GetVersion()
		{
			FileVersionInfo fileVersionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
			return Ok(fileVersionInfo.ProductVersion);
		}

		/// <summary>
		/// Get server information
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/info")]
		[ProducesResponseType(typeof(GetServerInfoResponse), 200)]
		public async Task<ActionResult<GetServerInfoResponse>> GetServerInfoAsync()
		{
			GetServerInfoResponse response = new GetServerInfoResponse();
			response.ApiVersion = HordeApiVersion.Latest;

			FileVersionInfo versionInfo = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
			response.ServerVersion = versionInfo.ProductVersion ?? String.Empty;
			response.OsDescription = RuntimeInformation.OSDescription;

			ITool? tool = await _toolCollection.GetAsync(AgentExtensions.AgentToolId, _globalConfig.Value);
			if (tool != null)
			{
				IToolDeployment? deployment = tool.GetCurrentDeployment(1.0, _clock.UtcNow);
				if (deployment != null)
				{
					response.AgentVersion = deployment.Version;
				}
			}

			return response;
		}

		/// <summary>
		/// Gets connection information
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/connection")]
		public ActionResult<GetConnectionResponse> GetConnection()
		{
			GetConnectionResponse response = new GetConnectionResponse();
			response.Ip = HttpContext.Connection.RemoteIpAddress?.ToString();
			response.Port = HttpContext.Connection.RemotePort;
			return response;
		}

		/// <summary>
		/// Gets ports used by the server
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/ports")]
		public ActionResult<GetPortsResponse> GetPorts()
		{
			ServerSettings serverSettings = _globalConfig.Value.ServerSettings;

			GetPortsResponse response = new GetPortsResponse();
			response.Http = serverSettings.HttpPort;
			response.Https = serverSettings.HttpsPort;
			response.UnencryptedHttp2 = serverSettings.Http2Port;
			return response;
		}

		/// <summary>
		/// Returns settings for automating auth against this server
		/// </summary>
		[HttpGet]
		[AllowAnonymous]
		[Route("/api/v1/server/auth")]
		public ActionResult<GetAuthConfigResponse> GetAuthConfig()
		{
			ServerSettings settings = _globalConfig.Value.ServerSettings;

			GetAuthConfigResponse response = new GetAuthConfigResponse();
			response.Method = settings.AuthMethod;
			response.ProfileName = settings.OidcProfileName;
			if (settings.AuthMethod == AuthMethod.Horde)
			{
				response.ServerUrl = new Uri(_globalConfig.Value.ServerSettings.ServerUrl, "api/v1/oauth2").ToString();
				response.ClientId = "default";
			}
			else
			{
				response.ServerUrl = settings.OidcAuthority;
				response.ClientId = settings.OidcClientId;
			}
			response.LocalRedirectUrls = settings.OidcLocalRedirectUrls;
			return response;
		}

		/// <summary>
		/// Returns settings for automating auth against this server
		/// </summary>
		[HttpPost]
		[Route("/api/v1/server/preflightconfig")]
		public async Task<ActionResult<PreflightConfigResponse>> PreflightConfigAsync(PreflightConfigRequest request, CancellationToken cancellationToken)
		{
			string cluster = request.Cluster ?? "default";

			IPooledPerforceConnection perforce = await _perforceService.ConnectAsync(cluster, cancellationToken: cancellationToken);

			PerforceResponse<DescribeRecord> describeResponse = await perforce.TryDescribeAsync(DescribeOptions.Shelved, -1, request.ShelvedChange, cancellationToken);
			if (!describeResponse.Succeeded)
			{
				return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "CL {Change} does not exist.", request.ShelvedChange);
			}

			DescribeRecord record = describeResponse.Data;

			List<string> configFiles = new List<string> { "/globals.json", "global.json", ".project.json", ".stream.json", ".dashboard.json", ".telemetry.json" };

			Dictionary<Uri, byte[]> files = new Dictionary<Uri, byte[]>();
			foreach (DescribeFileRecord fileRecord in record.Files)
			{
				if (configFiles.FirstOrDefault(config => fileRecord.DepotFile.EndsWith(config, StringComparison.OrdinalIgnoreCase)) != null)
				{
					PerforceResponse<PrintRecord<byte[]>> printRecordResponse = await perforce.TryPrintDataAsync($"{fileRecord.DepotFile}@={request.ShelvedChange}", cancellationToken);
					if (!printRecordResponse.Succeeded || printRecordResponse.Data.Contents == null)
					{
						return BadRequest($"Unable to print contents of {fileRecord.DepotFile}@={request.ShelvedChange}");
					}

					PrintRecord<byte[]> printRecord = printRecordResponse.Data;

					Uri uri = new Uri($"perforce://{cluster}{printRecord.DepotFile}");
					files.Add(uri, printRecord.Contents);
				}
			}

			if (files.Count == 0)
			{
				return BadRequest(KnownLogEvents.Horde_InvalidPreflight, "No config files found in CL {Change}.", request.ShelvedChange);
			}

			string? message = await _configService.ValidateAsync(files, cancellationToken);

			PreflightConfigResponse response = new PreflightConfigResponse();
			response.Result = message == null;
			response.Message = message;

			return response;
		}

		/// <summary>
		/// Converts all legacy pools into config entries
		/// </summary>
		[HttpGet]
		[Route("/api/v1/server/migrate/pool-config")]
		public async Task<ActionResult<object>> MigratePoolsAsync([FromQuery] int? minAgents = null, [FromQuery] int? maxAgents = null, CancellationToken cancellationToken = default)
		{
			if (!_globalConfig.Value.Authorize(PoolAclAction.ListPools, User))
			{
				return Forbid(PoolAclAction.ListPools);
			}

			IPoolCollection poolCollection = _serviceProvider.GetRequiredService<IPoolCollection>();
			List<IPoolConfig> poolConfigs = (await poolCollection.GetConfigsAsync(cancellationToken)).ToList();
			HashSet<PoolId> removePoolIds = _globalConfig.Value.Pools.Select(x => x.Id).ToHashSet();
			poolConfigs.RemoveAll(x => removePoolIds.Contains(x.Id));

			if (minAgents != null || maxAgents != null)
			{
				IAgentCollection agentCollection = _serviceProvider.GetRequiredService<IAgentCollection>();

				Dictionary<PoolId, int> poolIdToCount = new Dictionary<PoolId, int>();

				IReadOnlyList<IAgent> agents = await agentCollection.FindAsync(cancellationToken: cancellationToken);
				foreach (IAgent agent in agents)
				{
					foreach (PoolId poolId in agent.GetPools())
					{
						int count;
						if (!poolIdToCount.TryGetValue(poolId, out count))
						{
							count = 0;
						}
						poolIdToCount[poolId] = count + 1;
					}
				}

				if (minAgents != null && minAgents.Value > 0)
				{
					poolConfigs.RemoveAll(x => !poolIdToCount.TryGetValue(x.Id, out int count) || count < minAgents.Value);
				}
				if (maxAgents != null)
				{
					poolConfigs.RemoveAll(x => poolIdToCount.TryGetValue(x.Id, out int count) && count > maxAgents.Value);
				}
			}

			List<PoolConfig> configs = new List<PoolConfig>();
			foreach (IPoolConfig currentConfig in poolConfigs.OrderBy(x => x.Id.Id.Text))
			{
				PoolConfig config = new PoolConfig();
				config.Id = currentConfig.Id;
				config.Name = currentConfig.Name;
				config.Condition = currentConfig.Condition;
				if (currentConfig.Properties != null && currentConfig.Properties.Count > 0 && (currentConfig.Properties.Count != 0 && (currentConfig.Properties.First().Key != "color" && currentConfig.Properties.First().Value != "0")))
				{
					config.Properties = new Dictionary<string, string>(currentConfig.Properties);
				}
				config.EnableAutoscaling = currentConfig.EnableAutoscaling;
				config.MinAgents = currentConfig.MinAgents;
				config.NumReserveAgents = currentConfig.NumReserveAgents;
				config.ConformInterval = currentConfig.ConformInterval;
				config.ScaleInCooldown = currentConfig.ScaleInCooldown;
				config.ScaleOutCooldown = currentConfig.ScaleOutCooldown;
				config.ShutdownIfDisabledGracePeriod = currentConfig.ShutdownIfDisabledGracePeriod;
#pragma warning disable CS0618 // Type or member is obsolete
				config.SizeStrategy = currentConfig.SizeStrategy;
#pragma warning restore CS0618 // Type or member is obsolete
				if (currentConfig.SizeStrategies != null && currentConfig.SizeStrategies.Count > 0)
				{
					config.SizeStrategies = currentConfig.SizeStrategies.ToList();
				}
				if (currentConfig.FleetManagers != null && currentConfig.FleetManagers.Count > 0)
				{
					config.FleetManagers = currentConfig.FleetManagers.ToList();
				}
				config.LeaseUtilizationSettings = currentConfig.LeaseUtilizationSettings;
				config.JobQueueSettings = currentConfig.JobQueueSettings;
				config.ComputeQueueAwsMetricSettings = currentConfig.ComputeQueueAwsMetricSettings;
				configs.Add(config);
			}

			return new { Pools = configs };
		}
	}
}
