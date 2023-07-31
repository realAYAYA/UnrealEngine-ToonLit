// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;

namespace EpicGames.Core
{

	/// <summary>
	/// This interface exists to allow the CsProjBuilder in EpicGames.MsBuild to call back out
	/// into EpicGames.Build.  It is EXTREMELY important that any type definitions that must be 
	/// referenced or implemented in EpicGames.Build for use by EpicGame.MsBuild *NOT* be defined
	/// in EpicGames.MsBuild.  If they are, there is a strong chance of running into an issue 
	/// gathering types (Assembly.GetTypes()) on EpicGames.Build.  
	/// </summary>
	public interface CsProjBuildHook
	{

		/// <summary>
		/// Test the cache for a given file to get the last write time of the given file 
		/// </summary>
		/// <param name="BasePath">Base path of the file.</param>
		/// <param name="RelativeFilePath">Relative path of the file</param>
		/// <returns>Last write time of the file.</returns>
		DateTime GetLastWriteTime(DirectoryReference BasePath, string RelativeFilePath);

		/// <summary>
		/// Test the cache for a given file to get the last write time of the given file 
		/// </summary>
		/// <param name="BasePath">Base path of the file.</param>
		/// <param name="RelativeFilePath">Relative path of the file</param>
		/// <returns>Last write time of the file.</returns>
		DateTime GetLastWriteTime(string BasePath, string RelativeFilePath);

		/// <summary>
		/// Return the build record directory for the given base path (i.e. engine dir or project dir)
		/// </summary>
		/// <param name="BasePath">The base path for the directory</param>
		/// <returns>Directory for the build records</returns>
		DirectoryReference GetBuildRecordDirectory(DirectoryReference BasePath);

		/// <summary>
		/// Validate the given build records for the project
		/// </summary>
		/// <param name="buildRecords">Build records being validated.  This also includes build records for dependencies.</param>
		/// <param name="projectPath">Path of the project</param>
		void ValidateRecursively(Dictionary<FileReference, CsProjBuildRecordEntry> buildRecords, FileReference projectPath);

		/// <summary>
		/// Test to see if the given file spec has any wild cards
		/// </summary>
		/// <param name="FileSpec">File spec to test</param>
		/// <returns>True if wildcards are present</returns>
		bool HasWildcards(string FileSpec);

		/// <summary>
		/// Unreal engine directory
		/// </summary>
		DirectoryReference EngineDirectory { get; }

		/// <summary>
		/// Dotnet directory shipped with the engine
		/// </summary>
		DirectoryReference DotnetDirectory { get; }

		/// <summary>
		/// Dotnet program
		/// </summary>
		FileReference DotnetPath { get; }
	}
}
