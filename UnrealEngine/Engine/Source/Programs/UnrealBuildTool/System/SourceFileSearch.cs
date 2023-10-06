// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Linq;
using EpicGames.Core;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	static class SourceFileSearch
	{
		// Certain file types should never be added to project files. These extensions must all be lowercase.
		static readonly string[] DefaultExcludedFileSuffixes = new string[]
		{
			".vcxproj",				// Visual Studio project files
			".vcxproj.filters",		// Visual Studio filter file
			".vcxproj.user",		// Visual Studio project user file
			".vcxproj.vspscc",		// Visual Studio source control state
			".sln",					// Visual Studio solution file
			".sdf",					// Visual Studio embedded symbol database file
			".suo",					// Visual Studio solution option files
			".sln.docstates.suo",	// Visual Studio temp file
			".tmp",					// Temp files used by P4 diffing (e.g. t6884t87698.tmp)
			".csproj",				// Visual Studio C# project files
			".userprefs",			// MonoDevelop project settings
			".ds_store",			// Mac Desktop Services Store hidden files
			".bin",					// Binary files
			Path.DirectorySeparatorChar + "do_not_delete.txt",		// Perforce placeholder file
		};

		// Default directory names to exclude. Must be lowercase.
		static readonly string[] DefaultExcludedDirectorySuffixes = new string[]
		{
			Path.DirectorySeparatorChar + "intermediate",
			Path.DirectorySeparatorChar + "source" + Path.DirectorySeparatorChar + "thirdparty",
			Path.DirectorySeparatorChar + "third_party",
		};

		/// Finds mouse source files
		public static List<FileReference> FindModuleSourceFiles(FileReference ModuleRulesFile, bool SearchSubdirectories = true, HashSet<DirectoryReference>? SearchedDirectories = null)
		{
			// The module's "base directory" is simply the directory where its xxx.Build.cs file is stored.  We'll always
			// harvest source files for this module in this base directory directory and all of its sub-directories.
			return SourceFileSearch.FindFiles(ModuleRulesFile.Directory, SubdirectoryNamesToExclude: null, SearchSubdirectories: SearchSubdirectories, SearchedDirectories: SearchedDirectories);
		}

		/// <summary>
		/// Fills in a project file with source files discovered in the specified list of paths
		/// </summary>
		/// <param name="DirectoryToSearch">Directory to search</param>
		/// <param name="SubdirectoryNamesToExclude">Directory base names to ignore when searching subdirectories.  Can be null.</param>
		/// <param name="SearchSubdirectories">True to include subdirectories, otherwise we only search the list of base directories</param>
		/// <param name="SearchedDirectories">If non-null, is updated with a list of the directories searched</param>
		public static List<FileReference> FindFiles(DirectoryReference DirectoryToSearch, List<string>? SubdirectoryNamesToExclude = null, bool SearchSubdirectories = true, HashSet<DirectoryReference>? SearchedDirectories = null)
		{
			// Build a list of directory names that we don't want to search under. We always ignore intermediate directories.
			string[] ExcludedDirectorySuffixes;
			if (SubdirectoryNamesToExclude == null)
			{
				ExcludedDirectorySuffixes = DefaultExcludedDirectorySuffixes;
			}
			else
			{
				ExcludedDirectorySuffixes = SubdirectoryNamesToExclude.Select(x => Path.DirectorySeparatorChar + x.ToLowerInvariant()).Union(DefaultExcludedDirectorySuffixes).ToArray();
			}

			// Find all the files
			List<FileReference> FoundFiles = new List<FileReference>();
			if (SearchSubdirectories)
			{
				FindFilesInternalRecursive(DirectoryToSearch, ExcludedDirectorySuffixes, FoundFiles, SearchedDirectories);
			}
			else
			{
				FindFilesInternal(DirectoryToSearch, ExcludedDirectorySuffixes, FoundFiles, SearchedDirectories);
			}
			return FoundFiles;
		}

		static void FindFilesInternal(DirectoryReference Directory, string[] ExcludedDirectorySuffixes, List<FileReference> FoundFiles, HashSet<DirectoryReference>? SearchedDirectories)
		{
			if (SearchedDirectories != null)
			{
				SearchedDirectories.Add(Directory);
			}

			foreach (FileReference File in DirectoryLookupCache.EnumerateFiles(Directory))
			{
				if (ShouldInclude(File, DefaultExcludedFileSuffixes))
				{
					FoundFiles.Add(File);
				}
			}
		}

		static void FindFilesInternalRecursive(DirectoryReference Directory, string[] ExcludedDirectorySuffixes, List<FileReference> FoundFiles, HashSet<DirectoryReference>? SearchedDirectories)
		{
			FindFilesInternal(Directory, ExcludedDirectorySuffixes, FoundFiles, SearchedDirectories);

			foreach (DirectoryReference SubDirectory in DirectoryLookupCache.EnumerateDirectories(Directory))
			{
				if (ShouldInclude(SubDirectory, ExcludedDirectorySuffixes))
				{
					FindFilesInternalRecursive(SubDirectory, ExcludedDirectorySuffixes, FoundFiles, SearchedDirectories);
				}
			}
		}

		static bool ShouldInclude(FileSystemReference Reference, string[] ExcludedSuffixes)
		{
			// Ignore Mac resource fork files on non-HFS partitions
			if (Path.GetFileName(Reference.FullName).StartsWith("._"))
			{
				return false;
			}

			foreach (string ExcludedSuffix in ExcludedSuffixes)
			{
				if (Reference.FullName.EndsWith(ExcludedSuffix, FileSystemReference.Comparison))
				{
					return false;
				}
			}
			return true;
		}
	}
}
