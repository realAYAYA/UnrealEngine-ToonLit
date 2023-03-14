// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Core
{
	/// <summary>
	/// Extension methods for consuming REST APIs via JSON objects
	/// </summary>
	public static class HttpClientExtensions
	{
		/// <summary>
		/// Gets a resource from an HTTP endpoint and parses it as a JSON object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <param name="client">The http client instance</param>
		/// <param name="url">The url to retrieve</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>New instance of the object</returns>
		public static async Task<TResponse> GetAsync<TResponse>(this HttpClient client, string url, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await client.GetAsync(url, cancellationToken))
			{
				response.EnsureSuccessStatusCode();
				return await ParseJsonContent<TResponse>(response);
			}
		}

		/// <summary>
		/// Posts an object to an HTTP endpoint as a JSON object
		/// </summary>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="client">The http client instance</param>
		/// <param name="url">The url to post to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>Response message</returns>
		public static async Task<HttpResponseMessage> PostAsync<TRequest>(this HttpClient client, string url, TRequest request, CancellationToken cancellationToken)
		{
			return await client.PostAsync(url, ToJsonContent(request), cancellationToken);
		}

		/// <summary>
		/// Posts an object to an HTTP endpoint as a JSON object, and parses the response object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="client">The http client instance</param>
		/// <param name="url">The url to post to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>The response parsed into the requested type</returns>
		public static async Task<TResponse> PostAsync<TResponse, TRequest>(this HttpClient client, string url, TRequest request, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await PostAsync(client, url, request, cancellationToken))
			{
				response.EnsureSuccessStatusCode();
				return await ParseJsonContent<TResponse>(response);
			}
		}

		/// <summary>
		/// Puts an object to an HTTP endpoint as a JSON object
		/// </summary>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="client">The http client instance</param>
		/// <param name="url">The url to post to</param>
		/// <param name="obj">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>Response message</returns>
		public static async Task<HttpResponseMessage> PutAsync<TRequest>(this HttpClient client, string url, TRequest obj, CancellationToken cancellationToken)
		{
			return await client.PutAsync(url, ToJsonContent(obj), cancellationToken);
		}

		/// <summary>
		/// Puts an object to an HTTP endpoint as a JSON object, and parses the response object
		/// </summary>
		/// <typeparam name="TResponse">The object type to return</typeparam>
		/// <typeparam name="TRequest">The object type to post</typeparam>
		/// <param name="client">The http client instance</param>
		/// <param name="url">The url to post to</param>
		/// <param name="request">The object to post</param>
		/// <param name="cancellationToken">Cancels the request</param>
		/// <returns>The response parsed into the requested type</returns>
		public static async Task<TResponse> PutAsync<TResponse, TRequest>(this HttpClient client, string url, TRequest request, CancellationToken cancellationToken)
		{
			using (HttpResponseMessage response = await PutAsync(client, url, request, cancellationToken))
			{
				response.EnsureSuccessStatusCode();
				return await ParseJsonContent<TResponse>(response);
			}
		}

		/// <summary>
		/// Converts an object to a JSON http content object
		/// </summary>
		/// <typeparam name="T">Type of the object to parse</typeparam>
		/// <param name="obj">The object instance</param>
		/// <returns>Http content object</returns>
		private static HttpContent ToJsonContent<T>(T obj)
		{
			return new StringContent(JsonSerializer.Serialize<T>(obj), Encoding.UTF8, "application/json");
		}

		/// <summary>
		/// Parses a HTTP response as a JSON object
		/// </summary>
		/// <typeparam name="T">Type of the object to parse</typeparam>
		/// <param name="message">The message received</param>
		/// <returns>Parsed object instance</returns>
		private static async Task<T> ParseJsonContent<T>(HttpResponseMessage message)
		{
			byte[] bytes = await message.Content.ReadAsByteArrayAsync();
			return JsonSerializer.Deserialize<T>(bytes, new JsonSerializerOptions { PropertyNameCaseInsensitive = true });
		}
	}
}
