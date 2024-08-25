// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

#pragma warning disable CA2227

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Class which describes an artifact on Horde which can be serialized to JSON.
	/// </summary>
	public class ArtifactDescriptor
	{
		/// <summary>
		/// Base URL for downloading from
		/// </summary>
		public Uri BaseUrl { get; set; }

		/// <summary>
		/// Name of the ref to download
		/// </summary>
		public RefName RefName { get; set; }

		/// <summary>
		/// Filter for the files selected for download
		/// </summary>
		public List<string>? Filter { get; set; }

		/// <summary>
		/// Default constructor
		/// </summary>
		public ArtifactDescriptor()
		{
			BaseUrl = new Uri("http://horde");
			RefName = new RefName("default");
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public ArtifactDescriptor(Uri baseUrl, RefName refName, IReadOnlyCollection<string>? filter = null)
		{
			BaseUrl = baseUrl;
			RefName = refName;
			if (filter != null && filter.Count > 0)
			{
				Filter = new List<string>(filter);
			}
		}

		/// <summary>
		/// Deserialize a descriptor from utf8 bytes
		/// </summary>
		/// <param name="data">Data to deserialize from</param>
		/// <returns>New descriptor instance</returns>
		public static ArtifactDescriptor Deserialize(ReadOnlySpan<byte> data)
		{
			return JsonSerializer.Deserialize<ArtifactDescriptor>(data, HordeHttpClient.JsonSerializerOptions)
				?? throw new InvalidOperationException("Cannot deserialize descriptor");
		}

		/// <summary>
		/// Writes this descriptor to storage
		/// </summary>
		/// <param name="file">File to read from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task<ArtifactDescriptor> ReadAsync(FileReference file, CancellationToken cancellationToken)
		{
			byte[] data = await FileReference.ReadAllBytesAsync(file, cancellationToken);
			return Deserialize(data);
		}

		/// <summary>
		/// Serializes a descriptor
		/// </summary>
		/// <returns>Data for the serialized descriptor</returns>
		public byte[] Serialize()
		{
			JsonSerializerOptions serializerOptions = new JsonSerializerOptions(HordeHttpClient.JsonSerializerOptions);
			serializerOptions.WriteIndented = true;

			return JsonSerializer.SerializeToUtf8Bytes<ArtifactDescriptor>(this, serializerOptions);
		}

		/// <summary>
		/// Writes this descriptor to storage
		/// </summary>
		/// <param name="file">File to write to</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public async Task WriteAsync(FileReference file, CancellationToken cancellationToken)
		{
			byte[] data = Serialize();
			await FileReference.WriteAllBytesAsync(file, data, cancellationToken);
		}
	}
}
