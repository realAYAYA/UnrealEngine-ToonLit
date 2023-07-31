// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Information about a target, passed along when creating a module descriptor
	/// </summary>
	public class TargetInfo
	{
		/// <summary>
		/// Name of the target
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// The platform that the target is being built for
		/// </summary>
		public readonly UnrealTargetPlatform Platform;

		/// <summary>
		/// The configuration being built
		/// </summary>
		public readonly UnrealTargetConfiguration Configuration;

		/// <summary>
		/// Architecture that the target is being built for (or an empty string for the default)
		/// </summary>
		public readonly string Architecture;

		/// <summary>
		/// The project containing the target
		/// </summary>
		public readonly FileReference? ProjectFile;

		/// <summary>
		/// The current build version
		/// </summary>
		public ReadOnlyBuildVersion Version
		{
			get { return ReadOnlyBuildVersion.Current; }
		}

		/// <summary>
		/// Additional command line arguments for this target
		/// </summary>
		public CommandLineArguments? Arguments;

		/// <summary>
		/// Constructs a TargetInfo for passing to the TargetRules constructor.
		/// </summary>
		/// <param name="Name">Name of the target being built</param>
		/// <param name="Platform">The platform that the target is being built for</param>
		/// <param name="Configuration">The configuration being built</param>
		/// <param name="Architecture">The architecture being built for</param>
		/// <param name="ProjectFile">Path to the project file containing the target</param>
		/// <param name="Arguments">Additional command line arguments for this target</param>
		public TargetInfo(string Name, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture, FileReference? ProjectFile, CommandLineArguments? Arguments)
		{
			this.Name = Name;
			this.Platform = Platform;
			this.Configuration = Configuration;
			this.Architecture = Architecture;
			this.ProjectFile = ProjectFile;
			this.Arguments = Arguments;
		}

		/// <summary>
		/// Construct a TargetInfo from an archive on disk
		/// </summary>
		/// <param name="Reader">Archive to read from</param>
		public TargetInfo(BinaryArchiveReader Reader)
		{
			this.Name = Reader.ReadString()!;
			this.Platform = UnrealTargetPlatform.Parse(Reader.ReadString()!);
			string ConfigurationStr = Reader.ReadString()!;
			this.Architecture = Reader.ReadString()!;
			this.ProjectFile = Reader.ReadFileReferenceOrNull();
			string[]? ArgumentStrs = Reader.ReadArray(() => Reader.ReadString()!);

			if (!UnrealTargetConfiguration.TryParse(ConfigurationStr, out Configuration))
			{
				throw new BuildException(string.Format("The configration name {0} is not a valid configration name. Valid names are ({1})", Name,
					string.Join(",", Enum.GetValues(typeof(UnrealTargetConfiguration)).Cast<UnrealTargetConfiguration>().Select(x => x.ToString()))));
			}

			Arguments = ArgumentStrs == null ? null : new CommandLineArguments(ArgumentStrs);
		}

		/// <summary>
		/// Write a TargetInfo to an archive on disk
		/// </summary>
		/// <param name="Writer">Archive to write to</param>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteString(Name);
			Writer.WriteString(Platform.ToString());
			Writer.WriteString(Configuration.ToString());
			Writer.WriteString(Architecture);
			Writer.WriteFileReference(ProjectFile);
			Writer.WriteArray(Arguments?.GetRawArray(), Item => Writer.WriteString(Item));
		}
	}
}
