// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;

namespace UnrealBuildTool
{
	/// <summary>
	/// Class that stores info about aliased file.
	/// </summary>
	struct AliasedFile
	{
		public AliasedFile(FileReference Location, string FileSystemPath, string ProjectPath)
		{
			this.Location = Location;
			this.FileSystemPath = FileSystemPath;
			this.ProjectPath = ProjectPath;
		}

		// Full location on disk.
		public readonly FileReference Location;

		// File system path.
		public readonly string FileSystemPath;

		// Project path.
		public readonly string ProjectPath;
	}
}
