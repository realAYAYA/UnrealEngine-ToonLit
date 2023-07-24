// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildTool;
using UnrealBuildBase;

namespace AutomationTool.Benchmark
{

	[Help("Runs benchmarks and reports overall results")]
	[Help("Example1: RunUAT BenchmarkBuild -all -project=Unreal")]
	[Help("Example2: RunUAT BenchmarkBuild -allcompile -project=Unreal+EngineTest -platform=PS4")]
	[Help("Example3: RunUAT BenchmarkBuild -editor -client -cook -cooknoshaderddc -cooknoddc -xge -noxge -singlecompile -nopcompile -project=Unreal+QAGame+EngineTest -platform=WIn64+PS4+XboxOne+Switch -iterations=3")]
	[Help("preview", "List everything that will run but don't do it")]
	[Help("project=<name>", "Do tests on the specified project(s). E.g. -project=Unreal+FortniteGame+QAGame")]
	[Help("editor", "Time building the editor")]
	[Help("client", "Time building the for the specified platform(s)")]
	[Help("compile", "Time compiling the target")]
	[Help("singlecompile", "Do a single-file compile")]
	[Help("nopcompile", "Do a nothing-needs-compiled compile")]
	[Help("AllCompile", "Shorthand for -compile -singlecompile -nopcompile")]
	[Help("platform=<p1+p2>", "Specify the platform(s) to use for client compilation/cooking, if empty the local platform be used if -client or -cook is specified")]
	[Help("xge", "Do a pass with XGE / FASTBuild (default)")]
	[Help("noxge", "Do a pass without XGE / FASTBuild")]
	[Help("cores=X+Y+Z", "Do noxge builds with these processor counts (default is Environment.ProcessorCount)")]
	[Help("editor-startup", "Time launching the editor. Specify maps with -editor-startup=map1+map2")]
	[Help("editor-pie", "Time pie'ing for a project (only valid when -project is specified). Specify maps with -editor-pie=map1+map2")]
	[Help("editor-game", "Time launching the editor as -game (only valid when -project is specified). Specify maps with -editor-game=map1+map2")]
	[Help("AllEditor", "Shorthand for -editor-startup -editor-pie -editor-game")]
	[Help("editor-maps", "Map to Launch/PIE with (only valid when using a single project. Same as setting editor-pie=m1+m2, editor-startup=m1+m2 individually ")]
	[Help("cook", "Time cooking the project for the specified platform(s). Specify maps with -editor-cook=map1+map2")]
	[Help("cook-iterative", "Time an iterative cook for the specified platform(s) (will run a cook first if -cook is not specified). Specify maps with -editor-cook-iterative=map1+map2")]
	[Help("AllCook", "Shorthand for -cook -cook-iterative")]
	[Help("warmddc", "Cook / PIE with a warm DDC")]
	[Help("hotddc", "Cook / PIE with a hot local DDC (an untimed pre-run is performed)")]
	[Help("coldddc", "Cook / PIE with a cold local DDC (a temporary folder is used)")]
	[Help("coldddc-noshared", "Cook / PIE with a cold local DDC and no shared ddc ")]
	[Help("noshaderddc", "Cook / PIE with no shaders in the DDC")]
	[Help("AllDDC", "Shorthand for -coldddc -coldddc-noshared -noshaderddc -hotddc")]
	[Help("All", "Shorthand for -editor -client -AllCompile -AllEditor -AllCook -AllDDC")]
	[Help("editorxge", "Do a pass with XGE for editor DDC (default)")]
	[Help("noeditorxge", "Do a pass without XGE for editor DDC")]
	[Help("UBTArgs=", "Extra args to use when compiling. -UBTArgs=\"-foo\" -UBT2Args=\"-bar\" will run two compile passes with -foo and -bar")]
	[Help("CookArgs=", "Extra args to use when cooking. -CookArgs=\"-foo\" -Cook2Args=\"-bar\" will run two cook passes with -foo and -bar")]
	[Help("LaunchArgs=", "Extra args to use for launching. -LaunchArgs=\"-foo\" -Launch2Args=\"-bar\" will run two launch passes with -foo and -bar")]
	[Help("PIEArgs=", "Extra args to use for PIE. -PIEArgs=\"-foo\" -PIE2Args=\"-bar\" will run two PIE passes with -foo and -bar")]
	[Help("iterations=<n>", "How many times to perform each test)")]
	[Help("wait=<n>", "How many seconds to wait between each test)")]
	[Help("csv", "Name/path of file to write CSV results to. If empty the local machine name will be used")]
	[Help("noclean", "Don't build from clean. (Mostly just to speed things up when testing)")]
	[Help("nopostclean", "Don't clean artifacts after a task when building a lot of platforms/projects")]
	class BenchmarkBuild : BuildCommand
	{
		class BenchmarkOptions : BuildCommand
		{
			public bool Preview = false;

			public bool DoUETests = false;
			public IEnumerable<string> ProjectsToTest = Enumerable.Empty<string>();
			public IEnumerable<UnrealTargetPlatform> PlatformsToTest = Enumerable.Empty<UnrealTargetPlatform>();

			// building
			public bool DoBuildEditorTests = false;
			public bool DoBuildClientTests = false;
			public bool DoCompileTests = false;
			public bool DoNoCompileTests = false;
			public bool DoSingleCompileTests = false;

			public IEnumerable<int> CoresForLocalJobs = Enumerable.Empty<int>();

			// cooking
			public bool DoCookTests = false;
			public bool DoIterativeCookTests = false;

			// editor PIE tests
			public bool DoPIETests = false;

			// editor startup tests
			public bool DoLaunchEditorTests = false;
			public bool DoLaunchEditorGameTests = false;

			public IEnumerable<string> StartupMapList = Enumerable.Empty<string>();
			public IEnumerable<string> PIEMapList = Enumerable.Empty<string>();
			public IEnumerable<string> GameMapList = Enumerable.Empty<string>();
			public IEnumerable<string> CookMapList = Enumerable.Empty<string>();

			// misc
			public int				Iterations = 1;
			public UBTBuildOptions BuildOptions = UBTBuildOptions.None;
			public int				TimeBetweenTasks = 0;

			public List<string> UBTArgs = new List<string>();
			public List<string> CookArgs = new List<string>();
			public List<string> PIEArgs = new List<string>();
			public List<string> LaunchArgs = new List<string>();
			public string FileName = string.Format("{0}_Results.csv", Environment.MachineName);


			public SortedSet<XGETaskOptions> XGEOptions = new SortedSet<XGETaskOptions>();

			public SortedSet<DDCTaskOptions> DDCOptions = new SortedSet<DDCTaskOptions>();


			public void ParseParams(string[] InParams)
			{
				this.Params = InParams;

				bool AllThings = ParseParam("all");
				bool AllCompile = AllThings || ParseParam("AllCompile");
				bool AllCooks = AllThings || ParseParam("AllCook");
				bool AllEditor = AllThings || ParseParam("AllEditor");
				bool AllClient = AllThings || ParseParam("AllClient");
				bool AllDDC = AllThings || ParseParam("AllDDC");

				Preview = ParseParam("preview");
				DoUETests = AllThings || ParseParam("Unreal");

				// targets
				DoBuildEditorTests = AllThings | ParseParam("editor");
				DoBuildClientTests = AllThings | ParseParam("client");

				// compile tests
				DoCompileTests = AllCompile | ParseParam("compile");
				DoSingleCompileTests = AllCompile | ParseParam("singlecompile");
				DoNoCompileTests = AllCompile | ParseParam("nopcompile");				

				// cooking
				DoCookTests = AllCooks | ParseParam("cook");
				DoIterativeCookTests = AllCooks | ParseParam("cook-iterative");

				// editor launch tests
				DoLaunchEditorTests = AllEditor | ParseParam("editor-startup");
				DoLaunchEditorGameTests = AllEditor | ParseParam("editor-game");
				DoPIETests = AllEditor | ParseParam("editor-pie");

				var DDCCommandLineArgs = new Dictionary<string, DDCTaskOptions>
				{
					{"warmddc", DDCTaskOptions.WarmDDC },
					{"coldddc", DDCTaskOptions.ColdDDC },
					{"coldddc-noshared", DDCTaskOptions.ColdDDCNoShared },
					{"noshaderddc", DDCTaskOptions.NoShaderDDC },
					{"hotddc", DDCTaskOptions.HotDDC },
				};

				foreach (var K in DDCCommandLineArgs.Keys)
				{
					if (ParseParam(K))
					{
						DDCOptions.Add(DDCCommandLineArgs[K]);
					}
					else if (K != "warmddc" && AllDDC)
					{
						DDCOptions.Add(DDCCommandLineArgs[K]);
					}
				}

				var XGECommandLineArgs = new Dictionary<string, XGETaskOptions>
				{
					{"xge", XGETaskOptions.WithXGE },
					{"noxge", XGETaskOptions.NoXGE },
					{"noeditorxge", XGETaskOptions.NoEditorXGE },
					{"editorxge", XGETaskOptions.WithEditorXGE }
				};

				foreach (var K in XGECommandLineArgs.Keys)
				{
					if (ParseParam(K))
					{
						XGEOptions.Add(XGECommandLineArgs[K]);
					}
				}
			
				Preview = ParseParam("Preview");
				Iterations = ParseParamInt("Iterations", Iterations);
				TimeBetweenTasks = ParseParamInt("Wait", TimeBetweenTasks);

				// allow up to 10 UBT, Cook, PIE via -UBTArgs=etc, -UBT2Args=etc2, -CookArgs=etc -Cook2Args=etc2 etc
				for (int i = 0; i < 10; i++)
				{
					string PostFix = i == 0 ? "" : (i+1).ToString();

					// Parse CookArgs, Cook2Args etc
					string CookParam = ParseParamValue("Cook" + PostFix + "Args", null);

					if (CookParam != null)
					{
						CookArgs.Add(CookParam);
					}
					else if (i == 0)
					{
						// add a default for the first cook
						CookArgs.Add("");
					}

					// Parse PIEArgs, PIE22Args etc
					string PIEParam = ParseParamValue("PIE" + PostFix + "Args", null);

					if (PIEParam != null)
					{
						PIEArgs.Add(PIEParam);
					}
					else if (i == 0)
					{
						// add a default for the first PIE
						PIEArgs.Add("");
					}

					// Parse LaunchArgs, Launch2Args etc
					string LaunchParam = ParseParamValue("Launch" + PostFix + "Args", null);

					if (!string.IsNullOrEmpty(LaunchParam))
					{
						LaunchArgs.Add(LaunchParam);
					}
					else if (i == 0)
					{
						// add a default for the first launch
						LaunchArgs.Add("");
					}

					// Parse UBTArgs, UBT2Args etc
					string UBTParam = ParseParamValue("UBT" + PostFix + "Args", null);

					if (!string.IsNullOrEmpty(UBTParam))
					{
						UBTArgs.Add(UBTParam);
					}
					else if (i == 0)
					{
						// add a default for the first compile
						UBTArgs.Add("");
					}
				}

				FileName = ParseParamValue("csv", FileName);

				// Parse the project arg
				{
					string ProjectsArg = ParseParamValue("project", null);
					ProjectsArg = ParseParamValue("projects", ProjectsArg);

					// Look at the project argument and verify it's a valid uproject
					if (!string.IsNullOrEmpty(ProjectsArg))
					{
						ProjectsToTest = ProjectsArg.Split(new[] { '+', ',' }, StringSplitOptions.RemoveEmptyEntries);
					}
				}

				// Parse and validate platform list from arguments
				{
					string PlatformArg = ParseParamValue("platform", "");
					PlatformArg = ParseParamValue("platforms", PlatformArg);

					if (!string.IsNullOrEmpty(PlatformArg))
					{
						List<UnrealTargetPlatform> ClientPlatforms = new List<UnrealTargetPlatform>();

						var PlatformList = PlatformArg.Split(new[] { '+', ',' }, StringSplitOptions.RemoveEmptyEntries);

						foreach (var Platform in PlatformList)
						{
							UnrealTargetPlatform PlatformEnum;
							if (!UnrealTargetPlatform.TryParse(Platform, out PlatformEnum))
							{
								throw new AutomationException("{0} is not a valid Unreal Platform", Platform);
							}

							ClientPlatforms.Add(PlatformEnum);
						}

						PlatformsToTest = ClientPlatforms;
					}
					else
					{
						PlatformsToTest = new[] { BuildHostPlatform.Current.Platform };
					}
				}

				//  clean by default
				if (!ParseParam("noclean"))
				{
					BuildOptions |= UBTBuildOptions.PreClean;
				}
				// post-clean if we're building a lot of stuff
				if (!(ParseParam("nopostclean") && !ParseParam("noclean"))
					/*&& (PlatformsToTest.Count() > 1 || ProjectsToTest.Count() > 1)*/)
				{
					BuildOptions |= UBTBuildOptions.PostClean;
					Log.TraceInformation("Building multiple platforms. Will clean each platform after build step to save space. (use -nopostclean to prevent this)");
				}

				// parse processor args
				{
					string ProcessorArg = ParseParamValue("cores", "");

					if (!string.IsNullOrEmpty(ProcessorArg))
					{
						var ProcessorList = ProcessorArg.Split(new[] { '+', ',' }, StringSplitOptions.RemoveEmptyEntries);

						CoresForLocalJobs = ProcessorList.Select(P => Convert.ToInt32(P));
					}
				}

				Func<string, string[]> ParseMapList = (string ArgName) =>
				{
					string ArgValue = ParseParamValue(ArgName, "");

					if (!string.IsNullOrEmpty(ArgValue))
					{
						// don't remove empty entries so people can get the project default and map2 via +map2
						return ArgValue.Split(new[] { '+', ',' }, StringSplitOptions.None);
					}

					return new string[] { };
				};

				// parse map args
				{
					// master arg that sets all three
					var EditorMaps = ParseMapList("editor-maps");

					if (EditorMaps.Any())
					{
						StartupMapList = EditorMaps;
						PIEMapList = EditorMaps;
						GameMapList = EditorMaps;
						CookMapList = EditorMaps;
					}
					else
					{
						StartupMapList = ParseMapList("editor-startup");
						PIEMapList = ParseMapList("editor-pie");
						GameMapList = ParseMapList("editor-game");
						CookMapList = ParseMapList("cook");
						CookMapList = ParseMapList("cook-iterative");
					}
				}

				bool DefaultToXGE = BenchmarkBuildTask.SupportsAcceleration;

				// If they specified cores, ensure NoXGE is on
				if (CoresForLocalJobs.Any())
				{
					XGEOptions.Add(XGETaskOptions.NoXGE);
				}

				if (!DDCOptions.Any())
				{
					DDCOptions.Add(DDCTaskOptions.WarmDDC);
				}

				// If the user provided no XGE / NoXGE compile flags, then give them a default
				if (!XGEOptions.Contains(XGETaskOptions.WithXGE)
					&& !XGEOptions.Contains(XGETaskOptions.NoXGE))
				{
					XGEOptions.Add(DefaultToXGE ? XGETaskOptions.WithXGE : XGETaskOptions.NoXGE);
				}

				// If the user provided no XGE / NoXGE editor flags, then give them a default
				if (!XGEOptions.Contains(XGETaskOptions.WithEditorXGE)
					&& !XGEOptions.Contains(XGETaskOptions.NoEditorXGE))
				{
					XGEOptions.Add(DefaultToXGE ? XGETaskOptions.WithEditorXGE : XGETaskOptions.NoEditorXGE);
				}

				// Make sure there's a default here
				if (!CoresForLocalJobs.Any())
				{
					CoresForLocalJobs = new int[] { 0 };
				}

				// sanity
				if (!BenchmarkBuildTask.SupportsAcceleration)
				{
					if (XGEOptions.Contains(XGETaskOptions.WithXGE) 
						|| XGEOptions.Contains(XGETaskOptions.WithEditorXGE))
					{
						Log.TraceWarning("XGE requested but is not available. Removing XGE options");
						XGEOptions.Remove(XGETaskOptions.WithXGE);
						XGEOptions.Remove(XGETaskOptions.WithEditorXGE);
					}					
				}			
			}
		}

		struct BenchmarkResult
		{
			public TimeSpan TaskTime { get; set; }
			public bool Failed { get; set; }
		}
		
		public BenchmarkBuild()
		{
		}

		public override ExitCode Execute()
		{
			BenchmarkOptions Options = new BenchmarkOptions();
			Options.ParseParams(this.Params);

			List<BenchmarkTaskBase> Tasks = new List<BenchmarkTaskBase>();

			Dictionary<BenchmarkTaskBase, List<BenchmarkResult>> Results = new Dictionary<BenchmarkTaskBase, List<BenchmarkResult>>();

			for (int ProjectIndex = 0; ProjectIndex < Options.ProjectsToTest.Count(); ProjectIndex++)
			{
				string Project = Options.ProjectsToTest.ElementAt(ProjectIndex);

				FileReference ProjectFile = ProjectUtils.FindProjectFileFromName(Project);

				if (ProjectFile == null && !Project.Equals("Unreal", StringComparison.OrdinalIgnoreCase))
				{
					throw new AutomationException("Could not find project file for {0}", Project);
				}

				bool TargetIsClientBuild = ProjectSupportsClientBuild(ProjectFile);


				ProjectTargetInfo EditorTarget = new ProjectTargetInfo(ProjectFile, BuildHostPlatform.Current.Platform, TargetIsClientBuild);

				// Do compile tests of editor and platforms

				if (Options.DoBuildEditorTests)
				{
					Tasks.AddRange(AddBuildTests(ProjectFile, BuildHostPlatform.Current.Platform, "Editor", Options));
				}

				if (Options.DoBuildClientTests)
				{
					foreach (var ClientPlatform in Options.PlatformsToTest)
					{
						ProjectTargetInfo PlatformTarget = new ProjectTargetInfo(ProjectFile, ClientPlatform, TargetIsClientBuild);


						// do build tests
						Tasks.AddRange(AddBuildTests(ProjectFile, ClientPlatform, TargetIsClientBuild ? "Client" : "Game", Options));
					}
				}

				var XGEEditorOptions = Options.XGEOptions.Where(Opt => (Opt == XGETaskOptions.WithEditorXGE || Opt == XGETaskOptions.NoEditorXGE));

				List<BenchmarkTaskBase> EditorTasks = new List<BenchmarkTaskBase>();

				if (Options.DoLaunchEditorTests)
				{
					EditorTasks.AddRange(AddEditorTests<BenchmarkEditorStartupTask>(EditorTarget, Options.StartupMapList, Options.LaunchArgs, Options.CoresForLocalJobs, XGEEditorOptions, Options.DDCOptions, EditorTasks.Any()));
				}

				// do PIE tests, so long as there's a project
				if (Options.DoPIETests && EditorTarget.ProjectFile != null)
				{
					EditorTasks.AddRange(AddEditorTests<BenchmarPIEEditorTask>(EditorTarget, Options.PIEMapList, Options.PIEArgs, Options.CoresForLocalJobs, XGEEditorOptions, Options.DDCOptions, EditorTasks.Any()));
				}

				// do PIE tests, so long as there's a project
				if (Options.DoLaunchEditorGameTests && EditorTarget.ProjectFile != null)
				{
					EditorTasks.AddRange(AddEditorTests<BenchmarkEditorGameTask>(EditorTarget, Options.GameMapList, Options.LaunchArgs, Options.CoresForLocalJobs, XGEEditorOptions, Options.DDCOptions, EditorTasks.Any()));
				}

				// cook tests
				foreach (var ClientPlatform in Options.PlatformsToTest)
				{
					ProjectTargetInfo PlatformTarget = new ProjectTargetInfo(ProjectFile, ClientPlatform, TargetIsClientBuild);

					// do cook tests,. so long as there's a project
					if (Options.DoCookTests && PlatformTarget.ProjectFile != null)
					{
						EditorTasks.AddRange(AddEditorTests<BenchmarkCookTask>(PlatformTarget, Options.CookMapList, Options.CookArgs, Options.CoresForLocalJobs, XGEEditorOptions, Options.DDCOptions, EditorTasks.Any()));
					}

					// do cook tests,. so long as there's a project
					if (Options.DoIterativeCookTests && PlatformTarget.ProjectFile != null)
					{
						int[] CoreLimit = { 0 };
						XGETaskOptions[] DefaultXGE = { BenchmarkBuildTask.SupportsAcceleration ? XGETaskOptions.WithEditorXGE : XGETaskOptions.NoEditorXGE };
						DDCTaskOptions[] WarmDDC = { DDCTaskOptions.WarmDDC };

						// If not running any cooks run a single warm one so we can get iterative values 
						if (!Options.DoCookTests)
						{
							var WarmupTasks = AddEditorTests<BenchmarkCookTask>(PlatformTarget, Options.CookMapList, Options.CookArgs, CoreLimit, DefaultXGE, WarmDDC, EditorTasks.Any());
							WarmupTasks.ToList().ForEach(T => T.SkipReport = true);
							EditorTasks.AddRange(WarmupTasks);
						}

						EditorTasks.AddRange(AddEditorTests<BenchmarkIterativeCookTask>(PlatformTarget, Options.CookMapList, Options.CookArgs, CoreLimit, XGEEditorOptions, WarmDDC, EditorTasks.Any()));
					}
				}

				Tasks.AddRange(EditorTasks);
			}

			Log.TraceInformation("Will execute tasks:");

			foreach (var Task in Tasks)
			{
				Log.TraceInformation("{0}", Task.FullName);
			}

			if (!Options.Preview)
			{
				// create results lists
				foreach (var Task in Tasks)
				{
					Results.Add(Task, new List<BenchmarkResult>());
				}

				DateTime StartTime = DateTime.Now;

				for (int i = 0; i < Options.Iterations; i++)
				{
					foreach (var Task in Tasks)
					{
						Log.TraceInformation("Starting task {0} (Pass {1})", Task.FullName, i + 1);

						Task.Run();

						Log.TraceInformation("Task {0} took {1}", Task.FullName, Task.TaskTime.ToString(@"hh\:mm\:ss"));

						if (Task.Failed)
						{
							Log.TraceError("Task failed! Benchmark time may be inaccurate.");
						}

						if (Task.SkipReport)
						{
							Log.TraceInformation("Skipping reporting of {0}", Task.FullName);
						}
						else
						{
							Results[Task].Add(new BenchmarkResult
							{
								TaskTime = Task.TaskTime,
								Failed = Task.Failed
							});

							// write results so far
							WriteCSVResults(Options.FileName, Tasks, Results);
						}

						Log.TraceInformation("Waiting {0} secs until next task", Options.TimeBetweenTasks);
						Thread.Sleep(Options.TimeBetweenTasks * 1000);
					}
				}

				Log.TraceInformation("**********************************************************************");
				Log.TraceInformation("Test Results:");

				foreach (var Task in Tasks)
				{
					string TimeString = "";

					if (Task.SkipReport)
					{
						continue;
					}

					IEnumerable<BenchmarkResult> TaskResults = Results[Task];

					foreach (var Result in TaskResults)
					{
						if (TimeString.Length > 0)
						{
							TimeString += ", ";
						}

						if (Result.Failed)
						{
							TimeString += "Failed";
						}
						else
						{
							TimeString += Result.TaskTime.ToString(@"hh\:mm\:ss");
						}
					}

					var AvgTimeString = "";

					if (TaskResults.Count() > 1)
					{
						var AvgTime = new TimeSpan(TaskResults.Select(R => R.TaskTime).Sum(T => T.Ticks) / TaskResults.Count());

						AvgTimeString = string.Format(" (Avg: {0})", AvgTime.ToString(@"hh\:mm\:ss"));
					}

					Log.TraceInformation("Task {0}:\t\t{1}{2}", Task.FullName, TimeString, AvgTimeString);
				}
				Log.TraceInformation("**********************************************************************");

				TimeSpan Elapsed = DateTime.Now - StartTime;

				Log.TraceInformation("Total benchmark time: {0}", Elapsed.ToString(@"hh\:mm\:ss"));

				WriteCSVResults(Options.FileName, Tasks, Results);
			}

			return ExitCode.Success;
		}

		IEnumerable<BenchmarkTaskBase> AddBuildTests(FileReference InProjectFile, UnrealTargetPlatform InPlatform, string InTargetName, BenchmarkOptions InOptions)
		{
			List<BenchmarkTaskBase> NewTasks = new List<BenchmarkTaskBase>();

			if (InOptions.DoCompileTests)
			{
				IEnumerable<string> UBTArgList = InOptions.UBTArgs.Any() ? InOptions.UBTArgs : new[] { "" };

				if (InOptions.XGEOptions.Contains(XGETaskOptions.WithXGE))
				{
					foreach (string UBTArgs in UBTArgList)
					{
						NewTasks.Add(new BenchmarkBuildTask(InProjectFile, InTargetName, InPlatform, XGETaskOptions.WithXGE, UBTArgs, 0, InOptions.BuildOptions));
					}
				}

				if (InOptions.XGEOptions.Contains(XGETaskOptions.NoXGE))
				{
					foreach (int ProcessorCount in InOptions.CoresForLocalJobs)
					{
						foreach (string UBTArgs in UBTArgList)
						{
							NewTasks.Add(new BenchmarkBuildTask(InProjectFile, InTargetName, InPlatform, XGETaskOptions.NoXGE, UBTArgs, ProcessorCount, InOptions.BuildOptions));
						}
					}
				}
			}

			// If the user requested a single-compile /nop-compile and we haven't built anything, add one now
			if ((InOptions.DoSingleCompileTests || InOptions.DoNoCompileTests)
				&& NewTasks.Any() == false)
			{
				NewTasks.Add(new BenchmarkBuildTask(InProjectFile, InTargetName, InPlatform, 
								BenchmarkBuildTask.SupportsAcceleration ? XGETaskOptions.WithXGE : XGETaskOptions.NoXGE,
								"", 0));
			}

			if (InOptions.DoSingleCompileTests)
			{
				// note, don't clean since we build normally then build again
				NewTasks.Add(new BenchmarkSingleCompileTask(InProjectFile, InTargetName, InPlatform, InOptions.XGEOptions.First()));
			}

			if (InOptions.DoNoCompileTests)
			{
				// note, don't clean since we build normally then build a single file
				NewTasks.Add(new BenchmarkNopCompileTask(InProjectFile, InTargetName, InPlatform, InOptions.XGEOptions.First()));
			}

			// clean stuff if we're doing compilation tasks that aren't the editor as we can use masses of disk space...
			if (InOptions.DoCompileTests)
			{
				if (InOptions.BuildOptions.HasFlag(UBTBuildOptions.PostClean) && !InTargetName.Equals("Editor", StringComparison.OrdinalIgnoreCase))
				{
					var Task = new BenchmarkCleanBuildTask(InProjectFile, InTargetName, InPlatform);
					Task.SkipReport = true;
					NewTasks.Add(Task);
				}
			}

			return NewTasks;
		}


		IEnumerable<BenchmarkTaskBase> AddEditorTests<T>(ProjectTargetInfo InTargetInfo, IEnumerable<string> InMaps, IEnumerable<string> InArgVariations, IEnumerable<int> CoreVariations, IEnumerable<XGETaskOptions> InXGEOptions, IEnumerable<DDCTaskOptions> InDDCOptions, bool SkipBuildEditor)
			where T : BenchmarkEditorTaskBase
		{
			if (InTargetInfo == null)
			{			
				return Enumerable.Empty<BenchmarkTaskBase>();
			}

			List<BenchmarkTaskBase> NewTasks = new List<BenchmarkTaskBase>();

			IEnumerable<string> ArgVariations = InArgVariations.Any() ? InArgVariations : new List<string> { "" };
			IEnumerable<string> MapVariations = InMaps.Any() ? InMaps : new List<string> { "" };

			// If the user is running a hotddc test and there's only one type, run a warm pass first
			if (InDDCOptions.Contains(DDCTaskOptions.HotDDC) && InDDCOptions.Count() == 1)
			{
				ProjectTaskOptions TaskOptions = new ProjectTaskOptions(DDCTaskOptions.WarmDDC, InXGEOptions.First(), "", MapVariations.First(), 0);
				var NewTask = Activator.CreateInstance(typeof(T), new object[] { InTargetInfo, TaskOptions, SkipBuildEditor }) as BenchmarkEditorTaskBase;
				NewTask.SkipReport = true;
				NewTasks.Add(NewTask);

				// don't build the editor again
				SkipBuildEditor = true;
			}

			foreach (string Args in ArgVariations)
			{
				foreach (var Map in MapVariations)
				{
					foreach (XGETaskOptions XGEOption in InXGEOptions)
					{
						bool bCoreVariations = XGEOption == XGETaskOptions.NoEditorXGE && CoreVariations.Any();
						IEnumerable<int> CoresForJobs = bCoreVariations ? CoreVariations : new int[] { 0 };

						foreach (var CoreLimit in CoresForJobs)
						{			
							// DDC must be last expansion as things are ordered with assumptions
							foreach (var DDCOption in InDDCOptions)
							{
								ProjectTaskOptions TaskOptions = new ProjectTaskOptions(DDCOption, XGEOption, Args, Map, CoreLimit);
								var NewTask = Activator.CreateInstance(typeof(T), new object[] { InTargetInfo, TaskOptions, SkipBuildEditor }) as BenchmarkTaskBase;
								NewTasks.Add(NewTask);

								// don't build the editor again
								SkipBuildEditor = true;
							}
						}
					}
				}
			}	

			return NewTasks;
		}

		/// <summary>
		/// Writes our current result to a CSV file. It's expected that this function is called multiple times so results are
		/// updated as we go
		/// </summary>
		void WriteCSVResults(string InFileName, IEnumerable<BenchmarkTaskBase> InTasks, Dictionary<BenchmarkTaskBase, List<BenchmarkResult>> InResults)
		{
			Log.TraceInformation("Writing results to {0}", InFileName);

			try
			{
				List<string> Lines = new List<string>();

				// first line is machine name,CPU count,Iteration 1, Iteration 2 etc
				string FirstLine = string.Format("{0},{1} Cores,StartTime", Environment.MachineName, Environment.ProcessorCount);

				if (InTasks.Count() > 0)
				{
					int Iterations = InResults[InTasks.First()].Count();

					if (Iterations > 0)
					{
						for (int i = 0; i < Iterations; i++)
						{
							FirstLine += ",";
							FirstLine += string.Format("Iteration {0}", i + 1);
						}

						if (Iterations > 1)
						{
							FirstLine += ",Average";
						}
					}
				}

				Lines.Add(FirstLine);

				foreach (var Task in InTasks.Where(T => T.SkipReport == false))
				{
					// start with Name, StartTime
					string Line = string.Format("{0},{1},{2}", Task.ProjectName, Task.TaskNameWithModifiers, Task.StartTime.ToString("yyyy-dd-MM HH:mm:ss"));

					IEnumerable<BenchmarkResult> TaskResults = InResults[Task];

					bool DidFail = false;

					// now append all iteration times
					foreach (BenchmarkResult Result in TaskResults)
					{
						Line += ",";
						if (Result.Failed)
						{
							Line += "FAILED";
							DidFail = true;
						}
						else
						{
							Line += Result.TaskTime.ToString(@"hh\:mm\:ss");
						}
					}

					if (TaskResults.Count() > 1)
					{
						if (DidFail)
						{
							Line += ",FAILED";
						}
						else
						{
							var AvgTime = new TimeSpan(TaskResults.Select(R => R.TaskTime).Sum(T => T.Ticks) / InResults[Task].Count());
							Line += "," + AvgTime.ToString(@"hh\:mm\:ss");
						}
					}

					Lines.Add(Line);
				}

				File.WriteAllLines(InFileName, Lines.ToArray());
			}
			catch (Exception Ex)
			{
				Log.TraceError("Failed to write CSV to {0}. {1}", InFileName, Ex);
			}
		}

		/// <summary>
		/// Returns true/false based on whether the project supports a client configuration
		/// </summary>
		/// <param name="ProjectName"></param>
		/// <returns></returns>
		bool ProjectSupportsClientBuild(FileReference InProjectFile)
		{
			if (InProjectFile == null)
			{
				// UE
				return true;
			}

			ProjectProperties Properties = ProjectUtils.GetProjectProperties(InProjectFile);

			return Properties.Targets.Where(T => T.Rules.Type == TargetType.Client).Any();
		}
	}
}
