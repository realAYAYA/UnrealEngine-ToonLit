// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace EpicGames.UBA
{
	/// <summary>
	/// Utils
	/// </summary>
	public static class Utils
	{

		/// <summary>
		/// Is UBA available?
		/// </summary>
		public static bool IsAvailable() => s_available.Value;
		static readonly Lazy<bool> s_available = new Lazy<bool>(() => File.Exists(GetLibraryPath()));

		/// <summary>
		/// Paths that are not allowed to be transferred over the network for UBA remote agents.
		/// </summary>
		/// <returns>enumerable of disallowed paths</returns>
		public static IEnumerable<string> DisallowedPaths() => s_disallowedPaths;
		static readonly SortedSet<string> s_disallowedPaths = new();

		/// <summary>
		/// Registers a path that is not allowed to be transferred over the network for UBA remote agents.
		/// </summary>
		/// <param name="paths">The paths to add to thie disallowed list</param>
		public static void RegisterDisallowedPaths(params string[] paths) => s_disallowedPaths.UnionWith(paths);

		/// <summary>
		/// Get the path to the p/invoke library that would be loaded 
		/// </summary>
		/// <returns>The path to the library</returns>
		/// <exception cref="PlatformNotSupportedException">If the operating system is not supported</exception>
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Globalization", "CA1308:Normalize strings to uppercase", Justification = "folder path is lowercase")]
		static string GetLibraryPath()
		{
			string arch = RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant();
			string assemblyFolder = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location)!;
			if (OperatingSystem.IsWindows())
			{
				return Path.Combine(assemblyFolder, "runtimes", $"win-{arch}", "native", "UbaHost.dll");
			}
			else if (OperatingSystem.IsLinux())
			{
				return Path.Combine(assemblyFolder, "runtimes", $"linux-{arch}", "native", "libUbaHost.so");
			}
			else if (OperatingSystem.IsMacOS())
			{
				return Path.Combine(assemblyFolder, "runtimes", $"osx-{arch}", "native", "libUbaHost.dylib");
			}
			throw new PlatformNotSupportedException();
		}
	}
}
