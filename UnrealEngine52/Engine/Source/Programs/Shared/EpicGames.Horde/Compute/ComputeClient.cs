// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Interface for a compute request
	/// </summary>
	public interface IComputeRequest<TResult>
	{
		/// <summary>
		/// Task to await completion of this request
		/// </summary>
		Task<TResult> Result { get; }

		/// <summary>
		/// Cancel the current request
		/// </summary>
		void Cancel();
	}

	/// <summary>
	/// Helper class to enlist remote resources to perform compute-intensive tasks.
	/// </summary>
	public sealed class ComputeClient : IAsyncDisposable, IDisposable
	{
		const int NonceLength = 64;

		class AddComputeTaskRequest
		{
			public Requirements? Requirements { get; set; }
			public int? RemotePort { get; set; }
			public string Nonce { get; set; } = String.Empty;
			public string AesKey { get; set; } = String.Empty;
			public string AesIv { get; set; } = String.Empty;
		}

		class AddComputeTasksRequest
		{
			public Requirements? Requirements { get; set; }
			public int RemotePort { get; set; }
			public List<AddComputeTaskRequest> Tasks { get; set; } = new List<AddComputeTaskRequest>();
		}

		abstract class RequestInfo : IDisposable
		{
			public ReadOnlyMemory<byte> Nonce { get; }
			public ReadOnlyMemory<byte> AesKey { get; }
			public ReadOnlyMemory<byte> AesIv { get; }

			protected readonly TaskCompletionSource<IComputeChannel> _computeChannelSource;
			protected readonly CancellationTokenSource _cancellationSource;

			public RequestInfo(CancellationToken cancellationToken)
			{
				Nonce = RandomNumberGenerator.GetBytes(NonceLength);

				using (Aes aes = Aes.Create())
				{
					AesKey = aes.Key;
					AesIv = aes.IV;
				}

				_computeChannelSource = new TaskCompletionSource<IComputeChannel>();
				_cancellationSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
			}

			public abstract Task RunAsync(IComputeChannel channel);

			public void Cancel()
			{
				_computeChannelSource.TrySetCanceled();
				_cancellationSource.Cancel();
			}

			public void Dispose()
			{
				_cancellationSource.Dispose();
			}
		}

		class RequestInfo<TResult> : RequestInfo, IComputeRequest<TResult>
		{
			readonly Func<IComputeChannel, CancellationToken, Task<TResult>> _handler;

			public Task<TResult> Result { get; }

			public RequestInfo(Func<IComputeChannel, CancellationToken, Task<TResult>> handler, CancellationToken cancellationToken)
				: base(cancellationToken)
			{
				_handler = handler;

				Result = RunWrapperAsync();
			}

			public override Task RunAsync(IComputeChannel channel)
			{
				_computeChannelSource.SetResult(channel);
				return Result;
			}

			async Task<TResult> RunWrapperAsync()
			{
				IComputeChannel channel = await _computeChannelSource.Task;
				return await _handler(channel, _cancellationSource.Token);
			}
		}

		class MemoryComparer : IEqualityComparer<ReadOnlyMemory<byte>>
		{
			public static MemoryComparer Instance { get; } = new MemoryComparer();

			public bool Equals(ReadOnlyMemory<byte> x, ReadOnlyMemory<byte> y) => x.Span.SequenceEqual(y.Span);

			public int GetHashCode(ReadOnlyMemory<byte> memory)
			{
				HashCode hasher = new HashCode();
				hasher.AddBytes(memory.Span);
				return hasher.ToHashCode();
			}
		}

		readonly object _lockObject = new object();
		readonly Func<HttpClient> _createHttpClient;
		readonly CancellationTokenSource _cancellationSource = new CancellationTokenSource();
		readonly Dictionary<ReadOnlyMemory<byte>, RequestInfo> _requests = new Dictionary<ReadOnlyMemory<byte>, RequestInfo>(MemoryComparer.Instance);
		TcpListener? _listener;
		Task _listenTask = Task.CompletedTask;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="createHttpClient">Creates an HTTP client with the correct base address for the server</param>
		/// <param name="logger">Logger for diagnostic messages</param>
		public ComputeClient(Func<HttpClient> createHttpClient, ILogger logger)
		{
			_createHttpClient = createHttpClient;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await StopAsync();
			Dispose();
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_cancellationSource.Dispose();
		}

		/// <summary>
		/// Starts the client
		/// </summary>
		public void Start()
		{
			_listener = new TcpListener(IPAddress.IPv6Any, 0);
			_listener.Start();
			_listenTask = Task.Run(() => ListenAsync(_listener, _cancellationSource.Token));
		}

		/// <summary>
		/// Cancels all running tasks and waits for them to complete
		/// </summary>
		public async Task StopAsync()
		{
			_cancellationSource.Cancel();
			try
			{
				await _listenTask;
			}
			catch (OperationCanceledException)
			{
			}
		}

		/// <summary>
		/// Adds a new remote request
		/// </summary>
		/// <param name="clusterId">Cluster to execute the request</param>
		/// <param name="requirements">Requirements for the agent</param>
		/// <param name="handler">Handler for the connection</param>
		public async Task<IComputeRequest<TResult>> AddRequestAsync<TResult>(ClusterId clusterId, Requirements? requirements, Func<IComputeChannel, CancellationToken, Task<TResult>> handler)
		{
			RequestInfo<TResult> requestInfo = new RequestInfo<TResult>(handler, _cancellationSource.Token);
			await AddRequestAsync(clusterId, requirements, requestInfo);
			return requestInfo;
		}

		async Task AddRequestAsync(ClusterId clusterId, Requirements? requirements, RequestInfo requestInfo)
		{
			try
			{
				HttpClient client = _createHttpClient();

				AddComputeTaskRequest task = new AddComputeTaskRequest();
				task.Nonce = StringUtils.FormatHexString(requestInfo.Nonce.Span);
				task.AesKey = StringUtils.FormatHexString(requestInfo.AesKey.Span);
				task.AesIv = StringUtils.FormatHexString(requestInfo.AesIv.Span);

				AddComputeTasksRequest request = new AddComputeTasksRequest();
				request.Requirements = requirements;
				request.RemotePort = ((IPEndPoint)_listener!.LocalEndpoint).Port;
				request.Tasks.Add(task);

				using (HttpResponseMessage response = await client.PostAsync($"api/v2/compute/{clusterId}", request, _cancellationSource.Token))
				{
					response.EnsureSuccessStatusCode();
					_requests.Add(requestInfo.Nonce, requestInfo);
				}
			}
			catch
			{
				requestInfo.Dispose();
			}
		}

		async Task ListenAsync(TcpListener listener, CancellationToken cancellationToken)
		{
			List<Task> tasks = new List<Task>();
			try
			{
				for (; ; )
				{
					TcpClient client = await listener.AcceptTcpClientAsync(cancellationToken);
					_logger.LogInformation("Received connection from {Remote}", client.Client.RemoteEndPoint);

					Task task = HandleConnectionGuardedAsync(client, cancellationToken);
					tasks.Add(task);
				}
			}
			finally
			{
				await Task.WhenAll(tasks);
				listener.Stop();
			}
		}

		async Task HandleConnectionGuardedAsync(TcpClient client, CancellationToken cancellationToken)
		{
			try
			{
				await HandleConnectionAsync(client, cancellationToken);
			}
			catch (Exception ex)
			{
				_logger.LogInformation(ex, "Exception while handling request: {Message}", ex.Message);
			}
		}

		async Task HandleConnectionAsync(TcpClient client, CancellationToken cancellationToken)
		{
			using TcpClient disposeClient = client;
			Socket socket = client.Client;

			// Read the nonce for this connection
			byte[] nonceBuffer = new byte[NonceLength];
			for (int nonceLength = 0; nonceLength < nonceBuffer.Length;)
			{
				int read = await socket.ReceiveAsync(nonceBuffer.AsMemory(nonceLength), SocketFlags.Partial, cancellationToken);
				if (read == 0)
				{
					return;
				}
				nonceLength += read;
			}

			// Dispatch the connection to the appropriate handler
			RequestInfo? requestInfo = null;
			try
			{
				// Find the matching request
				lock (_lockObject)
				{
					if (!_requests.Remove(nonceBuffer, out requestInfo))
					{
						_logger.LogInformation("Invalid nonce for connection; ignoring.");
						return;
					}
				}

				// Create a crypto stream for communication, using the predetermined key for this request
				IComputeChannel channel = new SocketComputeChannel(socket, requestInfo.AesKey, requestInfo.AesIv);
				await requestInfo.RunAsync(channel);
			}
			finally
			{
				requestInfo?.Dispose();
			}
		}
	}
}
