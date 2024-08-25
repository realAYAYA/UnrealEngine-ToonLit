// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.Json;
using EpicGames.Core;

namespace Horde
{
	/// <summary>
	/// User configuration for the command line tool
	/// </summary>
	class CmdConfig
	{
		/// <summary>
		/// Gets the path to this config file
		/// </summary>
		public static FileReference Location { get; } = GetLocation();

		Uri _server = new Uri("http://localhost:5000/");

		/// <summary>
		/// Server to connect to
		/// </summary>
		public Uri Server
		{
			get => _server;
			set
			{
				Uri uri = value;
				if (!uri.OriginalString.EndsWith("/", StringComparison.Ordinal))
				{
					uri = new Uri(uri.OriginalString + "/");
				}
				_server = uri;
			}
		}

		/// <summary>
		/// Gets the location for the config file
		/// </summary>
		static FileReference GetLocation()
		{
			DirectoryReference? baseDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile);
			baseDir ??= DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData);
			baseDir ??= DirectoryReference.GetCurrentDirectory();
			return FileReference.Combine(baseDir, ".horde.json");
		}

		/// <summary>
		/// Read the current configuration from disk
		/// </summary>
		public static CmdConfig Read()
		{
			CmdConfig? config = null;
			if (FileReference.Exists(Location))
			{
				byte[] data = FileReference.ReadAllBytes(Location);
				config = JsonSerializer.Deserialize<CmdConfig>(data, new JsonSerializerOptions { AllowTrailingCommas = true, PropertyNameCaseInsensitive = true, ReadCommentHandling = JsonCommentHandling.Skip });
			}
			return config ?? new CmdConfig();
		}

		/// <summary>
		/// Saves the current configuration to disk
		/// </summary>
		public async Task WriteAsync(CancellationToken cancellationToken = default)
		{
			byte[] data = JsonSerializer.SerializeToUtf8Bytes(this, new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase, WriteIndented = true });
			DirectoryReference.CreateDirectory(Location.Directory);
			await FileReference.WriteAllBytesAsync(Location, data, cancellationToken);
		}
	}
}
