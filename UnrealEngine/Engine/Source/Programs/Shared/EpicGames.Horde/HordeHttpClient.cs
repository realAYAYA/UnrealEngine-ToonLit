// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;

#pragma warning disable CA2234 // Pass system uri objects instead of strings

namespace EpicGames.Horde
{
	/// <summary>
	/// Wraps an Http client which communicates with the Horde server
	/// </summary>
	public sealed class HordeHttpClient : IDisposable
	{
		/// <summary>
		/// The configured HTTP client
		/// </summary>
		public HttpClient HttpClient { get; }

		readonly JsonSerializerOptions _jsonSerializerOptions;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="httpClient">Client to use for communication</param>
		public HordeHttpClient(HttpClient httpClient)
		{
			HttpClient = httpClient;

			_jsonSerializerOptions = new JsonSerializerOptions();
			ConfigureJsonSerializer(_jsonSerializerOptions);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="uri">Base URI of the server</param>
		/// <param name="token">Access token for the connection</param>
		public HordeHttpClient(Uri uri, string token)
			: this(new HttpClient { BaseAddress = uri, DefaultRequestHeaders = { Authorization = new AuthenticationHeaderValue("Bearer", token) } })
		{
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			HttpClient.Dispose();
		}

		/// <summary>
		/// Configures a JSON serializer to read Horde responses
		/// </summary>
		/// <param name="options">options for the serializer</param>
		public static void ConfigureJsonSerializer(JsonSerializerOptions options)
		{
			options.AllowTrailingCommas = true;
			options.ReadCommentHandling = JsonCommentHandling.Skip;
			options.DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull;
			options.PropertyNamingPolicy = JsonNamingPolicy.CamelCase;
			options.PropertyNameCaseInsensitive = true;
			options.Converters.Add(new JsonStringEnumConverter());
			options.Converters.Add(new StringIdJsonConverterFactory());
		}

		/// <summary>
		/// Gets a resource from an HTTP endpoint and parses it as a JSON object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <param name="relativePath">The url to retrieve</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>New instance of the object</returns>
		public async Task<TResponse> GetAsync<TResponse>(string relativePath, CancellationToken cancellationToken = default)
		{
			using (HttpResponseMessage response = await HttpClient.GetAsync(relativePath, cancellationToken))
			{
				response.EnsureSuccessStatusCode();

				byte[] bytes = await response.Content.ReadAsByteArrayAsync(cancellationToken);
				return JsonSerializer.Deserialize<TResponse>(bytes, _jsonSerializerOptions)!;
			}
		}
	}
}
