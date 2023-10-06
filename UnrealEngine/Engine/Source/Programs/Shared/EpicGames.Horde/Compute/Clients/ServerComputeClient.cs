// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Compute.Transports;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Net.Sockets;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute.Clients
{
	/// <summary>
	/// Helper class to enlist remote resources to perform compute-intensive tasks.
	/// </summary>
	public sealed class ServerComputeClient : IComputeClient
	{
		/// <summary>
		/// Length of the nonce sent as part of handshaking between initiator and remote
		/// </summary>
		public const int NonceLength = 64;

		class AssignComputeRequest
		{
			public Requirements? Requirements { get; set; }
		}

		class AssignComputeResponse
		{
			public string Ip { get; set; } = String.Empty;
			public int Port { get; set; }
			public string Nonce { get; set; } = String.Empty;
			public string Key { get; set; } = String.Empty;
			public Dictionary<string, int> AssignedResources { get; set; } = new Dictionary<string, int>();
			public List<string> Properties { get; set; } = new List<string>();
		}

		record class LeaseInfo(IReadOnlyList<string> Properties, IReadOnlyDictionary<string, int> AssignedResources, ComputeSocket Socket);

		class LeaseImpl : IComputeLease
		{
			readonly IAsyncEnumerator<LeaseInfo> _source;

			public IReadOnlyList<string> Properties => _source.Current.Properties;
			public IReadOnlyDictionary<string, int> AssignedResources => _source.Current.AssignedResources;
			public ComputeSocket Socket => _source.Current.Socket;

			public LeaseImpl(IAsyncEnumerator<LeaseInfo> source)
			{
				_source = source;
			}

			/// <inheritdoc/>
			public async ValueTask DisposeAsync()
			{
				await _source.MoveNextAsync();
				await _source.DisposeAsync();
			}

			/// <inheritdoc/>
			public ValueTask CloseAsync(CancellationToken cancellationToken) => Socket.CloseAsync(cancellationToken);
		}

		readonly HttpClient? _defaultHttpClient;
		readonly Func<HttpClient> _createHttpClient;
		readonly CancellationTokenSource _cancellationSource = new CancellationTokenSource();
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="serverUri">Uri of the server to connect to</param>
		/// <param name="authHeader">Authentication header</param>
		/// <param name="logger">Logger for diagnostic messages</param>
		public ServerComputeClient(Uri serverUri, AuthenticationHeaderValue? authHeader, ILogger logger)
			{
#pragma warning disable CA2000 // Dispose objects before losing scope
			// This is disposed via HttpClient
			SocketsHttpHandler handler = new SocketsHttpHandler();
			handler.PooledConnectionLifetime = TimeSpan.FromMinutes(2.0);

			_defaultHttpClient = new HttpClient(handler, true);
			_defaultHttpClient.BaseAddress = serverUri;
			_defaultHttpClient.DefaultRequestHeaders.Authorization = authHeader;
#pragma warning restore CA2000 // Dispose objects before losing scope

			_createHttpClient = GetDefaultHttpClient;
			_logger = logger;
			}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="createHttpClient">Creates an HTTP client with the correct base address for the server</param>
		/// <param name="logger">Logger for diagnostic messages</param>
		public ServerComputeClient(Func<HttpClient> createHttpClient, ILogger logger)
		{
			_createHttpClient = createHttpClient;
			_logger = logger;
		}

		/// <summary>
		/// Gets the default http client
		/// </summary>
		/// <returns></returns>
		HttpClient GetDefaultHttpClient() => _defaultHttpClient!;

		/// <inheritdoc/>
		public ValueTask DisposeAsync()
		{
			Dispose();
			return new ValueTask();
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_cancellationSource.Dispose();
			_defaultHttpClient?.Dispose();
		}

		/// <inheritdoc/>
		public async Task<IComputeLease?> TryAssignWorkerAsync(ClusterId clusterId, Requirements? requirements, CancellationToken cancellationToken)
		{
			IAsyncEnumerator<LeaseInfo> source = ConnectAsync(clusterId, requirements, cancellationToken).GetAsyncEnumerator(cancellationToken);
			if (!await source.MoveNextAsync())
			{
				await source.DisposeAsync();
				return null;
			}
			return new LeaseImpl(source);
		}

		/// <inheritdoc/>
		async IAsyncEnumerable<LeaseInfo> ConnectAsync(ClusterId clusterId, Requirements? requirements, [EnumeratorCancellation] CancellationToken cancellationToken)
		{
			_logger.LogDebug("Requesting compute resource");

			// Assign a compute worker
			HttpClient client = _createHttpClient();

			AssignComputeRequest request = new AssignComputeRequest();
			request.Requirements = requirements;

			AssignComputeResponse? responseMessage;
			using (HttpResponseMessage response = await client.PostAsync($"api/v2/compute/{clusterId}", request, _cancellationSource.Token))
			{
				if (response.StatusCode == HttpStatusCode.NotFound)
				{
					throw new NoComputeAgentsFoundException(clusterId, requirements);
				}

				if (response.StatusCode == HttpStatusCode.ServiceUnavailable)
				{
					_logger.LogDebug("No compute resource is available.");
					yield break;
				}

				response.EnsureSuccessStatusCode();

				responseMessage = await response.Content.ReadFromJsonAsync<AssignComputeResponse>(cancellationToken: cancellationToken);
				if (responseMessage == null)
				{
					throw new InvalidOperationException();
				}
			}

			_logger.LogDebug("Connecting to {Ip} with nonce {Nonce}...", responseMessage.Ip, responseMessage.Nonce);

			// Connect to the remote machine
			using Socket socket = new Socket(SocketType.Stream, ProtocolType.Tcp);
			await socket.ConnectAsync(IPAddress.Parse(responseMessage.Ip), responseMessage.Port, cancellationToken);

			// Send the nonce
			byte[] nonce = StringUtils.ParseHexString(responseMessage.Nonce);
			await socket.SendMessageAsync(nonce, SocketFlags.None, cancellationToken);
			_logger.LogDebug("Connection established.");

			// Pass the rest of the call over to the handler
			byte[] key = StringUtils.ParseHexString(responseMessage.Key);

			await using ComputeSocket computeSocket = new ComputeSocket(new TcpTransport(socket), ComputeSocketEndpoint.Local, _logger);
			yield return new LeaseInfo(responseMessage.Properties, responseMessage.AssignedResources, computeSocket);
		}
	}

	/// <summary>
	/// Exception indicating that no matching compute agents were found
	/// </summary>
	public sealed class NoComputeAgentsFoundException : Exception
	{
		/// <summary>
		/// The compute cluster requested
		/// </summary>
		public ClusterId ClusterId { get; }

		/// <summary>
		/// Requested agent requirements
		/// </summary>
		public Requirements? Requirements { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public NoComputeAgentsFoundException(ClusterId clusterId, Requirements? requirements)
			: base($"No compute agents found matching '{requirements}' in cluster '{clusterId}'")
		{
			ClusterId = clusterId;
			Requirements = requirements;
		}
	}
}
