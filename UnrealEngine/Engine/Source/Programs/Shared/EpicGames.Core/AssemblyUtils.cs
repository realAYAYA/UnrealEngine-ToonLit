// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;

namespace EpicGames.Core
{
	/// <summary>
	/// Utilities for dealing with assembly loading
	/// </summary>
	public static class AssemblyUtils
	{
		/// <summary>
		/// Gets the original location (path and filename) of an assembly.
		/// This method is using Assembly.CodeBase property to properly resolve original
		/// assembly path in case shadow copying is enabled.
		/// </summary>
		/// <returns>Absolute path and filename to the assembly.</returns>
		public static string GetOriginalLocation(this Assembly thisAssembly)
		{
			return new Uri(thisAssembly.Location).LocalPath;
		}

		/// <summary>
		/// Version info of the executable which runs this code.
		/// </summary>
		public static FileVersionInfo ExecutableVersion => FileVersionInfo.GetVersionInfo(Assembly.GetEntryAssembly()!.GetOriginalLocation());

		/// <summary>
		/// Installs an assembly resolver. Mostly used to get shared assemblies that we don't want copied around to various output locations as happens when "Copy Local" is set to true
		/// for an assembly reference (which is the default).
		/// </summary>
		public static void InstallAssemblyResolver(string pathToBinariesDotNET)
		{
			AppDomain.CurrentDomain.AssemblyResolve += (sender, args) =>
			{
				// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
				string assemblyName = args.Name!.Split(',')[0];

				return (
					from knownAssemblyName in new[] { "SwarmAgent.exe", "../ThirdParty/Ionic/Ionic.Zip.Reduced.dll", "../ThirdParty/Newtonsoft/NewtonSoft.Json.dll" }
					where assemblyName.Equals(Path.GetFileNameWithoutExtension(knownAssemblyName), StringComparison.OrdinalIgnoreCase)
					let resolvedAssemblyFilename = Path.Combine(pathToBinariesDotNET, knownAssemblyName)
					// check if the file exists first. If we just try to load it, we correctly throw an exception, but it's a generic
					// FileNotFoundException, which is not informative. Better to return null.
					select File.Exists(resolvedAssemblyFilename) ? Assembly.LoadFile(resolvedAssemblyFilename) : null
					).FirstOrDefault();
			};
		}

		/// <summary>
		/// Installs an assembly resolver, which will load *any* assembly which exists recursively within the supplied folder.
		/// </summary>
		/// <param name="rootDirectory">The directory to enumerate.</param>
		public static void InstallRecursiveAssemblyResolver(string rootDirectory)
		{
			RefreshAssemblyCache(rootDirectory);

			AppDomain.CurrentDomain.AssemblyResolve += (sender, args) =>
			{
				// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
				string assemblyName = args.Name!.Split(',')[0];

				// The assembly wasn't found by other resolvers, though may have been compiled or copied as a dependency
				RefreshAssemblyCache(rootDirectory, String.Format("{0}.dll", assemblyName));
				if (s_assemblyLocationCache.TryGetValue(assemblyName, out string? assemblyLocation))
				{
					return Assembly.LoadFile(assemblyLocation);
				}

				return null;
			};
		}

		private static void RefreshAssemblyCache(string rootDirectory, string pattern = "*.dll")
		{
			// Initialize our cache of assemblies by enumerating all files in the given folder.
			foreach (string discoveredAssembly in Directory.EnumerateFiles(rootDirectory, pattern, SearchOption.AllDirectories))
			{
				AddFileToAssemblyCache(discoveredAssembly);
			}
		}

		/// <summary>
		/// Adds a file to the cache
		/// </summary>
		/// <param name="assemblyPath"></param>
		public static void AddFileToAssemblyCache(string assemblyPath)
		{
			// Ignore any reference assemblies
			string? directory = Path.GetFileName(Path.GetDirectoryName(assemblyPath));
			if (!String.IsNullOrEmpty(directory) && (directory == "ref" | directory == "refint"))
			{
				return;
			}

			string assemblyName = Path.GetFileNameWithoutExtension(assemblyPath);
			DateTime assemblyLastWriteTime = File.GetLastWriteTimeUtc(assemblyPath);
			lock(s_assemblyLocationCache)
			{
				if (s_assemblyLocationCache.ContainsKey(assemblyName))
				{
					// We already have this assembly in our cache. Only replace it if the discovered file is newer (to avoid stale assemblies breaking stuff).
					if (assemblyLastWriteTime > s_assemblyWriteTimes[assemblyName])
					{
						s_assemblyLocationCache[assemblyName] = assemblyPath;
						s_assemblyWriteTimes[assemblyName] = assemblyLastWriteTime;
					}
				}
				else
				{
					// This is the first copy of this assembly ... add it to our cache.
					s_assemblyLocationCache.Add(assemblyName, assemblyPath);
					s_assemblyWriteTimes.Add(assemblyName, assemblyLastWriteTime);
				}
			}

			if (!s_addedToAssemblyResolver)
			{
				AppDomain.CurrentDomain.AssemblyResolve += (sender, args) =>
				{
					// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
					string assemblyName = args.Name!.Split(',')[0];
					lock (s_assemblyLocationCache)
					{
						if (s_assemblyLocationCache.TryGetValue(assemblyName, out string? assemblyLocation))
						{
							// We have this assembly in our folder.					
							if (File.Exists(assemblyLocation))
							{
								// The assembly still exists, so load it.
								return Assembly.LoadFile(assemblyLocation);
							}
							else
							{
								// The assembly no longer exists on disk, so remove it from our cache.
								s_assemblyLocationCache.Remove(assemblyName);
							}
						}
					}

					return null;
				};

				s_addedToAssemblyResolver = true;
			}
		}

		// Map of assembly name to path on disk
		private static readonly Dictionary<string, string> s_assemblyLocationCache = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
		// Track last modified date of each assembly, so we can ensure we always reference the latest one in the case of stale assemblies on disk.
		private static readonly Dictionary<string, DateTime> s_assemblyWriteTimes = new Dictionary<string, DateTime>(StringComparer.OrdinalIgnoreCase);
		// Flag used to make sure we don't redundantly add resolvers
		private static bool s_addedToAssemblyResolver = false;

	}
}
