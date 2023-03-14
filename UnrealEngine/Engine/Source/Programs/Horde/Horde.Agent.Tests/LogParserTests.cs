// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using EpicGames.Core;
using EpicGames.Perforce;
using Horde.Agent.Parser;
using Horde.Agent.Utility;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests
{
	[TestClass]
	public class LogParserTests
	{
		const string LogLine = "LogLine";

		class LoggerCapture : ILogger
		{
			int _logLineIndex;

			public List<LogEvent> _events = new List<LogEvent>();

			public IDisposable BeginScope<TState>(TState state) => null!;

			public bool IsEnabled(LogLevel logLevel) => true;

			public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
			{
				LogEvent logEvent = LogEvent.Read(JsonLogEvent.FromLoggerState(logLevel, eventId, state, exception, formatter).Data.Span);
				if (logEvent.Level != LogLevel.Information || logEvent.Id != default || logEvent.Properties != null)
				{
					KeyValuePair<string, object>[] items = new[] { new KeyValuePair<string, object>(LogLine, _logLineIndex) };
					logEvent.Properties = (logEvent.Properties == null) ? items : Enumerable.Concat(logEvent.Properties, items);
					_events.Add(logEvent);
				}

				_logLineIndex++;
			}
		}

		[TestMethod]
		public void StructuredOutputMatcher()
		{
			string[] lines =
			{
				new LogEvent(DateTime.UtcNow, LogLevel.Information, new EventId(123), "Hello 123", null, null, null).ToJson(),
				new LogEvent(DateTime.UtcNow, LogLevel.Information, default, "Building 43 projects (see Log \u0027Engine/Programs/AutomationTool/Saved/Logs/Log.txt\u0027 for more details)", null, null, null).ToJson(),
				new LogEvent(DateTime.UtcNow, LogLevel.Warning, default, " Restore...", null, null, null).ToJson(),
				new LogEvent(DateTime.UtcNow, LogLevel.Error, default, " Build...", null, null, null).ToJson(),
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(3, logEvents.Count);

			int idx = 0;

			LogEvent logEvent = logEvents[idx++];
			Assert.AreEqual(LogLevel.Information, logEvent.Level);
			Assert.AreEqual(new EventId(123), logEvent.Id);
			Assert.AreEqual("Hello 123", logEvent.Message);

			logEvent = logEvents[idx++];
			Assert.AreEqual(LogLevel.Warning, logEvent.Level);
			Assert.AreEqual(new EventId(0), logEvent.Id);
			Assert.AreEqual(" Restore...", logEvent.Message);

			logEvent = logEvents[idx++];
			Assert.AreEqual(LogLevel.Error, logEvent.Level);
			Assert.AreEqual(new EventId(0), logEvent.Id);
			Assert.AreEqual(" Build...", logEvent.Message);
		}

		[TestMethod]
		public void ExitCodeEventMatcher()
		{
			string[] lines =
			{
				@"Took 620.820352s to run UE4Editor-Cmd.exe, ExitCode=777003",
				@"Editor terminated with exit code 777003 while running GenerateSkinSwapDetections for D:\Build\++UE5\Sync\FortniteGame\FortniteGame.uproject; see log D:\Build\++UE5\Sync\Engine\Programs\AutomationTool\Saved\Logs\GenerateSkinSwapDetections-2020.08.18-21.47.07.txt",
				@"Error executing D:\build\++Fortnite\Sync\Engine\Build\Windows\link - filter\link - filter.exe(tool returned code: STATUS_ACCESS_VIOLATION)",
				@"Error executing C:\Windows\system32\cmd.exe (tool returned code: 1)",
				@"AutomationTool exiting with ExitCode=1 (Error_Unknown)",
				@"BUILD FAILED"
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 1, 3, LogLevel.Error, KnownLogEvents.ExitCode);
		}

		[TestMethod]
		public void ExitCodeEventMatcher2()
		{
			string[] lines =
			{
				@"1 error generated.",
				@"",
				@"Error executing d:\build\AutoSDK\Sync\HostWin64\Android\-24\ndk\21.4.7075529\toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe (tool returned code: 1)",
				@"AnimNode_BlendListByInt.gen.cpp [arm64] (0:22.43 at +2:47)",
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 2, 1, LogLevel.Error, KnownLogEvents.ExitCode);
		}

		[TestMethod]
		public void CrashEventMatcher()
		{
			string[] lines =
			{
				@"   LogOutputDevice: Error: begin: stack for UAT",
				@"   LogOutputDevice: Error: === Handled ensure: ===",
				@"   LogOutputDevice: Error:",
				@"   LogOutputDevice: Error: Ensure condition failed: Foo [File:D:/build/++UE5/Sync/Foo.cpp] [Line: 233]",
				@"   LogOutputDevice: Error:",
				@"   LogOutputDevice: Error: Stack:",
				@"   LogOutputDevice: Error: [Callstack] 0x00007ff6b68a035d UnrealEditor-Cmd.exe!GuardedMain() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Launch.cpp:129]",
				@"   LogOutputDevice: Error: [Callstack] 0x00007ff6b68a05fa UnrealEditor-Cmd.exe!GuardedMainWrapper() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:142]",
				@"   LogOutputDevice: Error: [Callstack] 0x00007ff6b68b522d UnrealEditor-Cmd.exe!WinMain() [D:\build\++UE5\Sync\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:273]",
				@"   LogOutputDevice: Error: [Callstack] 0x00007ff6b68b7522 UnrealEditor-Cmd.exe!__scrt_common_main_seh() [D:\a01\_work\9\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:288]",
				@"   LogOutputDevice: Error: [Callstack] 0x00007ff96a517974 KERNEL32.DLL!UnknownFunction []",
				@"   LogOutputDevice: Error: [Callstack] 0x00007ff96d14a271 ntdll.dll!UnknownFunction []",
				@"   LogOutputDevice: Error:",
				@"   LogOutputDevice: Error: end: stack for UAT"
			};

			{
				List<LogEvent> logEvents = Parse(String.Join("\n", lines));
				CheckEventGroup(logEvents, 0, 14, LogLevel.Error, KnownLogEvents.Engine_Crash);
			}

			{
				List<LogEvent> logEvents = Parse(String.Join("\n", lines).Replace("Error:", "Warning:", StringComparison.Ordinal));
				CheckEventGroup(logEvents, 0, 14, LogLevel.Warning, KnownLogEvents.Engine_Crash);
			}
		}

		[TestMethod]
		public void CrashEventMatcher2()
		{
			string[] lines =
			{
				@"Took 620.820352s to run UE4Editor-Cmd.exe, ExitCode=3",
				@"Took 620.820352s to run UE4Editor-Cmd.exe, ExitCode=30",
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 0, 1, LogLevel.Error, KnownLogEvents.AutomationTool_CrashExitCode);
		}

		[TestMethod]
		public void SymbolStripSpuriousEventMatcher()
		{
			// Symbol stripping error
			{
				string[] lines =
				{
					@"Stripping symbols: d:\build\++UE5\Sync\Engine\Plugins\Runtime\GoogleVR\GoogleVRController\Binaries\Win64\UnrealEditor-GoogleVRController.pdb -> d:\build\++UE5\Sync\ArchiveForUGS\Staging\Engine\Plugins\Runtime\GoogleVR\GoogleVRController\Binaries\Win64\UnrealEditor-GoogleVRController.pdb",
					@"ERROR: Error: EC_OK -- ??",
					@"ERROR:",
					@"Stripping symbols: d:\build\++UE5\Sync\Engine\Plugins\Runtime\GoogleVR\GoogleVRHMD\Binaries\Win64\UnrealEditor-GoogleVRHMD.pdb -> d:\build\++UE5\Sync\ArchiveForUGS\Staging\Engine\Plugins\Runtime\GoogleVR\GoogleVRHMD\Binaries\Win64\UnrealEditor-GoogleVRHMD.pdb",
				};

				List<LogEvent> logEvents = Parse(String.Join("\n", lines));
				CheckEventGroup(logEvents, 1, 1, LogLevel.Information, KnownLogEvents.Systemic_PdbUtil);
			}
		}

		[TestMethod]
		public void CsCompileEventMatcher()
		{
			// C# compile error
			{
				string[] lines =
				{
					@"  GenerateSigningRequestDialog.cs(22,7): error CS0246: The type or namespace name 'Org' could not be found (are you missing a using directive or an assembly reference?) [c:\Horde\Engine\Source\Programs\IOS\iPhonePackager\iPhonePackager.csproj]",
					@"  Utilities.cs(16,7): error CS0246: The type or namespace name 'Org' could not be found (are you missing a using directive or an assembly reference?) [c:\Horde\Engine\Source\Programs\IOS\iPhonePackager\iPhonePackager.csproj]"
				};

				List<LogEvent> logEvents = Parse(String.Join("\n", lines));
				Assert.AreEqual(2, logEvents.Count);

				CheckEventGroup(logEvents.Slice(0, 1), 0, 1, LogLevel.Error, KnownLogEvents.Compiler);
				Assert.AreEqual("CS0246", logEvents[0].GetProperty("code").ToString());
				Assert.AreEqual("22", logEvents[0].GetProperty("line").ToString());

				LogValue fileProperty1 = (LogValue)logEvents[0].GetProperty("file");
				Assert.AreEqual(@"GenerateSigningRequestDialog.cs", fileProperty1.Text);
				Assert.AreEqual(@"SourceFile", fileProperty1.Type);
				Assert.AreEqual(@"Engine/Source/Programs/IOS/iPhonePackager/GenerateSigningRequestDialog.cs", fileProperty1.Properties!["relativePath"].ToString());
				Assert.AreEqual(@"//UE4/Main/Engine/Source/Programs/IOS/iPhonePackager/GenerateSigningRequestDialog.cs@12345", fileProperty1.Properties["depotPath"].ToString());

				CheckEventGroup(logEvents.Slice(1, 1), 1, 1, LogLevel.Error, KnownLogEvents.Compiler);
				Assert.AreEqual("CS0246", logEvents[1].GetProperty("code").ToString());
				Assert.AreEqual("16", logEvents[1].GetProperty("line").ToString());

				LogValue fileProperty2 = (LogValue)logEvents[1].GetProperty("file");
				Assert.AreEqual(@"Utilities.cs", fileProperty2.Text);
				Assert.AreEqual(@"SourceFile", fileProperty2.Type);
				Assert.AreEqual(@"Engine/Source/Programs/IOS/iPhonePackager/Utilities.cs", fileProperty2.Properties!["relativePath"].ToString());
				Assert.AreEqual(@"//UE4/Main/Engine/Source/Programs/IOS/iPhonePackager/Utilities.cs@12345", fileProperty2.Properties["depotPath"].ToString());
			}
		}

		[TestMethod]
		public void CsCompileEventMatcher2()
		{
			// C# compile error
			{
				string[] lines =
				{
					@"  Configuration\TargetRules.cs(1497,58): warning CS8625: Cannot convert null literal to non-nullable reference type. [C:\Horde\Engine\Source\Programs\UnrealBuildTool\UnrealBuildTool.csproj]",
				};

				List<LogEvent> events = Parse(String.Join("\n", lines));
				CheckEventGroup(events, 0, 1, LogLevel.Warning, KnownLogEvents.Compiler);

				Assert.AreEqual("CS8625", events[0].GetProperty("code").ToString());
				Assert.AreEqual("1497", events[0].GetProperty("line").ToString());

				LogValue fileProperty = (LogValue)events[0].GetProperty("file");
				Assert.AreEqual(@"Configuration\TargetRules.cs", fileProperty.Text);
				Assert.AreEqual(@"SourceFile", fileProperty.Type);
				Assert.AreEqual(@"Engine/Source/Programs/UnrealBuildTool/Configuration/TargetRules.cs", fileProperty.Properties!["relativePath"].ToString());
				Assert.AreEqual(@"//UE4/Main/Engine/Source/Programs/UnrealBuildTool/Configuration/TargetRules.cs@12345", fileProperty.Properties!["depotPath"].ToString());
			}
		}

		[TestMethod]
		public void CsCompileEventMatcher3()
		{
			// C# compile error from UBT
			{
				string[] lines =
				{
					@"  ERROR: c:\Horde\Engine\Source\Runtime\CoreOnline\CoreOnline.Build.cs(4,7): error CS0246: The type or namespace name 'Tools' could not be found (are you missing a using directive or an assembly reference?)",
					@"  WARNING: C:\horde\Engine\Source\Runtime\CoreOnline\CoreOnline.Build.cs(4,7): warning CS0246: The type or namespace name 'Tools' could not be found (are you missing a using directive or an assembly reference?)"
				};

				List<LogEvent> logEvents = Parse(String.Join("\n", lines));
				Assert.AreEqual(2, logEvents.Count);

				CheckEventGroup(logEvents.Slice(0, 1), 0, 1, LogLevel.Error, KnownLogEvents.Compiler);
				Assert.AreEqual("CS0246", logEvents[0].GetProperty("code").ToString());
				Assert.AreEqual("4", logEvents[0].GetProperty("line").ToString());

				LogValue fileProperty = (LogValue)logEvents[0].GetProperty("file");
				Assert.AreEqual(@"c:\Horde\Engine\Source\Runtime\CoreOnline\CoreOnline.Build.cs", fileProperty.Text);
				Assert.AreEqual(@"SourceFile", fileProperty.Type);
				Assert.AreEqual(@"Engine/Source/Runtime/CoreOnline/CoreOnline.Build.cs", fileProperty.Properties!["relativePath"].ToString());
				Assert.AreEqual(@"//UE4/Main/Engine/Source/Runtime/CoreOnline/CoreOnline.Build.cs@12345", fileProperty.Properties!["depotPath"].ToString());

				CheckEventGroup(logEvents.Slice(1, 1), 1, 1, LogLevel.Warning, KnownLogEvents.Compiler);
				Assert.AreEqual("CS0246", logEvents[1].GetProperty("code").ToString());
				Assert.AreEqual("4", logEvents[1].GetProperty("line").ToString());

				LogValue fileProperty1 = logEvents[1].GetProperty<LogValue>("file");
				Assert.AreEqual(@"C:\horde\Engine\Source\Runtime\CoreOnline\CoreOnline.Build.cs", fileProperty1.Text);
				Assert.AreEqual(@"SourceFile", fileProperty1.Type);
				Assert.AreEqual(@"Engine/Source/Runtime/CoreOnline/CoreOnline.Build.cs", fileProperty1.Properties!["relativePath"].ToString());
				Assert.AreEqual(@"//UE4/Main/Engine/Source/Runtime/CoreOnline/CoreOnline.Build.cs@12345", fileProperty1.Properties!["depotPath"].ToString());
			}
		}

		[TestMethod]
		public void HttpEventMatcher()
		{
			string[] lines =
			{
				@"WARNING: Failed to resolve binaries for artifact fe1b277b-7751-4a52-8059-ec3f943811de:xsx with error: fe1b277b-7751-4a52-8059-ec3f943811de:xsx Failed.Unexpected error retrieving response.BaseUrl = https://content-service-latest-gamedev.cdae.dev.use1a.on.epicgames.com/api. Status = Timeout. McpConfig = ValkyrieDevLatest."
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 0, 1, LogLevel.Warning, KnownLogEvents.Generic);
		}

		[TestMethod]
		public void MicrosoftEventMatcher()
		{
			// Generic Microsoft errors which can be parsed by visual studio
			{
				string[] lines =
				{
					@" C:\Horde\Foo\Bar.txt(20): warning TL2012: Some error message",
					@" C:\Horde\Foo\Bar.txt(20, 30) : warning TL2034: Some error message",
					@" CSC : error CS2012: Cannot open 'D:\Build\++UE4\Sync\Engine\Source\Programs\Enterprise\Datasmith\DatasmithRevitExporter\Resources\obj\Release\DatasmithRevitResources.dll' for writing -- 'The process cannot access the file 'D:\Build\++UE4\Sync\Engine\Source\Programs\Enterprise\Datasmith\DatasmithRevitExporter\Resources\obj\Release\DatasmithRevitResources.dll' because it is being used by another process.' [D:\Build\++UE4\Sync\Engine\Source\Programs\Enterprise\Datasmith\DatasmithRevitExporter\Resources\DatasmithRevitResources.csproj]"
				};

				List<LogEvent> logEvents = Parse(String.Join("\n", lines));
				Assert.AreEqual(3, logEvents.Count);

				// 0
				CheckEventGroup(logEvents.Slice(0, 1), 0, 1, LogLevel.Warning, KnownLogEvents.Microsoft);
				Assert.AreEqual("TL2012", logEvents[0].GetProperty("code").ToString());
				Assert.AreEqual("20", logEvents[0].GetProperty("line").ToString());

				LogValue fileProperty0 = (LogValue)logEvents[0].GetProperty("file");
				Assert.AreEqual(@"SourceFile", fileProperty0.Type);
				Assert.AreEqual(@"Foo/Bar.txt", fileProperty0.Properties!["relativePath"].ToString());

				// 1
				CheckEventGroup(logEvents.Slice(1, 1), 1, 1, LogLevel.Warning, KnownLogEvents.Microsoft);
				Assert.AreEqual("TL2034", logEvents[1].GetProperty("code").ToString());
				Assert.AreEqual("20", logEvents[1].GetProperty("line").ToString());
				Assert.AreEqual("30", logEvents[1].GetProperty("column").ToString());

				LogValue fileProperty1 = logEvents[1].GetProperty<LogValue>("file");
				Assert.AreEqual(@"SourceFile", fileProperty1.Type);
				Assert.AreEqual(@"Foo/Bar.txt", fileProperty1.Properties!["relativePath"].ToString());

				// 2
				CheckEventGroup(logEvents.Slice(2, 1), 2, 1, LogLevel.Error, KnownLogEvents.Microsoft);
				Assert.AreEqual("CS2012", logEvents[2].GetProperty("code").ToString());
				Assert.AreEqual("CSC", logEvents[2].GetProperty("tool").ToString());
			}
		}

		[TestMethod]
		public void WarningsAsErrorsEventMatcher()
		{
			// Visual C++ error
			{
				string[] lines =
				{
					@"C:\Horde\Engine\Plugins\Experimental\VirtualCamera\Source\VirtualCamera\Private\VCamBlueprintFunctionLibrary.cpp(249): error C2220: the following warning is treated as an error",
					@"C:\Horde\Engine\Plugins\Experimental\VirtualCamera\Source\VirtualCamera\Private\VCamBlueprintFunctionLibrary.cpp(249): warning C4996: 'UEditorLevelLibrary::PilotLevelActor': The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile.",
					@"..\Plugins\Editor\EditorScriptingUtilities\Source\EditorScriptingUtilities\Public\EditorLevelLibrary.h(122): note: see declaration of 'UEditorLevelLibrary::PilotLevelActor'",
					@"C:\Horde\Engine\Plugins\Experimental\VirtualCamera\Source\VirtualCamera\Private\VCamBlueprintFunctionLibrary.cpp(314): warning C4996: 'UEditorLevelLibrary::EditorSetGameView': The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile.",
					@"..\Plugins\Editor\EditorScriptingUtilities\Source\EditorScriptingUtilities\Public\EditorLevelLibrary.h(190): note: see declaration of 'UEditorLevelLibrary::EditorSetGameView'"
				};

				List<LogEvent> logEvents = Parse(String.Join("\n", lines));
				Assert.AreEqual(5, logEvents.Count);
				CheckEventGroup(logEvents.Slice(0, 1), 0, 1, LogLevel.Error, KnownLogEvents.Compiler);
				CheckEventGroup(logEvents.Slice(1, 2), 1, 2, LogLevel.Error, KnownLogEvents.Compiler);
				CheckEventGroup(logEvents.Slice(3, 2), 3, 2, LogLevel.Error, KnownLogEvents.Compiler);

				LogEvent logEvent = logEvents[1];
				Assert.AreEqual("C4996", logEvent.GetProperty("code").ToString());
				Assert.AreEqual(LogLevel.Error, logEvent.Level);

				//LogValue FileProperty = (LogValue)Event.Properties["file"];

				// FIXME: Fails on Linux. Properties dict is empty
				//Assert.AreEqual(@"//UE4/Main/Engine/Plugins/Experimental/VirtualCamera/Source/VirtualCamera/Private/VCamBlueprintFunctionLibrary.cpp@12345", FileProperty.Properties["depotPath"].ToString());

				LogValue noteProperty1 = logEvents[2].GetProperty<LogValue>("file");
				Assert.AreEqual(@"SourceFile", noteProperty1.Type);
				Assert.AreEqual(@"//UE4/Main/Engine/Plugins/Editor/EditorScriptingUtilities/Source/EditorScriptingUtilities/Public/EditorLevelLibrary.h@12345", noteProperty1.Properties!["depotPath"].ToString());
			}
		}

		[TestMethod]
		public void AssetLogEventMatcher()
		{
			string[] lines =
			{
				@"LogBlueprint: Warning: [AssetLog] D:\build\++UE5\Sync\QAGame\Plugins\NiagaraFluids\Content\Blueprints\Phsyarum_BP.uasset: [Compiler] Fill Texture 2D : Usage of 'Fill Texture 2D' has been deprecated. This function has been replaced by object user variables on the emitter to specify render targets to fill with data.",
			};

			List<LogEvent> events = Parse(String.Join("\n", lines), new DirectoryReference("D:\\build\\++UE5\\Sync"));
			Assert.AreEqual(1, events.Count);
			Assert.AreEqual(events[0].Id, KnownLogEvents.Engine_AssetLog);

			LogValue assetProperty = events[0].GetProperty<LogValue>("asset");
			Assert.AreEqual("Asset", assetProperty.Type);
			Assert.AreEqual(@"//UE4/Main/QAGame/Plugins/NiagaraFluids/Content/Blueprints/Phsyarum_BP.uasset@12345", assetProperty.Properties!["depotPath"].ToString());
			Assert.AreEqual(@"QAGame/Plugins/NiagaraFluids/Content/Blueprints/Phsyarum_BP.uasset", assetProperty.Properties!["relativePath"].ToString());
		}

		[TestMethod]
		public void SourceFileLineEventMatcher()
		{
			List<LogEvent> logEvents = Parse("ERROR: C:\\Horde\\InstalledEngineBuild.xml(50): Some error");
			CheckEventGroup(logEvents, 0, 1, LogLevel.Error, KnownLogEvents.AutomationTool_SourceFileLine);

			Assert.AreEqual("ERROR", logEvents[0].GetProperty("severity").ToString());
			Assert.AreEqual("C:\\Horde\\InstalledEngineBuild.xml", logEvents[0].GetProperty("file").ToString());
			Assert.AreEqual("50", logEvents[0].GetProperty("line").ToString());
		}

		[TestMethod]
		public void SourceFileEventMatcher()
		{
			List<LogEvent> logEvents = Parse("  WARNING: Engine\\Plugins\\Test\\Foo.cpp: Missing copyright boilerplate");
			CheckEventGroup(logEvents, 0, 1, LogLevel.Warning, KnownLogEvents.AutomationTool_MissingCopyright);

			Assert.AreEqual("WARNING", logEvents[0].GetProperty("severity").ToString());
			Assert.AreEqual("Engine\\Plugins\\Test\\Foo.cpp", logEvents[0].GetProperty("file").ToString());
		}

		[TestMethod]
		public void MSBuildEventMatcher()
		{
			List<LogEvent> logEvents = Parse(@"  C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\Microsoft.Common.CurrentVersion.targets(4207,5): warning MSB3026: Could not copy ""obj\Development\DotNETUtilities.dll"" to ""..\..\..\..\Binaries\DotNET\DotNETUtilities.dll"". Beginning retry 2 in 1000ms. The process cannot access the file '..\..\..\..\Binaries\DotNET\DotNETUtilities.dll' because it is being used by another process. The file is locked by: ""UnrealAutomationTool(13236)"" [C:\Horde\Engine\Source\Programs\DotNETCommon\DotNETUtilities\DotNETUtilities.csproj]");
			CheckEventGroup(logEvents, 0, 1, LogLevel.Information, KnownLogEvents.Systemic_MSBuild);

			Assert.AreEqual("warning", logEvents[0].GetProperty("severity").ToString());
			
			// FIXME: Fails on Linux. Properties dict is empty
			//Assert.AreEqual(@"Engine\Source\Programs\DotNETCommon\DotNETUtilities\DotNETUtilities.csproj", GetSubProperty(Events[0], "file", "relativePath"));
		}

		[TestMethod]
		public void MonoEventMatcher()
		{
			string[] lines =
			{
				@"Running: sh -c 'xbuild ""/Users/build/Build/++UE4/Sync/Engine/Source/Programs/AutomationTool/Gauntlet/Gauntlet.Automation.csproj"" /verbosity:quiet /nologo /target:Build  /p:Platform=AnyCPU  /p:Configuration=Development  /p:EngineDir=/Users/build/Build/++UE4/Sync/Engine /p:TreatWarningsAsErrors=false /p:NoWarn=""612,618,672,1591"" /p:BuildProjectReferences=true /p:DefineConstants=MONO /p:DefineConstants=__MonoCS__ /verbosity:quiet /nologo |grep -i error; if [ $? -ne 1 ]; then exit 1; else exit 0; fi'",
				@"  /Users/build/Build/++UE4/Sync/Engine/Source/Programs/AutomationTool/Gauntlet/Gauntlet.Automation.csproj: error : /Users/build/Build/++UE4/Sync/Engine/Source/Programs/AutomationTool/Gauntlet/Gauntlet.Automation.csproj: /Users/build/Build/++UE4/Sync/Engine/Source/Programs/AutomationTool/Gauntlet/Gauntlet.Automation.csproj could not import ""../../../../Platforms/*/Source/Programs/AutomationTool/Gauntlet/*.Gauntlet.targets""",
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 1, 1, LogLevel.Error, KnownLogEvents.Generic);
		}

		[TestMethod]
		public void AndroidGradleErrorMatcher()
		{
			string[] lines =
			{
				@"    AAPT2 aapt2-4.0.0-6051327-windows Daemon #0: shutdown",
				@"    AAPT2 aapt2-4.0.0-6051327-windows Daemon #1: shutdown",
				@"    AAPT2 aapt2-4.0.0-6051327-windows Daemon #2: shutdown",
				@"    AAPT2 aapt2-4.0.0-6051327-windows Daemon #3: shutdown",
				@"    AAPT2 aapt2-4.0.0-6051327-windows Daemon #4: shutdown",
				@"    AAPT2 aapt2-4.0.0-6051327-windows Daemon #5: shutdown",
				@"    AAPT2 aapt2-4.0.0-6051327-windows Daemon #6: shutdown",
				@"    AAPT2 aapt2-4.0.0-6051327-windows Daemon #7: shutdown",
				@"",
				@"    FAILURE: Build failed with an exception.",
				@"",
				@"    * What went wrong:",
				@"    Execution failed for task ':app:buildUeDevelopmentDebugPreBundle'.",
				@"    > Required array size too large",
				@"",
				@"    * Try:",
				@"    Run with --debug option to get more log output. Run with --scan to get full insights.",
				@"",
				@"    * Exception is:",
				@"    org.gradle.api.tasks.TaskExecutionException: Execution failed for task ':app:buildUeDevelopmentDebugPreBundle'.",
				@"    	at org.gradle.api.internal.tasks.execution.ExecuteActionsTaskExecuter.lambda$executeIfValid$1(ExecuteActionsTaskExecuter.java:205)",
				@"    	at org.gradle.internal.Try$Failure.ifSuccessfulOrElse(Try.java:263)",
				@"    	at org.gradle.api.internal.tasks.execution.ExecuteActionsTaskExecuter.executeIfValid(ExecuteActionsTaskExecuter.java:203)",
				@"    	at org.gradle.api.internal.tasks.execution.ExecuteActionsTaskExecuter.execute(ExecuteActionsTaskExecuter.java:184)",
				@"    	at org.gradle.api.internal.tasks.execution.CleanupStaleOutputsExecuter.execute(CleanupStaleOutputsExecuter.java:109)",
				@"    	at org.gradle.api.internal.tasks.execution.FinalizePropertiesTaskExecuter.execute(FinalizePropertiesTaskExecuter.java:46)",
				@"    	at org.gradle.api.internal.tasks.execution.ResolveTaskExecutionModeExecuter.execute(ResolveTaskExecutionModeExecuter.java:62)",
				@"    	at org.gradle.api.internal.tasks.execution.SkipTaskWithNoActionsExecuter.execute(SkipTaskWithNoActionsExecuter.java:57)",
				@"    	at org.gradle.api.internal.tasks.execution.SkipOnlyIfTaskExecuter.execute(SkipOnlyIfTaskExecuter.java:56)",
				@"    	at org.gradle.api.internal.tasks.execution.CatchExceptionTaskExecuter.execute(CatchExceptionTaskExecuter.java:36)",
				@"    	at org.gradle.api.internal.tasks.execution.EventFiringTaskExecuter$1.executeTask(EventFiringTaskExecuter.java:77)",
				@"    	at org.gradle.api.internal.tasks.execution.EventFiringTaskExecuter$1.call(EventFiringTaskExecuter.java:55)",
				@"    	at org.gradle.api.internal.tasks.execution.EventFiringTaskExecuter$1.call(EventFiringTaskExecuter.java:52)",
				@"    	at org.gradle.internal.operations.DefaultBuildOperationExecutor$CallableBuildOperationWorker.execute(DefaultBuildOperationExecutor.java:416)",
				@"    	at org.gradle.internal.operations.DefaultBuildOperationExecutor$CallableBuildOperationWorker.execute(DefaultBuildOperationExecutor.java:406)",
				@"    	at org.gradle.internal.operations.DefaultBuildOperationExecutor$1.execute(DefaultBuildOperationExecutor.java:165)",
				@"    	at org.gradle.internal.operations.DefaultBuildOperationExecutor.execute(DefaultBuildOperationExecutor.java:250)",
				@"    	at org.gradle.internal.operations.DefaultBuildOperationExecutor.execute(DefaultBuildOperationExecutor.java:158)",
				@"    	at org.gradle.internal.operations.DefaultBuildOperationExecutor.call(DefaultBuildOperationExecutor.java:102)",
				@"    	at org.gradle.internal.operations.DelegatingBuildOperationExecutor.call(DelegatingBuildOperationExecutor.java:36)",
				@"    	at org.gradle.api.internal.tasks.execution.EventFiringTaskExecuter.execute(EventFiringTaskExecuter.java:52)",
				@"    	at org.gradle.execution.plan.LocalTaskNodeExecutor.execute(LocalTaskNodeExecutor.java:41)",
				@"",
				@"    Something else"
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(33, logEvents.Count);
			for(int idx = 0; idx < 33; idx++)
			{
				Assert.AreEqual(LogLevel.Error, logEvents[idx].Level);
			}
			Assert.AreEqual(9, logEvents[0].GetProperty(LogLine));
			Assert.AreEqual(lines.Length, logEvents[0].GetProperty<int>(LogLine) + logEvents[0].LineCount + 2);
		}

		[TestMethod]
		public void SuspendLogParsing()
		{
			string[] lines =
			{
				@"  <-- Suspend Log Parsing -->",
				@"  Error: File Copy failed with Could not find a part of the path 'P:\Builds\Automation\Fortnite\Logs\++Fortnite+Release-14.60\CL-14584315\FortTest.QuickSmokeAthena_(XboxOne_Development_Client)\Client\Saved\Settings\FortniteGame\Saved\Config\CrashReportClient\UE4CC-XboxOne-C4477473430A2DD50ABDD297FF7811CD\CrashReportClient.ini'..",
				@"  <-- Resume Log Parsing -->"
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(0, logEvents.Count);
		}

		[TestMethod]
		public void GauntletGenericErrorMatcher()
		{
			string[] lines =
			{
				@"  Error: EngineTest.RunTests Group:HLOD (Win64 Development EditorGame) result=Failed",
				@"    # EngineTest.RunTests Group:HLOD Report",
				@"    ----------------------------------------",
				@"    ### Process Role: Editor (Win64 Development)",
				@"    ----------------------------------------",
				@"    ##### Result: Abnormal Exit: Reason=3/24 tests failed, Code=-1",
				@"    FatalErrors: 0, Ensures: 0, Errors: 8, Warnings: 20, Hash: 0",
				@"    ##### Artifacts",
				@"    Log: P:/Builds/Automation/Reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor\Saved_1\Editor\EditorOutput.log",
				@"    Commandline: d:\Build\++UE5\Sync\EngineTest\EngineTest.uproject   -gauntlet  -unattended  -stdout  -AllowStdOutLogVerbosity  -gauntlet.heartbeatperiod=30  -NoWatchdog  -FORCELOGFLUSH  -CrashForUAT  -buildmachine  -ReportExportPath=""P:/Builds/Automation/Reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor""  -ExecCmds=""Automation RunTests Group:HLOD; Quit;""  -ddc=default  -userdir=""d:\Build\++UE5\Sync/Tests\DeviceCache\Win64\LocalDevice0_UserDir""",
				@"    P:/Builds/Automation/Reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor\Saved_1\Editor",
				@"    ----------------------------------------",
				@"    ## Summary",
				@"    ### EngineTest.RunTests Group:HLOD Failed",
				@"    ### Editor: 3/24 tests failed",
				@"    See below for logs and any callstacks",
				@"    Context: Win64 Development EditorGame",
				@"    FatalErrors: 0, Ensures: 0, Errors: 8, Warnings: 20",
				@"    Result: Failed, ResultHash: 0",
				@"    21 of 24 tests passed"
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 0, 20, LogLevel.Error, KnownLogEvents.Gauntlet);
		}

		[TestMethod]
		public void DockerWarningMatcher()
		{
			string[] lines =
			{
				@"#14 8.477 cc -O2 -Wall -DLUA_ANSI -DENABLE_CJSON_GLOBAL -DREDIS_STATIC=''    -c -o lauxlib.o lauxlib.c",
				@"#14 8.499 lauxlib.c: In function 'luaL_loadfile':",
				@"#14 8.499 lauxlib.c:577:4: warning: this 'while' clause does not guard... [-Wmisleading-indentation]",
				@"#14 8.499     while ((c = getc(lf.f)) != EOF && c != LUA_SIGNATURE[0]) ;",
				@"#14 8.499     ^~~~~",
				@"#14 8.499 lauxlib.c:578:5: note: ...this statement, but the latter is misleadingly indented as if it were guarded by the 'while'",
				@"#14 8.499      lf.extraline = 0;",
				@"#14 8.499      ^~",
				@"#14 8.643 cc -O2 -Wall -DLUA_ANSI -DENABLE_CJSON_GLOBAL -DREDIS_STATIC=''    -c -o lbaselib.o lbaselib.c",
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 2, 1, LogLevel.Warning, KnownLogEvents.Compiler);
		}

		[TestMethod]
		public void DockerErrorMatcher()
		{
			string[] lines =
			{
				@"  #14 9.301 cc -O2 -Wall -DLUA_ANSI -DENABLE_CJSON_GLOBAL -DREDIS_STATIC=''    -c -o lua.o lua.c",
				@"  #14 9.419 cc -o lua  lua.o liblua.a -lm",
				@"  #14 9.447 /usr/bin/ld: liblua.a(loslib.o): in function `os_tmpname':",
				@"  #14 9.447 loslib.c:(.text+0x280): warning: the use of `tmpnam' is dangerous, better use `mkstemp'",
				@"  #14 9.448 cc -O2 -Wall -DLUA_ANSI -DENABLE_CJSON_GLOBAL -DREDIS_STATIC=''    -c -o luac.o luac.c"
			};
			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 2, 2, LogLevel.Warning, KnownLogEvents.Linker);
		}

		[TestMethod]
		public void GauntletErrorMatcher()
		{
			string[] lines =
			{
				@"  Error: EngineTest.RunTests Group:HLOD (Win64 Development EditorGame) result=Failed",
				@"    # EngineTest.RunTests Group:HLOD Report",
				@"    ----------------------------------------",
				@"    ### Process Role: Editor (Win64 Development)",
				@"    ----------------------------------------",
				@"    ##### Result: Abnormal Exit: Reason=3/24 tests failed, Code=-1",
				@"    FatalErrors: 0, Ensures: 0, Errors: 8, Warnings: 20, Hash: 0",
				@"",
				@"    ##### Artifacts",
				@"    Log: P:/Builds/Automation/Reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor\Saved_1\Editor\EditorOutput.log",
				@"",
				@"    Commandline: d:\Build\++UE5\Sync\EngineTest\EngineTest.uproject   -gauntlet  -unattended  -stdout  -AllowStdOutLogVerbosity  -gauntlet.heartbeatperiod=30  -NoWatchdog  -FORCELOGFLUSH  -CrashForUAT  -buildmachine  -ReportExportPath=""P:/Builds/Automation/Reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor""  -ExecCmds=""Automation RunTests Group:HLOD; Quit;""  -ddc=default  -userdir=""d:\Build\++UE5\Sync/Tests\DeviceCache\Win64\LocalDevice0_UserDir""",
				@"",
				@"    P:/Builds/Automation/Reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor\Saved_1\Editor",
				@"",
				@"    ----------------------------------------",
				@"    ## Summary",
				@"    ### EngineTest.RunTests Group:HLOD Failed",
				@"    ### Editor: 3/24 tests failed",
				@"    See below for logs and any callstacks",
				@"",
				@"    Context: Win64 Development EditorGame",
				@"",
				@"    FatalErrors: 0, Ensures: 0, Errors: 8, Warnings: 20",
				@"    Result: Failed, ResultHash: 0",
				@"",
				@"    21 of 24 tests passed",
				@"",
				@"    ### The following tests failed:",
				@"    ##### SectionFlags: SectionFlags",
				@"    * LogAutomationController: Building static mesh SectionFlags... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Building static mesh SectionFlags... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Err0r: Screenshot 'ActorMerging_SectionFlags_LOD_0_None' test failed, Screenshots were different!  Global Difference = 0.058361, Max Local Difference = 0.821376 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"",
				@"    ##### SimpleMerge: SimpleMerge",
				@"    * LogAutomationController: Building static mesh SM_TeapotHLOD... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Building static mesh SM_TeapotHLOD... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_0_None' was similar!  Global Difference = 0.000298, Max Local Difference = 0.010725 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Err0r: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_1_None' test failed, Screenshots were different!  Global Difference = 0.006954, Max Local Difference = 0.129438 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Err0r: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_2_None' test failed, Screenshots were different!  Global Difference = 0.007732, Max Local Difference = 0.127959 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Err0r: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_3_None' test failed, Screenshots were different!  Global Difference = 0.009140, Max Local Difference = 0.172337 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_0_BaseColor' was similar!  Global Difference = 0.000000, Max Local Difference = 0.000000 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_1_BaseColor' was similar!  Global Difference = 0.002068, Max Local Difference = 0.045858 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_2_BaseColor' was similar!  Global Difference = 0.002377, Max Local Difference = 0.045858 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"    * LogAutomationController: Screenshot 'ActorMerging_SimpleMeshMerge_LOD_3_BaseColor' was similar!  Global Difference = 0.002647, Max Local Difference = 0.057322 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"",
				@"    ##### SingleLODMerge: SingleLODMerge",
				@"    * LogAutomationController: Building static mesh Pencil2... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Building static mesh Pencil2... [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Private\Logging\LogMacros.cpp(92)]",
				@"    * LogAutomationController: Err0r: Screenshot 'ActorMerging_SingleLODMerge_LOD_0_BaseColor' test failed, Screenshots were different!  Global Difference = 0.013100, Max Local Difference = 0.131657 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]",
				@"",
				@"    ### Links",
				@"    View results here: http://automation.epicgames.net/reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor/index.html",
				@"",
				@"    Open results in UnrealEd from P:/Builds/Automation/Reports/++UE5+Main/EngineTest/++UE5+Main-CL-14167122/HLOD_Win64Editor",
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 0, 55, LogLevel.Error, KnownLogEvents.Gauntlet_UnitTest);

			Assert.AreEqual("HLOD", logEvents[29].GetProperty("group").ToString());
			Assert.AreEqual("SectionFlags", logEvents[29].GetProperty("name").ToString());
			Assert.AreEqual("SectionFlags", logEvents[29].GetProperty("friendly_name").ToString());

			Assert.AreEqual("HLOD", logEvents[34].GetProperty("group").ToString());
			Assert.AreEqual("SimpleMerge", logEvents[34].GetProperty("name").ToString());
			Assert.AreEqual("SimpleMerge", logEvents[34].GetProperty("friendly_name").ToString());

			Assert.AreEqual("HLOD", logEvents[46].GetProperty("group").ToString());
			Assert.AreEqual("SingleLODMerge", logEvents[46].GetProperty("name").ToString());
			Assert.AreEqual("SingleLODMerge", logEvents[46].GetProperty("friendly_name").ToString());
		}

		[TestMethod]
		public void GauntletScreenshotErrorMatcher()
		{
			string text = @"  Error: LogAutomationController: Error: Screenshot 'ActorMerging_SectionFlags_LOD_0_None' test failed, Screenshots were different!  Global Difference = 0.058361, Max Local Difference = 0.821376 [D:\Build\++UE5\Sync\Engine\Source\Runtime\Core\Public\Delegates\DelegateInstancesImpl.h(546)]";

			List<LogEvent> logEvents = Parse(text);
			CheckEventGroup(logEvents, 0, 1, LogLevel.Error, KnownLogEvents.Gauntlet_ScreenshotTest);

			LogEvent logEvent = logEvents[0];
			Assert.AreEqual("ActorMerging_SectionFlags_LOD_0_None", logEvent.GetProperty("screenshot").ToString());
		}

		[TestMethod]
		public void SystemicErrorMatcher()
		{
			string[] lines =
			{
				@"    LogDerivedDataCache: Warning: Access to //epicgames.net/root/DDC-Global-UE4 appears to be slow. 'Touch' will be disabled and queries/writes will be limited."
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 0, 1, LogLevel.Information, KnownLogEvents.Systemic_SlowDDC);
		}

		[TestMethod]
		public void XoreaxErrorMatcher()
		{
			string[] lines =
			{
				@"--------------------Build System Warning---------------------------------------",
				@"Failed to connect to Coordinator:",
				@"    All builds will run in standalone mode.",
				@"--------------------Build System Warning (Agent 'Cloud_1p5rlm3rq_10 (Core #9)')",
				@"Remote tasks distribution:",
				@"    Tasks execution is impeded due to low agent responsiveness",
				@"-------------------------------------------------------------------------------",
				@"    LogXGEController: Warning: XGE's background service (BuildService.exe) is not running - service is likely disabled on this machine.",
				@"BUILD FAILED: Command failed (Result:1): C:\Program Files (x86)\IncrediBuild\xgConsole.exe ""d:\build\Sync\Engine\Programs\AutomationTool\Saved\Logs\UAT_XGE.xml"" /Rebuild /NoLogo /ShowAgent /ShowTime"
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(8, logEvents.Count);
			CheckEventGroup(logEvents.Slice(0, 3), 0, 3, LogLevel.Information, KnownLogEvents.Systemic_Xge_Standalone);
			CheckEventGroup(logEvents.Slice(3, 3), 3, 3, LogLevel.Information, KnownLogEvents.Systemic_Xge);
			CheckEventGroup(logEvents.Slice(6, 1), 7, 1, LogLevel.Information, KnownLogEvents.Systemic_Xge_ServiceNotRunning);
			CheckEventGroup(logEvents.Slice(7, 1), 8, 1, LogLevel.Error, KnownLogEvents.Systemic_Xge_BuildFailed);
		}

		[TestMethod]
		public void LogChannelMatcher()
		{
			string[] lines =
			{
				@"Execution of commandlet took:  749.68 seconds",
				@"LogFort: Error: Serialized Class /Script/Engine.AnimSequence for a property of Class /Script/Engine.BlendSpace. Reference will be nullptred.",
				@"    Property = ObjectProperty /Game/Animation/Game/Enemies/HuskHusky/HuskyHusk_AnimBlueprint.HuskyHusk_AnimBlueprint_C:AnimBlueprintGeneratedConstantData:ObjectProperty_358",
				@"    Item = AnimSequence /Game/Animation/Game/Enemies/HuskyHusk_Riot/Locomotion/Idle/Idle_Shield.Idle_Shield",
				@"LogDataAssetDirectoryExporter: Display: 'Platform' property is of type: string",
				@"Took 0.17187880000000003s to run p4.exe, ExitCode=0"
			};

			{
				List<LogEvent> logEvents = Parse(lines);
				Assert.AreEqual(4, logEvents.Count);
				CheckEventGroup(logEvents.Slice(0, 3), 1, 3, LogLevel.Error, KnownLogEvents.Engine_LogChannel);
				CheckEventGroup(logEvents.Slice(3, 1), 4, 1, LogLevel.Information, KnownLogEvents.Engine_LogChannel);
			}

			{
				List<LogEvent> logEvents = Parse(String.Join("\n", lines).Replace("Error:", "Warning:", StringComparison.Ordinal));
				Assert.AreEqual(4, logEvents.Count);
				CheckEventGroup(logEvents.Slice(0, 3), 1, 3, LogLevel.Warning, KnownLogEvents.Engine_LogChannel);
				CheckEventGroup(logEvents.Slice(3, 1), 4, 1, LogLevel.Information, KnownLogEvents.Engine_LogChannel);
			}
		}

		[TestMethod]
		public void LocalizationChannelMatcher()
		{
			string[] lines =
			{
				@"[2022.05.31-04.25.40:235][  0]LogGatherTextFromSourceCommandlet: Warning: Plugins/Enterprise/VariantManager/Source/VariantManager/Private/SVariantManager.cpp(3717): LOCTEXT macro has an empty identifier and cannot be gathered.",
				@"[2022.06.01-04.29.09:630][  0]LogLocTextHelper: Warning: Plugins/Experimental/UVEditor/Source/UVEditor/Private/UVEditorCommands.cpp(39): Text conflict from UI_COMMAND macro for namespace ""UICommands.FUVEditorCommands"" and key ""SplitAction_ToolTip"". The conflicting sources are ""Given an edge selection, split those edges. Given a vertex selection, split any selected bowtie vertices. Given a triangle selection, split along selection boundaries."" and ""Given an edge selection, split those edges. Given a vertex selection, split any selected bowtie vertices."".",
				@"[2022.06.01-04.29.09:630][  0]LogLocTextHelper: Warning: Content/Localization/Engine/Engine.manifest: See conflicting location.",
			};

			{
				List<LogEvent> logEvents = Parse(lines);
				Assert.AreEqual(3, logEvents.Count);
				CheckEventGroup(logEvents.Slice(0, 1), 0, 1, LogLevel.Warning, KnownLogEvents.Engine_Localization);
				CheckEventGroup(logEvents.Slice(1, 2), 1, 2, LogLevel.Warning, KnownLogEvents.Engine_Localization);

				LogEvent logEvent = logEvents[0];
				Assert.AreEqual("LogGatherTextFromSourceCommandlet", logEvent.GetProperty("channel").ToString());
				Assert.AreEqual("Warning", logEvent.GetProperty("severity").ToString());
				Assert.AreEqual("Engine/Plugins/Enterprise/VariantManager/Source/VariantManager/Private/SVariantManager.cpp", logEvent.GetProperty<LogValue>("file")!.Properties!["relativePath"].ToString());
				Assert.AreEqual("3717", logEvent.GetProperty("line").ToString());
				Assert.AreEqual(0, logEvent.LineIndex);
				Assert.AreEqual(1, logEvent.LineCount);

				logEvent = logEvents[1];
				Assert.AreEqual("LogLocTextHelper", logEvent.GetProperty("channel").ToString());
				Assert.AreEqual("Warning", logEvent.GetProperty("severity").ToString());
				Assert.AreEqual("Engine/Plugins/Experimental/UVEditor/Source/UVEditor/Private/UVEditorCommands.cpp", logEvent.GetProperty<LogValue>("file")!.Properties!["relativePath"].ToString());
				Assert.AreEqual("39", logEvent.GetProperty("line").ToString());
				Assert.AreEqual(0, logEvent.LineIndex);
				Assert.AreEqual(2, logEvent.LineCount);

				logEvent = logEvents[2];
				Assert.AreEqual("LogLocTextHelper", logEvent.GetProperty("channel").ToString());
				Assert.AreEqual("Warning", logEvent.GetProperty("severity").ToString());
				Assert.AreEqual("Engine/Content/Localization/Engine/Engine.manifest", logEvent.GetProperty<LogValue>("file")!.Properties!["relativePath"].ToString());
				Assert.AreEqual(1, logEvent.LineIndex);
				Assert.AreEqual(2, logEvent.LineCount);
			}
		}

		[TestMethod]
		public void HangingIndentMatcher()
		{
			string[] lines =
			{
				@"first line",
				@"first line in multi-line message",
				@"  this is a hanging indent",
				@"   this is also hanging",
				@"this is a separate item",
			};

			LogBuffer buffer = new LogBuffer(10);
			buffer.AddLines(lines);

			Assert.IsTrue(buffer.IsAligned(0, buffer.CurrentLine));
			Assert.IsTrue(buffer.IsAligned(1, buffer.CurrentLine));
			Assert.IsTrue(buffer.IsAligned(2, buffer.CurrentLine));
			Assert.IsTrue(buffer.IsAligned(3, buffer.CurrentLine));
			Assert.IsTrue(buffer.IsAligned(4, buffer.CurrentLine));
			Assert.IsFalse(buffer.IsAligned(5, buffer.CurrentLine));

			string? firstIndentedLine = buffer[2];
			Assert.IsTrue(buffer.IsAligned(2, firstIndentedLine));
			Assert.IsTrue(buffer.IsAligned(3, firstIndentedLine));
			Assert.IsFalse(buffer.IsAligned(4, firstIndentedLine));

			Assert.IsFalse(buffer.IsHanging(1, buffer.CurrentLine));
			Assert.IsTrue(buffer.IsHanging(2, buffer.CurrentLine));
			Assert.IsTrue(buffer.IsHanging(3, buffer.CurrentLine));
			Assert.IsFalse(buffer.IsHanging(4, buffer.CurrentLine));

			Assert.IsFalse(buffer.IsHanging(2, firstIndentedLine));
			Assert.IsTrue(buffer.IsHanging(3, firstIndentedLine));
			Assert.IsFalse(buffer.IsHanging(4, firstIndentedLine));
		}

		[TestMethod]
		public void MsTestEventMatcher()
		{
			string[] lines =
			{
				@"Microsoft (R) Test Execution Command Line Tool Version 17.2.0 (x64)",
				@"Copyright (c) Microsoft Corporation.  All rights reserved.",
				@"Starting test execution, please wait...",
				@"A total of 1 test files matched the specified pattern.",
				@"",
				@"  Failed ExplicitGroupingTest [219 ms]",
				@"  Error Message:",
				@"   Assert.AreEqual failed. Expected:<2>. Actual:<1>.",
				@"  Stack Trace:",
				@"     at Horde.Build.Tests.IssueServiceTests.ExplicitGroupingTest() in /app/Source/Programs/Horde/Horde.Build.Tests/IssueServiceTests.cs:line 1382",
				@"   at Microsoft.VisualStudio.TestPlatform.MSTestAdapter.PlatformServices.ThreadOperations.ExecuteWithAbortSafety(Action action)",
				@"",
				@"  Standard Output Messages:",
				@" info: Horde.Build.Issues.IssueService[0]",
				@"       Updating issues for 62a21de84f3b4344e94f3f10:0000:0004",
				@" info: Horde.Build.Logs.LogEventCollection[0]",
				@"       Querying for log events for log 62a21de84f3b4344e94f3f15 creation time 06/09/2022 16:20:56",
				@" info: Horde.Build.Auditing.AuditLog[0]",
				@"       Created issue 1",
				@" info: Horde.Build.Auditing.AuditLog[0]",
				@"       Changed severity to Error",
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(6, logEvents.Count);
			CheckEventGroup(logEvents, 5, 6, LogLevel.Error, KnownLogEvents.MSTest);
		}

		static List<LogEvent> Parse(IEnumerable<string> lines)
		{
			return Parse(String.Join("\n", lines));
		}

		static List<LogEvent> Parse(string text)
		{
			return Parse(text, new DirectoryReference("C:\\Horde".Replace('\\', Path.DirectorySeparatorChar)));
		}

		static List<LogEvent> Parse(string text, DirectoryReference workspaceDir)
		{
			List<string> ignorePatterns = new List<string>();

			byte[] textBytes = Encoding.UTF8.GetBytes(text);

			Random generator = new Random(0);

			LoggerCapture logger = new LoggerCapture();

			PerforceLogger perforceLogger = new PerforceLogger(logger);
			perforceLogger.AddClientView(workspaceDir, "//UE4/Main/...", 12345);

			using (LogParser parser = new LogParser(perforceLogger, ignorePatterns))
			{
				int pos = 0;
				while(pos < textBytes.Length)
				{
					int len = Math.Min((int)(generator.NextDouble() * 256), textBytes.Length - pos);
					parser.WriteData(textBytes.AsMemory(pos, len));
					pos += len;
				}
			}
			return logger._events;
		}

		static void CheckEventGroup(IEnumerable<LogEvent> logEvents, int index, int count, LogLevel level, EventId eventId = default)
		{
			IEnumerator<LogEvent> enumerator = logEvents.GetEnumerator();
			for (int idx = 0; idx < count; idx++)
			{
				Assert.IsTrue(enumerator.MoveNext());

				LogEvent logEvent = enumerator.Current;
				Assert.AreEqual(level, logEvent.Level);
				Assert.AreEqual(eventId, logEvent.Id);
				Assert.AreEqual(idx, logEvent.LineIndex);
				Assert.AreEqual(count, logEvent.LineCount);
				Assert.AreEqual(index + idx, logEvent.GetProperty(LogLine));
			}
			Assert.IsFalse(enumerator.MoveNext());
		}
	}
}
