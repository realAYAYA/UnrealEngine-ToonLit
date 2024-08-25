// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.BuildGraph;
using EpicGames.BuildGraph.Expressions;
using EpicGames.Core;
using EpicGames.MCP.Automation;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using UnrealBuildBase;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Implementation of ScriptTaskParameter corresponding to a field in a parameter class
	/// </summary>
	class ScriptTaskParameterBinding : BgScriptTaskParameter
	{
		/// <summary>
		/// Field for this parameter
		/// </summary>
		public FieldInfo FieldInfo { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public ScriptTaskParameterBinding(string Name, FieldInfo FieldInfo, TaskParameterValidationType ValidationType, bool bOptional)
			: base(Name, FieldInfo.FieldType, ValidationType, bOptional)
		{
			this.FieldInfo = FieldInfo;
		}
	}

	/// <summary>
	/// Binding of a ScriptTask to a Script
	/// </summary>
	class ScriptTaskBinding : BgScriptTask
	{
		/// <summary>
		/// Type of the task to construct with this info
		/// </summary>
		public Type TaskClass;

		/// <summary>
		/// Type to construct with the parsed parameters
		/// </summary>
		public Type ParametersClass;

		/// <summary>
		/// Map from name to parameter
		/// </summary>
		public IReadOnlyDictionary<string, ScriptTaskParameterBinding> NameToParameter { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">Name of the task</param>
		/// <param name="TaskClass">Task class to create</param>
		/// <param name="ParametersClass">Class type of an object to be constructed and passed as an argument to the task class constructor</param>
		public ScriptTaskBinding(string Name, Type TaskClass, Type ParametersClass)
			: this(Name, TaskClass, ParametersClass, CreateParameters(ParametersClass))
		{
		}

		/// <summary>
		/// Private constructor
		/// </summary>
		private ScriptTaskBinding(string Name, Type TaskClass, Type ParametersClass, List<ScriptTaskParameterBinding> Parameters)
			: base(Name, Parameters.ConvertAll<BgScriptTaskParameter>(x => x))
		{
			this.TaskClass = TaskClass;
			this.ParametersClass = ParametersClass;
			this.NameToParameter = Parameters.ToDictionary(x => x.Name, x => x);
		}

		static List<ScriptTaskParameterBinding> CreateParameters(Type ParametersClass)
		{
			List<ScriptTaskParameterBinding> ScriptTaskParameters = new List<ScriptTaskParameterBinding>();
			foreach (FieldInfo Field in ParametersClass.GetFields())
			{
				if (Field.MemberType == MemberTypes.Field)
				{
					TaskParameterAttribute ParameterAttribute = Field.GetCustomAttribute<TaskParameterAttribute>();
					if (ParameterAttribute != null)
					{
						ScriptTaskParameters.Add(new ScriptTaskParameterBinding(Field.Name, Field, ParameterAttribute.ValidationType, ParameterAttribute.Optional));
					}
				}
			}
			return ScriptTaskParameters;
		}
	}

	/// <summary>
	/// Environment for graph evaluation
	/// </summary>
	public class BgEnvironment
	{
		/// <summary>
		/// The stream being built
		/// </summary>
		public BgString Stream { get; set; }

		/// <summary>
		/// Current changelist
		/// </summary>
		public BgInt Change { get; }

		/// <summary>
		/// Current code changelist
		/// </summary>
		public BgInt CodeChange { get; }

		/// <summary>
		/// Whether the graph is being run on a build machine
		/// </summary>
		public BgBool IsBuildMachine { get; }

		/// <summary>
		/// The current engine version
		/// </summary>
		public (BgInt Major, BgInt Minor, BgInt Patch) EngineVersion { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Branch">The current branch</param>
		/// <param name="Change">Changelist being built</param>
		/// <param name="CodeChange">Code changelist being built</param>
		internal BgEnvironment(string Branch, int? Change, int? CodeChange)
		{
			this.Stream = Branch ?? "Unknown";
			this.Change = Change ?? 0;
			this.CodeChange = CodeChange ?? 0;
			this.IsBuildMachine = CommandUtils.IsBuildMachine;

			ReadOnlyBuildVersion Version = ReadOnlyBuildVersion.Current;
			this.EngineVersion = (Version.MajorVersion, Version.MinorVersion, Version.PatchVersion);
		}
	}

	/// <summary>
	/// Tool to execute build automation scripts for UE projects, which can be run locally or in parallel across a build farm (assuming synchronization and resource allocation implemented by a separate system).
	///
	/// Build graphs are declared using an XML script using syntax similar to MSBuild, ANT or NAnt, and consist of the following components:
	///
	/// - Tasks:        Building blocks which can be executed as part of the build process. Many predefined tasks are provided ('Cook', 'Compile', 'Copy', 'Stage', 'Log', 'PakFile', etc...), and additional tasks may be 
	///                 added be declaring classes derived from AutomationTool.BuildTask in other UAT modules. 
	/// - Nodes:        A named sequence of tasks which are executed in order to produce outputs. Nodes may have dependencies on other nodes for their outputs before they can be executed. Declared with the 'Node' element.
	/// - Agents:		A machine which can execute a sequence of nodes, if running as part of a build system. Has no effect when building locally. Declared with the 'Agent' element.
	/// - Triggers:     Container for agents which should only be executed when explicitly triggered (using the -Trigger=... or -SkipTriggers command line argument). Declared with the 'Trigger' element.
	/// - Notifiers:    Specifies email recipients for failures in one or more nodes, whether they should receive notifications on warnings, and so on.
	/// 
	/// Scripts may set properties with the &lt;Property Name="Foo" Value="Bar"/&gt; syntax. Properties referenced with the $(Property Name) notation are valid within all strings, and will be expanded as macros when the 
	/// script is read. If a property name is not set explicitly, it defaults to the contents of an environment variable with the same name. Properties may be sourced from environment variables or the command line using
	/// the &lt;EnvVar&gt; and &lt;Option&gt; elements respectively.
	///
	/// Any elements can be conditionally defined via the "If" attribute. A full grammar for conditions is written up in Condition.cs.
	/// 
	/// File manipulation is done using wildcards and tags. Any attribute that accepts a list of files may consist of: a Perforce-style wildcard (matching any number of "...", "*" and "?" patterns in any location), a 
	/// full path name, or a reference to a tagged collection of files, denoted by prefixing with a '#' character. Files may be added to a tag set using the &lt;Tag&gt; Task, which also allows performing set union/difference 
	/// style operations. Each node can declare multiple outputs in the form of a list of named tags, which other nodes can then depend on.
	/// 
	/// Build graphs may be executed in parallel as part build system. To do so, the initial graph configuration is generated by running with the -Export=... argument (producing a JSON file listing the nodes 
	/// and dependencies to execute). Each participating agent should be synced to the same changelist, and UAT should be re-run with the appropriate -Node=... argument. Outputs from different nodes are transferred between 
	/// agents via shared storage, typically a network share, the path to which can be specified on the command line using the -SharedStorageDir=... argument. Note that the allocation of machines, and coordination between 
	/// them, is assumed to be managed by an external system based on the contents of the script generated by -Export=....
	/// 
	/// A schema for the known set of tasks can be generated by running UAT with the -Schema=... option. Generating a schema and referencing it from a BuildGraph script allows Visual Studio to validate and auto-complete 
	/// elements as you type.
	/// </summary>
	[Help("Tool for creating extensible build processes in UE which can be run locally or in parallel across a build farm.")]
	[ParamHelp("Script=<FileName>", "Path to the script describing the graph", ParamType = typeof(File), Required = true)]
	[ParamHelp("Target=<Name>", "Name of the node or output tag to be built")]
	[ParamHelp("Schema", "Generates a schema to the default location", ParamType = typeof(bool))]
	[ParamHelp("Schema=<FileName>", "Generate a schema describing valid script documents, including all the known tasks", ParamType = typeof(string))]
	[ParamHelp("ImportSchema=<FileName>", "Imports a schema from an existing schema file", ParamType = typeof(File))]
	[ParamHelp("Set:<Property>=<Value>", "Sets a named property to the given value", Action = ParamHelpAttribute.ParamAction.Append, Flag = "-Set:")]
	[ParamHelp("Branch=<Value>", "Overrides the auto-detection of the current branch")]
	[ParamHelp("Clean", "Cleans all cached state of completed build nodes before running", ParamType = typeof(bool), Deprecated = true)]
	[ParamHelp("CleanNode=<Name>[+<Name>...]", "Cleans just the given nodes before running", MultiSelectSeparator = "+")]
	[ParamHelp("Resume", "Resumes a local build from the last node that completed successfully", ParamType = typeof(bool))]
	[ParamHelp("ListOnly", "Shows the contents of the preprocessed graph, but does not execute it", ParamType = typeof(bool), DefaultValue = false)]
	[ParamHelp("ShowDiagnostics", "When running with -ListOnly, causes diagnostic messages entered when parsing the graph to be shown", ParamType = typeof(bool))]
	[ParamHelp("ShowDeps", "Show node dependencies in the graph output", ParamType = typeof(bool))]
	[ParamHelp("ShowNotifications", "Show notifications that will be sent for each node in the output", ParamType = typeof(bool))]
	[ParamHelp("Trigger=<Name>", "Executes only nodes behind the given trigger")]
	[ParamHelp("SkipTrigger=<Name>[+<Name>...]", "Skips the given triggers, including all the nodes behind them in the graph", MultiSelectSeparator = "+")]
	[ParamHelp("SkipTriggers", "Skips all triggers", ParamType = typeof(bool))]
	[ParamHelp("TokenSignature=<Name>", "Specifies the signature identifying the current job, to be written to tokens for nodes that require them. Tokens are ignored if this parameter is not specified.")]
	[ParamHelp("SkipTargetsWithoutTokens", "Excludes targets which we can't acquire tokens for, rather than failing", ParamType = typeof(bool))]
	[ParamHelp("Preprocess=<FileName>", "Writes the preprocessed graph to the given file", ParamType = typeof(File))]
	[ParamHelp("Export=<FileName>", "Exports a JSON file containing the preprocessed build graph, for use as part of a build system", ParamType = typeof(File))]
	[ParamHelp("HordeExport=<FileName>", "Exports a JSON file containing the full build graph for use by Horde.", ParamType = typeof(File))]
	[ParamHelp("PublicTasksOnly", "Only include built-in tasks in the schema, excluding any other UAT modules", ParamType = typeof(bool))]
	[ParamHelp("SharedStorageDir=<DirName>", "Sets the directory to use to transfer build products between agents in a build farm", ParamType = typeof(Directory))]
	[ParamHelp("SingleNode=<Name>", "Run only the given node. Intended for use on a build system after running with -Export.")]
	[ParamHelp("WriteToSharedStorage", "Allow writing to shared storage. If not set, but -SharedStorageDir is specified, build products will read but not written", ParamType = typeof(bool))]
	public class BuildGraph : BuildCommand
	{
		/// <summary>
		/// Main entry point for the BuildGraph command
		/// </summary>
		public override async Task<ExitCode> ExecuteAsync()
		{
			// Parse the command line parameters
			string ClassName = ParseParamValue("Class", null);
			string ScriptFileName = ParseParamValue("Script", null);
			string[] TargetNames = ParseParamValues("Target").SelectMany(x => x.Split(';', '+').Select(y => y.Trim()).Where(y => y.Length > 0)).ToArray();
			string DocumentationFileName = ParseParamValue("Documentation", null);
			string SchemaFileName = ParseParamValue("Schema", null);
			string ImportSchemaFileName = ParseParamValue("ImportSchema", null);
			string ExportFileName = ParseParamValue("Export", null);
			string HordeExportFileName = ParseParamValue("HordeExport", null);
			string PreprocessedFileName = ParseParamValue("Preprocess", null);
			string SharedStorageDir = ParseParamValue("SharedStorageDir", null);
			string SingleNodeName = ParseParamValue("SingleNode", null);
			string TriggerName = ParseParamValue("Trigger", null);
			string TokenSignature = ParseParamValue("TokenSignature", null);
			bool bSkipTargetsWithoutTokens = ParseParam("SkipTargetsWithoutTokens");
			bool bResume = SingleNodeName != null || ParseParam("Resume");
			bool bListOnly = ParseParam("ListOnly");
			bool bShowDiagnostics = ParseParam("ShowDiagnostics");
			bool bWriteToSharedStorage = ParseParam("WriteToSharedStorage") || CommandUtils.IsBuildMachine;
			bool bPublicTasksOnly = ParseParam("PublicTasksOnly");
			bool bSkipValidation = ParseParam("SkipValidation");
			string ReportName = ParseParamValue("ReportName", null);
			string BranchOverride = ParseParamValue("Branch", null);

			GraphPrintOptions PrintOptions = GraphPrintOptions.ShowCommandLineOptions;
			if (ParseParam("ShowDeps"))
			{
				PrintOptions |= GraphPrintOptions.ShowDependencies;
			}
			if (ParseParam("ShowNotifications"))
			{
				PrintOptions |= GraphPrintOptions.ShowNotifications;
			}

			if (SchemaFileName == null && ParseParam("Schema"))
			{
				SchemaFileName = FileReference.Combine(Unreal.EngineDirectory, "Build", "Graph", "Schema.xsd").FullName;
			}

			// Parse any specific nodes to clean
			List<string> CleanNodes = new List<string>();
			foreach (string NodeList in ParseParamValues("CleanNode"))
			{
				foreach (string NodeName in NodeList.Split('+', ';'))
				{
					CleanNodes.Add(NodeName);
				}
			}

			// Get the standard P4 properties, defaulting to the environment variables if not set. This allows setting p4-like properties without having a P4 connection.
			string Branch = P4Enabled ? P4Env.Branch : GetEnvVarOrNull(EnvVarNames.BuildRootP4);
			int? Change = P4Enabled ? P4Env.Changelist : GetEnvVarIntOrNull(EnvVarNames.Changelist);
			int? CodeChange = P4Enabled ? P4Env.CodeChangelist : GetEnvVarIntOrNull(EnvVarNames.CodeChangelist);

			// Set up the standard properties which build scripts might need
			Dictionary<string, string> DefaultProperties = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase);
			DefaultProperties["Branch"] = Branch ?? "Unknown";
			DefaultProperties["Depot"] = (Branch != null && Branch.StartsWith("//", StringComparison.Ordinal)) ? Branch.Substring(2).Split('/').First() : "Unknown";
			DefaultProperties["EscapedBranch"] = String.IsNullOrEmpty(Branch) ? "Unknown" : CommandUtils.EscapePath(Branch);
			DefaultProperties["Change"] = (Change ?? 0).ToString();
			DefaultProperties["CodeChange"] = (CodeChange ?? 0).ToString();
			DefaultProperties["IsBuildMachine"] = IsBuildMachine ? "true" : "false";
			DefaultProperties["HostPlatform"] = HostPlatform.Current.HostEditorPlatform.ToString();
			DefaultProperties["RestrictedFolderNames"] = String.Join(";", RestrictedFolder.GetNames());
			DefaultProperties["RestrictedFolderFilter"] = String.Join(";", RestrictedFolder.GetNames().Select(x => String.Format(".../{0}/...", x)));
			DefaultProperties["DataDrivenPlatforms"] = String.Join(";", DataDrivenPlatformInfo.GetAllPlatformInfos().Keys);

			// Look for overrides
			if (!string.IsNullOrEmpty(BranchOverride))
			{
				Logger.LogInformation("Overriding default branch '{Branch}' with '{BranchOverride}'", DefaultProperties["Branch"], BranchOverride);
				DefaultProperties["Branch"] = BranchOverride;
				DefaultProperties["EscapedBranch"] = CommandUtils.EscapePath(DefaultProperties["Branch"]);
			}

			// Prevent expansion of the root directory if we're just preprocessing the output. They may vary by machine.
			if (PreprocessedFileName == null)
			{
				DefaultProperties["RootDir"] = Unreal.RootDirectory.FullName;
			}
			else
			{
				DefaultProperties["RootDir"] = null;
			}

			// Attempt to read existing Build Version information
			BuildVersion Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				DefaultProperties["EngineMajorVersion"] = Version.MajorVersion.ToString();
				DefaultProperties["EngineMinorVersion"] = Version.MinorVersion.ToString();
				DefaultProperties["EnginePatchVersion"] = Version.PatchVersion.ToString();
				DefaultProperties["EngineCompatibleChange"] = Version.CompatibleChangelist.ToString();
			}

			// If the -project flag is given, pass useful information into the graph
			FileReference ProjectFile = ParseProjectParam();
			if (ProjectFile != null)
			{
				DefaultProperties["ProjectName"] = ProjectFile.GetFileNameWithoutExtension();
				DefaultProperties["ProjectFile"] = ProjectFile.FullName;
				DefaultProperties["ProjectDir"] = ProjectFile.Directory.FullName;
			}

			// Add any additional custom arguments from the command line (of the form -Set:X=Y)
			Dictionary<string, string> Arguments = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase);
			foreach (string Param in Params)
			{
				const string SetPrefix = "set:";
				if (Param.StartsWith(SetPrefix, StringComparison.InvariantCultureIgnoreCase))
				{
					int EqualsIdx = Param.IndexOf('=');
					if (EqualsIdx >= 0)
					{
						Arguments[Param.Substring(SetPrefix.Length, EqualsIdx - SetPrefix.Length)] = Param.Substring(EqualsIdx + 1);
					}
					else
					{
						Logger.LogWarning("Missing value for '{Arg0}'", Param.Substring(SetPrefix.Length));
					}
				}

				const string AppendPrefix = "append:";
				if (Param.StartsWith(AppendPrefix, StringComparison.InvariantCultureIgnoreCase))
				{
					int EqualsIdx = Param.IndexOf('=');
					if (EqualsIdx >= 0)
					{
						string Property = Param.Substring(AppendPrefix.Length, EqualsIdx - AppendPrefix.Length);
						string Value = Param.Substring(EqualsIdx + 1);
						if (Arguments.ContainsKey(Property))
						{
							Arguments[Property] = Arguments[Property] + ";" + Value;
						}
						else
						{
							Arguments[Property] = Value;
						}
					}
					else
					{
						Logger.LogWarning("Missing value for '{Arg0}'", Param.Substring(AppendPrefix.Length));
					}
				}
			}

			// Find all the tasks from the loaded assemblies
			Dictionary<string, ScriptTaskBinding> NameToTask = new Dictionary<string, ScriptTaskBinding>();
			if (!FindAvailableTasks(NameToTask, bPublicTasksOnly))
			{
				return ExitCode.Error_Unknown;
			}

			// Generate documentation
			if (DocumentationFileName != null)
			{
				WriteDocumentation(NameToTask, new FileReference(DocumentationFileName));
				return ExitCode.Success;
			}

			// Create the graph
			BgGraphDef Graph;
			if (ClassName != null)
			{
				// Find all the graph builders
				Dictionary<string, Type> NameToType = new Dictionary<string, Type>();
				FindAvailableGraphs(NameToType);

				Type BuilderType;
				if (!NameToType.TryGetValue(ClassName, out BuilderType))
				{
					Logger.LogError("Unable to find graph '{GraphName}'", ClassName);
					Logger.LogInformation("");
					Logger.LogInformation("Available graphs:");
					foreach (string Name in NameToType.Keys.OrderBy(x => x))
					{
						Logger.LogInformation("  {GraphName}", Name);
					}
					return ExitCode.Error_Unknown;
				}

				BgGraphBuilder Builder = (BgGraphBuilder)Activator.CreateInstance(BuilderType);
				BgGraph GraphSpec = Builder.CreateGraph(new BgEnvironment(Branch, Change, CodeChange));

				(byte[] Data, BgThunkDef[] Methods) = BgCompiler.Compile(GraphSpec);

				BgInterpreter Interpreter = new BgInterpreter(Data, Methods, Arguments);
				Interpreter.Disassemble(Logger);
				Graph = ((BgObjectDef)Interpreter.Evaluate()).Deserialize<BgGraphExpressionDef>().ToGraphDef();
			}
			else
			{
				// Import schema if one is passed in
				BgScriptSchema Schema;
				if (ImportSchemaFileName != null)
				{
					Schema = BgScriptSchema.Import(FileReference.FromString(ImportSchemaFileName));
				}
				else
				{
					// Add any primitive types
					List<(Type, ScriptSchemaStandardType)> PrimitiveTypes = new List<(Type, ScriptSchemaStandardType)>();
					PrimitiveTypes.Add((typeof(FileReference), ScriptSchemaStandardType.BalancedString));
					PrimitiveTypes.Add((typeof(DirectoryReference), ScriptSchemaStandardType.BalancedString));
					PrimitiveTypes.Add((typeof(UnrealTargetPlatform), ScriptSchemaStandardType.BalancedString));
					PrimitiveTypes.Add((typeof(MCPPlatform), ScriptSchemaStandardType.BalancedString));

					// Create a schema for the given tasks
					Schema = new BgScriptSchema(NameToTask.Values, PrimitiveTypes);
					if (SchemaFileName != null)
					{
						FileReference FullSchemaFileName = new FileReference(SchemaFileName);
						Logger.LogInformation("Writing schema to {Arg0}...", FullSchemaFileName.FullName);
						Schema.Export(FullSchemaFileName);
						if (ScriptFileName == null)
						{
							return ExitCode.Success;
						}
					}
				}

				// Check there was a script specified
				if (ScriptFileName == null)
				{
					Logger.LogError("Missing -Script= parameter for BuildGraph");
					return ExitCode.Error_Unknown;
				}

				// Normalize the script filename
				FileReference FullScriptFile = FileReference.Combine(Unreal.RootDirectory, ScriptFileName);

				// Read the script from disk
				Graph = BgScriptReader.ReadAsync(FullScriptFile, Arguments, DefaultProperties, Schema, Logger, SingleNodeName).Result;
				if (Graph == null)
				{
					return ExitCode.Error_Unknown;
				}
			}

			// Create the temp storage handler
			DirectoryReference RootDir = new DirectoryReference(CommandUtils.CmdEnv.LocalRoot);
			TempStorage Storage = new TempStorage(RootDir, DirectoryReference.Combine(RootDir, "Engine", "Saved", "BuildGraph"), (SharedStorageDir == null) ? null : new DirectoryReference(SharedStorageDir), bWriteToSharedStorage);
			if (!bResume)
			{
				Storage.CleanLocal();
			}
			foreach (string CleanNode in CleanNodes)
			{
				Storage.CleanLocalNode(CleanNode);
			}

			// Convert the supplied target references into nodes 
			HashSet<BgNodeDef> TargetNodes = new HashSet<BgNodeDef>();
			if (TargetNames.Length == 0)
			{
				if (!bListOnly && SingleNodeName == null)
				{
					Logger.LogError("Missing -Target= parameter for BuildGraph");
					return ExitCode.Error_Unknown;
				}
				TargetNodes.UnionWith(Graph.Agents.SelectMany(x => x.Nodes));
			}
			else
			{
				IEnumerable<string> NodesToResolve = null;

				// If we're only building a single node and using a preprocessed reference we only need to try to resolve the references
				// for that node.
				if (SingleNodeName != null && PreprocessedFileName != null)
				{
					NodesToResolve = new List<string> { SingleNodeName };
				}
				else
				{
					NodesToResolve = TargetNames;
				}

				foreach (string TargetName in NodesToResolve)
				{
					BgNodeDef[] Nodes;
					if (!Graph.TryResolveReference(TargetName, out Nodes))
					{
						Logger.LogError("Target '{TargetName}' is not in graph", TargetName);
						return ExitCode.Error_Unknown;
					}
					TargetNodes.UnionWith(Nodes);
				}
			}

			// Try to acquire tokens for all the target nodes we want to build
			if (TokenSignature != null)
			{
				// Find all the lock files
				HashSet<FileReference> RequiredTokens = new HashSet<FileReference>(TargetNodes.SelectMany(x => x.RequiredTokens));

				// List out all the required tokens
				if (SingleNodeName == null)
				{
					Logger.LogInformation("Required tokens:");
					foreach (BgNodeDef Node in TargetNodes)
					{
						foreach (FileReference RequiredToken in Node.RequiredTokens)
						{
							Logger.LogInformation("  '{Node}' requires {RequiredToken}", Node, RequiredToken);
						}
					}
				}

				// Try to create all the lock files
				List<FileReference> CreatedTokens = new List<FileReference>();
				if (!bListOnly)
				{
					CreatedTokens.AddRange(RequiredTokens.Where(x => WriteTokenFile(x, TokenSignature)));
				}

				// Find all the tokens that we don't have
				Dictionary<FileReference, string> MissingTokens = new Dictionary<FileReference, string>();
				foreach (FileReference RequiredToken in RequiredTokens)
				{
					string CurrentOwner = ReadTokenFile(RequiredToken);
					if (CurrentOwner != null && CurrentOwner != TokenSignature)
					{
						MissingTokens.Add(RequiredToken, CurrentOwner);
					}
				}

				// If we want to skip all the nodes with missing locks, adjust the target nodes to account for it
				if (MissingTokens.Count > 0)
				{
					if (bSkipTargetsWithoutTokens)
					{
						// Find all the nodes we're going to skip
						HashSet<BgNodeDef> SkipNodes = new HashSet<BgNodeDef>();
						foreach (IGrouping<string, FileReference> MissingTokensForBuild in MissingTokens.GroupBy(x => x.Value, x => x.Key))
						{
							Logger.LogInformation("Skipping the following nodes due to {Arg0}:", MissingTokensForBuild.Key);
							foreach (FileReference MissingToken in MissingTokensForBuild)
							{
								foreach (BgNodeDef SkipNode in TargetNodes.Where(x => x.RequiredTokens.Contains(MissingToken) && SkipNodes.Add(x)))
								{
									Logger.LogInformation("    {SkipNode}", SkipNode);
								}
							}
						}

						// Write a list of everything left over
						if (SkipNodes.Count > 0)
						{
							TargetNodes.ExceptWith(SkipNodes);
							Logger.LogInformation("Remaining target nodes:");
							foreach (BgNodeDef TargetNode in TargetNodes)
							{
								Logger.LogInformation("    {TargetNode}", TargetNode);
							}
							if (TargetNodes.Count == 0)
							{
								Logger.LogInformation("    None.");
							}
						}
					}
					else
					{
						foreach (KeyValuePair<FileReference, string> Pair in MissingTokens)
						{
							List<BgNodeDef> SkipNodes = TargetNodes.Where(x => x.RequiredTokens.Contains(Pair.Key)).ToList();
							Logger.LogError("Cannot run {Arg0} due to previous build: {Arg1}", String.Join(", ", SkipNodes), Pair.Value);
						}
						foreach (FileReference CreatedToken in CreatedTokens)
						{
							FileReference.Delete(CreatedToken);
						}
						return ExitCode.Error_Unknown;
					}
				}
			}

			// Cull the graph to include only those nodes
			Graph.Select(TargetNodes);

			// If a report for the whole build was requested, insert it into the graph
			if (ReportName != null)
			{
				BgReport NewReport = new BgReport(ReportName);
				NewReport.Nodes.UnionWith(Graph.Agents.SelectMany(x => x.Nodes));
				Graph.NameToReport.Add(ReportName, NewReport);
			}

			// Export the graph for Horde
			if (HordeExportFileName != null)
			{
				Graph.ExportForHorde(new FileReference(HordeExportFileName));
			}

			// Write out the preprocessed script
			if (PreprocessedFileName != null)
			{
				FileReference PreprocessedFileLocation = new FileReference(PreprocessedFileName);
				Logger.LogInformation("Writing {PreprocessedFileLocation}...", PreprocessedFileLocation);
				Graph.Write(PreprocessedFileLocation, (SchemaFileName != null) ? new FileReference(SchemaFileName) : null);
				bListOnly = true;
			}

			// If we're just building a single node, find it 
			BgNodeDef SingleNode = null;
			if (SingleNodeName != null && !Graph.NameToNode.TryGetValue(SingleNodeName, out SingleNode))
			{
				Logger.LogError("Node '{SingleNodeName}' is not in the trimmed graph", SingleNodeName);
				return ExitCode.Error_Unknown;
			}

			// If we just want to show the contents of the graph, do so and exit.
			if (bListOnly)
			{
				HashSet<BgNodeDef> CompletedNodes = FindCompletedNodes(Graph, Storage);
				Graph.Print(CompletedNodes, PrintOptions, Log.Logger);
			}

			// Print out all the diagnostic messages which still apply, unless we're running a step as part of a build system or just listing the contents of the file. 
			if (SingleNode == null && (!bListOnly || bShowDiagnostics))
			{
				List<BgDiagnosticDef> Diagnostics = Graph.GetAllDiagnostics();
				foreach (BgDiagnosticDef Diagnostic in Diagnostics)
				{
					if (Diagnostic.Level == LogLevel.Information)
					{
						Logger.LogInformation("{Arg0}({Arg1}): {Arg2}", Diagnostic.File, Diagnostic.Line, Diagnostic.Message);
					}
					else if (Diagnostic.Level == LogLevel.Warning)
					{
						Logger.LogWarning("{Arg0}({Arg1}): warning: {Arg2}", Diagnostic.File, Diagnostic.Line, Diagnostic.Message);
					}
					else
					{
						Logger.LogError("{Arg0}({Arg1}): error: {Arg2}", Diagnostic.File, Diagnostic.Line, Diagnostic.Message);
					}
				}
				if (Diagnostics.Any(x => x.Level == LogLevel.Error))
				{
					return ExitCode.Error_Unknown;
				}
			}

			// Export the graph to a file
			if (ExportFileName != null)
			{
				HashSet<BgNodeDef> CompletedNodes = FindCompletedNodes(Graph, Storage);
				Graph.Print(CompletedNodes, PrintOptions, Log.Logger);
				Graph.Export(new FileReference(ExportFileName), CompletedNodes);
				return ExitCode.Success;
			}

			// Create tasks for the entire graph
			Dictionary<BgNodeDef, BgNodeExecutor> NodeToExecutor = new Dictionary<BgNodeDef, BgNodeExecutor>();
			if (bSkipValidation && SingleNode != null)
			{
				if (!BindNodes(SingleNode, NameToTask, Graph.TagNameToNodeOutput, NodeToExecutor))
				{
					return ExitCode.Error_Unknown;
				}
			}
			else
			{
				if (!BindNodes(Graph, NameToTask, NodeToExecutor))
				{
					return ExitCode.Error_Unknown;
				}
			}

			// Execute the command
			if (!bListOnly)
			{
				if (SingleNode != null)
				{
					if (!await BuildNodeAsync(Graph, SingleNode, NodeToExecutor, Storage, bWithBanner: true))
					{
						return ExitCode.Error_Unknown;
					}
				}
				else
				{
					if (!await BuildAllNodesAsync(Graph, NodeToExecutor, Storage))
					{
						return ExitCode.Error_Unknown;
					}
				}
			}
			return ExitCode.Success;
		}

		bool BindNodes(BgGraphDef Graph, Dictionary<string, ScriptTaskBinding> NameToTask, Dictionary<BgNodeDef, BgNodeExecutor> NodeToExecutor)
		{
			bool bResult = true;
			foreach (BgAgentDef Agent in Graph.Agents)
			{
				foreach (BgNodeDef Node in Agent.Nodes)
				{
					bResult &= BindNodes(Node, NameToTask, Graph.TagNameToNodeOutput, NodeToExecutor);
				}
			}
			return bResult;
		}

		bool BindNodes(BgNodeDef Node, Dictionary<string, ScriptTaskBinding> NameToTask, Dictionary<string, BgNodeOutput> TagNameToNodeOutput, Dictionary<BgNodeDef, BgNodeExecutor> NodeToExecutor)
		{
			if (Node is BgScriptNode ScriptNode)
			{
				BgScriptNodeExecutor executor = new BgScriptNodeExecutor(ScriptNode);
				NodeToExecutor[Node] = executor;
				return executor.Bind(NameToTask, TagNameToNodeOutput, Logger);
			}
			else if (Node is BgNodeExpressionDef BytecodeNode)
			{
				BgBytecodeNodeExecutor executor = new BgBytecodeNodeExecutor(BytecodeNode);
				NodeToExecutor[Node] = executor;
				return executor.Bind(Logger);
			}
			else
			{
				throw new NotImplementedException();
			}
		}

		static void FindAvailableGraphs(Dictionary<string, Type> NameToType)
		{
			foreach (Assembly LoadedAssembly in ScriptManager.AllScriptAssemblies)
			{
				Type[] Types;
				try
				{
					Types = LoadedAssembly.GetTypes();
				}
				catch (ReflectionTypeLoadException ex)
				{
					Logger.LogWarning("Exception {ex} while trying to get types from assembly {LoadedAssembly}. LoaderExceptions: {Arg2}", ex, LoadedAssembly, string.Join("\n", ex.LoaderExceptions.Select(x => x.Message)));
					continue;
				}

				foreach (Type Type in Types)
				{
					if (Type.IsSubclassOf(typeof(BgGraphBuilder)))
					{
						NameToType.Add(Type.Name, Type);
					}
				}
			}
		}

		/// <summary>
		/// Find all the tasks which are available from the loaded assemblies
		/// </summary>
		/// <param name="NameToTask">Mapping from task name to information about how to serialize it</param>
		/// <param name="bPublicTasksOnly">Whether to include just public tasks, or all the tasks in any loaded assemblies</param>
		static bool FindAvailableTasks(Dictionary<string, ScriptTaskBinding> NameToTask, bool bPublicTasksOnly)
		{
			IEnumerable<Assembly> LoadedScriptAssemblies = ScriptManager.AllScriptAssemblies;

			if (bPublicTasksOnly)
			{
				LoadedScriptAssemblies = LoadedScriptAssemblies.Where(x => IsPublicAssembly(new FileReference(x.Location)));
			}
			foreach (Assembly LoadedAssembly in LoadedScriptAssemblies)
			{
				Type[] Types;
				try
				{
					Types = LoadedAssembly.GetTypes();
				}
				catch (ReflectionTypeLoadException ex)
				{
					Logger.LogWarning("Exception {ex} while trying to get types from assembly {LoadedAssembly}. LoaderExceptions: {Arg2}", ex, LoadedAssembly, string.Join("\n", ex.LoaderExceptions.Select(x => x.Message)));
					continue;
				}

				foreach (Type Type in Types)
				{
					foreach (TaskElementAttribute ElementAttribute in Type.GetCustomAttributes<TaskElementAttribute>())
					{
						if (!Type.IsSubclassOf(typeof(BgTaskImpl)))
						{
							Logger.LogError("Class '{Arg0}' has TaskElementAttribute, but is not derived from 'BgTaskImpl'", Type.Name);
							return false;
						}
						if (NameToTask.ContainsKey(ElementAttribute.Name))
						{
							Logger.LogError("Found multiple handlers for task elements called '{Arg0}'", ElementAttribute.Name);
							return false;
						}
						NameToTask.Add(ElementAttribute.Name, new ScriptTaskBinding(ElementAttribute.Name, Type, ElementAttribute.ParametersType));
					}
				}
			}
			return true;
		}

		/// <summary>
		/// Reads the contents of the given token
		/// </summary>
		/// <returns>Contents of the token, or null if it does not exist</returns>
		public string ReadTokenFile(FileReference Location)
		{
			return FileReference.Exists(Location) ? File.ReadAllText(Location.FullName) : null;
		}

		/// <summary>
		/// Attempts to write an owner to a token file transactionally
		/// </summary>
		/// <returns>True if the lock was acquired, false otherwise</returns>
		public bool WriteTokenFile(FileReference Location, string Signature)
		{
			// Check it doesn't already exist
			if (FileReference.Exists(Location))
			{
				return false;
			}

			// Make sure the directory exists
			try
			{
				DirectoryReference.CreateDirectory(Location.Directory);
			}
			catch (Exception Ex)
			{
				throw new AutomationException(Ex, "Unable to create '{0}'", Location.Directory);
			}

			// Create a temp file containing the owner name
			string TempFileName;
			for (int Idx = 0; ; Idx++)
			{
				TempFileName = String.Format("{0}.{1}.tmp", Location.FullName, Idx);
				try
				{
					byte[] Bytes = Encoding.UTF8.GetBytes(Signature);
					using (FileStream Stream = File.Open(TempFileName, FileMode.CreateNew, FileAccess.Write, FileShare.None))
					{
						Stream.Write(Bytes, 0, Bytes.Length);
					}
					break;
				}
				catch (IOException)
				{
					if (!File.Exists(TempFileName))
					{
						throw;
					}
				}
			}

			// Try to move the temporary file into place. 
			try
			{
				File.Move(TempFileName, Location.FullName);
				return true;
			}
			catch
			{
				if (!File.Exists(TempFileName))
				{
					throw;
				}
				return false;
			}
		}

		/// <summary>
		/// Checks whether the given assembly is a publically distributed engine assembly.
		/// </summary>
		/// <param name="File">Assembly location</param>
		/// <returns>True if the assembly is distributed publically</returns>
		static bool IsPublicAssembly(FileReference File)
		{
			DirectoryReference EngineDirectory = Unreal.EngineDirectory;
			if (File.IsUnderDirectory(EngineDirectory))
			{
				string[] PathFragments = File.MakeRelativeTo(EngineDirectory).Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
				if (PathFragments.All(x => !x.Equals("NotForLicensees", StringComparison.InvariantCultureIgnoreCase) && !x.Equals("NoRedist", StringComparison.InvariantCultureIgnoreCase)))
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Find all the nodes in the graph which are already completed
		/// </summary>
		/// <param name="Graph">The graph instance</param>
		/// <param name="Storage">The temp storage backend which stores the shared state</param>
		HashSet<BgNodeDef> FindCompletedNodes(BgGraphDef Graph, TempStorage Storage)
		{
			HashSet<BgNodeDef> CompletedNodes = new HashSet<BgNodeDef>();
			foreach (BgNodeDef Node in Graph.Agents.SelectMany(x => x.Nodes))
			{
				if (Storage.IsComplete(Node.Name))
				{
					CompletedNodes.Add(Node);
				}
			}
			return CompletedNodes;
		}

		/// <summary>
		/// Builds all the nodes in the graph
		/// </summary>
		/// <param name="Graph">The graph instance</param>
		/// <param name="NodeToExecutor">Map from node to executor</param>
		/// <param name="Storage">The temp storage backend which stores the shared state</param>
		/// <returns>True if everything built successfully</returns>
		async Task<bool> BuildAllNodesAsync(BgGraphDef Graph, Dictionary<BgNodeDef, BgNodeExecutor> NodeToExecutor, TempStorage Storage)
		{
			// Build a flat list of nodes to execute, in order
			BgNodeDef[] NodesToExecute = Graph.Agents.SelectMany(x => x.Nodes).ToArray();

			// Check the integrity of any local nodes that have been completed. It's common to run formal builds locally between regular development builds, so we may have 
			// stale local state. Rather than failing later, detect and clean them up now.
			HashSet<BgNodeDef> CleanedNodes = new HashSet<BgNodeDef>();
			foreach (BgNodeDef NodeToExecute in NodesToExecute)
			{
				if (NodeToExecute.InputDependencies.Any(x => CleanedNodes.Contains(x)) || !Storage.CheckLocalIntegrity(NodeToExecute.Name, NodeToExecute.Outputs.Select(x => x.TagName)))
				{
					Storage.CleanLocalNode(NodeToExecute.Name);
					CleanedNodes.Add(NodeToExecute);
				}
			}

			// Execute them in order
			int NodeIdx = 0;
			foreach (BgNodeDef NodeToExecute in NodesToExecute)
			{
				Logger.LogInformation("****** [{Arg0}/{Arg1}] {Arg2}", ++NodeIdx, NodesToExecute.Length, NodeToExecute.Name);
				if (!Storage.IsComplete(NodeToExecute.Name))
				{
					Logger.LogInformation("");
					if (!await BuildNodeAsync(Graph, NodeToExecute, NodeToExecutor, Storage, bWithBanner: false))
					{
						return false;
					}
					Logger.LogInformation("");
				}
			}
			return true;
		}

		/// <summary>
		/// Helper class to execute a cleanup script on termination
		/// </summary>
		class CleanupScriptRunner : IDisposable
		{
			readonly ILogger _logger;
			readonly FileReference _scriptFile;

			public CleanupScriptRunner(ILogger logger)
			{
				_logger = logger;

				string CleanupScriptEnvVar = Environment.GetEnvironmentVariable(CustomTask.CleanupScriptEnvVarName);
				if (String.IsNullOrEmpty(CleanupScriptEnvVar))
				{
					string extension = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "bat" : "sh";
					_scriptFile = FileReference.Combine(Unreal.EngineDirectory, "Intermediate", $"OnExit.{extension}");
					FileReference.Delete(_scriptFile);
					Environment.SetEnvironmentVariable(CustomTask.CleanupScriptEnvVarName, _scriptFile.FullName);
				}
			}

			public void Dispose()
			{
				if (_scriptFile != null)
				{
					if (FileReference.Exists(_scriptFile))
					{
						_logger.LogInformation("Executing cleanup commands from {ScriptFile}", _scriptFile);
						if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
						{
							CommandUtils.Run(BuildHostPlatform.Current.Shell.FullName, $"/C {CommandLineArguments.Quote(_scriptFile.FullName)}");
						}
						else
						{
							CommandUtils.Run(BuildHostPlatform.Current.Shell.FullName, CommandLineArguments.Quote(_scriptFile.FullName));
						}
					}
					Environment.SetEnvironmentVariable(CustomTask.CleanupScriptEnvVarName, null);
				}
			}
		}

		/// <summary>
		/// Build a node
		/// </summary>
		/// <param name="Graph">The graph to which the node belongs. Used to determine which outputs need to be transferred to temp storage.</param>
		/// <param name="Node">The node to build</param>
		/// <param name="NodeToExecutor">Map from node to executor</param>
		/// <param name="Storage">The temp storage backend which stores the shared state</param>
		/// <param name="bWithBanner">Whether to write a banner before and after this node's log output</param>
		/// <returns>True if the node built successfully, false otherwise.</returns>
		async Task<bool> BuildNodeAsync(BgGraphDef Graph, BgNodeDef Node, Dictionary<BgNodeDef, BgNodeExecutor> NodeToExecutor, TempStorage Storage, bool bWithBanner)
		{
			DirectoryReference RootDir = new DirectoryReference(CommandUtils.CmdEnv.LocalRoot);

			// Register something to execute cleanup commands
			using CleanupScriptRunner CleanupRunner = new CleanupScriptRunner(Logger);

			// Create the mapping of tag names to file sets
			Dictionary<string, HashSet<FileReference>> TagNameToFileSet = new Dictionary<string, HashSet<FileReference>>();

			// Read all the input tags for this node, and build a list of referenced input storage blocks
			HashSet<TempStorageBlock> InputStorageBlocks = new HashSet<TempStorageBlock>();
			foreach (BgNodeOutput Input in Node.Inputs)
			{
				TempStorageTagManifest FileList = Storage.ReadFileList(Input.ProducingNode.Name, Input.TagName);
				TagNameToFileSet[Input.TagName] = FileList.ToFileSet(RootDir);
				InputStorageBlocks.UnionWith(FileList.Blocks);
			}

			// Read the manifests for all the input storage blocks
			Dictionary<TempStorageBlock, TempStorageManifest> InputManifests = new Dictionary<TempStorageBlock, TempStorageManifest>();
			using (IScope Scope = GlobalTracer.Instance.BuildSpan("TempStorage").WithTag("resource", "read").StartActive())
			{
				Scope.Span.SetTag("blocks", InputStorageBlocks.Count);
				foreach (TempStorageBlock InputStorageBlock in InputStorageBlocks)
				{
					TempStorageManifest Manifest = Storage.Retrieve(InputStorageBlock.NodeName, InputStorageBlock.OutputName);
					InputManifests[InputStorageBlock] = Manifest;
				}
				Scope.Span.SetTag("size", InputManifests.Sum(x => x.Value.GetTotalSize()));
			}

			// Read all the input storage blocks, keeping track of which block each file came from
			Dictionary<FileReference, TempStorageBlock> FileToStorageBlock = new Dictionary<FileReference, TempStorageBlock>();
			foreach (KeyValuePair<TempStorageBlock, TempStorageManifest> Pair in InputManifests)
			{
				TempStorageBlock InputStorageBlock = Pair.Key;
				foreach (FileReference File in Pair.Value.Files.Select(x => x.ToFileReference(RootDir)))
				{
					TempStorageBlock CurrentStorageBlock;
					if (FileToStorageBlock.TryGetValue(File, out CurrentStorageBlock) && !TempStorage.IsDuplicateBuildProduct(File))
					{
						Logger.LogError("File '{File}' was produced by {InputStorageBlock} and {CurrentStorageBlock}", File, InputStorageBlock, CurrentStorageBlock);
					}
					FileToStorageBlock[File] = InputStorageBlock;
				}
			}

			// Add placeholder outputs for the current node
			foreach (BgNodeOutput Output in Node.Outputs)
			{
				TagNameToFileSet.Add(Output.TagName, new HashSet<FileReference>());
			}

			// Execute the node
			if (bWithBanner)
			{
				Console.WriteLine();
				Logger.LogInformation("========== Starting: {Arg0} ==========", Node.Name);
			}
			if (!await NodeToExecutor[Node].ExecuteAsync(new JobContext(Node.Name, this), TagNameToFileSet))
			{
				return false;
			}
			if (bWithBanner)
			{
				Logger.LogInformation("========== Finished: {Arg0} ==========", Node.Name);
				Console.WriteLine();
			}

			// Check that none of the inputs have been clobbered
			Dictionary<string, string> ModifiedFiles = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase);
			foreach (TempStorageFile File in InputManifests.Values.SelectMany(x => x.Files))
			{
				string Message;
				if (!ModifiedFiles.ContainsKey(File.RelativePath) && !File.Compare(Unreal.RootDirectory, out Message))
				{
					// look up the previous nodes to help with error diagnosis
					List<string> PreviousNodeNames = InputManifests.Where(x => x.Value.Files.Contains(File))
						.Select(x => x.Key.NodeName)
						.ToList();
					if (PreviousNodeNames.Count > 0)
					{
						Message += $" (previous {(PreviousNodeNames.Count == 1 ? "step" : "steps")}: {string.Join(" + ", PreviousNodeNames)})";
					}

					ModifiedFiles.Add(File.RelativePath, Message);
				}
			}
			if (ModifiedFiles.Count > 0)
			{
				string modifiedFileList = "";
				if (ModifiedFiles.Count < 100)
				{
					modifiedFileList = String.Join("\n", ModifiedFiles.Select(x => x.Value));
				}
				else
				{
					modifiedFileList = String.Join("\n", ModifiedFiles.Take(100).Select(x => x.Value));
					modifiedFileList += $"{Environment.NewLine}And {ModifiedFiles.Count - 100} more.";
				}
				throw new AutomationException("Build {0} from a previous step have been modified:\n{1}", (ModifiedFiles.Count == 1) ? "product" : "products", modifiedFileList);
			}

			// Determine all the output files which are required to be copied to temp storage (because they're referenced by nodes in another agent)
			HashSet<FileReference> ReferencedOutputFiles = new HashSet<FileReference>();
			foreach (BgAgentDef Agent in Graph.Agents)
			{
				bool bSameAgent = Agent.Nodes.Contains(Node);
				foreach (BgNodeDef OtherNode in Agent.Nodes)
				{
					if (!bSameAgent)
					{
						foreach (BgNodeOutput Input in OtherNode.Inputs.Where(x => x.ProducingNode == Node))
						{
							ReferencedOutputFiles.UnionWith(TagNameToFileSet[Input.TagName]);
						}
					}
				}
			}

			// Find a block name for all new outputs
			Dictionary<FileReference, string> FileToOutputName = new Dictionary<FileReference, string>();
			foreach (BgNodeOutput Output in Node.Outputs)
			{
				HashSet<FileReference> Files = TagNameToFileSet[Output.TagName];
				foreach (FileReference File in Files)
				{
					if (!FileToStorageBlock.ContainsKey(File) && File.IsUnderDirectory(RootDir))
					{
						if (Output == Node.DefaultOutput)
						{
							if (!FileToOutputName.ContainsKey(File))
							{
								FileToOutputName[File] = "";
							}
						}
						else
						{
							string OutputName;
							if (FileToOutputName.TryGetValue(File, out OutputName) && OutputName.Length > 0)
							{
								FileToOutputName[File] = String.Format("{0}+{1}", OutputName, Output.TagName.Substring(1));
							}
							else
							{
								FileToOutputName[File] = Output.TagName.Substring(1);
							}
						}
					}
				}
			}

			// Invert the dictionary to make a mapping of storage block to the files each contains
			Dictionary<string, HashSet<FileReference>> OutputStorageBlockToFiles = new Dictionary<string, HashSet<FileReference>>();
			foreach (KeyValuePair<FileReference, string> Pair in FileToOutputName)
			{
				HashSet<FileReference> Files;
				if (!OutputStorageBlockToFiles.TryGetValue(Pair.Value, out Files))
				{
					Files = new HashSet<FileReference>();
					OutputStorageBlockToFiles.Add(Pair.Value, Files);
				}
				Files.Add(Pair.Key);
			}

			// Write all the storage blocks, and update the mapping from file to storage block
			using (GlobalTracer.Instance.BuildSpan("TempStorage").WithTag("resource", "Write").StartActive())
			{
				foreach (KeyValuePair<string, HashSet<FileReference>> Pair in OutputStorageBlockToFiles)
				{
					TempStorageBlock OutputBlock = new TempStorageBlock(Node.Name, Pair.Key);
					foreach (FileReference File in Pair.Value)
					{
						FileToStorageBlock.Add(File, OutputBlock);
					}
					Storage.Archive(Node.Name, Pair.Key, Pair.Value.ToArray(), Pair.Value.Any(x => ReferencedOutputFiles.Contains(x)));
				}

				// Find all the output tags that are published artifacts
				Dictionary<string, BgArtifactDef> outputNameToArtifact = Graph.Artifacts.ToDictionary(x => x.TagName, x => x);

				// Publish all the output tags
				foreach (BgNodeOutput Output in Node.Outputs)
				{
					HashSet<FileReference> Files = TagNameToFileSet[Output.TagName];

					HashSet<TempStorageBlock> StorageBlocks = new HashSet<TempStorageBlock>();
					foreach (FileReference File in Files)
					{
						TempStorageBlock StorageBlock;
						if (FileToStorageBlock.TryGetValue(File, out StorageBlock))
						{
							StorageBlocks.Add(StorageBlock);
						}
					}

					IEnumerable<string> Keys = Enumerable.Empty<string>();
					if (outputNameToArtifact.TryGetValue(Output.TagName, out BgArtifactDef artifact))
					{
						Keys = artifact.Keys;
					}

					Storage.WriteFileList(Node.Name, Output.TagName, Files, StorageBlocks.ToArray(), Keys);
				}
			}

			// Mark the node as succeeded
			Storage.MarkAsComplete(Node.Name);
			return true;
		}

		/// <summary>
		/// Gets an environment variable, returning null if it's not set or empty.
		/// </summary>
		static string GetEnvVarOrNull(string Name)
		{
			string EnvVar = Environment.GetEnvironmentVariable(Name);
			return String.IsNullOrEmpty(EnvVar) ? null : EnvVar;
		}

		/// <summary>
		/// Gets an environment variable as an integer, returning null if it's not set or empty.
		/// </summary>
		static int? GetEnvVarIntOrNull(string Name)
		{
			string EnvVar = GetEnvVarOrNull(Name);
			if (EnvVar != null && Int32.TryParse(EnvVar, out int Value))
			{
				return Value;
			}
			return null;
		}

		/// <summary>
		/// Generate HTML documentation for all the tasks
		/// </summary>
		/// <param name="NameToTask">Map of task name to implementation</param>
		/// <param name="OutputFile">Output file</param>
		static void WriteDocumentation(Dictionary<string, ScriptTaskBinding> NameToTask, FileReference OutputFile)
		{
			// Find all the assemblies containing tasks
			Assembly[] TaskAssemblies = NameToTask.Values.Select(x => x.ParametersClass.Assembly).Distinct().ToArray();

			// Read documentation for each of them
			Dictionary<string, XmlElement> MemberNameToElement = new Dictionary<string, XmlElement>();
			foreach (Assembly TaskAssembly in TaskAssemblies)
			{
				string XmlFileName = Path.ChangeExtension(TaskAssembly.Location, ".xml");
				if (File.Exists(XmlFileName))
				{
					// Read the document
					XmlDocument Document = new XmlDocument();
					Document.Load(XmlFileName);

					// Parse all the members, and add them to the map
					foreach (XmlElement Element in Document.SelectNodes("/doc/members/member"))
					{
						string Name = Element.GetAttribute("name");
						MemberNameToElement.Add(Name, Element);
					}
				}
			}

			// Create the output directory
			if (FileReference.Exists(OutputFile))
			{
				FileReference.MakeWriteable(OutputFile);
			}
			else
			{
				DirectoryReference.CreateDirectory(OutputFile.Directory);
			}

			// Write the output file
			if (OutputFile.HasExtension(".udn"))
			{
				WriteDocumentationUDN(NameToTask, MemberNameToElement, OutputFile);
			}
			else if (OutputFile.HasExtension(".html"))
			{
				WriteDocumentationHTML(NameToTask, MemberNameToElement, OutputFile);
			}
			else
			{
				throw new BuildException("Unable to detect format from extension of output file ({0})", OutputFile);
			}
		}

		/// <summary>
		/// Writes documentation to a UDN file
		/// </summary>
		/// <param name="NameToTask">Map of name to script task</param>
		/// <param name="MemberNameToElement">Map of field name to XML documenation element</param>
		/// <param name="OutputFile">The output file to write to</param>
		static void WriteDocumentationUDN(Dictionary<string, ScriptTaskBinding> NameToTask, Dictionary<string, XmlElement> MemberNameToElement, FileReference OutputFile)
		{
			using (StreamWriter Writer = new StreamWriter(OutputFile.FullName))
			{
				Writer.WriteLine("Availability: NoPublish");
				Writer.WriteLine("Title: BuildGraph Predefined Tasks");
				Writer.WriteLine("Crumbs: %ROOT%, Programming, Programming/Development, Programming/Development/BuildGraph, Programming/Development/BuildGraph/BuildGraphScriptTasks");
				Writer.WriteLine("Description: This is a procedurally generated markdown page.");
				Writer.WriteLine("version: {0}.{1}", ReadOnlyBuildVersion.Current.MajorVersion, ReadOnlyBuildVersion.Current.MinorVersion);
				Writer.WriteLine("parent:Programming/Development/BuildGraph/BuildGraphScriptTasks");
				Writer.WriteLine();
				foreach (string TaskName in NameToTask.Keys.OrderBy(x => x))
				{
					// Get the task object
					ScriptTaskBinding Task = NameToTask[TaskName];

					// Get the documentation for this task
					XmlElement TaskElement;
					if (MemberNameToElement.TryGetValue("T:" + Task.TaskClass.FullName, out TaskElement))
					{
						// Write the task heading
						Writer.WriteLine("### {0}", TaskName);
						Writer.WriteLine();
						Writer.WriteLine(ConvertToMarkdown(TaskElement.SelectSingleNode("summary")));
						Writer.WriteLine();

						// Document the parameters
						List<string[]> Rows = new List<string[]>();
						foreach (string ParameterName in Task.NameToParameter.Keys)
						{
							// Get the parameter data
							ScriptTaskParameterBinding Parameter = Task.NameToParameter[ParameterName];

							// Get the documentation for this parameter
							XmlElement ParameterElement;
							if (MemberNameToElement.TryGetValue("F:" + Parameter.FieldInfo.DeclaringType.FullName + "." + Parameter.Name, out ParameterElement))
							{
								Type FieldType = Parameter.FieldInfo.FieldType;
								if (FieldType.IsGenericType && FieldType.GetGenericTypeDefinition() == typeof(Nullable<>))
								{
									FieldType = FieldType.GetGenericArguments()[0];
								}

								string TypeName;
								if (Parameter.ValidationType != TaskParameterValidationType.Default)
								{
									TypeName = Parameter.ValidationType.ToString();
								}
								else if (FieldType == typeof(int))
								{
									TypeName = "Integer";
								}
								else if (FieldType == typeof(HashSet<DirectoryReference>))
								{
									TypeName = "DirectoryList";
								}
								else
								{
									TypeName = FieldType.Name;
								}

								string[] Columns = new string[4];
								Columns[0] = ParameterName;
								Columns[1] = TypeName;
								Columns[2] = Parameter.Optional ? "Optional" : "Required";
								Columns[3] = ConvertToMarkdown(ParameterElement.SelectSingleNode("summary"));
								Rows.Add(Columns);
							}
						}

						// Always include the "If" attribute
						string[] IfColumns = new string[4];
						IfColumns[0] = "If";
						IfColumns[1] = "Condition";
						IfColumns[2] = "Optional";
						IfColumns[3] = "Whether to execute this task. It is ignored if this condition evaluates to false.";
						Rows.Add(IfColumns);

						// Get the width of each column
						int[] Widths = new int[4];
						for (int Idx = 0; Idx < 4; Idx++)
						{
							Widths[Idx] = Rows.Max(x => x[Idx].Length);
						}

						// Format the markdown table
						string Format = String.Format("| {{0,-{0}}} | {{1,-{1}}} | {{2,-{2}}} | {{3,-{3}}} |", Widths[0], Widths[1], Widths[2], Widths[3]);
						Writer.WriteLine(Format, "", "", "", "");
						Writer.WriteLine(Format, new string('-', Widths[0]), new string('-', Widths[1]), new string('-', Widths[2]), new string('-', Widths[3]));
						for (int Idx = 0; Idx < Rows.Count; Idx++)
						{
							Writer.WriteLine(Format, Rows[Idx][0], Rows[Idx][1], Rows[Idx][2], Rows[Idx][3]);
						}

						// Blank line before next task
						Writer.WriteLine();
					}
				}
			}
		}

		/// <summary>
		/// Writes documentation to an HTML file
		/// </summary>
		/// <param name="NameToTask">Map of name to script task</param>
		/// <param name="MemberNameToElement">Map of field name to XML documenation element</param>
		/// <param name="OutputFile">The output file to write to</param>
		static void WriteDocumentationHTML(Dictionary<string, ScriptTaskBinding> NameToTask, Dictionary<string, XmlElement> MemberNameToElement, FileReference OutputFile)
		{
			Logger.LogInformation("Writing {OutputFile}...", OutputFile);
			using (StreamWriter Writer = new StreamWriter(OutputFile.FullName))
			{
				Writer.WriteLine("<html>");
				Writer.WriteLine("  <head>");
				Writer.WriteLine("    <style>");
				Writer.WriteLine("      table { border-collapse: collapse }");
				Writer.WriteLine("      table, th, td { border: 1px solid black; }");
				Writer.WriteLine("    </style>");
				Writer.WriteLine("  </head>");
				Writer.WriteLine("  <body>");
				Writer.WriteLine("    <h1>BuildGraph Tasks</h1>");
				foreach (string TaskName in NameToTask.Keys.OrderBy(x => x))
				{
					// Get the task object
					ScriptTaskBinding Task = NameToTask[TaskName];

					// Get the documentation for this task
					XmlElement TaskElement;
					if (MemberNameToElement.TryGetValue("T:" + Task.TaskClass.FullName, out TaskElement))
					{
						// Write the task heading
						Writer.WriteLine("    <h2>{0}</h2>", TaskName);
						Writer.WriteLine("    <p>{0}</p>", TaskElement.SelectSingleNode("summary").InnerXml.Trim());

						// Start the parameter table
						Writer.WriteLine("    <table>");
						Writer.WriteLine("      <tr>");
						Writer.WriteLine("        <th>Attribute</th>");
						Writer.WriteLine("        <th>Type</th>");
						Writer.WriteLine("        <th>Usage</th>");
						Writer.WriteLine("        <th>Description</th>");
						Writer.WriteLine("      </tr>");

						// Document the parameters
						foreach (string ParameterName in Task.NameToParameter.Keys)
						{
							// Get the parameter data
							ScriptTaskParameterBinding Parameter = Task.NameToParameter[ParameterName];

							// Get the documentation for this parameter
							XmlElement ParameterElement;
							if (MemberNameToElement.TryGetValue("F:" + Parameter.FieldInfo.DeclaringType.FullName + "." + Parameter.Name, out ParameterElement))
							{
								string TypeName = Parameter.FieldInfo.FieldType.Name;
								if (Parameter.ValidationType != TaskParameterValidationType.Default)
								{
									StringBuilder NewTypeName = new StringBuilder(Parameter.ValidationType.ToString());
									for (int Idx = 1; Idx < NewTypeName.Length; Idx++)
									{
										if (Char.IsLower(NewTypeName[Idx - 1]) && Char.IsUpper(NewTypeName[Idx]))
										{
											NewTypeName.Insert(Idx, ' ');
										}
									}
									TypeName = NewTypeName.ToString();
								}

								Writer.WriteLine("      <tr>");
								Writer.WriteLine("         <td>{0}</td>", ParameterName);
								Writer.WriteLine("         <td>{0}</td>", TypeName);
								Writer.WriteLine("         <td>{0}</td>", Parameter.Optional ? "Optional" : "Required");
								Writer.WriteLine("         <td>{0}</td>", ParameterElement.SelectSingleNode("summary").InnerXml.Trim());
								Writer.WriteLine("      </tr>");
							}
						}

						// Always include the "If" attribute
						Writer.WriteLine("     <tr>");
						Writer.WriteLine("       <td>If</td>");
						Writer.WriteLine("       <td>Condition</td>");
						Writer.WriteLine("       <td>Optional</td>");
						Writer.WriteLine("       <td>Whether to execute this task. It is ignored if this condition evaluates to false.</td>");
						Writer.WriteLine("     </tr>");

						// Close the table
						Writer.WriteLine("    <table>");
					}
				}
				Writer.WriteLine("  </body>");
				Writer.WriteLine("</html>");
			}
		}

		/// <summary>
		/// Converts an XML documentation node to markdown
		/// </summary>
		/// <param name="Node">The node to read</param>
		/// <returns>Text in markdown format</returns>
		static string ConvertToMarkdown(XmlNode Node)
		{
			string Text = Node.InnerXml;

			StringBuilder Result = new StringBuilder();
			for (int Idx = 0; Idx < Text.Length; Idx++)
			{
				if (Char.IsWhiteSpace(Text[Idx]))
				{
					Result.Append(' ');
					while (Idx + 1 < Text.Length && Char.IsWhiteSpace(Text[Idx + 1]))
					{
						Idx++;
					}
				}
				else
				{
					Result.Append(Text[Idx]);
				}
			}
			return Result.ToString().Trim();
		}
	}

	/// <summary>
	/// Legacy command name for compatibility.
	/// </summary>
	public class Build : BuildGraph
	{
	}
}

