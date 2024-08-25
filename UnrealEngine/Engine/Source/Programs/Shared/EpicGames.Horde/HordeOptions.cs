// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using EpicGames.Horde.Storage.Bundles;
using Microsoft.Win32;

namespace EpicGames.Horde
{
	/// <summary>
	/// Options for configuring the Horde connection
	/// </summary>
	public class HordeOptions
	{
		/// <summary>
		/// Address of the Horde server
		/// </summary>
		public Uri? ServerUrl { get; set; }

		/// <summary>
		/// Access token to use for connecting to the server
		/// </summary>
		public string? AccessToken { get; set; }

		/// <summary>
		/// Whether to allow opening a browser window to prompt for authentication
		/// </summary>
		public bool AllowAuthPrompt { get; set; } = true;

		/// <summary>
		/// Callback to allow configuring any HTTP client created for Horde
		/// </summary>
		public Action<HttpClient>? ConfigureHttpClient { get; set; }

		/// <summary>
		/// Options for creating new bundles
		/// </summary>
		public BundleOptions Bundle { get; } = new BundleOptions();

		/// <summary>
		/// Options for caching bundles 
		/// </summary>
		public BundleCacheOptions BundleCache { get; } = new BundleCacheOptions();

		/// <summary>
		/// Gets the default server URL for the current user
		/// </summary>
		/// <returns>Default URL</returns>
		public static Uri? GetDefaultServerUrl()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				string? url =
					(Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Epic Games\Horde", "Url", null) as string) ??
					(Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Epic Games\Horde", "Url", null) as string);

				if (!String.IsNullOrEmpty(url))
				{
					try
					{
						return new Uri(url);
					}
					catch (UriFormatException)
					{
					}
				}
			}
			return null;
		}

		/// <summary>
		/// Sets the default server url for the current user
		/// </summary>
		/// <param name="serverUrl">Horde server URL to use</param>
		public static void SetDefaultServerUrl(Uri serverUrl)
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				string? newServerUrl = serverUrl.ToString();
				string? defaultServerUrl = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Epic Games\Horde", "Url", null) as string;

				if (String.Equals(newServerUrl, defaultServerUrl, StringComparison.Ordinal))
				{
					using RegistryKey key = Registry.CurrentUser.CreateSubKey("SOFTWARE\\Epic Games\\Horde");
					DeleteRegistryKey(key, "Url");
				}
				else
				{
					Registry.SetValue(@"HKEY_CURRENT_USER\SOFTWARE\Epic Games\Horde", "Url", serverUrl.ToString());
				}
			}
		}

		[SupportedOSPlatform("windows")]
		static void DeleteRegistryKey(RegistryKey key, string name)
		{
			string[] valueNames = key.GetValueNames();
			if (valueNames.Any(x => String.Equals(x, name, StringComparison.OrdinalIgnoreCase)))
			{
				try
				{
					key.DeleteValue(name);
				}
				catch
				{
				}
			}
		}
	}
}
