// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool.Modes
{
	/// <summary>
	/// Outputs information about the given target, including a module dependecy graph (in .gefx format and list of module references)
	/// </summary>
	[ToolMode("Analyze", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance | ToolModeOptions.StartPrefetchingEngine | ToolModeOptions.ShowExecutionTime)]
	class AnalyzeMode : ToolMode
	{
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

			// Parse all the target descriptors
			List<TargetDescriptor> TargetDescriptors = TargetDescriptor.ParseCommandLine(Arguments, BuildConfiguration.bUsePrecompiled, BuildConfiguration.bSkipRulesCompile, BuildConfiguration.bForceRulesCompile, Logger);

			// Generate the compile DB for each target
			using (ISourceFileWorkingSet WorkingSet = new EmptySourceFileWorkingSet())
			{
				// Find the compile commands for each file in the target
				Dictionary<FileReference, string> FileToCommand = new Dictionary<FileReference, string>();
				foreach (TargetDescriptor TargetDescriptor in TargetDescriptors)
				{
					AnalyzeTarget(TargetDescriptor, BuildConfiguration, Logger);
				}
			}

			return 0;
		}

		class ModuleInfo
		{
			public UEBuildModule Module;
			public string Chain;
			public HashSet<UEBuildModule> InwardRefs = new HashSet<UEBuildModule>();
			public HashSet<UEBuildModule> UniqueInwardRefs = new HashSet<UEBuildModule>();
			public HashSet<UEBuildModule> OutwardRefs = new HashSet<UEBuildModule>();
			public HashSet<UEBuildModule> UniqueOutwardRefs = new HashSet<UEBuildModule>();

			public List<FileReference> ObjectFiles = new List<FileReference>();
			public long ObjSize = 0;
			public List<FileReference> BinaryFiles = new List<FileReference>();
			public long BinSize = 0;

			public ModuleInfo(UEBuildModule Module, string Chain)
			{
				this.Module = Module;
				this.Chain = Chain;
			}
		}

		private void AnalyzeTarget(TargetDescriptor TargetDescriptor, BuildConfiguration BuildConfiguration, ILogger Logger)
		{
			// Create a makefile for the target
			UEBuildTarget Target = UEBuildTarget.Create(TargetDescriptor, BuildConfiguration.bSkipRulesCompile, BuildConfiguration.bForceRulesCompile, BuildConfiguration.bUsePrecompiled, Logger);
			DirectoryReference.CreateDirectory(Target.ReceiptFileName.Directory);

			// Find the shortest path from the target to each module
			Dictionary<UEBuildModule, ModuleInfo> ModuleToInfo = new Dictionary<UEBuildModule, ModuleInfo>();

			List<string> RootModuleNames = new List<string>(Target.Rules.ExtraModuleNames);
			if (Target.Rules.LaunchModuleName != null)
			{
				RootModuleNames.Add(Target.Rules.LaunchModuleName);
			}

			foreach (string RootModuleName in RootModuleNames)
			{
				UEBuildModule Module = Target.GetModuleByName(RootModuleName);
				if (Module != null)
				{
					string Chain = $"target -> {RootModuleName}";
					ModuleToInfo[Module] = new ModuleInfo(Module, Chain);
				}
			}

			// Also enable all the plugin modules
			foreach (UEBuildPlugin Plugin in Target.BuildPlugins!)
			{
				foreach (UEBuildModule Module in Plugin.Modules)
				{
					string Chain = $"{Plugin.ReferenceChain} -> {Module.Name}";
					ModuleToInfo[Module] = new ModuleInfo(Module, Chain);
				}
			}

			// Set of visited modules
			HashSet<UEBuildModule> VisitedModules = new HashSet<UEBuildModule>();

			// Recurse out to find new modules and the shortest path to each
			List<UEBuildModule> SourceModules = new List<UEBuildModule>(ModuleToInfo.Keys);
			while (SourceModules.Count > 0)
			{
				List<UEBuildModule> TargetModules = new List<UEBuildModule>();

				foreach (UEBuildModule SourceModule in SourceModules)
				{
					int Idx = TargetModules.Count;
					SourceModule.GetAllDependencyModules(TargetModules, VisitedModules, true, false, true);

					for (; Idx < TargetModules.Count; Idx++)
					{
						UEBuildModule TargetModule = TargetModules[Idx];
						string Chain = $"{ModuleToInfo[SourceModule].Chain} -> {TargetModule.Name}";
						ModuleToInfo[TargetModule] = new ModuleInfo(TargetModule, Chain);
					}
				}

				SourceModules = TargetModules;
			}

			// Find all the outward dependencies of each module
			foreach ((UEBuildModule SourceModule, ModuleInfo SourceModuleInfo) in ModuleToInfo)
			{
				SourceModuleInfo.OutwardRefs.Add(SourceModule);
				SourceModule.GetAllDependencyModules(new List<UEBuildModule>(), SourceModuleInfo.OutwardRefs, false, false, false);
				SourceModuleInfo.OutwardRefs.Remove(SourceModule);
			}

			// Find the direct output dependencies of each module
			foreach ((UEBuildModule SourceModule, ModuleInfo SourceModuleInfo) in ModuleToInfo)
			{
				SourceModuleInfo.UniqueOutwardRefs = new HashSet<UEBuildModule>(SourceModuleInfo.OutwardRefs);

				foreach (UEBuildModule TargetModule in SourceModuleInfo.OutwardRefs)
				{
					HashSet<UEBuildModule> VisitedTargetModules = new HashSet<UEBuildModule>();
					VisitedTargetModules.Add(SourceModule);

					List<UEBuildModule> DependencyModules = new List<UEBuildModule>();
					TargetModule.GetAllDependencyModules(DependencyModules, VisitedTargetModules, false, false, false);
					DependencyModules.Remove(TargetModule);

					SourceModuleInfo.UniqueOutwardRefs.ExceptWith(DependencyModules);
				}
			}

			// Find the direct inward dependencies of each module
			foreach ((UEBuildModule SourceModule, ModuleInfo SourceModuleInfo) in ModuleToInfo)
			{
				foreach (UEBuildModule TargetModule in SourceModuleInfo.OutwardRefs)
				{
					ModuleToInfo[TargetModule].InwardRefs.Add(SourceModule);
				}
				foreach (UEBuildModule TargetModule in SourceModuleInfo.UniqueOutwardRefs)
				{
					ModuleToInfo[TargetModule].UniqueInwardRefs.Add(SourceModule);
				}
			}

			// Estimate the size of object files for each module
			foreach ((UEBuildModule SourceModule, ModuleInfo SourceModuleInfo) in ModuleToInfo)
			{
				if (DirectoryReference.Exists(SourceModule.IntermediateDirectory))
				{
					foreach (FileReference IntermediateFile in DirectoryReference.EnumerateFiles(SourceModule.IntermediateDirectory, "*", SearchOption.AllDirectories))
					{
						if (IntermediateFile.HasExtension(".obj") || IntermediateFile.HasExtension(".o"))
						{
							SourceModuleInfo.ObjectFiles.Add(IntermediateFile);
							SourceModuleInfo.ObjSize += IntermediateFile.ToFileInfo().Length;
						}
					}
				}
			}

			HashSet<UEBuildModule> MissingModules = new HashSet<UEBuildModule>();
			foreach (UEBuildBinary Binary in Target.Binaries)
			{
				long BinSize = 0;
				foreach (FileReference OutputFilePath in Binary.OutputFilePaths)
				{
					FileInfo OutputFileInfo = OutputFilePath.ToFileInfo();
					if (OutputFileInfo.Exists)
					{
						BinSize += OutputFileInfo.Length;
					}
				}
				foreach (UEBuildModule Module in Binary.Modules)
				{
					ModuleInfo? ModuleInfo;
					if (!ModuleToInfo.TryGetValue(Module, out ModuleInfo))
					{
						MissingModules.Add(Module);
						continue;
					}

					ModuleInfo.BinaryFiles.AddRange(Binary.OutputFilePaths);
					ModuleInfo.BinSize += BinSize;
				}
			}

			// Warn about any missing modules
			foreach (UEBuildModule MissingModule in MissingModules.OrderBy(x => x.Name))
			{
				Logger.LogWarning("Missing module '{MissingModuleName}'", MissingModule.Name);
			}

			// Generate the dependency graph between modules
			FileReference DependencyGraphFile = Target.ReceiptFileName.ChangeExtension(".Dependencies.gexf");
			Logger.LogInformation("Writing dependency graph to {DependencyGraphFile}...", DependencyGraphFile);
			WriteDependencyGraph(Target, ModuleToInfo, DependencyGraphFile);

			// Generate the dependency graph between modules
			FileReference ShortestPathGraphFile = Target.ReceiptFileName.ChangeExtension(".ShortestPath.gexf");
			Logger.LogInformation("Writing shortest-path graph to {ShortestPathGraphFile}...", ShortestPathGraphFile);
			WriteShortestPathGraph(Target, ModuleToInfo, ShortestPathGraphFile);

			// Write all the target stats as a text file
			FileReference TextFile = Target.ReceiptFileName.ChangeExtension(".txt");
			Logger.LogInformation("Writing module information to {TextFile}", TextFile);
			using (StreamWriter Writer = new StreamWriter(TextFile.FullName))
			{
				Writer.WriteLine("All modules in {0}, ordered by number of indirect references", Target.TargetName);

				foreach (ModuleInfo ModuleInfo in ModuleToInfo.Values.OrderByDescending(x => x.InwardRefs.Count).ThenBy(x => x.BinSize))
				{
					Writer.WriteLine("");
					Writer.WriteLine("Module:        \"{0}\"", ModuleInfo.Module.Name);
					Writer.WriteLine("Shortest path: {0}", ModuleInfo.Chain);
					WriteDependencyList(Writer, "Unique inward refs:     ", ModuleInfo.UniqueInwardRefs);
					WriteDependencyList(Writer, "Unique outward refs:    ", ModuleInfo.UniqueOutwardRefs);
					WriteDependencyList(Writer, "Recursive inward refs:  ", ModuleInfo.InwardRefs);
					WriteDependencyList(Writer, "Recursive outward refs: ", ModuleInfo.OutwardRefs);
					Writer.WriteLine("Object size:             {0:n0}kb", (ModuleInfo.ObjSize + 1023) / 1024);
					Writer.WriteLine("Object files:            {0}", String.Join(", ", ModuleInfo.ObjectFiles.Select(x => x.GetFileName())));
					Writer.WriteLine("Binary size:             {0:n0}kb", (ModuleInfo.BinSize + 1023) / 1024);
					Writer.WriteLine("Binary files:            {0}", String.Join(", ", ModuleInfo.BinaryFiles.Select(x => x.GetFileName())));
				}
			}

			// Write all the target stats as a CSV file
			FileReference CsvFile = Target.ReceiptFileName.ChangeExtension(".csv");
			Logger.LogInformation("Writing module information to {CsvFile}", CsvFile);
			using (StreamWriter Writer = new StreamWriter(CsvFile.FullName))
			{
				List<string> Columns = new List<string>();
				Columns.Add("Module");
				Columns.Add("ShortestPath");
				Columns.Add("NumUniqueInwardRefs");
				Columns.Add("UniqueInwardRefs");
				Columns.Add("NumRecursiveInwardRefs");
				Columns.Add("RecursiveInwardRefs");
				Columns.Add("NumUniqueOutwardRefs");
				Columns.Add("UniqueOutwardRefs");
				Columns.Add("NumRecursiveOutwardRefs");
				Columns.Add("RecursiveOutwardRefs");
				Columns.Add("ObjSize");
				Columns.Add("ObjFiles");
				Columns.Add("BinSize");
				Columns.Add("BinFiles");
				Writer.WriteLine(String.Join(",", Columns));

				foreach (ModuleInfo ModuleInfo in ModuleToInfo.Values.OrderByDescending(x => x.InwardRefs.Count).ThenBy(x => x.BinSize))
				{
					Columns.Clear();
					Columns.Add(ModuleInfo.Module.Name);
					Columns.Add(ModuleInfo.Chain);
					Columns.Add($"{ModuleInfo.UniqueInwardRefs.Count}");
					Columns.Add($"\"{String.Join(", ", ModuleInfo.UniqueInwardRefs.Select(x => x.Name))}\"");
					Columns.Add($"{ModuleInfo.InwardRefs.Count}");
					Columns.Add($"\"{String.Join(", ", ModuleInfo.InwardRefs.Select(x => x.Name))}\"");
					Columns.Add($"{ModuleInfo.UniqueOutwardRefs.Count}");
					Columns.Add($"\"{String.Join(", ", ModuleInfo.UniqueOutwardRefs.Select(x => x.Name))}\"");
					Columns.Add($"{ModuleInfo.OutwardRefs.Count}");
					Columns.Add($"\"{String.Join(", ", ModuleInfo.OutwardRefs.Select(x => x.Name))}\"");
					Columns.Add($"{ModuleInfo.ObjSize}");
					Columns.Add($"\"{String.Join(", ", ModuleInfo.ObjectFiles.Select(x => x.GetFileName()))}\"");
					Columns.Add($"{ModuleInfo.BinSize}");
					Columns.Add($"\"{String.Join(", ", ModuleInfo.BinaryFiles.Select(x => x.GetFileName()))}\"");
					Writer.WriteLine(String.Join(",", Columns));
				}
			}
		}

		private void WriteDependencyList(TextWriter Writer, string Prefix, HashSet<UEBuildModule> Modules)
		{
			if (Modules.Count == 0)
			{
				Writer.WriteLine("{0} 0", Prefix);
			}
			else
			{
				Writer.WriteLine("{0} {1} ({2})", Prefix, Modules.Count, String.Join(", ", Modules.Select(x => x.Name).OrderBy(x => x)));
			}
		}

		private void WriteDependencyGraph(UEBuildTarget Target, Dictionary<UEBuildModule, ModuleInfo> ModuleToInfo, FileReference FileName)
		{
			List<GraphNode> Nodes = new List<GraphNode>();

			Dictionary<UEBuildModule, GraphNode> ModuleToNode = new Dictionary<UEBuildModule, GraphNode>();
			foreach (ModuleInfo ModuleInfo in ModuleToInfo.Values)
			{
				GraphNode Node = new GraphNode(ModuleInfo.Module.Name);

				long Size;
				if (Target.ShouldCompileMonolithic())
				{
					Size = ModuleInfo.ObjSize;
				}
				else
				{
					Size = ModuleInfo.BinSize;
				}

				Node.Size = 1.0f + (Size / (50.0f * 1024.0f * 1024.0f));
				Nodes.Add(Node);
				ModuleToNode[ModuleInfo.Module] = Node;
			}

			List<GraphEdge> Edges = new List<GraphEdge>();
			foreach ((UEBuildModule SourceModule, ModuleInfo SourceModuleInfo) in ModuleToInfo)
			{
				GraphNode SourceNode = ModuleToNode[SourceModule];
				foreach (UEBuildModule TargetModule in SourceModuleInfo.UniqueOutwardRefs)
				{
					ModuleInfo TargetModuleInfo = ModuleToInfo[TargetModule];

					GraphNode? TargetNode;
					if (ModuleToNode.TryGetValue(TargetModule, out TargetNode))
					{
						GraphEdge Edge = new GraphEdge(SourceNode, TargetNode);
						Edge.Thickness = TargetModuleInfo.InwardRefs.Count;
						Edges.Add(Edge);
					}
				}
			}

			GraphVisualization.WriteGraphFile(FileName, $"Module dependency graph for {Target.TargetName}", Nodes, Edges);
		}

		private void WriteShortestPathGraph(UEBuildTarget Target, Dictionary<UEBuildModule, ModuleInfo> ModuleToInfo, FileReference FileName)
		{
			Dictionary<string, GraphNode> NameToNode = new Dictionary<string, GraphNode>(StringComparer.Ordinal);

			HashSet<(GraphNode, GraphNode)> EdgesSet = new HashSet<(GraphNode, GraphNode)>();
			List<GraphEdge> Edges = new List<GraphEdge>();

			foreach ((UEBuildModule Module, ModuleInfo ModuleInfo) in ModuleToInfo)
			{
				string[] Parts = ModuleInfo.Chain.Split(" -> ");

				GraphNode? PrevNode = null;
				foreach (string Part in Parts)
				{
					GraphNode? NextNode;
					if (!NameToNode.TryGetValue(Part, out NextNode))
					{
						NextNode = new GraphNode(Part);
						NameToNode[Part] = NextNode;
					}
					if (PrevNode != null && EdgesSet.Add((PrevNode, NextNode)))
					{
						GraphEdge Edge = new GraphEdge(PrevNode, NextNode);
						Edges.Add(Edge);
					}
					PrevNode = NextNode;
				}
			}

			GraphVisualization.WriteGraphFile(FileName, $"Module dependency graph for {Target.TargetName}", NameToNode.Values.ToList(), Edges);
		}

		private static HashSet<UEBuildModule> GetDirectDependencyModules(UEBuildModule Module)
		{
			HashSet<UEBuildModule> ReferencedModules = new HashSet<UEBuildModule>();
			Module.GetAllDependencyModules(new List<UEBuildModule>(), ReferencedModules, true, false, false);

			HashSet<UEBuildModule> Modules = new HashSet<UEBuildModule>(Module.GetDirectDependencyModules());
			Modules.ExceptWith(ReferencedModules);
			return Modules;
		}
	}
}

