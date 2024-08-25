// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting;

#nullable enable

namespace UnrealBuildToolTests
{
	[TestClass]
	public class LogEventMatcherTests
	{
		const string LogLine = "LogLine";

		class LoggerCapture : ILogger
		{
			class Scope : IDisposable
			{
				public void Dispose() { }
			}

			int _logLineIndex;

			public void Reset()
			{
				_events.Clear();
				_logLineIndex = 0;
			}

			public List<LogEvent> _events = new List<LogEvent>();

			public IDisposable BeginScope<TState>(TState state) => new Scope();

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
		public void CompileEventMatcher()
		{
			// Visual C++ error
			{
				string[] lines =
				{
					@"  C:\Horde/Fortnite Game/Source/FortniteGame/Private/FortVehicleManager.cpp(78): error C2664: 'FDelegateHandle TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject&lt;AFortVehicleManager,&gt;(const UserClass *,void (__cdecl AFortVehicleManager::* )(FChaosScene *) const)': cannot convert argument 2 from 'void (__cdecl AFortVehicleManager::* )(FPhysScene *)' to 'void (__cdecl AFortVehicleManager::* )(FChaosScene *)'",
					@"          with",
					@"          [",
					@"              UserClass=AFortVehicleManager",
					@"          ]",
					@"  C:\Horde/Fortnite Game/Source/FortniteGame/Private/FortVehicleManager.cpp(78): note: Types pointed to are unrelated; conversion requires reinterpret_cast, C-style cast or function-style cast",
					@"  C:\Horde\Sync\Engine\Source\Runtime\Core\Public\Delegates/DelegateSignatureImpl.inl(871): note: see declaration of 'TBaseMulticastDelegate&lt;void,FChaosScene *&gt;::AddUObject'",
					@"",
					@"Error executing d:\build\AutoSDK\Sync\HostWin64\Win64\VS2019\14.29.30145\bin\HostX64\x64\cl.exe (tool returned code: 2)",
				};

				List<LogEvent> logEvents = Parse(String.Join("\n", lines));
				CheckEventGroup(logEvents, 0, 7, LogLevel.Error, KnownLogEvents.Compiler);

				Assert.AreEqual("C2664", logEvents[0].GetProperty("code").ToString());
				Assert.AreEqual("78", logEvents[0].GetProperty("line").ToString());

				LogValue fileProperty = (LogValue)logEvents[0].GetProperty("file");
				Assert.AreEqual(@"C:\Horde/Fortnite Game/Source/FortniteGame/Private/FortVehicleManager.cpp", fileProperty.Text);
				Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);

				LogValue noteProperty1 = logEvents[5].GetProperty<LogValue>("file");
				Assert.AreEqual(@"C:\Horde/Fortnite Game/Source/FortniteGame/Private/FortVehicleManager.cpp", noteProperty1.Text);
				Assert.AreEqual(LogValueType.SourceFile, noteProperty1.Type);

				LogValue noteProperty2 = logEvents[6].GetProperty<LogValue>("file");
				Assert.AreEqual(@"C:\Horde\Sync\Engine\Source\Runtime\Core\Public\Delegates/DelegateSignatureImpl.inl", noteProperty2.Text);

				// FIXME: Fails on Linux. Properties dict is empty
				//Assert.AreEqual(@"SourceFile", NoteProperty2.Properties["type"].ToString());
			}
		}

		[TestMethod]
		public void CompileHppEventMatcher()
		{
			// Visual C++ error
			{
				string[] lines =
				{
					@"D:/build/U5M+Inc/Sync/Engine/Source/ThirdParty/nanoflann/1.4.2/include\nanoflann/nanoflann.hpp(129,9): error: cannot use 'throw' with exceptions disabled",
					@"        throw std::logic_error(""Try to change the size of a std::array."");",
					@"        ^",
				};

				List<LogEvent> logEvents = Parse(String.Join("\n", lines));
				CheckEventGroup(logEvents, 0, 3, LogLevel.Error, KnownLogEvents.Compiler);
			}
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
				Assert.AreEqual(LogValueType.SourceFile, fileProperty0.Type);
				Assert.AreEqual(@"C:\Horde\Foo\Bar.txt", fileProperty0.Text);

				// 1
				CheckEventGroup(logEvents.Slice(1, 1), 1, 1, LogLevel.Warning, KnownLogEvents.Microsoft);
				Assert.AreEqual("TL2034", logEvents[1].GetProperty("code").ToString());
				Assert.AreEqual("20", logEvents[1].GetProperty("line").ToString());
				Assert.AreEqual("30", logEvents[1].GetProperty("column").ToString());

				LogValue fileProperty1 = logEvents[1].GetProperty<LogValue>("file");
				Assert.AreEqual(LogValueType.SourceFile, fileProperty1.Type);
				Assert.AreEqual(@"C:\Horde\Foo\Bar.txt", fileProperty1.Text);

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
				Assert.AreEqual(LogValueType.SourceFile, noteProperty1.Type);
				Assert.AreEqual(@"Engine\Plugins\Editor\EditorScriptingUtilities\Source\EditorScriptingUtilities\Public\EditorLevelLibrary.h", noteProperty1.Properties![LogEventPropertyName.File].ToString()!);
			}
		}

		[TestMethod]
		public void ClangEventMatcher()
		{
			string[] lines =
			{
				@"  In file included from D:\\build\\++Fortnite+Dev+Build+AWS+Incremental\\Sync\\FortniteGame\\Intermediate\\Build\\PS5\\FortniteClient\\Development\\FortInstallBundleManager\\Module.FortInstallBundleManager.cpp:3:",
				@"  In file included from D:\\build\\++Fortnite+Dev+Build+AWS+Incremental\\Sync\\FortniteGame\\Intermediate\\Build\\PS5\\FortniteClient\\Development\\FortInstallBundleManager\\Module.FortInstallBundleManager.cpp:3:",
				@"  D:/build/++Fortnite+Dev+Build+AWS+Incremental/Sync/FortniteGame/Plugins/Runtime/FortInstallBundleManager/Source/Private/FortInstallBundleManagerUtil.cpp(38,11): fatal error: 'PlatformInstallBundleSource.h' file not found",
				@"          #include ""PlatformInstallBundleSource.h""",
				@"                   ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
				@"  1 error generated."
			};

			List<LogEvent> events = Parse(String.Join("\n", lines));
			CheckEventGroup(events, 0, 5, LogLevel.Error, KnownLogEvents.Compiler);
		}

		[TestMethod]
		public void ClangEventMatcher2()
		{
			// Linux Clang error
			string[] lines =
			{
				@"[1/7] Compile NaniteMeshLodGroupUpdateCommandlet.cpp",
				@"[2/7] Compile NaniteMeshLodGroupUpdateCommandlet.cpp",
				@"/mnt/horde/U5+RES+Inc+Min/Sync/Samples/Showcases/CitySample/Source/CitySampleEditor/Commandlets/NaniteMeshLodGroupUpdateCommandlet.cpp:33:9: error: 'ClassNames' is deprecated: Class names are now represented by path names. Please use ClassPaths. Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile. [-Werror,-Wdeprecated-declarations]",
				@"        Filter.ClassNames.Add(FName(TEXT(""StaticMesh""))); ",
				@"               ^",
				@"/mnt/horde/U5+RES+Inc+Min/Sync/Engine/Source/Runtime/CoreUObject/Public/AssetRegistry/ARFilter.h:35:2: note: 'ClassNames' has been explicitly marked deprecated here",
				@"        UE_DEPRECATED(5.1, ""Class names are now represented by path names. Please use ClassPaths."")",
				@"        ^",
				@"/mnt/horde/U5+RES+Inc+Min/Sync/Engine/Source/Runtime/Core/Public/Misc/CoreMiscDefines.h:232:43: note: expanded from macro 'UE_DEPRECATED'",
				@"#define UE_DEPRECATED(Version, Message) [[deprecated(Message "" Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile."")]]",
				@"                                          ^",
				@"1 error generated.",
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 1, 10, LogLevel.Error, KnownLogEvents.Compiler);

			LogValue fileProperty = (LogValue)logEvents[1].GetProperty("file");
			Assert.AreEqual(@"/mnt/horde/U5+RES+Inc+Min/Sync/Samples/Showcases/CitySample/Source/CitySampleEditor/Commandlets/NaniteMeshLodGroupUpdateCommandlet.cpp", fileProperty.Text);
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);

			LogValue noteProperty1 = logEvents[4].GetProperty<LogValue>("file");
			Assert.AreEqual(@"/mnt/horde/U5+RES+Inc+Min/Sync/Engine/Source/Runtime/CoreUObject/Public/AssetRegistry/ARFilter.h", noteProperty1.Text);
			Assert.AreEqual(LogValueType.SourceFile, noteProperty1.Type);

			LogValue noteProperty2 = logEvents[7].GetProperty<LogValue>("file");
			Assert.AreEqual(@"/mnt/horde/U5+RES+Inc+Min/Sync/Engine/Source/Runtime/Core/Public/Misc/CoreMiscDefines.h", noteProperty2.Text);
		}

		[TestMethod]
		public void ClangEventMatcher3()
		{
			string[] lines =
			{
				@"In file included from ../Intermediate/Build/Windows/UnrealGame/Development/Launch/Module.Launch.cpp:2:",
				@"Engine/Platforms/Windows/Source/Runtime/Launch/Private/LaunchWindows.cpp(95,15): error: member access into incomplete type 'FConfigCacheIni'",
				@"                        if (GConfig->GetBool(TEXT(""/Script/WindowsRuntimeSettings.WindowsRuntimeSettings""), TEXT(""bRouteGameUserSettingsToSaveGame""), bUseSaveForGameUserSettings, GEngineIni) && bUseSaveForGameUserSettings)",
				@"                                   ^",
				@"Engine/Source/Runtime/Core/Public/CoreGlobals.h(16,7): note: forward declaration of 'FConfigCacheIni'",
				@"class FConfigCacheIni;",
				@"      ^",
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 0, 7, LogLevel.Error, KnownLogEvents.Compiler);

			LogValue fileProperty = (LogValue)logEvents[1].GetProperty("file");
			Assert.AreEqual(@"Engine/Platforms/Windows/Source/Runtime/Launch/Private/LaunchWindows.cpp", fileProperty.Text);
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);

			LogValue noteProperty1 = logEvents[4].GetProperty<LogValue>("file");
			Assert.AreEqual(@"Engine/Source/Runtime/Core/Public/CoreGlobals.h", noteProperty1.Text);
			Assert.AreEqual(LogValueType.SourceFile, noteProperty1.Type);
		}

		[TestMethod]
		public void ClangEventMatcher4()
		{
			string[] lines =
			{
				@"In file included from D:/build/++UE5/Sync/Engine/Intermediate/Build/Android/UnrealGame/Inc/NavigationSystem/UHT/AbstractNavData.gen.cpp:8:",
				@"Engine/Source/Runtime/NavigationSystem/Public/AbstractNavData.h(45,2): error: allocating an object of abstract class type 'AAbstractNavData'",
				@"        GENERATED_BODY()",
				@"        ^",
				@"Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectMacros.h(687,29): note: expanded from macro 'GENERATED_BODY'",
				@"#define GENERATED_BODY(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_GENERATED_BODY);",
				@"                            ^",
				@"D:/build/++UE5/Sync/Engine/Source/Runtime/CoreUObject/Public\UObject/ObjectMacros.h(682,37): note: expanded from macro 'BODY_MACRO_COMBINE'",
				@"#define BODY_MACRO_COMBINE(A,B,C,D) BODY_MACRO_COMBINE_INNER(A,B,C,D)",
				@"                                    ^",
				@"D:/build/++UE5/Sync/Engine/Source/Runtime/CoreUObject/Public\UObject/ObjectMacros.h(681,43): note: expanded from macro 'BODY_MACRO_COMBINE_INNER'",
				@"#define BODY_MACRO_COMBINE_INNER(A,B,C,D) A##B##C##D",
				@"                                          ^",
				@"<scratch space>(114,1): note: expanded from here",
				@"FID_Engine_Source_Runtime_NavigationSystem_Public_AbstractNavData_h_45_GENERATED_BODY",
				@"^",
				@"D:/build/++UE5/Sync/Engine/Intermediate/Build/Android/UnrealGame/Inc/NavigationSystem/UHT\AbstractNavData.generated.h(82,2): note: expanded from macro 'FID_Engine_Source_Runtime_NavigationSystem_Public_AbstractNavData_h_45_GENERATED_BODY'",
				@"        FID_Engine_Source_Runtime_NavigationSystem_Public_AbstractNavData_h_45_ENHANCED_CONSTRUCTORS \",
				@"        ^",
				@"D:/build/++UE5/Sync/Engine/Intermediate/Build/Android/UnrealGame/Inc/NavigationSystem/UHT\AbstractNavData.generated.h(59,53): note: expanded from macro 'FID_Engine_Source_Runtime_NavigationSystem_Public_AbstractNavData_h_45_ENHANCED_CONSTRUCTORS'",
				@"        DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(AAbstractNavData)",
				@"                                                           ^",
				@"D:/build/++UE5/Sync/Engine/Source/Runtime/NavigationSystem/Public/NavigationData.h(836,15): note: unimplemented pure virtual method 'FindOverlappingEdges' in 'AAbstractNavData'",
				@"        virtual bool FindOverlappingEdges(const FNavLocation& StartLocation, TConstArrayView<FVector> ConvexPolygon, TArray<FVector>& OutEdges, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const PURE_VIRTUAL(ANavigationData::FindOverlappingEdges, return false;);",
				@"                     ^",
				@"1 error generated."
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 0, 25, LogLevel.Error, KnownLogEvents.Compiler);

			LogValue fileProperty = (LogValue)logEvents[1].GetProperty("file");
			Assert.AreEqual(@"Engine/Source/Runtime/NavigationSystem/Public/AbstractNavData.h", fileProperty.Text);
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);

			LogValue noteProperty1 = logEvents[4].GetProperty<LogValue>("file");
			Assert.AreEqual(@"Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectMacros.h", noteProperty1.Text);
			Assert.AreEqual(LogValueType.SourceFile, noteProperty1.Type);

			LogValue noteProperty2 = logEvents[7].GetProperty<LogValue>("file");
			Assert.AreEqual(@"D:/build/++UE5/Sync/Engine/Source/Runtime/CoreUObject/Public\UObject/ObjectMacros.h", noteProperty2.Text);
			Assert.AreEqual(LogValueType.SourceFile, noteProperty2.Type);

			LogValue noteProperty3 = logEvents[10].GetProperty<LogValue>("file");
			Assert.AreEqual(@"D:/build/++UE5/Sync/Engine/Source/Runtime/CoreUObject/Public\UObject/ObjectMacros.h", noteProperty3.Text);
			Assert.AreEqual(LogValueType.SourceFile, noteProperty3.Type);

			Assert.IsFalse(logEvents[13].TryGetProperty<LogValue>("file", out _));

			LogValue noteProperty5 = logEvents[16].GetProperty<LogValue>("file");
			Assert.AreEqual(@"D:/build/++UE5/Sync/Engine/Intermediate/Build/Android/UnrealGame/Inc/NavigationSystem/UHT\AbstractNavData.generated.h", noteProperty5.Text);
			Assert.AreEqual(LogValueType.SourceFile, noteProperty5.Type);

			LogValue noteProperty6 = logEvents[19].GetProperty<LogValue>("file");
			Assert.AreEqual(@"D:/build/++UE5/Sync/Engine/Intermediate/Build/Android/UnrealGame/Inc/NavigationSystem/UHT\AbstractNavData.generated.h", noteProperty6.Text);
			Assert.AreEqual(LogValueType.SourceFile, noteProperty6.Type);

			LogValue noteProperty7 = logEvents[22].GetProperty<LogValue>("file");
			Assert.AreEqual(@"D:/build/++UE5/Sync/Engine/Source/Runtime/NavigationSystem/Public/NavigationData.h", noteProperty7.Text);
			Assert.AreEqual(LogValueType.SourceFile, noteProperty7.Type);
		}

		[TestMethod]
		public void ClangEventMatcher5()
		{
			string[] lines =
			{
				@"Module.StateTreeModule.cpp (0:25.07 at +6:14)",
				@"In file included from ..\Plugins\Runtime\StateTree\Intermediate\Build\Win64\UnrealEditor\Debug\StateTreeModule\Module.StateTreeModule.cpp:10:",
				@"In file included from .\../Plugins/Runtime/StateTree/Source/StateTreeModule/Private/StateTree.cpp:8:",
				@"Engine/Source/Runtime/AssetRegistry/Public/AssetData.h(6,9): warning: Runtime\AssetRegistry\Public\AssetData.h(6): warning: #include AssetRegistry/AssetData.h instead of AssetData.h [-W#pragma-messages]",
				@"#pragma message (__FILE__""(6): warning: #include AssetRegistry/AssetData.h instead of AssetData.h"")",
				@"        ^",
				@"1 warning generated."
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 1, 5, LogLevel.Warning, KnownLogEvents.Compiler);

			LogValue fileProperty = (LogValue)logEvents[2].GetProperty("file");
			Assert.AreEqual(@"Engine/Source/Runtime/AssetRegistry/Public/AssetData.h", fileProperty.Text);
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);
		}

		[TestMethod]
		public void ClangEventMatcher6()
		{
			string[] lines =
			{
				@"[19884/26184] Compile KismetCastingUtils.cpp",
				@"In file included from Editor/KismetCompiler/Private/KismetCastingUtils.cpp:3:",
				@"/Users/build/Build/++UE5/Sync/Engine/Source/Editor/KismetCompiler/Public/KismetCastingUtils.h:33:3: error: unknown type name 'UEdGraphNode'; did you mean 'UEdGraphPin'?",
				@"                UEdGraphNode* TargetNode = nullptr;",
				@"                ^~~~~~~~~~~~",
				@"                UEdGraphPin",
				@"/Users/build/Build/++UE5/Sync/Engine/Source/Editor/KismetCompiler/Public/KismetCastingUtils.h:10:7: note: 'UEdGraphPin' declared here",
				@"class UEdGraphPin;",
				@"      ^",
				@"/Users/build/Build/++UE5/Sync/Engine/Source/Editor/KismetCompiler/Public/KismetCastingUtils.h:47:110: error: unknown type name 'UEdGraphNode'; did you mean 'UEdGraphPin'?",
				@"        KISMETCOMPILER_API FBPTerminal* MakeImplicitCastTerminal(FKismetFunctionContext& Context, UEdGraphPin* Net, UEdGraphNode* SourceNode = nullptr);",
				@"                                                                                                                    ^~~~~~~~~~~~",
				@"                                                                                                                    UEdGraphPin",
				@"/Users/build/Build/++UE5/Sync/Engine/Source/Editor/KismetCompiler/Public/KismetCastingUtils.h:10:7: note: 'UEdGraphPin' declared here",
				@"class UEdGraphPin;",
				@"      ^",
				@"/Users/build/Build/++UE5/Sync/Engine/Source/Editor/KismetCompiler/Private/KismetCastingUtils.cpp:34:88: error: cannot initialize a member subobject of type 'UEdGraphPin *' with an lvalue of type 'UEdGraphNode *'",
				@"                Context.ImplicitCastMap.Add(DestinationPin, FImplicitCastParams{Conversion, NewTerm, OwningNode});",
				@"                                                                                                     ^~~~~~~~~~",
				@"/Users/build/Build/++UE5/Sync/Engine/Source/Editor/KismetCompiler/Private/KismetCastingUtils.cpp:121:78: error: cannot initialize a parameter of type 'UEdGraphNode *' with an lvalue of type 'UEdGraphPin *const'",
				@"        FBlueprintCompiledStatement& CastStatement = Context.AppendStatementForNode(CastParams.TargetNode);",
				@"                                                                                    ^~~~~~~~~~~~~~~~~~~~~",
				@"/Users/build/Build/++UE5/Sync/Engine/Source/Editor/KismetCompiler/Public/KismetCompiledFunctionContext.h:265:68: note: passing argument to parameter 'Node' here",
				@"        FBlueprintCompiledStatement& AppendStatementForNode(UEdGraphNode* Node)",
				@"                                                                          ^",
				@"/Users/build/Build/++UE5/Sync/Engine/Source/Editor/KismetCompiler/Private/KismetCastingUtils.cpp:121:31: error: non-const lvalue reference to type 'FBlueprintCompiledStatement' cannot bind to a temporary of type 'FBlueprintCompiledStatement'",
				@"        FBlueprintCompiledStatement& CastStatement = Context.AppendStatementForNode(CastParams.TargetNode);",
				@"                                     ^               ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
				@"5 errors generated."
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents.Slice(0, 9), 0, 9, LogLevel.Error, KnownLogEvents.Compiler);
			CheckEventGroup(logEvents.Slice(9, 7), 9, 7, LogLevel.Error, KnownLogEvents.Compiler);
			CheckEventGroup(logEvents.Slice(16, 3), 16, 3, LogLevel.Error, KnownLogEvents.Compiler);
			CheckEventGroup(logEvents.Slice(19, 6), 19, 6, LogLevel.Error, KnownLogEvents.Compiler);
			CheckEventGroup(logEvents.Slice(25, 3), 25, 3, LogLevel.Error, KnownLogEvents.Compiler);

			LogValue fileProperty = (LogValue)logEvents[1].GetProperty("file");
			Assert.AreEqual(@"Editor/KismetCompiler/Private/KismetCastingUtils.cpp", fileProperty.Text);
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);
		}

		[TestMethod]
		public void ClangEventMatcher7()
		{
			string[] lines =
			{
				@"In file included from ../Plugins/FX/Niagara/Intermediate/Build/Linux/B4D820EA/UnrealEditor/Development/NiagaraEditor/Module.NiagaraEditor.1_of_12.cpp:25:",
				@"Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Private/Customizations/NiagaraTypeCustomizations.cpp:31:10: fatal error: 'Framework/Multibox/MultiBoxBuilder.h' file not found"
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 0, 2, LogLevel.Error, KnownLogEvents.Compiler);

			LogValue fileProperty = (LogValue)logEvents[1].GetProperty("file");
			Assert.AreEqual(@"Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Private/Customizations/NiagaraTypeCustomizations.cpp", fileProperty.Text);
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);
		}

		[TestMethod]
		public void ClangEventMatcher8()
		{
			string[] lines =
			{
				@"[332/1772] Analyze graph_viewer.cc",

				// group 1
				@"In file included from Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:4:",
				@"In file included from Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Internal/core/graph/graph_viewer.h:6:",
				@"In file included from Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Internal/core/graph/graph.h:31:",
				@"In file included from ../Plugins/Experimental/NNI/Source/ThirdParty/Deps/gsl/gsl:31:",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/Deps/gsl/gsl-lite.hpp:3217:12: warning: Forwarding reference passed to std::move(), which may unexpectedly cause lvalues to be moved; use std::forward() instead [bugprone-move-forwarding-reference]",
				@"    return std::move( p );",
				@"           ^",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/Deps/gsl/gsl-lite.hpp:3217:12: note: Forwarding reference passed to std::move(), which may unexpectedly cause lvalues to be moved; use std::forward() instead",

				// group 2
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:89:26: warning: Access to field 'nodes' results in a dereference of a null pointer (loaded from variable 'filter_info') [core.NullDereference]",
				@"    for (NodeIndex idx : filter_info->nodes) {",
				@"                         ^",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:45:26: note: Passing null pointer value via 2nd parameter 'filter_info'",
				@"    : GraphViewer(graph, nullptr) {",
				@"                         ^~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:45:7: note: Calling constructor for 'GraphViewer'",
				@"    : GraphViewer(graph, nullptr) {",
				@"      ^~~~~~~~~~~~~~~~~~~~~~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:56:11: note: 'filter_info' is null",
				@"          filter_info ? [this](NodeIndex idx) { return filtered_node_indices_.count(idx) == 0; }",
				@"          ^~~~~~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:56:11: note: '?' condition is false",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:83:7: note: Calling '~function'",
				@"      },",
				@"      ^",
				@"/Applications/Xcode-v13.2.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/functional:2547:43: note: Calling '~__value_func'",
				@"function<_Rp(_ArgTypes...)>::~function() {}",
				@"                                          ^",
				@"/Applications/Xcode-v13.2.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/functional:1843:9: note: Taking true branch",
				@"        if ((void*)__f_ == &__buf_)",
				@"        ^",
				@"/Applications/Xcode-v13.2.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/functional:1844:13: note: Calling '__func::destroy'",
				@"            __f_->destroy();",
				@"            ^~~~~~~~~~~~~~~",
				@"/Applications/Xcode-v13.2.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/functional:1714:5: note: Calling '__alloc_func::destroy'",
				@"    __f_.destroy();",
				@"    ^~~~~~~~~~~~~~",
				@"/Applications/Xcode-v13.2.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/functional:1577:32: note: Value assigned to field 'filter_info_', which participates in a condition later",
				@"    void destroy() _NOEXCEPT { __f_.~__compressed_pair<_Target, _Alloc>(); }",
				@"                               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
				@"/Applications/Xcode-v13.2.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/functional:1714:5: note: Returning from '__alloc_func::destroy'",
				@"    __f_.destroy();",
				@"    ^~~~~~~~~~~~~~",
				@"/Applications/Xcode-v13.2.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/functional:1844:13: note: Returning from '__func::destroy'",
				@"            __f_->destroy();",
				@"            ^~~~~~~~~~~~~~~",
				@"/Applications/Xcode-v13.2.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1/functional:2547:43: note: Returning from '~__value_func'",
				@"function<_Rp(_ArgTypes...)>::~function() {}",
				@"                                          ^",
				@"/Users/build/Build/++UE5/Sync/Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:83:7: note: Returning from '~function'",
				@"      },",
				@"      ^",
				@"/Users/build/Build/++UE5/Sync/Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:87:7: note: Assuming field 'filter_info_' is non-null",
				@"  if (filter_info_) {",
				@"      ^~~~~~~~~~~~",
				@"/Users/build/Build/++UE5/Sync/Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:87:3: note: Taking true branch",
				@"  if (filter_info_) {",
				@"  ^",
				@"/Users/build/Build/++UE5/Sync/Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:89:26: note: Access to field 'nodes' results in a dereference of a null pointer (loaded from variable 'filter_info')",
				@"    for (NodeIndex idx : filter_info->nodes) {",
				@"                         ^~~~~~~~~~~",

				// group 3
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:119:5: warning: Method called on moved-from object 'nodes_in_topological_order_' of type 'std::vector' [cplusplus.Move]",
				@"    nodes_in_topological_order_.reserve(filter_info->nodes.size());",
				@"    ^",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:49:7: note: Calling constructor for 'GraphViewer'",
				@"    : GraphViewer(graph, &filter_info) {",
				@"      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:56:11: note: 'filter_info' is non-null",
				@"          filter_info ? [this](NodeIndex idx) { return filtered_node_indices_.count(idx) == 0; }",
				@"          ^~~~~~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:56:11: note: '?' condition is true",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:87:7: note: Assuming field 'filter_info_' is non-null",
				@"  if (filter_info_) {",
				@"      ^~~~~~~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:87:3: note: Taking true branch",
				@"  if (filter_info_) {",
				@"  ^",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:118:23: note: Object 'nodes_in_topological_order_' of type 'std::vector' is left in a valid but unspecified state after move",
				@"    auto orig_order = std::move(nodes_in_topological_order_);",
				@"                      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:119:5: note: Method called on moved-from object 'nodes_in_topological_order_' of type 'std::vector'",
				@"    nodes_in_topological_order_.reserve(filter_info->nodes.size());",
				@"    ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",

				// group 4
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:145:5: warning: Method called on moved-from object 'nodes_in_topological_order_with_priority_' of type 'std::vector' [cplusplus.Move]",
				@"    nodes_in_topological_order_with_priority_.reserve(filter_info->nodes.size());",
				@"    ^",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:49:7: note: Calling constructor for 'GraphViewer'",
				@"    : GraphViewer(graph, &filter_info) {",
				@"      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:56:11: note: 'filter_info' is non-null",
				@"          filter_info ? [this](NodeIndex idx) { return filtered_node_indices_.count(idx) == 0; }",
				@"          ^~~~~~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:56:11: note: '?' condition is true",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:87:7: note: Assuming field 'filter_info_' is non-null",
				@"  if (filter_info_) {",
				@"      ^~~~~~~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:87:3: note: Taking true branch",
				@"  if (filter_info_) {",
				@"  ^",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:144:32: note: Object 'nodes_in_topological_order_with_priority_' of type 'std::vector' is left in a valid but unspecified state after move",
				@"    auto orig_priority_order = std::move(nodes_in_topological_order_with_priority_);",
				@"                               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
				@"Engine/Plugins/Experimental/NNI/Source/ThirdParty/ORT_4_1/Private/core/graph/graph_viewer.cc:145:5: note: Method called on moved-from object 'nodes_in_topological_order_with_priority_' of type 'std::vector'",
				@"    nodes_in_topological_order_with_priority_.reserve(filter_info->nodes.size());",
				@"    ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
				@"4 warnings generated"
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents.Slice(0, 8), 1, 8, LogLevel.Warning, KnownLogEvents.Compiler);
			CheckEventGroup(logEvents.Slice(8, 52), 9, 52, LogLevel.Warning, KnownLogEvents.Compiler);
			CheckEventGroup(logEvents.Slice(60, 22), 61, 22, LogLevel.Warning, KnownLogEvents.Compiler);
			CheckEventGroup(logEvents.Slice(82, 22), 83, 22, LogLevel.Warning, KnownLogEvents.Compiler);
		}

		[TestMethod]
		public void ClangEventMatcher9()
		{
			string[] lines =
			{
				@"[421/1292] Compile Module.ApplicationCore.cpp",
				@"In file included from Engine/Intermediate/Build/IOS/UnrealGame/Development/ApplicationCore/Module.ApplicationCore.cpp:14:",
				@"Engine/Source/Runtime/ApplicationCore/Private/IOS/Accessibility/IOSAccessibilityCache.cpp:119:26: error: implicit conversion loses floating-point precision: 'CGFloat' (aka 'double') to 'const float' [-Werror,-Wimplicit-float-conversion]",
				@"                                        const float Scale = [IOSAppDelegate GetDelegate].IOSView.contentScaleFactor;",
				@"                                                    ~~~~~   ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 0, 5, LogLevel.Error, KnownLogEvents.Compiler);
		}

		[TestMethod]
		public void IOSCompileErrorMatcher()
		{
			string[] lines =
			{
				@"  [76/9807] Compile MemoryChunkStoreStatistics.cpp",
				@"  /Users/build/Build/++UE4/Sync/Engine/Plugins/Runtime/AR/AzureSpatialAnchorsForARKit/Source/AzureSpatialAnchorsForARKit/Private/AzureSpatialAnchorsForARKit.cpp:7:48: error: unknown type name 'AzureSpatialAnchorsForARKit'; did you mean 'FAzureSpatialAnchorsForARKit'?",
				@"  IMPLEMENT_MODULE(FAzureSpatialAnchorsForARKit, AzureSpatialAnchorsForARKit)",
				@"                                                 ^~~~~~~~~~~~~~~~~~~~~~~~~~~",
				@"                                                 FAzureSpatialAnchorsForARKit",
				@"  /Users/build/Build/++UE4/Sync/Engine/Plugins/Runtime/AR/AzureSpatialAnchorsForARKit/Source/AzureSpatialAnchorsForARKit/Public/AzureSpatialAnchorsForARKit.h:10:7: note: 'FAzureSpatialAnchorsForARKit' declared here",
				@"  class FAzureSpatialAnchorsForARKit : public FAzureSpatialAnchorsBase, public IModuleInterface",
				@"        ^"
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 0, 8, LogLevel.Error, KnownLogEvents.Compiler);

			LogEvent logEvent = logEvents[1];
			Assert.AreEqual("7", logEvent.GetProperty("line").ToString());
			Assert.AreEqual("48", logEvent.GetProperty("column").ToString());

			LogValue fileProperty = logEvent.GetProperty<LogValue>("file");
			Assert.AreEqual("/Users/build/Build/++UE4/Sync/Engine/Plugins/Runtime/AR/AzureSpatialAnchorsForARKit/Source/AzureSpatialAnchorsForARKit/Private/AzureSpatialAnchorsForARKit.cpp", fileProperty.Text);
		}

		[TestMethod]
		public void LinkerEventMatcher()
		{
			{
				List<LogEvent> logEvents = Parse(@"  TP_VehicleAdvPawn.cpp.obj : error LNK2019: unresolved external symbol ""__declspec(dllimport) private: static class UClass * __cdecl UPhysicalMaterial::GetPrivateStaticClass(void)"" (__imp_?GetPrivateStaticClass@UPhysicalMaterial@@CAPEAVUClass@@XZ) referenced in function ""class UPhysicalMaterial * __cdecl ConstructorHelpersInternal::FindOrLoadObject<class UPhysicalMaterial>(class FString &,unsigned int)"" (??$FindOrLoadObject@VUPhysicalMaterial@@@ConstructorHelpersInternal@@YAPEAVUPhysicalMaterial@@AEAVFString@@I@Z)");
				CheckEventGroup(logEvents, 0, 1, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);
				Assert.AreEqual("__declspec(dllimport) private: static class UClass * __cdecl UPhysicalMaterial::GetPrivateStaticClass(void)", logEvents[0].GetProperty("symbol").ToString());
			}

			{
				List<LogEvent> logEvents = Parse(@"  D:\Build\++UE4\Sync\Templates\TP_VehicleAdv\Binaries\Win64\UE4Editor-TP_VehicleAdv.dll : fatal error LNK1120: 1 unresolved externals");
				Assert.AreEqual(1, logEvents.Count);
				Assert.AreEqual(LogLevel.Error, logEvents[0].Level);
			}

			{

				string[] lines =
				{
					@"tool 19.50.0.12 (rel,tool,19.500 @527452 x64) D:\Workspaces\AutoSDK\HostWin64\9.508.001\9.500\host_tools\bin\tool.exe",
					@"Link : error: L0039: reference to undefined symbol `UProjectPlayControllerComponent::HandleMoveToolSpawnedActor(ATestGameCreativeMoveTool*, AActor*, bool)' in file ""D:\Workspaces\TestGame\TestGame\Intermediate\Build\Win64\TestGame\Development\ProjectPlayGameRuntime\Module.ProjectPlayGameRuntime.gen.cpp.o""\",
					@"Link : error: L0039: reference to undefined symbol `UProjectPlayControllerComponent::HandleWeaponEquipped(ATestGameWeapon*, ATestGameWeapon*)' in file ""D:\Workspaces\TestGame\TestGame\Intermediate\Build\Win64\TestGameClient\Development\ProjectPlayGameRuntime\Module.ProjectPlayGameRuntime.gen.cpp.o""\",
					@"Module.ProjectPlayGameRuntime.gen.cpp : error: L0039: reference to undefined symbol `UProjectPlayControllerComponent::HandleMoveToolSpawnedActor(ATestGameMoveTool*, AActor*, bool)' in file ""D:\Workspaces\TestGame\TestGame\Intermediate\Build\Win64\TestGameClient\Development\ProjectPlayGameRuntime\Module.ProjectPlayGameRuntime.gen.cpp.o""\",
					@"Module.ProjectPlayGameRuntime.gen.cpp : error: L0039: reference to undefined symbol `UProjectPlayControllerComponent::HandleWeaponEquipped(ATestGameWeapon*, ATestGameWeapon*)' in file ""D:\Workspaces\TestGame\TestGame\Intermediate\Build\Win64\TestGame\Development\ProjectPlayGameRuntime\Module.ProjectPlayGameRuntime.gen.cpp.o""\",
					@"orbis-clang: error: linker command failed with exit code 1 (use -v to see invocation)"
				};

				List<LogEvent> logEvents = Parse(String.Join("\n", lines));
				Assert.AreEqual(5, logEvents.Count);
				CheckEventGroup(logEvents.Slice(0, 5), 1, 5, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);

				Assert.AreEqual("UProjectPlayControllerComponent::HandleMoveToolSpawnedActor(ATestGameCreativeMoveTool*, AActor*, bool)", logEvents[0].GetProperty("symbol").ToString());
				Assert.AreEqual("UProjectPlayControllerComponent::HandleWeaponEquipped(ATestGameWeapon*, ATestGameWeapon*)", logEvents[1].GetProperty("symbol").ToString());
				Assert.AreEqual("UProjectPlayControllerComponent::HandleMoveToolSpawnedActor(ATestGameMoveTool*, AActor*, bool)", logEvents[2].GetProperty("symbol").ToString());
				Assert.AreEqual("UProjectPlayControllerComponent::HandleWeaponEquipped(ATestGameWeapon*, ATestGameWeapon*)", logEvents[3].GetProperty("symbol").ToString());

			}
		}

		[TestMethod]
		public void LinkerFatalEventMatcher()
		{
			string[] lines =
			{
				@"  webrtc.lib(celt.obj) : error LNK2005: tf_select_table already defined in celt.lib(celt.obj)",
				@"     Creating library D:\Build\++UE4\Sync\Engine\Binaries\Win64\UE4Game.lib and object D:\Build\++UE4\Sync\Engine\Binaries\Win64\UE4Game.exp",
				@"  Engine\Binaries\Win64\UE4Game.exe: fatal error LNK1169: one or more multiply defined symbols found"
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			Assert.AreEqual(2, logEvents.Count);

			CheckEventGroup(logEvents.Slice(0, 1), 0, 1, LogLevel.Error, KnownLogEvents.Linker_DuplicateSymbol);
			Assert.AreEqual("tf_select_table", logEvents[0].GetProperty("symbol").ToString());

			CheckEventGroup(logEvents.Slice(1, 1), 2, 1, LogLevel.Error, KnownLogEvents.Linker);
		}

		[TestMethod]
		public void LinkEventMatcher()
		{
			string[] lines =
			{
				@"  Undefined symbols for architecture x86_64:",
				@"    ""FUdpPingWorker::SendDataSize"", referenced from:",
				@"        FUdpPingWorker::SendPing(FSocket&, unsigned char*, unsigned int, FInternetAddr const&, UDPPing::FUdpPingPacket&, double&) in Module.Icmp.cpp.o",
				@"  ld: symbol(s) not found for architecture x86_64",
				@"  clang: error: linker command failed with exit code 1 (use -v to see invocation)",
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 0, 5, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);
		}

		[TestMethod]
		public void LinkEventMatcher2()
		{
			string[] lines =
			{
				@"  ld.lld.exe: error: undefined symbol: Foo::Bar() const",
				@"  ld.lld.exe: error: undefined symbol: Foo::Bar2() const",
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			Assert.AreEqual(2, logEvents.Count);
			CheckEventGroup(logEvents, 0, 2, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);

			LogValue symbolProperty = (LogValue)logEvents[0].GetProperty("symbol");
			Assert.AreEqual("Foo::Bar", symbolProperty.Properties![LogEventPropertyName.Identifier].ToString());

			LogValue symbolProperty2 = (LogValue)logEvents[1].GetProperty("symbol");
			Assert.AreEqual("Foo::Bar2", symbolProperty2.Properties![LogEventPropertyName.Identifier].ToString());
		}

		[TestMethod]
		public void LinkEventMatcher3()
		{
			string[] lines =
			{
				@"  prospero-lld: error: undefined symbol: USkeleton::GetBlendProfile(FName const&)",
				@"  >>> referenced by Module.Frosty.cpp",
				@"  >>>               D:\\build\\UE5M+Inc\\Sync\\Collaboration\\Frosty\\Frosty\\Intermediate\\Build\\PS5\\Frosty\\Development\\Frosty\\Module.Frosty.cpp.o:(UFrostyAnimInstance::GetBlendProfile(FName const&))",
				@"  prospero-clang: error: linker command failed with exit code 1 (use -v to see invocation)"
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 0, 4, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);

			LogValue symbolProperty = (LogValue)logEvents[0].GetProperty("symbol");
			Assert.AreEqual("USkeleton::GetBlendProfile", symbolProperty.Properties![LogEventPropertyName.Identifier].ToString());
		}

		[TestMethod]
		public void LinkEventMatcher4()
		{
			string[] lines =
			{
				@"Engine/Source/Module.DataInterface.cpp : error: L0019: symbol `IMPLEMENT_MODULE_DataInterface' multiply defined",
				@"Engine/Source/Module.DataInterface.cpp : error:  ...first in ""D:\build\U5M+Inc\Sync\EngineTest\Intermediate\Build\PS4\EngineTest\Development\DataInterfaceGraph\Module.DataInterfaceGraph.cpp.o""",
				@"Engine/Source/Module.DataInterface.cpp : error:  ...now in ""D:\build\U5M+Inc\Sync\EngineTest\Intermediate\Build\PS4\EngineTest\Development\DataInterface\Module.DataInterface.cpp.o"".",
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 0, 3, LogLevel.Error, KnownLogEvents.Linker_DuplicateSymbol);

			LogValue symbolProperty = (LogValue)logEvents[0].GetProperty("symbol");
			Assert.AreEqual("IMPLEMENT_MODULE_DataInterface", symbolProperty.Properties![LogEventPropertyName.Identifier].ToString());
		}

		[TestMethod]
		public void MacLinkErrorMatcher()
		{
			string[] lines =
			{
				@"   Undefined symbols for architecture arm64:",
				@"     ""Foo::Bar() const"", referenced from:"
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 0, 2, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);

			LogValue symbolProperty = (LogValue)logEvents[1].GetProperty("symbol");
			Assert.AreEqual(LogValueType.Symbol, symbolProperty.Type);
			Assert.AreEqual("Foo::Bar", symbolProperty.Properties![LogEventPropertyName.Identifier].ToString());
		}

		[TestMethod]
		public void MacLinkEventMatcher2()
		{
			string[] lines =
			{
				@"  Undefined symbols for architecture x86_64:",
				@"    ""_OBJC_CLASS_$_NSAppleScript"", referenced from:",
				@"        objc-class-ref in libsupp.a(macutil.o)",
				@"    ""_OBJC_CLASS_$_NSString"", referenced from:",
				@"        objc-class-ref in libsupp.a(macutil.o)",
				@"    ""_OBJC_CLASS_$_NSBundle"", referenced from:",
				@"        objc-class-ref in libsupp.a(macutil.o)",
				@"    ""_CGSessionCopyCurrentDictionary"", referenced from:",
				@"        _WindowServicesAvailable in libsupp.a(macutil.o)",
				@"  ld: symbol(s) not found for architecture x86_64"
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents, 0, 10, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);
		}

		[TestMethod]
		public void LinuxLinkErrorMatcher()
		{
			string[] lines =
			{
				@"  ld.lld: error: unable to find library -lstdc++",
				@"  clang++: error: linker command failed with exit code 1 (use -v to see invocation)",
				@"  ld.lld: error: unable to find library -lstdc++",
				@"  clang++: error: linker command failed with exit code 1 (use -v to see invocation)"
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(4, logEvents.Count);

			CheckEventGroup(logEvents.Slice(0, 2), 0, 2, LogLevel.Error, KnownLogEvents.Linker);
			CheckEventGroup(logEvents.Slice(2, 2), 2, 2, LogLevel.Error, KnownLogEvents.Linker);
		}

		[TestMethod]
		public void AndroidLinkErrorMatcher()
		{
			string[] lines =
			{
				@"   Foo.o:(.data.rel.ro + 0x5d88): undefined reference to `Foo::Bar()'"
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 0, 1, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);

			LogValue symbolProperty = (LogValue)logEvents[0].GetProperty("symbol");
			Assert.AreEqual("Foo::Bar", symbolProperty.Properties![LogEventPropertyName.Identifier].ToString());
		}

		[TestMethod]
		public void AndroidLinkWarningMatcher()
		{
			string[] lines =
			{
				@"  ld.lld: warning: found local symbol '__bss_start__' in global part of symbol table in file D:/Build/++UE4/Sync/Engine/Plugins/Runtime/GooglePAD/Source/ThirdParty/play-core-native-sdk/libs/arm64-v8a/ndk21.3.6528147/c++_shared\libplaycore.so",
				@"  ld.lld: warning: found local symbol '_bss_end__' in global part of symbol table in file D:/Build/++UE4/Sync/Engine/Plugins/Runtime/GooglePAD/Source/ThirdParty/play-core-native-sdk/libs/arm64-v8a/ndk21.3.6528147/c++_shared\libplaycore.so",
				@"  ld.lld: warning: found local symbol '_edata' in global part of symbol table in file D:/Build/++UE4/Sync/Engine/Plugins/Runtime/GooglePAD/Source/ThirdParty/play-core-native-sdk/libs/arm64-v8a/ndk21.3.6528147/c++_shared\libplaycore.so",
				@"  ld.lld: warning: found local symbol '__bss_end__' in global part of symbol table in file D:/Build/++UE4/Sync/Engine/Plugins/Runtime/GooglePAD/Source/ThirdParty/play-core-native-sdk/libs/arm64-v8a/ndk21.3.6528147/c++_shared\libplaycore.so",
				@"  ld.lld: warning: found local symbol '_end' in global part of symbol table in file D:/Build/++UE4/Sync/Engine/Plugins/Runtime/GooglePAD/Source/ThirdParty/play-core-native-sdk/libs/arm64-v8a/ndk21.3.6528147/c++_shared\libplaycore.so",
				@"  ld.lld: warning: found local symbol '__end__' in global part of symbol table in file D:/Build/++UE4/Sync/Engine/Plugins/Runtime/GooglePAD/Source/ThirdParty/play-core-native-sdk/libs/arm64-v8a/ndk21.3.6528147/c++_shared\libplaycore.so",
				@"  ld.lld: warning: found local symbol '__bss_start' in global part of symbol table in file D:/Build/++UE4/Sync/Engine/Plugins/Runtime/GooglePAD/Source/ThirdParty/play-core-native-sdk/libs/arm64-v8a/ndk21.3.6528147/c++_shared\libplaycore.so",
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 0, 7, LogLevel.Warning, KnownLogEvents.Linker);
		}

		[TestMethod]
		public void LldLinkErrorMatcher()
		{
			string[] lines =
			{
				@"   ld.lld.exe: error: undefined symbol: Foo::Bar() const",
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 0, 1, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);

			LogValue symbolProperty = logEvents[0].GetProperty<LogValue>("symbol");
			Assert.AreEqual("Foo::Bar", symbolProperty.Properties![LogEventPropertyName.Identifier].ToString());
		}

		[TestMethod]
		public void GnuLinkErrorMatcher()
		{
			string[] lines =
			{
				@"   Link: error: L0039: reference to undefined symbol `Foo::Bar() const' in file",
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 0, 1, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);

			LogValue symbolProperty = logEvents[0].GetProperty<LogValue>("symbol");
			Assert.AreEqual("Foo::Bar", symbolProperty.Properties![LogEventPropertyName.Identifier].ToString());
		}

		[TestMethod]
		public void MicrosoftLinkErrorMatcher()
		{
			string[] lines =
			{
				@"   Foo.cpp.obj : error LNK2001: unresolved external symbol ""private: virtual void __cdecl Foo::Bar(class UStruct const *,void const *,struct FAssetBundleData &,class FName,class TSet<void const *,struct DefaultKeyFuncs<void const *,0>,class FDefaultSetAllocator> &)const "" (?InitializeAssetBundlesFromMetadata_Recursive@UAssetManager@@EEBAXPEBVUStruct@@PEBXAEAUFAssetBundleData@@VFName@@AEAV?$TSet@PEBXU?$DefaultKeyFuncs@PEBX$0A@@@VFDefaultSetAllocator@@@@@Z)"
			};

			List<LogEvent> logEvents = Parse(lines);
			CheckEventGroup(logEvents, 0, 1, LogLevel.Error, KnownLogEvents.Linker_UndefinedSymbol);

			LogValue symbolProperty = logEvents[0].GetProperty<LogValue>("symbol");
			Assert.AreEqual("Foo::Bar", symbolProperty.Properties![LogEventPropertyName.Identifier].ToString());
		}

		[TestMethod]
		public void IOSIgnoredLinkEvents()
		{
			string[] lines =
			{
				@"ld: warning: Linker asked to preserve internal global: 'inflateEnd'",
				@"ld: warning: Linker asked to preserve internal global: 'inflateReset'",
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			CheckEventGroup(logEvents.Slice(0, 1), 0, 1, LogLevel.Information, KnownLogEvents.Linker);
			CheckEventGroup(logEvents.Slice(1, 1), 1, 1, LogLevel.Information, KnownLogEvents.Linker);
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
		public void XoreaxErrorMatcher2()
		{
			string[] lines =
			{
				@"--------------------Build Cache summary----------------------------------------",
				@"Build Tasks: 8042",
				@"Build Cache Efficiency: 0% (0 tasks)",
				@"New cache size: 17 GB",
				@"Updated 352 items (20 GB)",
				@"WARNING: 389 items (19.4 GB) removed from the cache due to reaching the cache size limit",
				@"",
				@"1 build system warning(s):",
				@"   - Build Cache performance hit",
				@"",
				@"Took 2515.0181741s to run xgConsole.exe, ExitCode=0",
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(3, logEvents.Count);
			CheckEventGroup(logEvents.Slice(0, 1), 5, 1, LogLevel.Information, KnownLogEvents.Systemic_Xge_CacheLimit);
			CheckEventGroup(logEvents.Slice(1, 2), 7, 2, LogLevel.Information, KnownLogEvents.Systemic_Xge);
		}

		[TestMethod]
		public void XoreaxErrorMatcher3()
		{
			string[] lines =
			{
				@"Module.ExternalSource.cpp (Agent '10-99-199-46 (Core #3)', 0:16.43 at +23:42)",
				@"Default.rc2 (0:00.29 at +23:59)",
				@"--------------------Build System Error-----------------------------------------",
				@"Fatal error:",
				@"    Task queue management failed.",
				@"    Error starting Task 'clang-cl: Task: Env_0->Action1715_1 (Tool1715_1)' on machine 'Local CPU 8'",
				@"    Failed to map view of file mapping 0x082C at position 0:0 (0 bytes): Not enough memory resources are available to process this command (8)",
				@"-------------------------------------------------------------------------------",
				@"",
				@"--------------------Build Cache summary----------------------------------------",
				@"Build Tasks: 15751",
				@"Build Cache Efficiency: 0% (0 tasks)"
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(6, logEvents.Count);
			CheckEventGroup(logEvents, 2, 6, LogLevel.Error, KnownLogEvents.Systemic_Xge);
		}

		[TestMethod]
		public void XoreaxErrorMatcher4()
		{
			string[] lines =
			{
				@"--------------------Build Cache summary----------------------------------------",
				@"Build Tasks: 2007",
				@"Build Cache Efficiency: 0% (0 tasks)",
				@"New cache size: 24.9 GB",
				@"Updated 1899 items (9.8 GB)",
				@"WARNING: Several items removed from the cache due to reaching the cache size limit.",
				@"WARNING: The Build Cache is close to full, limiting your ability to benefit from previously cached data to speed up your builds. It is recommended to increase the cache size to be larger than the sum of all build artifacts that are cached.",
				@"1 build system warning(s):",
				@"   - Build Cache performance hit",
				@"Took 413.95489860000004s to run xgConsole.exe, ExitCode=0"
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(4, logEvents.Count);
			CheckEventGroup(logEvents.Slice(0, 1), 5, 1, LogLevel.Information, KnownLogEvents.Systemic_Xge_CacheLimit);
			CheckEventGroup(logEvents.Slice(1, 1), 6, 1, LogLevel.Information, KnownLogEvents.Systemic_Xge_CacheLimit);
			CheckEventGroup(logEvents.Slice(2, 2), 7, 2, LogLevel.Information, KnownLogEvents.Systemic_Xge);
		}

		[TestMethod]
		public void UhtErrorMatcher()
		{
			string[] lines =
			{
				@"Parsing headers for UnrealEditor",
				@"  Running UnrealHeaderTool UnrealEditor ""/mnt/horde/FN+NC+PF/Sync/Engine/Intermediate/Build/Linux/B4D820EA/UnrealEditor/Development/UnrealEditor.uhtmanifest"" -LogCmds=""loginit warning, logexit warning, logdatabase error"" -Unattended -WarningsAsErrors -abslog=""/mnt/horde/FN+NC+PF/Sync/Engine/Programs/AutomationTool/Saved/Logs/UHT-UnrealEditor-Linux-Development.txt""",
				@"LogStreaming: Display: NotifyRegistrationEvent: Replay 112 entries",
				@"/mnt/horde/FN+NC+PF/Sync/Engine/Plugins/Experimental/DataInterfaceGraph/Source/DataInterfaceGraphUncookedOnly/Internal/DataInterfaceGraph_EditorData.h(91): Error: Do not specify struct property containing editor only properties inside an optional class.",
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(1, logEvents.Count);
			CheckEventGroup(logEvents, 3, 1, LogLevel.Error, KnownLogEvents.Compiler);

			LogValue fileProperty = (LogValue)logEvents[0].GetProperty("file");
			Assert.AreEqual(@"/mnt/horde/FN+NC+PF/Sync/Engine/Plugins/Experimental/DataInterfaceGraph/Source/DataInterfaceGraphUncookedOnly/Internal/DataInterfaceGraph_EditorData.h", fileProperty.Text);
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);
		}

		[TestMethod]
		public void UhtErrorMatcher2()
		{
			string[] lines =
			{
				@"Parsing headers for UnrealEditor",
				@"  Running Internal UnrealHeaderTool UnrealEditor /Users/build/Build/U5M+Inc/Sync/Engine/Intermediate/Build/Mac/x86_64/UnrealEditor/Debug/UnrealEditor.uhtmanifest -WarningsAsErrors",
				@"/Users/build/Build/U5M+Inc/Sync/Engine/Restricted/NotForLicensees/Plugins/Compositor/Source/Compositor/Public/Assets/Composite.h(88): Error: Native pointer usage in member declaration detected [[[UComposite*]]].  This is disallowed for the target/module, consider TObjectPtr as an alternative.",
				@"/Users/build/Build/U5M+Inc/Sync/Engine/Restricted/NotForLicensees/Plugins/Compositor/Source/Compositor/Public/Components/CompositeCaptureComponent2D.h(53): Error: Native pointer usage in member declaration detected [[[APlayerCameraManager*]]].  This is disallowed for the target/module, consider TObjectPtr as an alternative.",
				@"/Users/build/Build/U5M+Inc/Sync/Engine/Restricted/NotForLicensees/Plugins/Compositor/Source/Compositor/Public/Assets/Composite.h(92): Error: Native pointer usage in member declaration detected [[[UComposite*]]].  This is disallowed for the target/module, consider TObjectPtr as an alternative.",
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(3, logEvents.Count);
			CheckEventGroup(logEvents.Slice(0, 1), 2, 1, LogLevel.Error, KnownLogEvents.Compiler);
			CheckEventGroup(logEvents.Slice(1, 1), 3, 1, LogLevel.Error, KnownLogEvents.Compiler);
			CheckEventGroup(logEvents.Slice(2, 1), 4, 1, LogLevel.Error, KnownLogEvents.Compiler);

			LogValue fileProperty = (LogValue)logEvents[0].GetProperty("file");
			Assert.AreEqual(@"/Users/build/Build/U5M+Inc/Sync/Engine/Restricted/NotForLicensees/Plugins/Compositor/Source/Compositor/Public/Assets/Composite.h", fileProperty.Text);
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);
		}

		[TestMethod]
		public void VerseErrorMatcher()
		{
			string[] lines =
			{
				@"C:/Horde/TestGame/Plugins/TestPlugin/Test.verse(9,9, 9,43): Verse compiler error V3587: Something went wrong"
			};

			List<LogEvent> logEvents = Parse(lines);
			Assert.AreEqual(1, logEvents.Count);
			CheckEventGroup(logEvents, 0, 1, LogLevel.Error, KnownLogEvents.Compiler);

			LogValue fileProperty = (LogValue)logEvents[0].GetProperty("file");
			Assert.AreEqual(@"C:/Horde/TestGame/Plugins/TestPlugin/Test.verse", fileProperty.Text);
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);
		}

		[TestMethod]
		public void VerseInEngineErrorMatcher()
		{
			string[] lines =
			{
				@"LogSolarisIde: Error: C:/Horde/Foo/Plugins/VerseAI/CompanionAI/Source/CompanionAI/Verse/CompanionAI.verse(651,46, 651,64): Script error 3506: Unknown identifier `sphere_draw_params`."
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			Assert.AreEqual(1, logEvents.Count);
			CheckEventGroup(logEvents.Slice(0, 1), 0, 1, LogLevel.Error, KnownLogEvents.Compiler);

			LogEvent logEvent = logEvents[0];
			Assert.AreEqual("3506", logEvent.GetProperty("code").ToString());
			Assert.AreEqual(LogLevel.Error, logEvent.Level);

			LogValue fileProperty = logEvents[0].GetProperty<LogValue>("file");
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);
			Assert.AreEqual(@"C:/Horde/Foo/Plugins/VerseAI/CompanionAI/Source/CompanionAI/Verse/CompanionAI.verse", fileProperty.Text);
		}

		[TestMethod]
		public void VerseInEngineWarningMatcher()
		{
			string[] lines =
			{
				@"LogSolarisIde: Warning: C:/Horde/Plugins/PlayerProfileManager.verse(78,31, 84,14): Script Warning 2011: This expression can fail, but the meaning of failure in the right operand of 'set ... = ...' will change in a future version of Verse."
			};

			List<LogEvent> logEvents = Parse(String.Join("\n", lines));
			Assert.AreEqual(1, logEvents.Count);
			CheckEventGroup(logEvents.Slice(0, 1), 0, 1, LogLevel.Warning, KnownLogEvents.Compiler);

			LogEvent logEvent = logEvents[0];
			Assert.AreEqual("2011", logEvent.GetProperty("code").ToString());
			Assert.AreEqual(LogLevel.Warning, logEvent.Level);

			LogValue fileProperty = logEvents[0].GetProperty<LogValue>("file");
			Assert.AreEqual(LogValueType.SourceFile, fileProperty.Type);
			Assert.AreEqual(@"C:/Horde/Plugins/PlayerProfileManager.verse", fileProperty.Text);
		}

		static List<LogEvent> Parse(IEnumerable<string> lines)
		{
			return Parse(String.Join("\n", lines));
		}

		static List<LogEvent> Parse(string text)
		{
			byte[] textBytes = Encoding.UTF8.GetBytes(text);

			Random generator = new Random(0);

			LoggerCapture logger = new LoggerCapture();

			using (LogEventParser parser = new LogEventParser(logger))
			{
				parser.AddMatchersFromAssembly(typeof(UnrealBuildTool.UnrealBuildTool).Assembly);

				logger.Reset();

				int pos = 0;
				while (pos < textBytes.Length)
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
