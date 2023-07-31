// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Web;

namespace UnrealGameSync
{
	/// <summary>
	/// Interface for a telemetry sink
	/// </summary>
	public interface ITelemetrySink : IDisposable
	{
		/// <summary>
		/// Sends a telemetry event with the given information
		/// </summary>
		/// <param name="eventName">Name of the event</param>
		/// <param name="attributes">Arbitrary object to include in the payload</param>
		void SendEvent(string eventName, object attributes);
	}

	/// <summary>
	/// Telemetry sink that discards all events
	/// </summary>
	public class NullTelemetrySink : ITelemetrySink
	{
		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public void SendEvent(string eventName, object attributes)
		{
		}
	}

	/// <summary>
	/// Epic internal telemetry sink using the data router
	/// </summary>
	public class EpicTelemetrySink : ITelemetrySink
	{
		/// <summary>
		/// Combined url to post event streams to
		/// </summary>
		string _url;

		/// <summary>
		/// Lock used to modify the event queue
		/// </summary>
		object _lockObject = new object();

		/// <summary>
		/// Whether a flush is queued
		/// </summary>
		bool _hasPendingFlush = false;

		/// <summary>
		/// List of pending events
		/// </summary>
		List<string> _pendingEvents = new List<string>();

		/// <summary>
		/// The log writer to use
		/// </summary>
		ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public EpicTelemetrySink(string url, ILogger logger)
		{
			this._url = url;
			this._logger = logger;

			logger.LogInformation("Posting to URL: {Url}", url);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			Flush();
		}

		/// <inheritdoc/>
		public void Flush()
		{
			for (; ; )
			{
				lock (_lockObject)
				{
					if (!_hasPendingFlush)
					{
						break;
					}
				}
				Thread.Sleep(10);
			}
		}

		/// <inheritdoc/>
		public void SendEvent(string eventName, object attributes)
		{
			string attributesText = JsonSerializer.Serialize(attributes);
			if (attributesText[0] != '{')
			{
				throw new Exception("Expected event data with named properties");
			}

			string eventText = attributesText.Insert(1, String.Format("\"EventName\":\"{0}\",", HttpUtility.JavaScriptStringEncode(eventName)));
			lock (_pendingEvents)
			{
				_pendingEvents.Add(eventText);
				if (!_hasPendingFlush)
				{
					ThreadPool.QueueUserWorkItem(obj => BackgroundFlush());
					_hasPendingFlush = true;
				}
			}
		}

		/// <summary>
		/// Synchronously sends a telemetry event
		/// </summary>
		void BackgroundFlush()
		{
			for (; ; )
			{
				try
				{
					// Generate the content for this event
					List<string> events = new List<string>();
					lock (_lockObject)
					{
						if (_pendingEvents.Count == 0)
						{
							_hasPendingFlush = false;
							break;
						}

						events.AddRange(_pendingEvents);
						_pendingEvents.Clear();
					}

					// Print all the events we're sending
					foreach (string evt in events)
					{
						_logger.LogInformation("Sending Event: {0}", evt);
					}

					// Convert the content to UTF8
					string contentText = String.Format("{{\"Events\":[{0}]}}", String.Join(",", events));
					byte[] content = Encoding.UTF8.GetBytes(contentText);

					// Post the event data
					HttpWebRequest request = (HttpWebRequest)WebRequest.Create(_url);
					request.Method = "POST";
					request.ContentType = "application/json";
					request.UserAgent = "ue/ugs";
					request.Timeout = 5000;
					request.ContentLength = content.Length;
					request.ContentType = "application/json";
					using (Stream requestStream = request.GetRequestStream())
					{
						requestStream.Write(content, 0, content.Length);
					}

					// Wait for the response and dispose of it immediately
					using (HttpWebResponse response = (HttpWebResponse)request.GetResponse())
					{
						_logger.LogInformation("Response: {StatusCode}", (int)response.StatusCode);
					}
				}
				catch (WebException ex)
				{
					// Handle errors. Any non-200 responses automatically generate a WebException.
					HttpWebResponse response = (HttpWebResponse)ex.Response;
					if (response == null)
					{
						_logger.LogError(ex, "Exception while attempting to send event");
					}
					else
					{
						string responseText;
						using (Stream responseStream = response.GetResponseStream())
						{
							MemoryStream memoryStream = new MemoryStream();
							responseStream.CopyTo(memoryStream);
							responseText = Encoding.UTF8.GetString(memoryStream.ToArray());
						}
						_logger.LogError("Failed to send analytics event. Code = {Code}. Desc = {Dec}. Response = {Response}.", (int)response.StatusCode, response.StatusDescription, responseText);
					}
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while attempting to send event");
				}
			}
		}
	}

	/// <summary>
	/// Global telemetry static class
	/// </summary>
	public static class Telemetry
	{
		/// <summary>
		/// The current telemetry provider
		/// </summary>
		public static ITelemetrySink? ActiveSink
		{
			get; set;
		}

		/// <summary>
		/// Sends a telemetry event with the given information
		/// </summary>
		/// <param name="eventName">Name of the event</param>
		/// <param name="attributes">Arbitrary object to include in the payload</param>
		public static void SendEvent(string eventName, object attributes)
		{
			ActiveSink?.SendEvent(eventName, attributes);
		}
	}
}
