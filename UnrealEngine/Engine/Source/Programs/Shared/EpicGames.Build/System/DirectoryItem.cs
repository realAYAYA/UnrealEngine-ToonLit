// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using EpicGames.Core;

namespace UnrealBuildBase
{
	/// <summary>
	/// Stores the state of a directory. May or may not exist.
	/// </summary>
	public class DirectoryItem : IComparable<DirectoryItem>, IEquatable<DirectoryItem>
	{
		/// <summary>
		/// Full path to the directory on disk
		/// </summary>
		public readonly DirectoryReference Location;

		/// <summary>
		/// Cached value for whether the directory exists
		/// </summary>
		Lazy<DirectoryInfo> Info;

		/// <summary>
		/// Cached map of name to subdirectory item
		/// </summary>
		Dictionary<string, DirectoryItem>? Directories;

		/// <summary>
		/// Cached map of name to file
		/// </summary>
		Dictionary<string, FileItem>? Files;

		/// <summary>
		/// Global map of location to item
		/// </summary>
		static ConcurrentDictionary<DirectoryReference, DirectoryItem> LocationToItem = new ConcurrentDictionary<DirectoryReference, DirectoryItem>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Location">Path to this directory</param>
		/// <param name="Info">Information about this directory</param>
		private DirectoryItem(DirectoryReference Location, DirectoryInfo Info)
		{
			this.Location = Location;
			if (RuntimePlatform.IsWindows)
			{
				this.Info = new Lazy<DirectoryInfo>(Info);
			}
			else
			{
				// For some reason we need to call an extra Refresh on linux/mac to not get wrong results from "Exists"
				this.Info = new Lazy<DirectoryInfo>(() =>
				{
					Info.Refresh();
					return Info;
				});
			}
		}

		/// <summary>
		/// The name of this directory
		/// </summary>
		public string Name => Info.Value.Name;

		/// <summary>
		/// The full name of this directory
		/// </summary>
		public string FullName => Location.FullName;

		/// <summary>
		/// Whether the directory exists or not
		/// </summary>
		public bool Exists => Info.Value.Exists;

		/// <summary>
		/// The last write time of the file.
		/// </summary>
		public DateTime LastWriteTimeUtc => Info.Value.LastWriteTimeUtc;

		/// <summary>
		/// The creation time of the file.
		/// </summary>
		public DateTime CreationTimeUtc => Info.Value.CreationTimeUtc;

		/// <summary>
		/// Gets the parent directory item
		/// </summary>
		public DirectoryItem? GetParentDirectoryItem()
		{
			if (Info.Value.Parent == null)
			{
				return null;
			}
			else
			{
				return GetItemByDirectoryInfo(Info.Value.Parent);
			}
		}

		/// <summary>
		/// Gets a new directory item by combining the existing directory item with the given path fragments
		/// </summary>
		/// <param name="BaseDirectory">Base directory to append path fragments to</param>
		/// <param name="Fragments">The path fragments to append</param>
		/// <returns>Directory item corresponding to the combined path</returns>
		public static DirectoryItem Combine(DirectoryItem BaseDirectory, params string[] Fragments)
		{
			return DirectoryItem.GetItemByDirectoryReference(DirectoryReference.Combine(BaseDirectory.Location, Fragments));
		}

		/// <summary>
		/// Finds or creates a directory item from its location
		/// </summary>
		/// <param name="Location">Path to the directory</param>
		/// <returns>The directory item for this location</returns>
		public static DirectoryItem GetItemByPath(string Location)
		{
			return GetItemByDirectoryReference(new DirectoryReference(Location));
		}

		/// <summary>
		/// Finds or creates a directory item from its location
		/// </summary>
		/// <param name="Location">Path to the directory</param>
		/// <returns>The directory item for this location</returns>
		public static DirectoryItem GetItemByDirectoryReference(DirectoryReference Location)
		{
			if (LocationToItem.TryGetValue(Location, out DirectoryItem? Result))
			{
				return Result;
			}
			return LocationToItem.GetOrAdd(Location, new DirectoryItem(Location, new DirectoryInfo(Location.FullName)));
		}

		/// <summary>
		/// Finds or creates a directory item from a DirectoryInfo object
		/// </summary>
		/// <param name="Info">Path to the directory</param>
		/// <returns>The directory item for this location</returns>
		public static DirectoryItem GetItemByDirectoryInfo(DirectoryInfo Info)
		{
			DirectoryReference Location = new DirectoryReference(Info);
			if (LocationToItem.TryGetValue(Location, out DirectoryItem? Result))
			{
				return Result;
			}
			return LocationToItem.GetOrAdd(Location, new DirectoryItem(Location, Info));
		}

		/// <summary>
		/// Reset the contents of the directory and allow them to be fetched again
		/// </summary>
		public void ResetCachedInfo()
		{
			Info = new Lazy<DirectoryInfo>(() =>
			{
				DirectoryInfo Info = Location.ToDirectoryInfo();
				Info.Refresh();
				return Info;
			});

			Dictionary<string, DirectoryItem>? PrevDirectories = Directories;
			if (PrevDirectories != null)
			{
				foreach (DirectoryItem SubDirectory in PrevDirectories.Values)
				{
					SubDirectory.ResetCachedInfo();
				}
				Directories = null;
			}

			Dictionary<string, FileItem>? PrevFiles = Files;
			if (PrevFiles != null)
			{
				foreach (FileItem File in PrevFiles.Values)
				{
					File.ResetCachedInfo();
				}
				Files = null;
			}
		}

		/// <summary>
		/// Resets the cached info, if the DirectoryInfo is not found don't create a new entry
		/// </summary>
		public static void ResetCachedInfo(string Path)
		{
			if (LocationToItem.TryGetValue(new DirectoryReference(Path), out DirectoryItem? Result))
			{
				Result.ResetCachedInfo();
			}
		}

		/// <summary>
		/// Resets all cached directory info. Significantly reduces performance; do not use unless strictly necessary.
		/// </summary>
		public static void ResetAllCachedInfo_SLOW()
		{
			foreach (DirectoryItem Item in LocationToItem.Values)
			{
				Item.Info = new Lazy<DirectoryInfo>(() =>
				{
					DirectoryInfo Info = Item.Location.ToDirectoryInfo();
					Info.Refresh();
					return Info;
				});
				Item.Directories = null;
				Item.Files = null;
			}
			FileItem.ResetAllCachedInfo_SLOW();
		}

		/// <summary>
		/// Caches the subdirectories of this directories
		/// </summary>
		public void CacheDirectories()
		{
			if (Directories == null)
			{
				Dictionary<string, DirectoryItem> NewDirectories;
				if (Info.Value.Exists)
				{
					DirectoryInfo[] Directories = Info.Value.GetDirectories();
					NewDirectories = new Dictionary<string, DirectoryItem>(Directories.Length, DirectoryReference.Comparer);
					foreach (DirectoryInfo SubDirectoryInfo in Directories)
					{
						if (NewDirectories.ContainsKey(SubDirectoryInfo.Name))
						{
							throw new Exception($"Trying to add {SubDirectoryInfo.FullName} as '{SubDirectoryInfo.Name}' yet exists as {NewDirectories[SubDirectoryInfo.Name].FullName}");
						}

						NewDirectories.Add(SubDirectoryInfo.Name, DirectoryItem.GetItemByDirectoryInfo(SubDirectoryInfo));
					}
				}
				else
				{
					NewDirectories = new Dictionary<string, DirectoryItem>(DirectoryReference.Comparer);
				}
				Directories = NewDirectories;
			}
		}

		/// <summary>
		/// Enumerates all the subdirectories
		/// </summary>
		/// <returns>Sequence of subdirectory items</returns>
		public IEnumerable<DirectoryItem> EnumerateDirectories()
		{
			CacheDirectories();
			return Directories!.Values;
		}

		/// <summary>
		/// Attempts to get a sub-directory by name
		/// </summary>
		/// <param name="Name">Name of the directory</param>
		/// <param name="OutDirectory">If successful receives the matching directory item with this name</param>
		/// <returns>True if the file exists, false otherwise</returns>
		public bool TryGetDirectory(string Name, [NotNullWhen(true)] out DirectoryItem? OutDirectory)
		{
			if (Name.Length > 0 && Name[0] == '.')
			{
				if (Name.Length == 1)
				{
					OutDirectory = this;
					return true;
				}
				else if (Name.Length == 2 && Name[1] == '.')
				{
					OutDirectory = GetParentDirectoryItem();
					return OutDirectory != null;
				}
			}

			CacheDirectories();
			return Directories!.TryGetValue(Name, out OutDirectory);
		}

		/// <summary>
		/// Caches the files in this directory
		/// </summary>
		public void CacheFiles()
		{
			if (Files == null)
			{
				Dictionary<string, FileItem> NewFiles;
				if (Info.Value.Exists)
				{
					FileInfo[] FileInfos = Info.Value.GetFiles();
					NewFiles = new Dictionary<string, FileItem>(FileInfos.Length, FileReference.Comparer);
					foreach (FileInfo FileInfo in FileInfos)
					{
						FileItem FileItem = FileItem.GetItemByFileInfo(FileInfo);
						FileItem.UpdateCachedDirectory(this);
						NewFiles[FileInfo.Name] = FileItem;
					}
				}
				else
				{
					NewFiles = new Dictionary<string, FileItem>(FileReference.Comparer);
				}
				Files = NewFiles;
			}
		}

		/// <summary>
		/// Enumerates all the files
		/// </summary>
		/// <returns>Sequence of FileItems</returns>
		public IEnumerable<FileItem> EnumerateFiles()
		{
			CacheFiles();
			return Files!.Values;
		}

		/// <summary>
		/// Attempts to get a file from this directory by name. Unlike creating a file item and checking whether it exists, this will
		/// not create a permanent FileItem object if it does not exist.
		/// </summary>
		/// <param name="Name">Name of the file</param>
		/// <param name="OutFile">If successful receives the matching file item with this name</param>
		/// <returns>True if the file exists, false otherwise</returns>
		public bool TryGetFile(string Name, [NotNullWhen(true)] out FileItem? OutFile)
		{
			CacheFiles();
			return Files!.TryGetValue(Name, out OutFile);
		}

		/// <summary>
		/// Formats this object as a string for debugging
		/// </summary>
		/// <returns>Location of the directory</returns>
		public override string ToString()
		{
			return Location.FullName;
		}

		/// <summary>
		/// Writes out all the enumerated files full names sorted to OutFile
		/// </summary>
		public static void WriteDebugFileWithAllEnumeratedFiles(string OutFile)
		{
			SortedSet<string> AllFiles = new SortedSet<string>();
			foreach (DirectoryItem Item in DirectoryItem.LocationToItem.Values)
			{
				if (Item.Files != null)
				{
					foreach (FileItem File in Item.EnumerateFiles())
					{
						AllFiles.Add(File.FullName);
					}
				}
			}
			File.WriteAllLines(OutFile, AllFiles);
		}

		#region IComparable, IEquatbale
		public int CompareTo(DirectoryItem? other)
		{
			return FullName.CompareTo(other?.FullName);
		}

		public bool Equals(DirectoryItem? other)
		{
			return FullName.Equals(other?.FullName);
		}

		public override bool Equals(object? obj)
		{
			if (ReferenceEquals(this, obj))
			{
				return true;
			}

			if (ReferenceEquals(obj, null))
			{
				return false;
			}

			return Equals((DirectoryItem?)obj);
		}

		public override int GetHashCode()
		{
			return FullName.GetHashCode();
		}

		public static bool operator ==(DirectoryItem? left, DirectoryItem? right)
		{
			if (ReferenceEquals(left, null))
			{
				return ReferenceEquals(right, null);
			}

			return left.Equals(right);
		}

		public static bool operator !=(DirectoryItem? left, DirectoryItem? right)
		{
			return !(left == right);
		}

		public static bool operator <(DirectoryItem? left, DirectoryItem? right)
		{
			return ReferenceEquals(left, null) ? !ReferenceEquals(right, null) : left.CompareTo(right) < 0;
		}

		public static bool operator <=(DirectoryItem? left, DirectoryItem? right)
		{
			return ReferenceEquals(left, null) || left.CompareTo(right) <= 0;
		}

		public static bool operator >(DirectoryItem? left, DirectoryItem? right)
		{
			return !ReferenceEquals(left, null) && left.CompareTo(right) > 0;
		}

		public static bool operator >=(DirectoryItem? left, DirectoryItem? right)
		{
			return ReferenceEquals(left, null) ? ReferenceEquals(right, null) : left.CompareTo(right) >= 0;
		}
		#endregion
	}

	/// <summary>
	/// Helper functions for serialization
	/// </summary>
	public static class DirectoryItemExtensionMethods
	{
		/// <summary>
		/// Read a directory item from a binary archive
		/// </summary>
		/// <param name="Reader">Reader to serialize data from</param>
		/// <returns>Instance of the serialized directory item</returns>
		public static DirectoryItem? ReadDirectoryItem(this BinaryArchiveReader Reader)
		{
			// Use lambda that doesn't require anything to be captured thus eliminating an allocation.
			return Reader.ReadObjectReference<DirectoryItem>((BinaryArchiveReader Reader) => DirectoryItem.GetItemByDirectoryReference(Reader.ReadDirectoryReferenceNotNull()));
		}

		/// <summary>
		/// Write a directory item to a binary archive
		/// </summary>
		/// <param name="Writer">Writer to serialize data to</param>
		/// <param name="DirectoryItem">Directory item to write</param>
		public static void WriteDirectoryItem(this BinaryArchiveWriter Writer, DirectoryItem DirectoryItem)
		{
			// Use lambda that doesn't require anything to be captured thus eliminating an allocation.
			Writer.WriteObjectReference<DirectoryItem>(DirectoryItem, (BinaryArchiveWriter Writer, DirectoryItem DirectoryItem) => Writer.WriteDirectoryReference(DirectoryItem.Location));
		}

		/// <summary>
		/// Writes a directory reference  to a binary archive
		/// </summary>
		/// <param name="Writer">The writer to output data to</param>
		/// <param name="Directory">The item to write</param>
		public static void WriteCompactDirectoryReference(this BinaryArchiveWriter Writer, DirectoryReference Directory)
		{
			DirectoryItem Item = DirectoryItem.GetItemByDirectoryReference(Directory);
			Writer.WriteDirectoryItem(Item);
		}

		/// <summary>
		/// Reads a directory reference from a binary archive
		/// </summary>
		/// <param name="Reader">Reader to serialize data from</param>
		/// <returns>New directory reference instance</returns>
		public static DirectoryReference ReadCompactDirectoryReference(this BinaryArchiveReader Reader)
		{
			return Reader.ReadDirectoryItem()!.Location;
		}

	}
}
