// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Utility functions for querying native projects (ie. those found via a .uprojectdirs query)
	/// </summary>
	public class NativeProjects : NativeProjectsBase
	{
		/// <summary>
		/// Cached map of target names to the project file they belong to
		/// </summary>
		static Dictionary<string, FileReference>? CachedTargetNameToProjectFile;

		/// <summary>
		/// Clear our cached properties. Generally only needed if your script has modified local files...
		/// </summary>
		public static void ClearCache()
		{
			ClearCacheBase();
			CachedTargetNameToProjectFile = null;
		}

		/// <summary>
		/// Get the project folder for the given target name
		/// </summary>
		/// <param name="InTargetName">Name of the target of interest</param>
		/// <param name="Logger">Logger for output</param>
		/// <param name="OutProjectFileName">The project filename</param>
		/// <returns>True if the target was found</returns>
		public static bool TryGetProjectForTarget(string InTargetName, ILogger Logger, [NotNullWhen(true)] out FileReference? OutProjectFileName)
		{
			if(CachedTargetNameToProjectFile == null)
			{
				lock(LockObject)
				{
					if(CachedTargetNameToProjectFile == null)
					{
						Dictionary<string, FileReference> TargetNameToProjectFile = new Dictionary<string, FileReference>();
						foreach(FileReference ProjectFile in EnumerateProjectFiles(Logger))
						{
							foreach (DirectoryReference ExtensionDir in Unreal.GetExtensionDirs(ProjectFile.Directory))
							{
								DirectoryReference SourceDirectory = DirectoryReference.Combine(ExtensionDir, "Source");
								if (DirectoryLookupCache.DirectoryExists(SourceDirectory))
								{
									FindTargetFiles(SourceDirectory, TargetNameToProjectFile, ProjectFile);
								}

								DirectoryReference IntermediateSourceDirectory = DirectoryReference.Combine(ExtensionDir, "Intermediate", "Source");
								if (DirectoryLookupCache.DirectoryExists(IntermediateSourceDirectory))
								{
									FindTargetFiles(IntermediateSourceDirectory, TargetNameToProjectFile, ProjectFile);
								}
							}
						}
						CachedTargetNameToProjectFile = TargetNameToProjectFile;
					}
				}
			}
			return CachedTargetNameToProjectFile.TryGetValue(InTargetName, out OutProjectFileName);
		}
	}
}
