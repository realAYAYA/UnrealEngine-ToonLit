// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Telemetry;
using HordeCommon.Rpc.Messages.Telemetry;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Telemetry.Sinks
{
	/// <summary>
	/// ClickHouse telemetry sink
	/// </summary>
	public sealed class ClickHouseTelemetrySink : ITelemetrySinkInternal
	{
		/// <summary>
		/// Name of the HTTP client for writing telemetry data
		/// </summary>
		public const string HttpClientName = "ClickHouseTelemetrySink";

		private const string EmptyObjectId = "000000000000000000000000";

		private readonly IHttpClientFactory _httpClientFactory;
		private readonly Uri? _uri;

		private readonly ILogger _logger;
		private readonly ConcurrentQueue<TelemetryEvent> _queuedEvents = new();

		/// <inheritdoc/>
		public bool Enabled => _uri != null;

		/// <summary>
		/// Constructor
		/// </summary>
		public ClickHouseTelemetrySink(ClickHouseTelemetryConfig config, IHttpClientFactory httpClientFactory, ILogger<ClickHouseTelemetrySink> logger)
		{
			_httpClientFactory = httpClientFactory;
			_logger = logger;
			_uri = config.Url;
		}

		/// <inheritdoc/>
		public ValueTask DisposeAsync()
		{
			return ValueTask.CompletedTask;
		}

		/// <inheritdoc/>
		public void SendEvent(TelemetryStoreId telemetryStoreId, TelemetryEvent telemetryEvent)
		{
			_queuedEvents.Enqueue(telemetryEvent);
		}

		/// <inheritdoc />
		public async ValueTask FlushAsync(CancellationToken cancellationToken)
		{
			List<AgentMetadataEvent> agentMetadataEvents = new();
			List<AgentCpuMetricsEvent> agentCpuEvents = new();
			List<AgentMemoryMetricsEvent> agentMemEvents = new();

			int c = 0;
			while (_queuedEvents.TryDequeue(out TelemetryEvent? evt))
			{
				switch (evt.Payload)
				{
					case AgentMetadataEvent agentMetadata: agentMetadataEvents.Add(agentMetadata); break;
					case AgentCpuMetricsEvent agentCpu: agentCpuEvents.Add(agentCpu); break;
					case AgentMemoryMetricsEvent agentMem: agentMemEvents.Add(agentMem); break;
				}
				c++;
			}

			StringBuilder sb = new(c * 200); // Rough size of about 200 bytes per event
			WriteAgentMetadataSql(agentMetadataEvents, sb);
			WriteAgentCpuSql(agentCpuEvents, sb);

			if (sb.Length > 0)
			{
				await SendClickHouseQueryAsync(Encoding.UTF8.GetBytes(sb.ToString()), cancellationToken);
			}
		}

		private async Task SendClickHouseQueryAsync(byte[] query, CancellationToken cancellationToken)
		{
			HttpClient httpClient = _httpClientFactory.CreateClient(HttpClientName);

			using HttpRequestMessage request = new();
			request.RequestUri = _uri;
			request.Method = HttpMethod.Post;
			request.Content = new ByteArrayContent(query);

			using HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken);
			if (response.IsSuccessStatusCode)
			{
				_logger.LogDebug("Sending {Size} bytes of telemetry data to {Url}", query.Length, _uri);
			}
			else
			{
				string responseContent = await response.Content.ReadAsStringAsync(cancellationToken);
				string queryStr = Encoding.UTF8.GetString(query);
				queryStr = queryStr.Substring(0, Math.Min(queryStr.Length, 300));

				_logger.LogError("Unable to send telemetry data to server. StatusCode={StatusCode}) Response={Response} Query={Query}",
					(int)response.StatusCode, responseContent, queryStr);
			}
		}

		/// <summary>
		/// Generate a INSERT INTO statement for a list of AgentCpuMetricsEvent
		/// </summary>
		/// <param name="events"></param>
		/// <param name="sb"></param>
		private static void WriteAgentCpuSql(List<AgentCpuMetricsEvent> events, StringBuilder sb)
		{
			if (events.Count > 0)
			{
				sb.Append("INSERT INTO agentCpu (time, agentId, leaseId, jobId, jobBatchId, user, system, idle) VALUES \n");
				foreach (AgentCpuMetricsEvent e in events)
				{
					ExecutionMetadata? em = e.ExecutionMetadata;
					sb.AppendFormat("({0}, {1}, '{2}', '{3}', {4}, {5:F3}, {6:F3}, {7:F3}),\n",
						e.Timestamp?.Seconds, (ulong)e.AgentId,
						WriteEscapedObjectId(em?.LeaseId ?? EmptyObjectId),
						WriteEscapedObjectId(em?.JobId ?? EmptyObjectId),
						SubResourceId.Parse(em?.JobBatchId ?? "0000").Value,
						e.User, e.System, e.Idle);
				}
				sb.Remove(sb.Length - 2, 2); // Remove trailing comma and newline
				sb.Append(';');
			}
		}

		/// <summary>
		/// Generate a INSERT INTO statement for a list of AgentMetadataEvent
		/// </summary>
		/// <param name="events"></param>
		/// <param name="sb"></param>
		private static void WriteAgentMetadataSql(List<AgentMetadataEvent> events, StringBuilder sb)
		{
			if (events.Count > 0)
			{
				sb.Append("INSERT INTO agentMetadata (id, ip, hostname, region, az, env, version, os, osVersion, arch, props) VALUES \n");
				foreach (AgentMetadataEvent e in events)
				{
					sb.AppendFormat("({0}, '{1}', '{2}', '{3}', '{4}', '{5}', '{6}', '{7}', '{8}', '{9}', {10}),\n",
						(ulong)e.AgentId, e.Ip, e.Hostname, e.Region, e.AvailabilityZone, e.Environment, e.AgentVersion,
						e.Os, e.OsVersion, e.Architecture, WriteDictionaryAsClickHouseMap(e.Properties));
				}
				sb.Remove(sb.Length - 2, 2); // Remove trailing comma and newline
				sb.Append(';');
			}
		}

		private static string WriteEscapedObjectId(string value)
		{
			if (value.Length != 24)
			{
				throw new ArgumentException($"Expected a MongoDB ObjectId string of 24 chars. Got: '{value}'");
			}

			StringBuilder sb = new(value.Length * 2);
			for (int i = 0; i < value.Length; i += 2)
			{
				sb.Append("\\x");
				sb.Append(value[i]);
				sb.Append(value[i + 1]);
			}

			return sb.ToString();
		}

		private static string WriteDictionaryAsClickHouseMap(IDictionary<string, string> dict)
		{
			if (dict.Count == 0)
			{
				return "{}";
			}

			StringBuilder sb = new(dict.Count * 2 * 20);
			sb.Append('{');
			foreach ((string key, string value) in dict)
			{
				sb.AppendFormat("'{0}': '{1}', ", key, value);
			}
			sb.Remove(sb.Length - 2, 2); // Remove trailing comma and whitespace
			sb.Append('}');

			return sb.ToString();
		}
	}
}
