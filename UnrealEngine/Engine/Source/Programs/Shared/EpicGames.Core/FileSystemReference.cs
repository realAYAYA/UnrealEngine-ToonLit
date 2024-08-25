// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

namespace EpicGames.Core
{
	/// <summary>
	/// Base class for file system objects (files or directories).
	/// </summary>
	[Serializable]
	public abstract class FileSystemReference
	{
		/// <summary>
		/// The path to this object. Stored as an absolute path, with O/S preferred separator characters, and no trailing slash for directories.
		/// </summary>
		public string FullName { get; }

		/// <summary>
		/// The comparer to use for file system references
		/// </summary>
		public static StringComparer Comparer { get; } = StringComparer.OrdinalIgnoreCase;

		/// <summary>
		/// The comparison to use for file system references
		/// </summary>
		public static StringComparison Comparison { get; } = StringComparison.OrdinalIgnoreCase;

		/// <summary>
		/// Direct constructor for a path
		/// </summary>
		protected FileSystemReference(string fullName) => FullName = fullName;

		static readonly ThreadLocal<StringBuilder> s_combineStringsStringBuilder = new ThreadLocal<StringBuilder>(() => new StringBuilder(260));

		/// <summary>
		/// Create a full path by concatenating multiple strings
		/// </summary>
		protected static string CombineStrings(DirectoryReference baseDirectory, params string[] fragments)
		{
			// Get the initial string to append to, and strip any root directory suffix from it
			StringBuilder newFullName = s_combineStringsStringBuilder.Value!.Clear().Append(baseDirectory.FullName);
			if (newFullName.Length > 0 && newFullName[^1] == Path.DirectorySeparatorChar)
			{
				newFullName.Remove(newFullName.Length - 1, 1);
			}

			// Scan through the fragments to append, appending them to a string and updating the base length as we go
			foreach (string fragment in fragments)
			{
				// Check if this fragment is an absolute path
				if ((fragment.Length >= 2 && fragment[1] == ':') || (fragment.Length >= 1 && (fragment[0] == '\\' || fragment[0] == '/')))
				{
					// It is. Reset the new name to the full version of this path.
					newFullName.Clear();
					newFullName.Append(Path.GetFullPath(fragment).TrimEnd(Path.DirectorySeparatorChar));
				}
				else
				{
					// Append all the parts of this fragment to the end of the existing path.
					int startIdx = 0;
					while (startIdx < fragment.Length)
					{
						// Find the end of this fragment. We may have been passed multiple paths in the same string.
						int endIdx = startIdx;
						while (endIdx < fragment.Length && fragment[endIdx] != '\\' && fragment[endIdx] != '/')
						{
							endIdx++;
						}

						// Ignore any empty sections, like leading or trailing slashes, and '.' directory references.
						int length = endIdx - startIdx;
						if (length == 0)
						{
							// Multiple directory separators in a row; illegal.
							throw new ArgumentException(String.Format("Path fragment '{0}' contains invalid directory separators.", fragment));
						}
						else if (length == 2 && fragment[startIdx] == '.' && fragment[startIdx + 1] == '.')
						{
							// Remove the last directory name
							for (int separatorIdx = newFullName.Length - 1; separatorIdx >= 0; separatorIdx--)
							{
								if (newFullName[separatorIdx] == Path.DirectorySeparatorChar)
								{
									newFullName.Remove(separatorIdx, newFullName.Length - separatorIdx);
									break;
								}
							}
						}
						else if (length != 1 || fragment[startIdx] != '.')
						{
							// Append this fragment
							newFullName.Append(Path.DirectorySeparatorChar);
							newFullName.Append(fragment, startIdx, length);
						}

						// Move to the next part
						startIdx = endIdx + 1;
					}
				}
			}

			// Append the directory separator
			if (newFullName.Length == 0 || (newFullName.Length == 2 && newFullName[1] == ':'))
			{
				newFullName.Append(Path.DirectorySeparatorChar);
			}

			// Set the new path variables
			return newFullName.ToString();
		}

		/// <summary>
		/// Checks whether this name has the given extension.
		/// </summary>
		/// <param name="extension">The extension to check</param>
		/// <returns>True if this name has the given extension, false otherwise</returns>
		public bool HasExtension(string extension) => extension.Length > 0 && extension[0] != '.'
				? FullName.Length >= extension.Length + 1 && FullName[FullName.Length - extension.Length - 1] == '.' && FullName.EndsWith(extension, Comparison)
				: FullName.EndsWith(extension, Comparison);

		/// <summary>
		/// Determines if the given object is at or under the given directory
		/// </summary>
		/// <param name="other">Directory to check against</param>
		/// <returns>True if this path is under the given directory</returns>
		public bool IsUnderDirectory(DirectoryReference other) => FullName.StartsWith(other.FullName, Comparison) && (FullName.Length == other.FullName.Length || FullName[other.FullName.Length] == Path.DirectorySeparatorChar || other.IsRootDirectory());

		/// <summary>
		/// Checks to see if this exists as either a file or directory
		/// This is helpful for Mac, because a binary may be a .app which is a directory
		/// </summary>
		/// <param name="location">FileSsytem object to check</param>
		/// <returns>True if a file or a directory exists</returns>
		public static bool Exists(FileSystemReference location) => File.Exists(location.FullName) || Directory.Exists(location.FullName);

		/// <summary>
		/// Searches the path fragments for the given name. Only complete fragments are considered a match.
		/// </summary>
		/// <param name="name">Name to check for</param>
		/// <param name="offset">Offset within the string to start the search</param>
		/// <returns>True if the given name is found within the path</returns>
		public bool ContainsName(string name, int offset) => ContainsName(name, offset, FullName.Length - offset);

		/// <summary>
		/// Searches the path fragments for the given name. Only complete fragments are considered a match.
		/// </summary>
		/// <param name="name">Name to check for</param>
		/// <param name="offset">Offset within the string to start the search</param>
		/// <param name="length">Length of the substring to search</param>
		/// <returns>True if the given name is found within the path</returns>
		public bool ContainsName(string name, int offset, int length)
		{
			// Check the substring to search is at least long enough to contain a match
			if (length < name.Length)
			{
				return false;
			}

			// Find each occurence of the name within the remaining string, then test whether it's surrounded by directory separators
			int matchIdx = offset;
			for (; ; )
			{
				// Find the next occurrence
				matchIdx = FullName.IndexOf(name, matchIdx, offset + length - matchIdx, Comparison);
				if (matchIdx == -1)
				{
					return false;
				}

				// Check if the substring is a directory
				int matchEndIdx = matchIdx + name.Length;
				if (FullName[matchIdx - 1] == Path.DirectorySeparatorChar && (matchEndIdx == FullName.Length || FullName[matchEndIdx] == Path.DirectorySeparatorChar))
				{
					return true;
				}

				// Move past the string that didn't match
				matchIdx += name.Length;
			}
		}

		/// <summary>
		/// Determines if the given object is under the given directory, within a subfolder of the given name. Useful for masking out directories by name.
		/// </summary>
		/// <param name="name">Name of a subfolder to also check for</param>
		/// <param name="baseDir">Base directory to check against</param>
		/// <returns>True if the path is under the given directory</returns>
		public bool ContainsName(string name, DirectoryReference baseDir) => IsUnderDirectory(baseDir) && ContainsName(name, baseDir.FullName.Length);

		/// <summary>
		/// Determines if the given object is under the given directory, within a subfolder of the given name. Useful for masking out directories by name.
		/// </summary>
		/// <param name="names">Names of subfolders to also check for</param>
		/// <param name="baseDir">Base directory to check against</param>
		/// <returns>True if the path is under the given directory</returns>
		public bool ContainsAnyNames(IEnumerable<string> names, DirectoryReference baseDir) => IsUnderDirectory(baseDir) && names.Any(x => ContainsName(x, baseDir.FullName.Length));

		static readonly ThreadLocal<StringBuilder> s_makeRelativeToStringBuilder = new ThreadLocal<StringBuilder>(() => new StringBuilder(260));

		/// <summary>
		/// Creates a relative path from the given base directory
		/// </summary>
		/// <param name="directory">The directory to create a relative path from</param>
		/// <returns>A relative path from the given directory</returns>
		public string MakeRelativeTo(DirectoryReference directory)
		{
			StringBuilder result = s_makeRelativeToStringBuilder.Value!.Clear();
			WriteRelativeTo(directory, result);
			return result.ToString();
		}

		/// <summary>
		/// Appens a relative path to a string builder
		/// </summary>
		/// <param name="directory"></param>
		/// <param name="result"></param>
		public void WriteRelativeTo(DirectoryReference directory, StringBuilder result)
		{
			// Find how much of the path is common between the two paths. This length does not include a trailing directory separator character.
			int commonDirectoryLength = -1;
			for (int idx = 0; ; idx++)
			{
				if (idx == FullName.Length)
				{
					// The two paths are identical. Just return the "." character.
					if (idx == directory.FullName.Length)
					{
						result.Append('.');
						return;
					}

					// Check if we're finishing on a complete directory name
					if (directory.FullName[idx] == Path.DirectorySeparatorChar)
					{
						commonDirectoryLength = idx;
					}
					break;
				}
				else if (idx == directory.FullName.Length)
				{
					// Check whether the end of the directory name coincides with a boundary for the current name.
					if (FullName[idx] == Path.DirectorySeparatorChar)
					{
						commonDirectoryLength = idx;
					}
					break;
				}
				else
				{
					// Check the two paths match, and bail if they don't. Increase the common directory length if we've reached a separator.
					if (String.Compare(FullName, idx, directory.FullName, idx, 1, Comparison) != 0)
					{
						break;
					}
					if (FullName[idx] == Path.DirectorySeparatorChar)
					{
						commonDirectoryLength = idx;
					}
				}
			}

			// If there's no relative path, just return the absolute path
			if (commonDirectoryLength == -1)
			{
				result.Append(FullName);
				return;
			}

			// Append all the '..' separators to get back to the common directory, then the rest of the string to reach the target item
			for (int idx = commonDirectoryLength + 1; idx < directory.FullName.Length; idx++)
			{
				// Move up a directory
				if (result.Length != 0)
				{
					result.Append(Path.DirectorySeparatorChar);
				}
				result.Append("..");

				// Scan to the next directory separator
				while (idx < directory.FullName.Length && directory.FullName[idx] != Path.DirectorySeparatorChar)
				{
					idx++;
				}
			}

			if (commonDirectoryLength + 1 < FullName.Length)
			{
				if (result.Length != 0)
				{
					result.Append(Path.DirectorySeparatorChar);
				}
				result.Append(FullName, commonDirectoryLength + 1, FullName.Length - commonDirectoryLength - 1);
			}
		}

		/// <summary>
		/// Normalize the path to using forward slashes
		/// </summary>
		/// <returns></returns>
		public string ToNormalizedPath() => FullName.Replace("\\", "/", StringComparison.Ordinal);

		/// <summary>
		/// Returns a string representation of this filesystem object
		/// </summary>
		/// <returns>Full path to the object</returns>
		public override string ToString() => FullName;
	}
}
