// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	class VSWorkspaceProjectFileGenerator : ProjectFileGenerator
	{
		public override string ProjectFileExtension => ".json";

		// These properties are used by Visual Studio to determine where to read the project files.
		// So they must remain constant.
		private const string VSUnrealWorkspaceFileName = ".vs-unreal-workspace";
		private const string ProjectFilesFolder = "VisualStudio";

		private readonly CommandLineArguments Arguments;

		/// <summary>
		/// List of deprecated platforms.
		/// Don't generate project model for these platforms unless they are specified in "Platforms" console arguments. 
		/// </summary>
		/// <returns></returns>
		private readonly HashSet<UnrealTargetPlatform> DeprecatedPlatforms = new();

		/// <summary>
		/// Platforms to generate project files for
		/// </summary>
		[CommandLine("-Platforms=", ListSeparator = '+')]
		HashSet<UnrealTargetPlatform> Platforms = new();

		/// <summary>
		/// Target types to generate project files for
		/// </summary>
		[CommandLine("-TargetTypes=", ListSeparator = '+')]
		HashSet<TargetType> TargetTypes = new();

		/// <summary>
		/// Target configurations to generate project files for
		/// </summary>
		[CommandLine("-TargetConfigurations=", ListSeparator = '+')]
		HashSet<UnrealTargetConfiguration> TargetConfigurations = new();

		/// <summary>
		/// Projects to generate project files for
		/// </summary>
		[CommandLine("-ProjectNames=", ListSeparator = '+')]
		HashSet<string> ProjectNames = new();

		/// <summary>
		/// Should format JSON files in human readable form, or use packed one without indents
		/// </summary>
		[CommandLine("-Minimize", Value = "Compact")]
		private JsonWriterStyle Minimize = JsonWriterStyle.Readable;

		public VSWorkspaceProjectFileGenerator(FileReference? InOnlyGameProject,
			CommandLineArguments InArguments)
			: base(InOnlyGameProject)
		{
			Arguments = InArguments;
			Arguments.ApplyTo(this);
		}

		public override bool ShouldGenerateIntelliSenseData() => true;

		public override void CleanProjectFiles(DirectoryReference InPrimaryProjectDirectory, string InPrimaryProjectName,
			DirectoryReference InIntermediateProjectFilesDirectory, ILogger Logger)
		{
			DirectoryReference.Delete(InPrimaryProjectDirectory);
		}

		protected override void ConfigureProjectFileGeneration(string[] Arguments, ref bool IncludeAllPlatforms, ILogger Logger)
		{
			base.ConfigureProjectFileGeneration(Arguments, ref IncludeAllPlatforms, Logger);
		}

		protected override ProjectFile AllocateProjectFile(FileReference InitFilePath, DirectoryReference BaseDir)
		{
			VSWorkspaceProjectFile projectFile = new(InitFilePath, BaseDir, RootPath: InitFilePath.Directory,
				Arguments: Arguments, TargetTypes: TargetTypes);
			return projectFile;
		}

		protected override bool WriteProjectFiles(PlatformProjectGeneratorCollection PlatformProjectGenerators, ILogger Logger)
		{
			using ProgressWriter Progress = new("Writing project files...", true, Logger);
			List<ProjectFile> ProjectsToGenerate = new(GeneratedProjectFiles);
			if (ProjectNames.Any())
			{
				ProjectsToGenerate = ProjectsToGenerate.Where(Project =>
					ProjectNames.Contains(Project.ProjectFilePath.GetFileNameWithoutAnyExtensions())).ToList();
			}

			int TotalProjectFileCount = ProjectsToGenerate.Count;

			HashSet<UnrealTargetPlatform> PlatformsToGenerate = new(SupportedPlatforms);
			if (Platforms.Any())
			{
				PlatformsToGenerate.IntersectWith(Platforms);
			}

			List<UnrealTargetPlatform> FilteredPlatforms = PlatformsToGenerate.Where(Platform =>
			{
				// Skip deprecated unless explicitly specified in the command line.
				return (!DeprecatedPlatforms.Contains(Platform) || Platforms.Contains(Platform))
					   && UEBuildPlatform.IsPlatformAvailable(Platform);
			}).ToList();

			HashSet<UnrealTargetConfiguration> ConfigurationsToGenerate = new(SupportedConfigurations);
			if (TargetConfigurations.Any())
			{
				ConfigurationsToGenerate.IntersectWith(TargetConfigurations);
			}

			List<UnrealTargetConfiguration> Configurations = ConfigurationsToGenerate.ToList();

			for (int ProjectFileIndex = 0; ProjectFileIndex < ProjectsToGenerate.Count; ++ProjectFileIndex)
			{
				if (ProjectsToGenerate[ProjectFileIndex] is not VSWorkspaceProjectFile CurrentProject)
				{
					return false;
				}

				if (!CurrentProject.WriteProjectFile(FilteredPlatforms, Configurations, PlatformProjectGenerators, Minimize, Logger))
				{
					return false;
				}

				Progress.Write(ProjectFileIndex + 1, TotalProjectFileCount);
			}

			Progress.Write(TotalProjectFileCount, TotalProjectFileCount);

			return true;
		}

		public override bool GenerateProjectFiles(PlatformProjectGeneratorCollection PlatformProjectGenerators,
			String[] Arguments, bool bCacheDataForEditor, ILogger Logger)
		{
			bool IncludeAllPlatforms = true;
			ConfigureProjectFileGeneration(Arguments, ref IncludeAllPlatforms, Logger);

			if (bGeneratingGameProjectFiles || Unreal.IsEngineInstalled())
			{
				PrimaryProjectPath = OnlyGameProject!.Directory;
				PrimaryProjectName = OnlyGameProject.GetFileNameWithoutExtension();

				IntermediateProjectFilesPath =
					DirectoryReference.Combine(PrimaryProjectPath, "Intermediate", "ProjectFiles");
			}

			SetupSupportedPlatformsAndConfigurations(IncludeAllPlatforms: true, Logger, out string SupportedPlatformNames);
			Logger.LogDebug("Supported platforms: {Platforms}", SupportedPlatformNames);

			List<FileReference> AllGames = FindGameProjects(Logger);

			{
				// Find all of the target files.
				List<FileReference> AllTargetFiles = DiscoverTargets(
					AllGames,
					Logger,
					OnlyGameProject,
					SupportedPlatforms,
					bIncludeEngineSource: bIncludeEngineSource,
					bIncludeTempTargets: bIncludeTempTargets);

				// If there are multiple targets of a given type for a project, use the order to determine which one gets a suffix.
				AllTargetFiles = AllTargetFiles.OrderBy(x => x.FullName, StringComparer.OrdinalIgnoreCase).ToList();

				List<ProjectFile> EngineProjects = new();
				List<ProjectFile> GameProjects = new();
				List<ProjectFile> ModProjects = new();
				Dictionary<FileReference, ProjectFile> ProgramProjects = new();
				Dictionary<RulesAssembly, DirectoryReference> RulesAssemblies = new();
				Dictionary<ProjectFile, FileReference> ProjectFileToUProjectFile = new();

				AddProjectsForAllTargets(
					PlatformProjectGenerators,
					AllGames,
					AllTargetFiles,
					Arguments,
					EngineProjects,
					GameProjects,
					ProjectFileToUProjectFile,
					ProgramProjects,
					RulesAssemblies,
					Logger);

				AddAllGameProjects(GameProjects);
			}

			WriteProjectFiles(PlatformProjectGenerators, Logger);
			WritePrimaryProjectFile(UBTProject, PlatformProjectGenerators, Logger);

			return true;
		}

		protected override bool WritePrimaryProjectFile(ProjectFile? UBTProject,
			PlatformProjectGeneratorCollection PlatformProjectGenerators,
			ILogger Logger)
		{
			try
			{
				FileReference PrimaryProjectFile = FileReference.Combine(
					IntermediateProjectFilesPath, ProjectFilesFolder, VSUnrealWorkspaceFileName);

				DirectoryReference.CreateDirectory(PrimaryProjectFile.Directory);

				// Collect all the resulting project files and aggregate the target-level data
				var AggregatedProjectInfo = GeneratedProjectFiles
					.Where(Project => Project is VSWorkspaceProjectFile)
					.OfType<VSWorkspaceProjectFile>()
					.SelectMany(Project => Project.ExportedTargetProjects)
					.GroupBy(TargetProject => TargetProject.TargetName)
					.Select(g => (g.Key, Target: new
					{
						TargetType = g.Select(i => i.TargetType).Distinct().Single(),
						TargetPath = g.Select(i => i.TargetPath).Distinct().Single(),
						ProjectPath = g.Select(i => i.ProjectPath).Distinct().Single(),
						Configurations = g.Select(i => i.Configuration).Distinct().ToList(),
						Platforms = g.Select(i => i.Platform).Distinct().ToList(),
					}));

				// The inner Targets object is needed for schema compatibility with the Query Mode API.
				var Result = new
				{
					Targets = AggregatedProjectInfo.ToDictionary(item => item.Key, item => item.Target)
				};

				using FileStream Stream = new(PrimaryProjectFile.FullName, FileMode.Create, FileAccess.Write);
				JsonSerializer.Serialize(Stream, Result, new JsonSerializerOptions
				{
					PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
					WriteIndented = Minimize == JsonWriterStyle.Readable,
				});
			}
			catch (Exception Ex)
			{
				Logger.LogWarning("Exception while writing root project file: {Ex}", Ex.ToString());
				return false;
			}

			return true;
		}

		/// <inheritdoc />
		protected override FileReference GetProjectLocation(string BaseName)
		{
			return FileReference.Combine(IntermediateProjectFilesPath, ProjectFilesFolder, BaseName + ProjectFileExtension);
		}
	}
}
