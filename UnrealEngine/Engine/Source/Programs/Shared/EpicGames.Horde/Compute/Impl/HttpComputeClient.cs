// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Net.Http;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute.Impl
{
	#region Requests / Responses

	/// <summary>
	/// Information about a compute cluster
	/// </summary>
	public class GetComputeClusterInfo : IComputeClusterInfo
	{
		/// <inheritdoc/>
		public ClusterId Id { get; set; }

		/// <inheritdoc/>
		public NamespaceId NamespaceId { get; set; }

		/// <inheritdoc/>
		public BucketId RequestBucketId { get; set; }

		/// <inheritdoc/>
		public BucketId ResponseBucketId { get; set; }
	}

	/// <summary>
	/// Request to add tasks to the compute queue
	/// </summary>
	public class AddTasksRequest
	{
		/// <summary>
		/// Channel to post the new tasks to. This should be a unique identifier (eg. a GUID) synthesized by the client to distinguish it from other clients, and used for querying status later.
		/// </summary>
		[CbField("c")]
		public ChannelId ChannelId { get; set; }

		/// <summary>
		/// Refs for tasks to be executed
		/// </summary>
		[Required, CbField("t")]
		public List<RefId> TaskRefIds { get; } = new List<RefId>();

		/// <summary>
		/// Requirements for agents executing the tasks
		/// </summary>
		[CbField("r")]
		public CbObjectAttachment RequirementsHash { get; set; }

		/// <summary>
		/// If set, prevents data being cached.
		/// </summary>
		[CbField("nc")]
		public bool DoNotCache { get; set; }
	}

	/// <summary>
	/// Specifies updates from the given channel
	/// </summary>
	public class GetTaskUpdatesResponse
	{
		/// <summary>
		/// Task updates
		/// </summary>
		[CbField("u")]
		public List<ComputeTaskStatus> Updates { get; } = new List<ComputeTaskStatus>();
	}

	#endregion

	/// <summary>
	/// Implementation of <see cref="IComputeClient"/> which uses HTTP to communicate with a Horde Storage server
	/// </summary>
	public class HttpComputeClient : IComputeClient
	{
		readonly HttpClient _httpClient;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="httpClient"></param>
		public HttpComputeClient(HttpClient httpClient)
		{
			_httpClient = httpClient;
		}

		/// <inheritdoc/>
		public async Task<IComputeClusterInfo> GetClusterInfoAsync(ClusterId clusterId, CancellationToken cancellationToken)
		{
			return await _httpClient.GetAsync<GetComputeClusterInfo>($"api/v1/compute/{clusterId}", cancellationToken);
		}

		/// <inheritdoc/>
		public async Task AddTasksAsync(ClusterId clusterId, ChannelId channelId, IEnumerable<RefId> taskRefIds, IoHash requirementsHash, bool skipCacheLookup, CancellationToken cancellationToken)
		{
			AddTasksRequest addTasks = new AddTasksRequest();
			addTasks.ChannelId = channelId;
			addTasks.TaskRefIds.AddRange(taskRefIds);
			addTasks.RequirementsHash = requirementsHash;
			addTasks.DoNotCache = skipCacheLookup;

			ReadOnlyMemoryContent content = new ReadOnlyMemoryContent(CbSerializer.Serialize(addTasks).GetView());
			content.Headers.Add("Content-Type", "application/x-ue-cb");

			using HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, $"api/v1/compute/{clusterId}");
			request.Content = content;

			HttpResponseMessage response = await _httpClient.SendAsync(request, cancellationToken);
			response.EnsureSuccessStatusCode();
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<ComputeTaskStatus> GetTaskUpdatesAsync(ClusterId clusterId, ChannelId channelId, [EnumeratorCancellation] CancellationToken cancellationToken)
		{
			for (; ; )
			{
				HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, $"api/v1/compute/{clusterId}/updates/{channelId}?wait=10");
				request.Headers.Add("Accept", "application/x-ue-cb");

				using HttpResponseMessage response = await _httpClient.SendAsync(request, HttpCompletionOption.ResponseContentRead, cancellationToken);
				response.EnsureSuccessStatusCode();

				byte[] data = await response.Content.ReadAsByteArrayAsync(cancellationToken);
				GetTaskUpdatesResponse parsedResponse = CbSerializer.Deserialize<GetTaskUpdatesResponse>(new CbField(data));

				foreach (ComputeTaskStatus update in parsedResponse.Updates)
				{
					cancellationToken.ThrowIfCancellationRequested();
					yield return update;
				}
			}
		}
	}
}
