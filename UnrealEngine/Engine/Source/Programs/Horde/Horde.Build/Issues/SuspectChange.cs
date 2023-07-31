// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using EpicGames.Core;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Logs;
using Horde.Build.Perforce;

namespace Horde.Build.Issues
{
	/// <summary>
	/// Information about a changelist and a value ranking the likelihood that it caused an issue
	/// </summary>
	class SuspectChange
	{
		/// <summary>
		/// Set of extensions to treat as code
		/// </summary>
		static readonly HashSet<StringView> s_codeExtensions = new HashSet<StringView>(StringViewComparer.OrdinalIgnoreCase)
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
		/// Set of file extensions to treat as content
		/// </summary>
		static readonly HashSet<StringView> s_contentExtensions = new HashSet<StringView>(StringViewComparer.OrdinalIgnoreCase)
		{
			".uasset",
			".umap",
			".ini"
		};

		/// <summary>
		/// The change detials
		/// </summary>
		public ChangeDetails Details { get; set; }

		/// <summary>
		/// Rank for the change. A value less than or equal to zero indicates a lack of culpability, positive values indicate
		/// the possibility of a change being the culprit.
		/// </summary>
		public int Rank { get; set; }

		/// <summary>
		/// Whether the change contains code
		/// </summary>
		public bool ContainsCode { get; }

		/// <summary>
		/// Whether the change modifies content
		/// </summary>
		public bool ContainsContent { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="details">The changelist details</param>
		public SuspectChange(ChangeDetails details)
		{
			Details = details;

			foreach (ChangeFile file in details.Files)
			{
				int idx = file.Path.LastIndexOf('.');
				if (idx != -1)
				{
					StringView extension = new StringView(file.Path, idx);
					if (s_codeExtensions.Contains(extension))
					{
						ContainsCode = true;
					}
					if (s_contentExtensions.Contains(extension))
					{
						ContainsContent = true;
					}
					if (ContainsCode && ContainsContent)
					{
						break;
					}
				}
			}
		}

		/// <summary>
		/// Determines whether this change modifies the given file
		/// </summary>
		/// <param name="fileToCheck">The file to look for</param>
		/// <returns>True if the change modifies the given file</returns>
		public bool ModifiesFile(string fileToCheck)
		{
			foreach (ChangeFile file in Details.Files)
			{
				if (file.Path.EndsWith(fileToCheck, StringComparison.OrdinalIgnoreCase) && (file.Length == fileToCheck.Length || file.Path[file.Path.Length - fileToCheck.Length - 1] == '/'))
				{
					return true;
				}
			}
			return false;
		}
	}
}
