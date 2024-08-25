// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Security;
using System.Net.Sockets;
using Grpc.Core;
using Grpc.Core.Interceptors;
using Grpc.Net.Client;
using Horde.Agent.Utility;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Services
{
	/// <summary>
	/// Service which creates a configured Grpc channel
	/// </summary>
	class GrpcService
	{
		class GetPortsResponse
		{
			public int UnencryptedHttp2 { get; set; }
		}

		private readonly IOptions<AgentSettings> _settings;
		private readonly ServerProfile _serverProfile;
		private readonly ILogger _logger;
		private readonly ILoggerFactory _loggerFactory;

		/// <summary>
		/// The current server profile
		/// </summary>
		public ServerProfile ServerProfile => _serverProfile;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="settings"></param>
		/// <param name="logger"></param>
		/// <param name="loggerFactory"></param>
		public GrpcService(IOptions<AgentSettings> settings, ILogger<GrpcService> logger, ILoggerFactory loggerFactory)
		{
			_settings = settings;
			_serverProfile = settings.Value.GetCurrentServerProfile();
			_logger = logger;
			_loggerFactory = loggerFactory;
		}

		/// <summary>
		/// Create a GRPC channel with a default token
		/// </summary>
		/// <returns>New grpc channel</returns>
		public Task<GrpcChannel> CreateGrpcChannelAsync(CancellationToken cancellationToken)
		{
			return CreateGrpcChannelAsync(_serverProfile.Token, cancellationToken);
		}

		/// <summary>
		/// Create a GRPC channel with the given bearer token
		/// </summary>
		/// <returns>New grpc channel</returns>
		public async Task<GrpcChannel> CreateGrpcChannelAsync(string? bearerToken, CancellationToken cancellationToken)
		{
#pragma warning disable CA2000 // Dispose objects before losing scope
			// HTTP handler is disposed by GrpcChannel below
			SocketsHttpHandler httpHandler = new()
			{
				ConnectCallback = async (context, connectCt) =>
				{
					using IScope scope = GlobalTracer.Instance
						.BuildSpan($"{nameof(GrpcService)}.ConnectCallback")
						.WithResourceName(context.DnsEndPoint.Host)
						.StartActive();

					IPAddress[] ips;
					using (IScope _ = GlobalTracer.Instance.BuildSpan("DnsResolve").StartActive())
					{
						ips = await Dns.GetHostAddressesAsync(context.DnsEndPoint.Host, connectCt);
					}

					IPEndPoint ipEndpoint = new(ips[0], context.DnsEndPoint.Port);
					using (IScope _ = GlobalTracer.Instance.BuildSpan("TcpConnect").StartActive())
					{
						Socket socket = new(SocketType.Stream, ProtocolType.Tcp);
						try
						{
							await socket.ConnectAsync(ipEndpoint, connectCt);
							return new NetworkStream(socket, ownsSocket: true);
						}
						catch
						{
							socket.Dispose();
							throw;
						}
					}
				},

				SslOptions = new SslClientAuthenticationOptions
				{
					RemoteCertificateValidationCallback = (sender, cert, chain, errors) => CertificateHelper.CertificateValidationCallBack(_logger, sender, cert, chain, errors, _serverProfile)
				}
			};
#pragma warning restore CA2000 // Dispose objects before losing scope

			HttpClient httpClient = new(httpHandler, true);
			httpClient.DefaultRequestHeaders.Add("Accept", "application/json");
			if (bearerToken != null)
			{
				httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", bearerToken);
			}

			httpClient.Timeout = TimeSpan.FromSeconds(210); // Need to make sure this doesn't cancel any long running gRPC streaming calls (eg. session update)

			// Get the server URL for gRPC traffic. If we're using an unencrpyted connection we need to use a different port for http/2, so 
			// send a http1 request to the server to query it.
			Uri serverUri = _serverProfile.Url;
			if (serverUri.Scheme.Equals("http", StringComparison.Ordinal))
			{
				_logger.LogInformation("Querying server {BaseUrl} for rpc port", serverUri);
				using (HttpResponseMessage response = await httpClient.GetAsync(new Uri(serverUri, "api/v1/server/ports"), cancellationToken))
				{
					GetPortsResponse? ports = await response.Content.ReadFromJsonAsync<GetPortsResponse>(AgentApp.DefaultJsonSerializerOptions, cancellationToken);
					if (ports != null && ports.UnencryptedHttp2 != 0)
					{
						UriBuilder builder = new UriBuilder(serverUri);
						builder.Port = ports.UnencryptedHttp2;
						serverUri = builder.Uri;
					}
				}
			}

			_logger.LogInformation("Connecting to rpc server {BaseUrl}", serverUri);
			return GrpcChannel.ForAddress(serverUri, new GrpcChannelOptions
			{
				// Required payloads coming from CAS service can be large
				MaxReceiveMessageSize = 1024 * 1024 * 1024, // 1 GB
				MaxSendMessageSize = 1024 * 1024 * 1024, // 1 GB
				LoggerFactory = _loggerFactory,
				HttpClient = httpClient,
				DisposeHttpClient = true
			});
		}

		/// <summary>
		/// Get a gRPC call invoker for the given channel with extra metadata attached,
		/// such as current version and name
		/// </summary>
		/// <param name="channel">gRPC channel to use</param>
		/// <returns>A call invoker</returns>
		public CallInvoker GetInvoker(GrpcChannel channel)
		{
			CallInvoker invoker = channel.Intercept(headers =>
			{
				headers.Add("Horde-Agent-Version", AgentApp.Version);
				headers.Add("Horde-Agent-Name", _settings.Value.GetAgentName());
				return headers;
			});

			return invoker;
		}
	}
}
