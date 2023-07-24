// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Mime;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class RestException : Exception
	{
		public RestException(string method, string uri, Exception innerException)
			: base(String.Format("Error executing {0} {1}", method, uri), innerException)
		{
		}

		public override string ToString()
		{
			return String.Format("{0}\n\n{1}", Message, InnerException!.ToString());
		}
	}

	public static class RestApi
	{
		private static async Task<string> SendRequestInternal(string url, string method, string? requestBody, CancellationToken cancellationToken)
		{
			HttpWebRequest request = (HttpWebRequest)WebRequest.Create(url);
			request.ContentType = "application/json";
			request.Method = method;

			// Add json to request body
			if (!string.IsNullOrEmpty(requestBody))
			{
				if (method == "POST" || method == "PUT")
				{
					byte[] bytes = Encoding.UTF8.GetBytes(requestBody);
					using (Stream requestStream = request.GetRequestStream())
					{
						await requestStream.WriteAsync(bytes, 0, bytes.Length, cancellationToken);
					}
				}
			}
			try
			{
				using (WebResponse response = request.GetResponse())
				{
					byte[] data;
					using (MemoryStream buffer = new MemoryStream())
					{
						await response.GetResponseStream().CopyToAsync(buffer, cancellationToken);
						data = buffer.ToArray();
					}
					return Encoding.UTF8.GetString(data);
				}
			}
			catch (Exception ex)
			{
				throw new RestException(method, request.RequestUri.ToString(), ex);
			}
		}

		public static Task<string> PostAsync(string url, string requestBody, CancellationToken cancellationToken)
		{
			return SendRequestInternal(url, "POST", requestBody, cancellationToken);
		}

		public static Task<string> GetAsync(string url, CancellationToken cancellationToken)
		{
			return SendRequestInternal(url, "GET", null, cancellationToken);
		}

		public static async Task<T> GetAsync<T>(string url, CancellationToken cancellationToken)
		{
			return JsonSerializer.Deserialize<T>(await GetAsync(url, cancellationToken), Utility.DefaultJsonSerializerOptions)!;
		}

		public static Task<string> PutAsync<T>(string url, T obj, CancellationToken cancellationToken)
		{
			return SendRequestInternal(url, "PUT", JsonSerializer.Serialize(obj), cancellationToken);
		}
	}
}
