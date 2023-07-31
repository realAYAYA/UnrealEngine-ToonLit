// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Represents a Mac/IOS framework
	/// </summary>
	class UEBuildFramework
	{
		/// <summary>
		/// The name of this framework
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// Path to a zip file containing the framework. May be null.
		/// </summary>
		public readonly FileReference? ZipFile;

		/// <summary>
		/// Path to the framework on disk.
		/// </summary>
		public readonly DirectoryReference? FrameworkDirectory;

		/// <summary>
		/// 
		/// </summary>
		public readonly string? CopyBundledAssets;

		/// <summary>
		/// Copy the framework to the target's Framework directory
		/// </summary>
		public readonly bool bCopyFramework = false;

		/// <summary>
		/// File created after the framework has been extracted. Used to add dependencies into the action graph.
		/// </summary>
		public FileItem? ExtractedTokenFile;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">The framework name</param>
		/// <param name="CopyBundledAssets"></param>
		public UEBuildFramework(string Name, string? CopyBundledAssets = null)
		{
			this.Name = Name;
			this.CopyBundledAssets = CopyBundledAssets;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">The framework name</param>
		/// <param name="ZipFile">Path to the zip file for this framework</param>
		/// <param name="OutputDirectory">Path for the extracted zip file</param>
		/// <param name="CopyBundledAssets"></param>
		/// <param name="bCopyFramework">Copy the framework to the target's Framework directory</param>
		public UEBuildFramework(string Name, FileReference? ZipFile, DirectoryReference? OutputDirectory, string? CopyBundledAssets, bool bCopyFramework)
		{
			this.Name = Name;
			this.ZipFile = ZipFile;
			this.FrameworkDirectory = OutputDirectory;
			this.CopyBundledAssets = CopyBundledAssets;
			this.bCopyFramework = bCopyFramework;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">The framework name</param>
		/// <param name="FrameworkDirectory">Path for the framework on disk</param>
		/// <param name="CopyBundledAssets"></param>
		/// <param name="bCopyFramework">Copy the framework to the target's Framework directory</param>
		public UEBuildFramework(String Name, DirectoryReference? FrameworkDirectory, string? CopyBundledAssets, bool bCopyFramework)
		{
			this.Name = Name;
			this.FrameworkDirectory = FrameworkDirectory;
			this.CopyBundledAssets = CopyBundledAssets;
			this.bCopyFramework = bCopyFramework;
		}
	}
}
