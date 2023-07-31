// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Generate a clang compile_commands file for a target
	/// </summary>
	[ToolMode("GenerateClangDatabase", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance | ToolModeOptions.StartPrefetchingEngine | ToolModeOptions.ShowExecutionTime)]
	class GenerateClangDatabase : ToolMode
	{
		static Regex ArgumentRegex = new Regex(@"^(\-include|\-I|\/I|\/imsvc|\-isystem|\/FI|\/Fo)\s*(.*)");

		/// <summary>
		/// Set of filters for files to include in the database. Relative to the root directory, or to the project file.
		/// </summary>
		[CommandLine("-Filter=")]
		List<string> FilterRules = new List<string>();

		/// <summary>
		/// Execute any actions which result in code generation (eg. ISPC compilation)
		/// </summary>
		[CommandLine("-ExecCodeGenActions")]
		public bool bExecCodeGenActions = false;

		/// <summary>
		/// This ActionGraphBuilder captures the build output from a UEBuildModuleCPP so it can be consumed later.
		/// </summary>
		private class CaptureActionGraphBuilder : IActionGraphBuilder
		{
			private readonly ILogger Logger;
			public List<IExternalAction> CapturedActions = new List<IExternalAction>();
			public List<Tuple<FileItem, IEnumerable<string>>> CapturedTextFiles = new List<Tuple<FileItem, IEnumerable<string>>>();

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="InLogger"></param>
			public CaptureActionGraphBuilder(ILogger InLogger)
			{
				Logger = InLogger;
			}

			/// <inheritdoc/>
			public void AddAction(IExternalAction Action)
			{
				CapturedActions.Add(Action);
			}

			/// <inheritdoc/>
			public void CreateIntermediateTextFile(FileItem FileItem, string Contents)
			{
				Utils.WriteFileIfChanged(FileItem, Contents, Logger);
			}

			/// <inheritdoc/>
			public void CreateIntermediateTextFile(FileItem FileItem, IEnumerable<string> ContentLines)
			{
				Utils.WriteFileIfChanged(FileItem, ContentLines, Logger);
				CapturedTextFiles.Add(new Tuple<FileItem, IEnumerable<string>>(FileItem, ContentLines));
			}

			/// <inheritdoc/>
			public void AddSourceDir(DirectoryItem SourceDir)
			{
			}

			/// <inheritdoc/>
			public void AddSourceFiles(DirectoryItem SourceDir, FileItem[] SourceFiles)
			{
			}

			/// <inheritdoc/>
			public void AddFileToWorkingSet(FileItem File)
			{
			}

			/// <inheritdoc/>
			public void AddCandidateForWorkingSet(FileItem File)
			{
			}

			/// <inheritdoc/>
			public void AddDiagnostic(string Message)
			{
			}

			/// <inheritdoc/>
			public void SetOutputItemsForModule(string ModuleName, FileItem[] OutputItems)
			{
			}
		}

		/// <summary>
		/// Execute the command
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code</returns>
		/// <param name="Logger"></param>
		public override int Execute(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);

			// Create the build configuration object, and read the settings
			BuildConfiguration BuildConfiguration = new BuildConfiguration();
			XmlConfig.ApplyTo(BuildConfiguration);
			Arguments.ApplyTo(BuildConfiguration);

			// Parse the filter argument
			FileFilter? FileFilter = null;
			if (FilterRules.Count > 0)
			{
				FileFilter = new FileFilter(FileFilterType.Exclude);
				foreach (string FilterRule in FilterRules)
				{
					FileFilter.AddRules(FilterRule.Split(';'));
				}
			}

			// Force C++ modules to always include their generated code directories
			UEBuildModuleCPP.bForceAddGeneratedCodeIncludePath = true;

			// Parse all the target descriptors
			List<TargetDescriptor> TargetDescriptors = TargetDescriptor.ParseCommandLine(Arguments, BuildConfiguration.bUsePrecompiled, BuildConfiguration.bSkipRulesCompile, BuildConfiguration.bForceRulesCompile, Logger);

			// Generate the compile DB for each target
			using (ISourceFileWorkingSet WorkingSet = new EmptySourceFileWorkingSet())
			{
				// Find the compile commands for each file in the target
				Dictionary<FileReference, string> FileToCommand = new Dictionary<FileReference, string>();
				foreach (TargetDescriptor TargetDescriptor in TargetDescriptors)
				{
					// Disable PCHs and unity builds for the target
					TargetDescriptor.AdditionalArguments = TargetDescriptor.AdditionalArguments.Append(new string[] { "-NoPCH", "-DisableUnity" });

					// Create a makefile for the target
					UEBuildTarget Target = UEBuildTarget.Create(TargetDescriptor, BuildConfiguration.bSkipRulesCompile, BuildConfiguration.bForceRulesCompile, BuildConfiguration.bUsePrecompiled, Logger);
					UEToolChain TargetToolChain = Target.CreateToolchain(Target.Platform);

					// Execute code generation actions
					if (bExecCodeGenActions)
					{
						// Create the makefile
						TargetMakefile Makefile = Target.Build(BuildConfiguration, WorkingSet, TargetDescriptor, Logger);
						List<LinkedAction> Actions = Makefile.Actions.ConvertAll(x => new LinkedAction(x, TargetDescriptor));
						ActionGraph.Link(Actions, Logger);

						// Filter all the actions to execute
						HashSet<FileItem> PrerequisiteItems = new HashSet<FileItem>(Makefile.Actions.SelectMany(x => x.ProducedItems).Where(x => x.HasExtension(".h") || x.HasExtension(".cpp")));
						List<LinkedAction> PrerequisiteActions = ActionGraph.GatherPrerequisiteActions(Actions, PrerequisiteItems);

						// Execute these actions
						if (PrerequisiteActions.Count > 0)
						{
							Logger.LogInformation("Executing actions that produce source files...");
							ActionGraph.ExecuteActions(BuildConfiguration, PrerequisiteActions, new List<TargetDescriptor> { TargetDescriptor }, Logger);
						}
					}

					// Create all the binaries and modules
					CppCompileEnvironment GlobalCompileEnvironment = Target.CreateCompileEnvironmentForProjectFiles(Logger);
					foreach (UEBuildBinary Binary in Target.Binaries)
					{
						CppCompileEnvironment BinaryCompileEnvironment = Binary.CreateBinaryCompileEnvironment(GlobalCompileEnvironment);
						foreach (UEBuildModuleCPP Module in Binary.Modules.OfType<UEBuildModuleCPP>())
						{
							if (!Module.Rules.bUsePrecompiled)
							{
								// Gather all the files we care about
								UEBuildModuleCPP.InputFileCollection InputFileCollection = Module.FindInputFiles(Target.Platform, new Dictionary<DirectoryItem, FileItem[]>());
								List<FileItem> InputFiles = new List<FileItem>();
								InputFiles.AddRange(InputFileCollection.CPPFiles);
								InputFiles.AddRange(InputFileCollection.CCFiles);

								var fileList = new List<FileReference>();
								foreach (FileItem InputFile in InputFiles)
								{
									if (FileFilter == null || FileFilter.Matches(InputFile.Location.MakeRelativeTo(Unreal.RootDirectory)))
									{
										var fileRef = new FileReference(InputFile.AbsolutePath);
										fileList.Add(fileRef);
									}
								}

								if (fileList.Any())
								{
									CaptureActionGraphBuilder ActionGraphBuilder = new CaptureActionGraphBuilder(Logger);

									Module.Compile(Target.Rules, TargetToolChain, BinaryCompileEnvironment, fileList, WorkingSet, ActionGraphBuilder, Logger);

									IEnumerable<IExternalAction> CompileActions = ActionGraphBuilder.CapturedActions.Where(Action => Action.ActionType == ActionType.Compile);

									if (CompileActions.Any())
									{
										// convert any rsp files
										Dictionary<string, string> UpdatedResFiles = new Dictionary<string, string>();
										foreach (Tuple<FileItem, IEnumerable<string>> FileAndContents in ActionGraphBuilder.CapturedTextFiles)
										{
											if (FileAndContents.Item1.AbsolutePath.EndsWith(".rsp") || FileAndContents.Item1.AbsolutePath.EndsWith(".response"))
											{
												string NewResPath = ConvertResponseFile(FileAndContents.Item1, FileAndContents.Item2, Logger);
												UpdatedResFiles[FileAndContents.Item1.AbsolutePath] = NewResPath;
											}
										}

										foreach (IExternalAction Action in CompileActions)
										{
											// Create the command
											StringBuilder CommandBuilder = new StringBuilder();
											string CommandArguments = Action.CommandArguments.Replace(".rsp", ".rsp.gcd").Replace(".response", ".response.gcd");
											CommandBuilder.AppendFormat("\"{0}\" {1}", Action.CommandPath, CommandArguments);

											foreach (string ExtraArgument in GetExtraPlatformArguments(TargetToolChain))
											{
												CommandBuilder.AppendFormat(" {0}", ExtraArgument);
											}

											// find source file
											var SourceFile = Action.PrerequisiteItems.FirstOrDefault(fi => fi.HasExtension(".cpp") || fi.HasExtension(".c") || fi.HasExtension(".c"));
											if (SourceFile != null)
											{
												FileToCommand[SourceFile.Location] = CommandBuilder.ToString();
											}
										}
									}
								}
							}
						}
					}
				}

				// Write the compile database
				DirectoryReference DatabaseDirectory = Arguments.GetDirectoryReferenceOrDefault("-OutputDir=", Unreal.RootDirectory);
				FileReference DatabaseFile = FileReference.Combine(DatabaseDirectory, "compile_commands.json");
				using (JsonWriter Writer = new JsonWriter(DatabaseFile))
				{
					Writer.WriteArrayStart();
					foreach (KeyValuePair<FileReference, string> FileCommandPair in FileToCommand.OrderBy(x => x.Key.FullName))
					{
						Writer.WriteObjectStart();
						Writer.WriteValue("file", FileCommandPair.Key.FullName);
						Writer.WriteValue("command", FileCommandPair.Value);
						Writer.WriteValue("directory", Unreal.EngineSourceDirectory.ToString());
						Writer.WriteObjectEnd();
					}
					Writer.WriteArrayEnd();
				}
			}

			return 0;
		}

		private IEnumerable<string> GetExtraPlatformArguments(UEToolChain TargetToolChain)
		{
			IList<string> ExtraPlatformArguments = new List<string>();

			ClangToolChain? ClangToolChain = TargetToolChain as ClangToolChain;
			if (ClangToolChain != null)
			{
				ClangToolChain.AddExtraToolArguments(ExtraPlatformArguments);
			}

			return ExtraPlatformArguments;
		}

		static private string ConvertResponseFile(FileItem OriginalFileItem, IEnumerable<string> FileContents, ILogger Logger)
		{
			List<string> NewFileContents = new List<string>(FileContents);
			FileItem NewFileItem = FileItem.GetItemByFileReference(new FileReference(OriginalFileItem.AbsolutePath + ".gcd"));

			for (int i = 0; i < NewFileContents.Count; i++)
			{
				string Line = NewFileContents[i].TrimStart();

				// Ignore empty strings
				if (String.IsNullOrEmpty(Line)) 
				{
					continue;
				}
				// The file that is going to compile
				else if (!Line.StartsWith("-") && !Line.StartsWith("/"))
				{
					Line = ConvertPath(Line, Line);
				}
				// Arguments
				else
				{
					int StrIndex = Line.IndexOf("..\\");
					if (StrIndex != -1)
					{
						Line = ConvertPath(Line, Line.Substring(StrIndex));
					}
					else
					{
						Match LineMatch = ArgumentRegex.Match(Line);
						if (LineMatch.Success)
						{
							Line = ConvertPath(Line, LineMatch.Groups[2].Value);
						}
					}
				}

				NewFileContents[i] = Line;
			}

			Utils.WriteFileIfChanged(NewFileItem, NewFileContents, Logger);

			return NewFileItem.AbsolutePath;
		}

		static private string ConvertPath(string Line, string OldPath)
		{
			OldPath = OldPath.Replace("\"", "");
			if (OldPath[^1] == '\\' || OldPath[^1] == '/')
			{
				OldPath = OldPath.Remove(OldPath.Length - 1, 1);
			}

			var FileReference = new FileReference(OldPath);
			return Line.Replace(OldPath, FileReference.FullName.Replace("\\", "/"));
		}
	}
}
