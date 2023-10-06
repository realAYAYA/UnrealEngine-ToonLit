// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.AspNetCore.WebUtilities;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Telemetry
{
	/// <summary>
	/// Options for the Epic telemetry sink
	/// </summary>
	public interface IEpicTelemetrySinkConfig
	{
		/// <summary>
		/// Base URL for sending events
		/// </summary>
		public Uri? Url { get; }

		/// <summary>
		/// The application id
		/// </summary>
		public string AppId { get; }
	}

	/// <summary>
	/// Epic internal telemetry sink using the data router
	/// </summary>
	public sealed class EpicTelemetrySink : ITelemetrySinkInternal
	{
		// Converter for datetime formats that Tableau can ingest
		class TableauDateTimeConverter : JsonConverter<DateTime>
		{
			const string Format = "yyyy-MM-dd HH:mm:ss";

			public override void Write(Utf8JsonWriter writer, DateTime date, JsonSerializerOptions options)
			{
				writer.WriteStringValue(date.ToString(Format));
			}

			public override DateTime Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
			{
				return DateTime.ParseExact(reader.GetString() ?? String.Empty, Format, null);
			}
		}

		/// <summary>
		/// Name of the HTTP client for writing telemetry data
		/// </summary>
		public const string HttpClientName = "EpicTelemetrySink";

		static readonly JsonEncodedText s_eventsPropertyName = JsonEncodedText.Encode("Events");
		static readonly JsonEncodedText s_eventNamePropertyName = JsonEncodedText.Encode("EventName");

		readonly object _lockObject = new object();

		readonly IHttpClientFactory _httpClientFactory;
		readonly Uri? _uri;
		readonly JsonSerializerOptions _jsonOptions;

		readonly ArrayMemoryWriter _packetMemoryWriter;
		readonly Utf8JsonWriter _packetWriter;

		readonly ArrayMemoryWriter _eventMemoryWriter;
		readonly Utf8JsonWriter _eventWriter;

		readonly ILogger _logger;

		/// <inheritdoc/>
		public bool Enabled => _uri != null;

		/// <summary>
		/// Constructor
		/// </summary>
		public EpicTelemetrySink(IEpicTelemetrySinkConfig config, IHttpClientFactory httpClientFactory, ILogger<EpicTelemetrySink> logger)
		{
			_httpClientFactory = httpClientFactory;

			_jsonOptions = new JsonSerializerOptions();
			Startup.ConfigureJsonSerializer(_jsonOptions);
			_jsonOptions.PropertyNamingPolicy = null;
			_jsonOptions.Converters.Insert(0, new TableauDateTimeConverter());

			_packetMemoryWriter = new ArrayMemoryWriter(65536);
			_packetWriter = new Utf8JsonWriter(_packetMemoryWriter);

			_eventMemoryWriter = new ArrayMemoryWriter(65536);
			_eventWriter = new Utf8JsonWriter(_eventMemoryWriter);

			_logger = logger;

			if (config.Url != null)
			{
				Dictionary<string, string?> queryParams = new Dictionary<string, string?>()
				{
					["SessionID"] = Guid.NewGuid().ToString(),
					["AppID"] = config.AppId,
					["AppVersion"] = Program.Version.ToString(),
					["AppEnvironment"] = Program.DeploymentEnvironment,
					["UploadType"] = "eteventstream"
				};
				_uri = new Uri(QueryHelpers.AddQueryString(config.Url.ToString(), queryParams));
			}
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await _packetWriter.DisposeAsync();
			await _eventWriter.DisposeAsync();
		}
		
		/// <inheritdoc/>
		public async ValueTask FlushAsync(CancellationToken cancellationToken)
		{
			// Generate the content for this event
			byte[] packet;
			lock (_lockObject)
			{
				if (_packetMemoryWriter.Length == 0)
				{
					return;
				}

				_packetWriter.WriteEndArray();
				_packetWriter.WriteEndObject();
				_packetWriter.Flush();

				packet = _packetMemoryWriter.WrittenMemory.ToArray();

				_packetMemoryWriter.Clear();
				_packetWriter.Reset(_packetMemoryWriter);
			}

			// Make sure the settings are valid
			if (_uri == null)
			{
				return;
			}

			// Post the event data
			HttpClient httpClient = _httpClientFactory.CreateClient(HttpClientName);
			using (HttpRequestMessage request = new HttpRequestMessage())
			{
				request.RequestUri = _uri;
				request.Method = HttpMethod.Post;
				request.Headers.UserAgent.Add(new ProductInfoHeaderValue("Horde", Program.Version.ToString()));
				request.Content = new ByteArrayContent(packet);
				request.Content.Headers.ContentType = new MediaTypeHeaderValue("application/json");

				using (HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken))
				{
					if (response.IsSuccessStatusCode)
					{
						_logger.LogDebug("Sending {Size} bytes of telemetry data to {Url}", packet.Length, _uri);
					}
					else
					{
						string content = await response.Content.ReadAsStringAsync(cancellationToken);
						_logger.LogError("Unable to send telemetry data to server ({Code}): {Message}", response.StatusCode, content);
					}
				}
			}
		}

		/// <inheritdoc/>
		public void SendEvent(string eventName, object attributes)
		{
			lock (_lockObject)
			{
				// If this is the first event written, write the outer events array
				if (_packetMemoryWriter.Length == 0)
				{
					_packetWriter.WriteStartObject();
					_packetWriter.WriteStartArray(s_eventsPropertyName);
				}

				// Serialize the event data
				_eventMemoryWriter.Clear();
				_eventWriter.Reset(_eventMemoryWriter);
				JsonSerializer.Serialize(_eventWriter, attributes, _jsonOptions);
				_eventWriter.Flush();

				// Get the event bytes and check it's a full object
				Span<byte> eventData = _eventMemoryWriter.WrittenSpan;
				if (eventData[0] != (byte)'{' || eventData[^1] != (byte)'}')
				{
					_logger.LogError("Unexpected output from JSON serialization: {Message}", Encoding.UTF8.GetString(eventData));
					return;
				}

				// Modify the serialized data so we can append it to another object
				eventData[0] = (byte)',';
				eventData = eventData.Slice(0, eventData.Length - 1);

				// Copy the event data to the buffer along with the event name
				_packetWriter.WriteStartObject();
				_packetWriter.WriteString(s_eventNamePropertyName, eventName);
				_packetWriter.Flush();
				_packetMemoryWriter.WriteFixedLengthBytes(eventData);
				_packetWriter.WriteEndObject();
			}
		}
	}
}
