// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Linq;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Horde.Build.Users;

namespace Horde.Build.Perforce
{
	/// <summary>
	/// Summary information for a change
	/// </summary>
	[DebuggerDisplay("{Number}: {Description}")]
	public class ChangeSummary
	{
		/// <summary>
		/// The changelist number
		/// </summary>
		public int Number { get; set; }

		/// <summary>
		/// Author of the change
		/// </summary>
		public IUser Author { get; set; }

		/// <summary>
		/// The base path for modified files
		/// </summary>
		public string Path { get; set; }

		/// <summary>
		/// Abbreviated changelist description
		/// </summary>
		public string Description { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="number">Changelist number</param>
		/// <param name="author">Author of the change</param>
		/// <param name="path">Base path for modified files</param>
		/// <param name="description">Changelist description</param>
		public ChangeSummary(int number, IUser author, string path, string description)
		{
			Number = number;
			Author = author;
			Path = path;
			Description = description;
		}
	}

	/// <summary>
	/// Flags identifying content of a changelist
	/// </summary>
	[Flags]
	public enum ChangeContentFlags
	{
		/// <summary>
		/// The change contains code
		/// </summary>
		ContainsCode = 1,

		/// <summary>
		/// The change contains content
		/// </summary>
		ContainsContent = 2,
	}

	/// <summary>
	/// Modified file in a changelist
	/// </summary>
	public class ChangeFile
	{
		/// <summary>
		/// Path to the file
		/// </summary>
		public string Path { get; set; }

		/// <summary>
		/// Path to the file within the depot
		/// </summary>
		public string DepotPath { get; set; }

		/// <summary>
		/// Revision of the file. A value of -1 indicates that the file was deleted.
		/// </summary>
		public int Revision { get; set; }

		/// <summary>
		/// Length of the file
		/// </summary>
		public long Length { get; set; }

		/// <summary>
		/// MD5 digest of the file
		/// </summary>
		public Md5Hash? Digest { get; set; }

		/// <summary>
		/// The file type
		/// </summary>
		public string Type { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="path"></param>
		/// <param name="depotPath"></param>
		/// <param name="revision"></param>
		/// <param name="length"></param>
		/// <param name="digest"></param>
		/// <param name="type"></param>
		public ChangeFile(string path, string depotPath, int revision, long length, Md5Hash? digest, string type)
		{
			Path = path;
			DepotPath = depotPath;
			Revision = revision;
			Length = length;
			Digest = digest;
			Type = type;
		}
	}

	/// <summary>
	/// Information about a commit
	/// </summary>
	[DebuggerDisplay("{Number}: {Description}")]
	public class ChangeDetails
	{
		/// <summary>
		/// Set of extensions to treat as code
		/// </summary>
		static readonly HashSet<string> s_codeExtensions = new HashSet<string>
		{
			".c",
			".cc",
			".cpp",
			".inl",
			".m",
			".mm",
			".rc",
			".cs",
			".csproj",
			".h",
			".hpp",
			".inl",
			".usf",
			".ush",
			".uproject",
			".uplugin",
			".sln"
		};

		/// <summary>
		/// The source changelist number
		/// </summary>
		public int Number { get; set; }

		/// <summary>
		/// Name of the user that authored this change
		/// </summary>
		public IUser Author { get; set; }

		/// <summary>
		/// The base path for modified files
		/// </summary>
		public string Path { get; set; }

		/// <summary>
		/// The description text
		/// </summary>
		public string Description { get; set; }

		/// <summary>
		/// List of files that were modified, relative to the stream base
		/// </summary>
		public List<ChangeFile> Files { get; set; }

		/// <summary>
		/// Date that the change was submitted
		/// </summary>
		public DateTime Date { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="number">Changelist number</param>
		/// <param name="author">Author of the change</param>
		/// <param name="path">Base path for modified files</param>
		/// <param name="description">Changelist description</param>
		/// <param name="files">List of files modified, relative to the stream base</param>
		/// <param name="date">Date that the change was submitted</param>
		public ChangeDetails(int number, IUser author, string path, string description, List<ChangeFile> files, DateTime date)
		{
			Number = number;
			Author = author;
			Path = path;
			Description = description;
			Files = files;
			Date = date;
		}

		/// <summary>
		/// Determines if this change is a code change
		/// </summary>
		/// <returns>True if this change is a code change</returns>
		public ChangeContentFlags GetContentFlags()
		{
			ChangeContentFlags scope = 0;

			// Check whether the files are code or content
			foreach (ChangeFile file in Files)
			{
				if (s_codeExtensions.Any(extension => file.Path.EndsWith(extension, StringComparison.OrdinalIgnoreCase)))
				{
					scope |= ChangeContentFlags.ContainsCode;
				}
				else
				{
					scope |= ChangeContentFlags.ContainsContent;
				}

				if (scope == (ChangeContentFlags.ContainsCode | ChangeContentFlags.ContainsContent))
				{
					break;
				}
			}
			return scope;
		}

		/// <summary>
		/// Attempts to parse the Robomerge source from this commit information
		/// </summary>
		/// <param name="source">On success, receives the source information</param>
		/// <returns>True if the commit was merged from another stream</returns>
		public bool TryParseRobomergeSource([NotNullWhen(true)] out (string, int)? source)
		{
			Match match = Regex.Match(Description, @"#ROBOMERGE-SOURCE: CL (\d+) in (//[^ ]*)/...", RegexOptions.Multiline);
			if (match.Success)
			{
				source = (match.Groups[2].Value, Int32.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture));
				return true;
			}
			else
			{
				source = null;
				return false;
			}
		}
	}

	/// <summary>
	/// Summary of a file in the depot
	/// </summary>
	public class FileSummary
	{
		/// <summary>
		/// Depot path to the file
		/// </summary>
		public string DepotPath { get; set; }

		/// <summary>
		/// Whether the file exists
		/// </summary>
		public bool Exists { get; set; }

		/// <summary>
		/// Last changelist number that the file was modified
		/// </summary>
		public int Change { get; set; }

		/// <summary>
		/// Error about this file, if it exists
		/// </summary>
		public string? Error { get; set; }

		/// <summary>
		/// Information about a file
		/// </summary>
		/// <param name="depotPath">Depot </param>
		/// <param name="exists"></param>
		/// <param name="change"></param>
		/// <param name="error"></param>
		public FileSummary(string depotPath, bool exists, int change, string? error = null)
		{
			DepotPath = depotPath;
			Exists = exists;
			Change = change;
			Error = error;
		}
	}
}
