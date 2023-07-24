// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;
using System.Linq;

namespace UnrealBuildBase
{
	static public class DirectoryLookupCache
	{
		static public bool FileExists(FileReference File)
		{
			return FileItem.GetItemByFileReference(File).Exists;
		}

		static public bool DirectoryExists(DirectoryReference Directory)
		{
			return DirectoryItem.GetItemByDirectoryReference(Directory).Exists;
		}

		static public IEnumerable<FileReference> EnumerateFiles(DirectoryReference Directory)
		{
			return DirectoryItem.GetItemByDirectoryReference(Directory).EnumerateFiles().Select(x => x.Location);
		}

		static public IEnumerable<DirectoryReference> EnumerateDirectories(DirectoryReference Directory)
		{
			return DirectoryItem.GetItemByDirectoryReference(Directory).EnumerateDirectories().Select(x => x.Location);
		}

        static public void InvalidateCachedDirectory(DirectoryReference Directory)
        {
			DirectoryItem.GetItemByDirectoryReference(Directory).ResetCachedInfo();
        }
	}
}
