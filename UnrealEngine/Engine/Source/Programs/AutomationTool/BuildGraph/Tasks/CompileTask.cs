// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;
using AutomationTool;
using System.Xml;
using EpicGames.Core;
using OpenTracing;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a compile task
	/// </summary>
	public class CompileTaskParameters
	{
		/// <summary>
		/// The target to compile.
		/// </summary>
		[TaskParameter(Optional=true)]
		public string Target;

		/// <summary>
		/// The configuration to compile.
		/// </summary>
		[TaskParameter]
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// The platform to compile for.
		/// </summary>
		[TaskParameter]
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// The project to compile with.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string Project;

		/// <summary>
		/// Additional arguments for UnrealBuildTool.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// Whether to allow using XGE for compilation.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool AllowXGE = true;

		/// <summary>
		/// No longer necessary as UnrealBuildTool is run to compile targets.
		/// </summary>
		[TaskParameter(Optional = true)]
		[Obsolete]
		public bool AllowParallelExecutor = true;

		/// <summary>
		/// Whether to allow UBT to use all available cores, when AllowXGE is disabled.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool AllowAllCores = false;

		/// <summary>
		/// Whether to allow cleaning this target. If unspecified, targets are cleaned if the -Clean argument is passed on the command line.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool? Clean = null;

		/// <summary>
		/// Tag to be applied to build products of this task.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Executor for compile tasks, which can compile multiple tasks simultaneously
	/// </summary>
	public class CompileTaskExecutor : ITaskExecutor
	{
		/// <summary>
		/// List of targets to compile. As well as the target specifically added for this task, additional compile tasks may be merged with it.
		/// </summary>
		List<UnrealBuild.BuildTarget> Targets = new List<UnrealBuild.BuildTarget>();

		/// <summary>
		/// Mapping of receipt filename to its corresponding tag name
		/// </summary>
		Dictionary<UnrealBuild.BuildTarget, string> TargetToTagName = new Dictionary<UnrealBuild.BuildTarget,string>();

		/// <summary>
		/// Whether to allow using XGE for this job
		/// </summary>
		bool bAllowXGE = true;

		/// <summary>
		/// Whether to allow using all available cores for this job, when bAllowXGE is false
		/// </summary>
		bool bAllowAllCores = false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Task">Initial task to execute</param>
		public CompileTaskExecutor(CompileTask Task)
		{
			Add(Task);
		}

		/// <summary>
		/// Adds another task to this executor
		/// </summary>
		/// <param name="Task">Task to add</param>
		/// <returns>True if the task could be added, false otherwise</returns>
		public bool Add(BgTaskImpl Task)
		{
			CompileTask CompileTask = Task as CompileTask;
			if(CompileTask == null)
			{
				return false;
			}

			if(Targets.Count > 0)
			{
				if (bAllowXGE != CompileTask.Parameters.AllowXGE)
				{
					return false;
				}
			}

			CompileTaskParameters Parameters = CompileTask.Parameters;
			bAllowXGE &= Parameters.AllowXGE;
			bAllowAllCores &= Parameters.AllowAllCores;

			UnrealBuild.BuildTarget Target = new UnrealBuild.BuildTarget { TargetName = Parameters.Target, Platform = Parameters.Platform, Config = Parameters.Configuration, UprojectPath = CompileTask.FindProjectFile(), UBTArgs = (Parameters.Arguments ?? ""), Clean = Parameters.Clean };
			if(!String.IsNullOrEmpty(Parameters.Tag))
			{
				TargetToTagName.Add(Target, Parameters.Tag);
			}
			Targets.Add(Target);

			return true;
		}

		/// <summary>
		/// Execute all the tasks added to this executor.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		/// <returns>Whether the task succeeded or not. Exiting with an exception will be caught and treated as a failure.</returns>
		public Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Create the agenda
			UnrealBuild.BuildAgenda Agenda = new UnrealBuild.BuildAgenda();
			Agenda.Targets.AddRange(Targets);

			// Build everything
			Dictionary<UnrealBuild.BuildTarget, BuildManifest> TargetToManifest = new Dictionary<UnrealBuild.BuildTarget,BuildManifest>();
			UnrealBuild Builder = new UnrealBuild(Job.OwnerCommand);

			bool bAllCores = (CommandUtils.IsBuildMachine || bAllowAllCores);	// Enable using all cores if this is a build agent or the flag was passed in to the task and XGE is disabled.
			Builder.Build(Agenda, InDeleteBuildProducts: null, InUpdateVersionFiles: false, InForceNoXGE: !bAllowXGE, InAllCores: bAllCores, InTargetToManifest: TargetToManifest);

			UnrealBuild.CheckBuildProducts(Builder.BuildProductFiles);

			// Tag all the outputs
			foreach(KeyValuePair<UnrealBuild.BuildTarget, string> TargetTagName in TargetToTagName)
			{
				BuildManifest Manifest;
				if(!TargetToManifest.TryGetValue(TargetTagName.Key, out Manifest))
				{
					throw new AutomationException("Missing manifest for target {0} {1} {2}", TargetTagName.Key.TargetName, TargetTagName.Key.Platform, TargetTagName.Key.Config);
				}

				HashSet<FileReference> ManifestBuildProducts = Manifest.BuildProducts.Select(x => new FileReference(x)).ToHashSet();

				// when we make a Mac/IOS build, Xcode will finalize the .app directory, adding files that UBT has no idea about, so now we recursively add any files in the .app
				// as BuildProducts. look for any .apps that we have any files as BuildProducts, and expand to include all files in the .app
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					HashSet<string> AppBundleLocations = new();
					foreach (FileReference File in ManifestBuildProducts)
					{
						// look for a ".app/" portion and chop off anything after it
						int AppLocation = File.FullName.IndexOf(".app/", StringComparison.InvariantCultureIgnoreCase);
						if (AppLocation > 0)
						{
							AppBundleLocations.Add(File.FullName.Substring(0, AppLocation + 4));
						}
					}

					// now with a unique set of app bundles, add all files in them
					foreach (string AppBundleLocation in AppBundleLocations)
					{
						ManifestBuildProducts.UnionWith(DirectoryReference.EnumerateFiles(new DirectoryReference(AppBundleLocation), "*", System.IO.SearchOption.AllDirectories));
					}
				}

				foreach (string TagName in CustomTask.SplitDelimitedList(TargetTagName.Value))
				{
					HashSet<FileReference> FileSet = CustomTask.FindOrAddTagSet(TagNameToFileSet, TagName);
					FileSet.UnionWith(ManifestBuildProducts);
				}
			}

			// Add everything to the list of build products
			BuildProducts.UnionWith(Builder.BuildProductFiles.Select(x => new FileReference(x)));
			return Task.CompletedTask;
		}
	}

	/// <summary>
	/// Compiles a target with UnrealBuildTool.
	/// </summary>
	[TaskElement("Compile", typeof(CompileTaskParameters))]
	public class CompileTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		public CompileTaskParameters Parameters;

		/// <summary>
		/// Resolved path to Project file
		/// </summary>
		public FileReference ProjectFile = null;

		/// <summary>
		/// Construct a compile task
		/// </summary>
		/// <param name="Parameters">Parameters for this task</param>
		public CompileTask(CompileTaskParameters Parameters)
		{
			this.Parameters = Parameters;
		}

		/// <summary>
		/// Resolve the path to the project file
		/// </summary>
		public FileReference FindProjectFile()
		{
			FileReference ProjectFile = null;

			// Resolve the full path to the project file
			if(!String.IsNullOrEmpty(Parameters.Project))
			{
				if(Parameters.Project.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase))
				{
					ProjectFile = CustomTask.ResolveFile(Parameters.Project);
				}
				else
				{
					ProjectFile = NativeProjects.EnumerateProjectFiles(Log.Logger).FirstOrDefault(x => x.GetFileNameWithoutExtension().Equals(Parameters.Project, StringComparison.OrdinalIgnoreCase));
				}

				if(ProjectFile == null || !FileReference.Exists(ProjectFile))
				{
					throw new BuildException("Unable to resolve project '{0}'", Parameters.Project);
				}
			}

			return ProjectFile;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			//
			// Don't do any logic here. You have to do it in the ctor or a getter
			//  otherwise you break the ITaskExecutor pathway, which doesn't call this function!
			//
			return GetExecutor().ExecuteAsync(Job, BuildProducts, TagNameToFileSet);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public override ITaskExecutor GetExecutor()
		{
			return new CompileTaskExecutor(this);
		}

		/// <summary>
		/// Get properties to include in tracing info
		/// </summary>
		/// <param name="Span">The span to add metadata to</param>
		/// <param name="Prefix">Prefix for all metadata keys</param>
		public override void GetTraceMetadata(ITraceSpan Span, string Prefix)
		{
			base.GetTraceMetadata(Span, Prefix);

			Span.AddMetadata(Prefix + "target.name", Parameters.Target);
			Span.AddMetadata(Prefix + "target.config", Parameters.Configuration.ToString());
			Span.AddMetadata(Prefix + "target.platform", Parameters.Platform.ToString());

			if (Parameters.Project != null)
			{
				Span.AddMetadata(Prefix + "target.project", Parameters.Project);
			}
		}
		
		/// <summary>
		/// Get properties to include in tracing info
		/// </summary>
		/// <param name="Span">The span to add metadata to</param>
		/// <param name="Prefix">Prefix for all metadata keys</param>
		public override void GetTraceMetadata(ISpan Span, string Prefix)
		{
			base.GetTraceMetadata(Span, Prefix);

			Span.SetTag(Prefix + "target.name", Parameters.Target);
			Span.SetTag(Prefix + "target.config", Parameters.Configuration.ToString());
			Span.SetTag(Prefix + "target.platform", Parameters.Platform.ToString());

			if (Parameters.Project != null)
			{
				Span.SetTag(Prefix + "target.project", Parameters.Project);
			}
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			yield break;
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return FindTagNamesFromList(Parameters.Tag);
		}
	}

	public static partial class StandardTasks
	{
		/// <summary>
		/// Compiles a target
		/// </summary>
		/// <param name="Target">The target to compile</param>
		/// <param name="Configuration">The configuration to compile</param>
		/// <param name="Platform">The platform to compile for</param>
		/// <param name="Project">The project to compile with</param>
		/// <param name="Arguments">Additional arguments for UnrealBuildTool</param>
		/// <param name="AllowXGE">Whether to allow using XGE for compilation</param>
		/// <param name="Clean">Whether to allow cleaning this target. If unspecified, targets are cleaned if the -Clean argument is passed on the command line</param>
		/// <returns>Build products from the compile</returns>
		public static async Task<FileSet> CompileAsync(string Target, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, FileReference Project = null, string Arguments = null, bool AllowXGE = true, bool? Clean = null)
		{
			CompileTaskParameters Parameters = new CompileTaskParameters();
			Parameters.Target = Target;
			Parameters.Platform = Platform;
			Parameters.Configuration = Configuration;
			Parameters.Project = Project?.FullName;
			Parameters.Arguments = Arguments;
			Parameters.AllowXGE = AllowXGE;
			Parameters.Clean = Clean;
			return await ExecuteAsync(new CompileTask(Parameters));
		}
	}
}
