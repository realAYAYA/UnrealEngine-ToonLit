// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool.Modes
{

	/// <summary>
	/// Implement the UHT configuration interface.  Due to the configuration system being fairly embedded into
	/// UBT, the implementation must be part of UBT.
	/// </summary>
	public class UhtConfigImpl : IUhtConfig
	{
		private readonly ConfigHierarchy _ini;

		/// <summary>
		/// Types that have been renamed, treat the old deprecated name as the new name for code generation
		/// </summary>
		private readonly IReadOnlyDictionary<StringView, StringView> _typeRedirectMap;

		/// <summary>
		/// Metadata that have been renamed, treat the old deprecated name as the new name for code generation
		/// </summary>
		private readonly IReadOnlyDictionary<string, string> _metaDataRedirectMap;

		/// <summary>
		/// Supported units in the game
		/// </summary>
		private readonly ReadOnlyHashSet<StringView> _units;

		/// <summary>
		/// Special parsed struct names that do not require a prefix
		/// </summary>
		[System.Diagnostics.CodeAnalysis.SuppressMessage("CodeQuality", "IDE0052:Remove unread private members", Justification = "<Pending>")]
		private readonly ReadOnlyHashSet<StringView> _structsWithNoPrefix;

		/// <summary>
		/// Special parsed struct names that have a 'T' prefix
		/// </summary>
		private readonly ReadOnlyHashSet<StringView> _structsWithTPrefix;

		/// <summary>
		/// Mapping from 'human-readable' macro substring to # of parameters for delegate declarations
		/// Index 0 is 1 parameter, Index 1 is 2, etc...
		/// </summary>
		private readonly IReadOnlyList<StringView> _delegateParameterCountStrings;

		/// <summary>
		/// Default version of generated code. Defaults to oldest possible, unless specified otherwise in config.
		/// </summary>
		private readonly EGeneratedCodeVersion _defaultGeneratedCodeVersion = EGeneratedCodeVersion.V1;

		/// <summary>
		/// Internal version of pointer warning for native pointers in the engine
		/// </summary>
		private readonly UhtPointerMemberBehavior _engineNativePointerMemberBehavior = UhtPointerMemberBehavior.AllowSilently;

		/// <summary>
		/// Internal version of pointer warning for object pointers in the engine
		/// </summary>
		private readonly UhtPointerMemberBehavior _engineObjectPtrMemberBehavior = UhtPointerMemberBehavior.AllowSilently;

		/// <summary>
		/// Internal version of pointer warning for native pointers in engine plugins
		/// </summary>
		private readonly UhtPointerMemberBehavior _enginePluginNativePointerMemberBehavior = UhtPointerMemberBehavior.AllowSilently;

		/// <summary>
		/// Internal version of pointer warning for object pointers in engine plugins
		/// </summary>
		private readonly UhtPointerMemberBehavior _enginePluginObjectPtrMemberBehavior = UhtPointerMemberBehavior.AllowSilently;

		/// <summary>
		/// Internal version of pointer warning for native pointers outside the engine
		/// </summary>
		private readonly UhtPointerMemberBehavior _nonEngineNativePointerMemberBehavior = UhtPointerMemberBehavior.AllowSilently;

		/// <summary>
		/// Internal version of pointer warning for object pointers outside the engine
		/// </summary>
		private readonly UhtPointerMemberBehavior _nonEngineObjectPtrMemberBehavior = UhtPointerMemberBehavior.AllowSilently;

		/// <summary>
		/// If true, deprecation warnings should be shown
		/// </summary>
		private readonly bool _showDeprecations = true;

		/// <summary>
		/// If true, UObject properties are enabled in RigVM
		/// </summary>
		private readonly bool _areRigVMUObjectProeprtiesEnabled = false;

		/// <summary>
		/// If true, UInterface properties are enabled in RigVM
		/// </summary>
		private readonly bool _areRigVMUInterfaceProeprtiesEnabled = false;

		#region IUhtConfig Implementation
		/// <inheritdoc/>
		public EGeneratedCodeVersion DefaultGeneratedCodeVersion => this._defaultGeneratedCodeVersion;

		/// <inheritdoc/>
		public UhtPointerMemberBehavior EngineNativePointerMemberBehavior => this._engineNativePointerMemberBehavior;

		/// <inheritdoc/>
		public UhtPointerMemberBehavior EngineObjectPtrMemberBehavior => this._engineObjectPtrMemberBehavior;

		/// <inheritdoc/>
		public UhtPointerMemberBehavior EnginePluginNativePointerMemberBehavior => this._enginePluginNativePointerMemberBehavior;

		/// <inheritdoc/>
		public UhtPointerMemberBehavior EnginePluginObjectPtrMemberBehavior => this._enginePluginObjectPtrMemberBehavior;

		/// <inheritdoc/>
		public UhtPointerMemberBehavior NonEngineNativePointerMemberBehavior => this._nonEngineNativePointerMemberBehavior;

		/// <inheritdoc/>
		public UhtPointerMemberBehavior NonEngineObjectPtrMemberBehavior => this._nonEngineObjectPtrMemberBehavior;

		/// <summary>
		/// If true, UObject properties are enabled in RigVM
		/// </summary>
		public bool AreRigVMUObjectProeprtiesEnabled => this._areRigVMUObjectProeprtiesEnabled;

		/// <summary>
		/// If true, UInterface properties are enabled in RigVM
		/// </summary>
		public bool AreRigVMUInterfaceProeprtiesEnabled => this._areRigVMUInterfaceProeprtiesEnabled;

		/// <summary>
		/// If true, deprecation warnings should be shown
		/// </summary>
		public bool ShowDeprecations => this._showDeprecations;

		/// <inheritdoc/>
		public void RedirectTypeIdentifier(ref UhtToken Token)
		{
			if (!Token.IsIdentifier())
			{
				throw new Exception("Attempt to redirect type identifier when the token isn't an identifier.");
			}

			if (this._typeRedirectMap.TryGetValue(Token.Value, out StringView Redirect))
			{
				Token.Value = Redirect;
			}
		}

		/// <inheritdoc/>
		public bool RedirectMetaDataKey(string Key, out string NewKey)
		{
			if (this._metaDataRedirectMap.TryGetValue(Key, out string? Redirect))
			{
				NewKey = Redirect;
				return Key != NewKey;
			}
			else
			{
				NewKey = Key;
				return false;
			}
		}

		/// <inheritdoc/>
		public bool IsValidUnits(StringView Units)
		{
			return this._units.Contains(Units);
		}

		/// <inheritdoc/>
		public bool IsStructWithTPrefix(StringView Name)
		{
			return this._structsWithTPrefix.Contains(Name);
		}

		/// <inheritdoc/>
		public int FindDelegateParameterCount(StringView DelegateMacro)
		{
			for (int Index = 0, Count = this._delegateParameterCountStrings.Count; Index < Count; ++Index)
			{
				if (DelegateMacro.Span.Contains(this._delegateParameterCountStrings[Index].Span, StringComparison.Ordinal))
				{
					return Index;
				}
			}
			return -1;
		}

		/// <inheritdoc/>
		public StringView GetDelegateParameterCountString(int Index)
		{
			return Index >= 0 ? this._delegateParameterCountStrings[Index] : "";
		}

		/// <inheritdoc/>
		public bool IsExporterEnabled(string Name)
		{
			this._ini.GetBool("UnrealHeaderTool", Name, out bool Value);
			return Value;
		}
		#endregion

		/// <summary>
		/// Read the UHT configuration
		/// </summary>
		/// <param name="Args">Extra command line arguments</param>
		public UhtConfigImpl(CommandLineArguments Args)
		{
			DirectoryReference ConfigDirectory = DirectoryReference.Combine(Unreal.EngineDirectory, "Programs", "UnrealHeaderTool");
			this._ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ConfigDirectory, BuildHostPlatform.Current.Platform, "", Args.GetRawArray());

			this._typeRedirectMap = GetRedirectsStringView("UnrealHeaderTool", "TypeRedirects", "OldType", "NewType");
			this._metaDataRedirectMap = GetRedirectsString("CoreUObject.Metadata", "MetadataRedirects", "OldKey", "NewKey");
			this._structsWithNoPrefix = GetHashSet("UnrealHeaderTool", "StructsWithNoPrefix", StringViewComparer.Ordinal);
			this._structsWithTPrefix = GetHashSet("UnrealHeaderTool", "StructsWithTPrefix", StringViewComparer.Ordinal);
			this._units = GetHashSet("UnrealHeaderTool", "Units", StringViewComparer.OrdinalIgnoreCase);
			this._delegateParameterCountStrings = GetList("UnrealHeaderTool", "DelegateParameterCountStrings");
			this._defaultGeneratedCodeVersion = GetGeneratedCodeVersion("UnrealHeaderTool", "DefaultGeneratedCodeVersion", EGeneratedCodeVersion.V1);
			this._engineNativePointerMemberBehavior = GetPointerMemberBehavior("UnrealHeaderTool", "EngineNativePointerMemberBehavior", UhtPointerMemberBehavior.AllowSilently);
			this._engineObjectPtrMemberBehavior = GetPointerMemberBehavior("UnrealHeaderTool", "EngineObjectPtrMemberBehavior", UhtPointerMemberBehavior.AllowSilently);
			this._enginePluginNativePointerMemberBehavior = GetPointerMemberBehavior("UnrealHeaderTool", "EnginePluginNativePointerMemberBehavior", UhtPointerMemberBehavior.AllowSilently);
			this._enginePluginObjectPtrMemberBehavior = GetPointerMemberBehavior("UnrealHeaderTool", "EnginePluginObjectPtrMemberBehavior", UhtPointerMemberBehavior.AllowSilently);
			this._nonEngineNativePointerMemberBehavior = GetPointerMemberBehavior("UnrealHeaderTool", "NonEngineNativePointerMemberBehavior", UhtPointerMemberBehavior.AllowSilently);
			this._nonEngineObjectPtrMemberBehavior = GetPointerMemberBehavior("UnrealHeaderTool", "NonEngineObjectPtrMemberBehavior", UhtPointerMemberBehavior.AllowSilently);
			this._areRigVMUObjectProeprtiesEnabled = GetBoolean("UnrealHeaderTool", "AreRigVMUObjectProeprtiesEnabled", false);
			this._areRigVMUInterfaceProeprtiesEnabled = GetBoolean("UnrealHeaderTool", "AreRigVMUInterfaceProeprtiesEnabled", false);
			this._showDeprecations = GetBoolean("UnrealHeaderTool", "ShowDeprecations", true);
		}

		private bool GetBoolean(string SectionName, string KeyName, bool bDefault)
		{
			if (this._ini.TryGetValue(SectionName, KeyName, out bool value))
			{
				return value;
			}
			return bDefault;
		}

		private UhtPointerMemberBehavior GetPointerMemberBehavior(string SectionName, string KeyName, UhtPointerMemberBehavior Default)
		{
			if (this._ini.TryGetValue(SectionName, KeyName, out string? BehaviorStr))
			{
				if (!Enum.TryParse(BehaviorStr, out UhtPointerMemberBehavior Value))
				{
					throw new Exception(string.Format("Unrecognized native pointer member behavior '{0}'", BehaviorStr));
				}
				return Value;
			}
			return Default;
		}

		private EGeneratedCodeVersion GetGeneratedCodeVersion(string SectionName, string KeyName, EGeneratedCodeVersion Default)
		{
			if (this._ini.TryGetValue(SectionName, KeyName, out string? BehaviorStr))
			{
				if (!Enum.TryParse(BehaviorStr, out EGeneratedCodeVersion Value))
				{
					throw new Exception(string.Format("Unrecognized generated code version '{0}'", BehaviorStr));
				}
				return Value;
			}
			return Default;
		}

		private IReadOnlyDictionary<StringView, StringView> GetRedirectsStringView(string Section, string Key, string OldKeyName, string NewKeyName)
		{
			Dictionary<StringView, StringView> Redirects = new();

			if (this._ini.TryGetValues(Section, Key, out IReadOnlyList<string>? StringList))
			{
				foreach (string Line in StringList)
				{
					if (ConfigHierarchy.TryParse(Line, out Dictionary<string, string>? Properties))
					{
						if (!Properties.TryGetValue(OldKeyName, out string? OldKey))
						{
							throw new Exception(string.Format("Unable to get the {0} from the {1} value", OldKeyName, Key));
						}
						if (!Properties.TryGetValue(NewKeyName, out string? NewKey))
						{
							throw new Exception(string.Format("Unable to get the {0} from the {1} value", NewKeyName, Key));
						}
						Redirects.Add(OldKey, NewKey);
					}
				}
			}
			return Redirects;
		}

		private IReadOnlyDictionary<string, string> GetRedirectsString(string Section, string Key, string OldKeyName, string NewKeyName)
		{
			Dictionary<string, string> Redirects = new();

			if (this._ini.TryGetValues(Section, Key, out IReadOnlyList<string>? StringList))
			{
				foreach (string Line in StringList)
				{
					if (ConfigHierarchy.TryParse(Line, out Dictionary<string, string>? Properties))
					{
						if (!Properties.TryGetValue(OldKeyName, out string? OldKey))
						{
							throw new Exception(string.Format("Unable to get the {0} from the {1} value", OldKeyName, Key));
						}
						if (!Properties.TryGetValue(NewKeyName, out string? NewKey))
						{
							throw new Exception(string.Format("Unable to get the {0} from the {1} value", NewKeyName, Key));
						}
						Redirects.Add(OldKey, NewKey);
					}
				}
			}
			return Redirects;
		}

		private IReadOnlyList<StringView> GetList(string Section, string Key)
		{
			List<StringView> List = new();

			if (this._ini.TryGetValues(Section, Key, out IReadOnlyList<string>? StringList))
			{
				foreach (string Value in StringList)
				{
					List.Add(new StringView(Value));
				}
			}
			return List;
		}

		private ReadOnlyHashSet<StringView> GetHashSet(string Section, string Key, StringViewComparer Comparer)
		{
			HashSet<StringView> Set = new(Comparer);

			if (this._ini.TryGetValues(Section, Key, out IReadOnlyList<string>? StringList))
			{
				foreach (string Value in StringList)
				{
					Set.Add(new StringView(Value));
				}
			}
			return Set;
		}
	}

	/// <summary>
	/// Global options for UBT (any modes)
	/// </summary>
	class UhtGlobalOptions
	{
		/// <summary>
		/// User asked for help
		/// </summary>
		[CommandLine(Prefix = "-Help", Description = "Display this help.")]
		[CommandLine(Prefix = "-h")]
		[CommandLine(Prefix = "--help")]
		public bool bGetHelp = false;

		/// <summary>
		/// The amount of detail to write to the log
		/// </summary>
		[CommandLine(Prefix = "-Verbose", Value = "Verbose", Description = "Increase output verbosity")]
		[CommandLine(Prefix = "-VeryVerbose", Value = "VeryVerbose", Description = "Increase output verbosity more")]
		public LogEventType LogOutputLevel = LogEventType.Log;

		/// <summary>
		/// Specifies the path to a log file to write. Note that the default mode (eg. building, generating project files) will create a log file by default if this not specified.
		/// </summary>
		[CommandLine(Prefix = "-Log", Description = "Specify a log file location instead of the default Engine/Programs/UnrealHeaderTool/Saved/Logs/UnrealHeaderTool.log")]
		public FileReference? LogFileName = null;

		/// <summary>
		/// Whether to include timestamps in the log
		/// </summary>
		[CommandLine(Prefix = "-Timestamps", Description = "Include timestamps in the log")]
		public bool bLogTimestamps = false;

		/// <summary>
		/// Whether to format messages in MsBuild format
		/// </summary>
		[CommandLine(Prefix = "-FromMsBuild", Description = "Format messages for msbuild")]
		public bool bLogFromMsBuild = false;

		/// <summary>
		/// Disables all logging including the default log location
		/// </summary>
		[CommandLine(Prefix = "-NoLog", Description = "Disable log file creation including the default log file")]
		public bool bNoLog = false;

		[CommandLine(Prefix = "-Test", Description = "Run testing scripts")]
		public bool bTest = false;

		[CommandLine("-WarningsAsErrors", Description = "Treat warnings as errors")]
		public bool bWarningsAsErrors = false;

		[CommandLine("-NoGoWide", Description = "Disable concurrent parsing and code generation")]
		public bool bNoGoWide = false;

		[CommandLine("-WriteRef", Description = "Write all the output to a reference directory")]
		public bool bWriteRef = false;

		[CommandLine("-VerifyRef", Description = "Write all the output to a verification directory and compare to the reference output")]
		public bool bVerifyRef = false;

		[CommandLine("-FailIfGeneratedCodeChanges", Description = "Consider any changes to output files as being an error")]
		public bool bFailIfGeneratedCodeChanges = false;

		[CommandLine("-NoOutput", Description = "Do not save any output files other than reference output")]
		public bool bNoOutput = false;

		[CommandLine("-IncludeDebugOutput", Description = "Include extra content in generated output to assist with debugging")]
		public bool bIncludeDebugOutput = false;

		[CommandLine("-NoDefaultExporters", Description = "Disable all default exporters.  Useful for when a specific exporter is to be run")]
		public bool bNoDefaultExporters = false;

		/// <summary>
		/// Initialize the options with the given command line arguments
		/// </summary>
		/// <param name="Arguments"></param>
		public UhtGlobalOptions(CommandLineArguments Arguments)
		{
			Arguments.ApplyTo(this);
		}
	}

	/// <summary>
	/// File manager for the test harness
	/// </summary>
	public class UhtTestFileManager : IUhtFileManager
	{
		/// <summary>
		/// Collection of test fragments that can be read
		/// </summary>
		public Dictionary<string, UhtSourceFragment> SourceFragments = new();

		/// <summary>
		/// All output segments generated by code gen
		/// </summary>
		public SortedDictionary<string, string> Outputs = new();

		private readonly IUhtFileManager InnerManager;
		private readonly string? RootDirectory;

		/// <summary>
		/// Construct a new instance of the test file manager
		/// </summary>
		/// <param name="RootDirectory">Root directory of the UE</param>
		public UhtTestFileManager(string RootDirectory)
		{
			this.RootDirectory = RootDirectory;
			this.InnerManager = new UhtStdFileManager();
		}

		/// <inheritdoc/>
		public string GetFullFilePath(string FilePath)
		{
			if (this.RootDirectory == null)
			{
				return FilePath;
			}
			else
			{
				return Path.Combine(this.RootDirectory, FilePath);
			}
		}

		/// <inheritdoc/>
		public bool ReadSource(string FilePath, out UhtSourceFragment Fragment)
		{
			if (this.SourceFragments.TryGetValue(FilePath, out Fragment))
			{
				return true;
			}

			return InnerManager.ReadSource(GetFullFilePath(FilePath), out Fragment);
		}

		/// <inheritdoc/>
		public UhtBuffer? ReadOutput(string FilePath)
		{
			return null;
		}

		/// <inheritdoc/>
		public bool WriteOutput(string FilePath, ReadOnlySpan<char> Contents)
		{
			lock (this.Outputs)
			{
				this.Outputs.Add(FilePath, Contents.ToString());
			}
			return true;
		}

		/// <inheritdoc/>
		public bool RenameOutput(string OldFilePath, string NewFilePath)
		{
			lock (this.Outputs)
			{
				if (this.Outputs.TryGetValue(OldFilePath, out string? Content))
				{
					this.Outputs.Remove(OldFilePath);
					this.Outputs.Add(NewFilePath, Content);
				}
			}
			return true;
		}

		/// <summary>
		/// Add a source file fragment to the session.  When requests are made to read sources, the 
		/// fragment list will be searched first.
		/// </summary>
		/// <param name="SourceFile">Source file</param>
		/// <param name="FilePath">The relative path to add</param>
		/// <param name="LineNumber">Starting line number</param>
		/// <param name="Data">The data associated with the path</param>
		public void AddSourceFragment(UhtSourceFile SourceFile, string FilePath, int LineNumber, StringView Data)
		{
			this.SourceFragments.Add(FilePath, new UhtSourceFragment { SourceFile = SourceFile, FilePath = FilePath, LineNumber = LineNumber, Data = Data });
		}
	}

	/// <summary>
	/// Testing harness to run the test scripts
	/// </summary>
	class UhtTestHarness
	{
		private enum ScriptFragmentType
		{
			Unknown,
			Manifest,
			Header,
			Console,
			Output,
		}

		private struct ScriptFragment
		{
			public ScriptFragmentType Type;
			public string Name;
			public int LineNumber;
			public StringView Header;
			public StringView Body;
			public bool External;
		}

		private static bool RunScriptTest(UhtTables Tables, IUhtConfig Config, UhtGlobalOptions Options, string TestDirectory, string TestOutputDirectory, string Script, ILogger Logger)
		{
			string InPath = Path.Combine(TestDirectory, Script);
			string OutPath = Path.Combine(TestOutputDirectory, Script);

			UhtTestFileManager TestFileManager = new(TestDirectory);
			UhtSession Session = new()
			{
				Logger = Logger,
				Tables = Tables,
				Config = Config,
				FileManager = TestFileManager,
				RootDirectory = TestDirectory,
				WarningsAsErrors = Options.bWarningsAsErrors,
				RelativePathInLog = true,
				GoWide = !Options.bNoGoWide,
				NoOutput = false,
				CullOutput = false,
				CacheMessages = true,
				IncludeDebugOutput = true,
			};

			// Read the testing script
			List<ScriptFragment> ScriptFragments = new();
			int ManifestIndex = -1;
			int ConsoleIndex = -1;
			UhtSourceFile ScriptSourceFile = new(Session, Script);
			Dictionary<string, int> OutputFragments = new();
			Session.Try(ScriptSourceFile, () =>
			{
				ScriptSourceFile.Read();
				UhtTokenBufferReader Reader = new(ScriptSourceFile, ScriptSourceFile.Data.Memory);

				bool done = false;
				while (!done)
				{

					// Scan for the fragment header
					ScriptFragmentType Type = ScriptFragmentType.Unknown;
					string Name = "";
					int HeaderStartPos = Reader.InputPos;
					int HeaderEndPos = HeaderStartPos;
					int LineNumber = 1;
					while (true)
					{
						using UhtTokenSaveState SaveState = new(Reader);
						UhtToken Token = Reader.GetLine();
						if (Token.TokenType == UhtTokenType.EndOfFile)
						{
							break;
						}
						if (Token.Value.Span.Length == 0 || (Token.Value.Span.Length > 0 && Token.Value.Span[0] != '!'))
						{
							break;
						}
						HeaderEndPos = Reader.InputPos;

						int EndCommandPos = Token.Value.Span.IndexOf(' ');
						if (EndCommandPos == -1)
						{
							EndCommandPos = Token.Value.Span.Length;
						}
						string ScriptFragmentTypeString = Token.Value.Span[1..EndCommandPos].Trim().ToString();

						if (!System.Enum.TryParse<ScriptFragmentType>(ScriptFragmentTypeString, true, out Type))
						{
							continue;
						}
						if (Type == ScriptFragmentType.Unknown)
						{
							continue;
						}

						Name = Token.Value.Span[EndCommandPos..].Trim().ToString();
						LineNumber = Token.InputLine;
						SaveState.AbandonState();
						break;
					}

					// Scan for the fragment body
					int BodyStartPos = Reader.InputPos;
					int BodyEndPos = BodyStartPos;
					while (true)
					{
						using var SaveState = new UhtTokenSaveState(Reader);
						UhtToken Token = Reader.GetLine();
						if (Token.TokenType == UhtTokenType.EndOfFile)
						{
							done = true;
							break;
						}
						if (Token.Value.Span.Length > 0 && Token.Value.Span[0] == '!')
						{
							break;
						}
						BodyEndPos = Reader.InputPos;
						SaveState.AbandonState();
					}

					ScriptFragments.Add(new ScriptFragment
					{
						Type = Type,
						Name = Name.Replace("\\\\", "\\"), // Be kind to people cut/copy/paste escaped strings around
						LineNumber = LineNumber,
						Header = new StringView(ScriptSourceFile.Data.Memory[HeaderStartPos..HeaderEndPos]),
						Body = new StringView(ScriptSourceFile.Data.Memory[BodyStartPos..BodyEndPos]),
						External = false,
					});
				}

				// Search for the manifest and any output.  Add fragments to the session
				for (int i = 0; i < ScriptFragments.Count; ++i)
				{
					switch (ScriptFragments[i].Type)
					{
						case ScriptFragmentType.Manifest:
							if (ManifestIndex != -1)
							{
								ScriptSourceFile.LogError(ScriptFragments[i].LineNumber, "There can be only one manifest section in a test script");
								break;
							}
							ManifestIndex = i;
							if (ScriptFragments[i].Name.Length == 0)
							{
								ScriptSourceFile.LogError(ScriptFragments[i].LineNumber, "Manifest name can not be blank");
								break;
							}
							TestFileManager.AddSourceFragment(ScriptSourceFile, ScriptFragments[i].Name, ScriptFragments[i].LineNumber, ScriptFragments[i].Body);
							break;

						case ScriptFragmentType.Console:
							if (ConsoleIndex != -1)
							{
								ScriptSourceFile.LogError(ScriptFragments[i].LineNumber, "There can be only one console section in a test script");
								break;
							}
							ConsoleIndex = i;
							break;

						case ScriptFragmentType.Header:
							if (ScriptFragments[i].Name.Length == 0)
							{
								ScriptSourceFile.LogError(ScriptFragments[i].LineNumber, "Header name can not be blank");
								break;
							}
							if (ScriptFragments[i].Body.Length == 0)
							{
								// Read the NoExportTypes.h file from the engine source so we don't have to keep a copy
								if (Path.GetFileName(ScriptFragments[i].Name).Equals("NoExportTypes.h", StringComparison.OrdinalIgnoreCase))
								{
									string ExternalPath = Path.Combine(Unreal.EngineDirectory.FullName, ScriptFragments[i].Name);
									if (File.Exists(ExternalPath))
									{
										ScriptFragment Copy = ScriptFragments[i];
										Copy.Body = new StringView(File.ReadAllText(ExternalPath));
										Copy.External = true;
										ScriptFragments[i] = Copy;
									}
								}
							}
							TestFileManager.AddSourceFragment(ScriptSourceFile, ScriptFragments[i].Name, ScriptFragments[i].LineNumber, ScriptFragments[i].Body);
							break;

						case ScriptFragmentType.Output:
							OutputFragments.Add(ScriptFragments[i].Name, i);
							break;
					}
				}

				if (ManifestIndex == -1)
				{
					ScriptSourceFile.LogError("There must be a manifest section in a test script");
				}

				if (ConsoleIndex == -1)
				{
					ScriptSourceFile.LogError("There must be a console section in a test script");
				}
			});

			// Run UHT
			if (!Session.HasErrors)
			{
				Session.Run(ScriptFragments[ManifestIndex].Name);
			}

			// If we have no console index, then there is nothing we can do.  This is a fatal error than can not be tested
			bool bSuccess = true;
			if (ConsoleIndex == -1)
			{
				ScriptSourceFile.LogError("Unable to do any verification without a console section");
				Session.LogMessages();
				File.Copy(InPath, OutPath, true);
				bSuccess = false;
			}
			else
			{

				// Generate the console block
				List<string> ConsoleLines = Session.CollectMessages();
				StringBuilder SBConsole = new();
				foreach (string Line in ConsoleLines)
				{
					SBConsole.AppendLine(Line);
				}

				// Verify the console block 
				// We trim the ends because it is too easy to leave off the ending CRLF in the script file.
				if (ScriptFragments[ConsoleIndex].Body.ToString().TrimEnd() != SBConsole.ToString().TrimEnd())
				{
					Logger.LogError("Console output failed to match");
					bSuccess = false;
				}

				// Check the output
				foreach (KeyValuePair<string, string> KVP in TestFileManager.Outputs)
				{
					if (OutputFragments.TryGetValue(KVP.Key, out int Index))
					{
						if (ScriptFragments[Index].Body.ToString().TrimEnd() != KVP.Value.TrimEnd())
						{
							Logger.LogError("Output \"{Key}\" failed to match", KVP.Key);
							bSuccess = false;
						}
						OutputFragments.Remove(KVP.Key);
					}
					else
					{
						Logger.LogError("Output \"{Key}\" not found in test script", KVP.Key);
					}
				}
				foreach (KeyValuePair<string, int> KVP in OutputFragments)
				{
					Logger.LogError("Output \"{Key}\" in test script but not generated", KVP.Key);
				}

				// Create the complete output.  Output includes all of the source fragments and console fragments
				// and followed the output data sorted by file name.
				StringBuilder SBTest = new();
				for (int i = 0; i < ScriptFragments.Count; ++i)
				{
					if (ScriptFragments[i].Type != ScriptFragmentType.Output)
					{
						SBTest.Append(ScriptFragments[i].Header);
						if (i == ConsoleIndex)
						{
							SBTest.Append(SBConsole);
						}
						else if (!ScriptFragments[i].External)
						{
							SBTest.Append(ScriptFragments[i].Body);
						}
					}
				}

				// Add the output
				foreach (KeyValuePair<string, string> KVP in TestFileManager.Outputs)
				{
					SBTest.Append($"!output {KVP.Key}\r\n");
					SBTest.Append(KVP.Value);
				}

				// Write the final content
				try
				{
					File.WriteAllText(OutPath, SBTest.ToString());
				}
				catch (Exception E)
				{
					Logger.LogError(E, "Unable to write test result to \"{Ex}\"", E.Message);
				}
			}

			if (bSuccess)
			{
				Logger.LogInformation("Test {InPath} succeeded", InPath);
			}
			else
			{
				Logger.LogError("Test {InPath} failed", InPath);
			}
			return bSuccess;
		}

		private static bool RunScriptTests(UhtTables Tables, IUhtConfig Config, UhtGlobalOptions Options, string TestDirectory, string TestOutputDirectory, List<string> Scripts, ILogger Logger)
		{
			bool bResult = true;
			foreach (string Script in Scripts)
			{
				bResult &= RunScriptTest(Tables, Config, Options, TestDirectory, TestOutputDirectory, Script, Logger);
			}
			return bResult;
		}

		private static bool RunDirectoryTests(UhtTables Tables, IUhtConfig Config, UhtGlobalOptions Options, string TestDirectory, string TestOutputDirectory, List<string> Directories, ILogger Logger)
		{
			bool bResult = true;
			foreach (string Directory in Directories)
			{
				bResult &= RunTests(Tables, Config, Options, Path.Combine(TestDirectory, Directory), Path.Combine(TestOutputDirectory, Directory), Logger);
			}
			return bResult;
		}

		private static bool RunTests(UhtTables Tables, IUhtConfig Config, UhtGlobalOptions Options, string TestDirectory, string TestOutputDirectory, ILogger Logger)
		{
			// Create output directory
			Directory.CreateDirectory(TestOutputDirectory);

			List<string> Scripts = new();
			foreach (string Script in Directory.EnumerateFiles(TestDirectory, "*.uhttest"))
			{
				Scripts.Add(Path.GetFileName(Script));
			}
			Scripts.Sort(StringComparer.OrdinalIgnoreCase);

			List<string> Directories = new();
			foreach (string Directory in Directory.EnumerateDirectories(TestDirectory))
			{
				Directories.Add(Path.GetFileName(Directory));
			}
			Directories.Sort(StringComparer.OrdinalIgnoreCase);

			List<string> Manifests = new();
			foreach (string Manifest in Directory.EnumerateFiles(TestDirectory, "*.uhtmanifest"))
			{
				Manifests.Add(Path.GetFileName(Manifest));
			}
			Manifests.Sort(StringComparer.OrdinalIgnoreCase);

			return
				RunScriptTests(Tables, Config, Options, TestDirectory, TestOutputDirectory, Scripts, Logger) &&
				RunDirectoryTests(Tables, Config, Options, TestDirectory, TestOutputDirectory, Directories, Logger);
		}

		public static bool RunTests(UhtTables Tables, IUhtConfig Config, UhtGlobalOptions Options, ILogger Logger)
		{
			DirectoryReference EngineSourceProgramDirectory = DirectoryReference.Combine(Unreal.EngineDirectory, "Source", "Programs");
			string TestDirectory = FileReference.Combine(EngineSourceProgramDirectory, "UnrealBuildTool.Tests", "UHT").FullName;
			string TestOutputDirectory = FileReference.Combine(EngineSourceProgramDirectory, "UnrealBuildTool.Tests", "UHT.Out").FullName;

			// Clear the output directory
			try
			{
				Directory.Delete(TestOutputDirectory, true);
			}
			catch (Exception)
			{ }

			// Collect a list of all the test scripts
			Logger.LogInformation("Running tests in {TestDirectory}", TestDirectory);
			Logger.LogInformation("Output can be compared in {TestOutputDirectory}", TestOutputDirectory);

			// Run the tests on the directory
			return RunTests(Tables, Config, Options, TestDirectory, TestOutputDirectory, Logger);
		}
	}

	/// <summary>
	/// Invoke UHT
	/// </summary>
	[ToolMode("UnrealHeaderTool", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.ShowExecutionTime)]
	class UnrealHeaderToolMode : ToolMode
	{
		/// <summary>
		/// Directory for saved application settings (typically Engine/Programs)
		/// </summary>
		static DirectoryReference? CachedEngineProgramSavedDirectory;

		/// <summary>
		/// The engine programs directory
		/// </summary>
		public static DirectoryReference EngineProgramSavedDirectory
		{
			get
			{
				if (CachedEngineProgramSavedDirectory == null)
				{
					if (Unreal.IsEngineInstalled())
					{
						CachedEngineProgramSavedDirectory = Utils.GetUserSettingDirectory() ?? DirectoryReference.Combine(Unreal.EngineDirectory, "Programs");
					}
					else
					{
						CachedEngineProgramSavedDirectory = DirectoryReference.Combine(Unreal.EngineDirectory, "Programs");
					}
				}
				return CachedEngineProgramSavedDirectory;
			}
		}

		/// <summary>
		/// Print (incomplete) usage information
		/// </summary>
		/// <param name="ExporterTable">Defined exporters</param>
		/// <param name="Config">Configuration</param>
		private static void PrintUsage(UhtExporterTable ExporterTable, IUhtConfig Config)
		{
			Console.WriteLine("UnrealBuildTool -Mode=UnrealHeaderTool [ProjectFile ManifestFile] -OR [\"-Target...\"] [Options]");
			Console.WriteLine("");
			Console.WriteLine("Options:");
			int LongestPrefix = 0;
			foreach (FieldInfo Info in typeof(UhtGlobalOptions).GetFields())
			{
				foreach (CommandLineAttribute Att in Info.GetCustomAttributes<CommandLineAttribute>())
				{
					if (Att.Prefix != null && Att.Description != null)
					{
						LongestPrefix = Att.Prefix.Length > LongestPrefix ? Att.Prefix.Length : LongestPrefix;
					}
				}
			}

			foreach (UhtExporter Generator in ExporterTable)
			{
				LongestPrefix = Generator.Name.Length + 2 > LongestPrefix ? Generator.Name.Length + 2 : LongestPrefix;
			}

			foreach (FieldInfo Info in typeof(UhtGlobalOptions).GetFields())
			{
				foreach (CommandLineAttribute Att in Info.GetCustomAttributes<CommandLineAttribute>())
				{
					if (Att.Prefix != null && Att.Description != null)
					{
						Console.WriteLine($"  {Att.Prefix.PadRight(LongestPrefix)} :  {Att.Description}");
					}
				}
			}

			Console.WriteLine("");
			Console.WriteLine("Generators: Prefix with 'no' to disable a generator");
			foreach (UhtExporter Generator in ExporterTable)
			{
				string IsDefault = Config.IsExporterEnabled(Generator.Name) || Generator.Options.HasAnyFlags(UhtExporterOptions.Default) ? " (Default)" : "";
				Console.WriteLine($"  -{Generator.Name.PadRight(LongestPrefix)} :  {Generator.Description}{IsDefault}");
			}
			Console.WriteLine("");
		}

		/// <summary>
		/// Execute the command
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code</returns>
		/// <param name="Logger"></param>
		public override int Execute(CommandLineArguments Arguments, ILogger Logger)
		{
			try
			{

				// Initialize the attributes
				UhtTables Tables = new();

				// Initialize the config
				IUhtConfig Config = new UhtConfigImpl(Arguments);

				// Parse the global options
				UhtGlobalOptions Options = new(Arguments);
				int TargetArgumentIndex = -1;
				if (Arguments.GetPositionalArgumentCount() == 0)
				{
					for (int Index = 0; Index < Arguments.Count; ++Index)
					{
						if (Arguments[Index].StartsWith("-Target", StringComparison.OrdinalIgnoreCase))
						{
							TargetArgumentIndex = Index;
							break;
						}
					}
				}
				int RequiredArgCount = TargetArgumentIndex >= 0 || Options.bTest ? 0 : 2;
				if (Arguments.GetPositionalArgumentCount() != RequiredArgCount || Options.bGetHelp)
				{
					PrintUsage(Tables.ExporterTable, Config);
					return Options.bGetHelp ? (int)CompilationResult.Succeeded : (int)CompilationResult.OtherCompilationError;
				}

				// Configure the log system
				Log.OutputLevel = Options.LogOutputLevel;
				Log.IncludeTimestamps = Options.bLogTimestamps;
				Log.IncludeProgramNameWithSeverityPrefix = Options.bLogFromMsBuild;

				// Add the log writer if requested. When building a target, we'll create the writer for the default log file later.
				if (!Options.bNoLog)
				{
					if (Options.LogFileName != null)
					{
						Log.AddFileWriter("LogTraceListener", Options.LogFileName);
					}

					if (!Log.HasFileWriter())
					{
						string BaseLogFileName = FileReference.Combine(EngineProgramSavedDirectory, "UnrealHeaderTool", "Saved", "Logs", "UnrealHeaderTool.log").FullName;

						FileReference LogFile = new(BaseLogFileName);
						foreach (string LogSuffix in Arguments.GetValues("-LogSuffix="))
						{
							LogFile = LogFile.ChangeExtension(null) + "_" + LogSuffix + LogFile.GetExtension();
						}

						Log.AddFileWriter("DefaultLogTraceListener", LogFile);
					}
				}

				// If we are running test scripts
				if (Options.bTest)
				{
					return UhtTestHarness.RunTests(Tables, Config, Options, Logger) ? (int)CompilationResult.Succeeded : (int)CompilationResult.OtherCompilationError;
				}

				string? ProjectFile = null;
				string? ManifestPath = null;

				if (TargetArgumentIndex >= 0)
				{
					CommandLineArguments LocalArguments = new(new string[] { Arguments[TargetArgumentIndex] });
					List<TargetDescriptor> TargetDescriptors = TargetDescriptor.ParseCommandLine(LocalArguments, false, false, false, Logger);
					if (TargetDescriptors.Count == 0)
					{
						Logger.LogError("No target descriptors found.");
						return (int)CompilationResult.OtherCompilationError;
					}

					TargetDescriptor TargetDesc = TargetDescriptors[0];

					// Create the target
					UEBuildTarget Target = UEBuildTarget.Create(TargetDesc, false, false, false, Logger);

					// Create the makefile for the target and export the module information
					using ISourceFileWorkingSet WorkingSet = new EmptySourceFileWorkingSet();

					// Create the build configuration object, and read the settings
					BuildConfiguration BuildConfiguration = new();
					XmlConfig.ApplyTo(BuildConfiguration);
					Arguments.ApplyTo(BuildConfiguration);

					// Create the makefile
					TargetMakefile Makefile = Target.Build(BuildConfiguration, WorkingSet, TargetDesc, Logger, true);

					FileReference ModuleInfoFileName = ExternalExecution.GetUHTModuleInfoFileName(Makefile, Target.TargetName);
					FileReference DepsFileName = ExternalExecution.GetUHTDepsFileName(ModuleInfoFileName);
					ManifestPath = ModuleInfoFileName.FullName;
					ExternalExecution.WriteUHTManifest(Makefile, Target.TargetName, ModuleInfoFileName, DepsFileName);

					if (Target.ProjectFile != null)
					{
						ProjectFile = Target.ProjectFile.FullName;
					}
				}
				else
				{
					ProjectFile = Arguments.GetPositionalArguments()[0];
					ManifestPath = Arguments.GetPositionalArguments()[1];
				}

				string? ProjectPath = ProjectFile != null ? Path.GetDirectoryName(ProjectFile) : null;

				UhtSession Session = new()
				{
					Logger = Logger,
					Tables = Tables,
					Config = Config,
					FileManager = new UhtStdFileManager(),
					EngineDirectory = Unreal.EngineDirectory.FullName,
					ProjectFile = ProjectFile,
					ProjectDirectory = string.IsNullOrEmpty(ProjectPath) ? null : ProjectPath,
					ReferenceDirectory = FileReference.Combine(EngineProgramSavedDirectory, "UnrealHeaderTool", "Saved", "ReferenceExports").FullName,
					VerifyDirectory = FileReference.Combine(EngineProgramSavedDirectory, "UnrealHeaderTool", "Saved", "VerifyExports").FullName,
					WarningsAsErrors = Options.bWarningsAsErrors,
					GoWide = !Options.bNoGoWide,
					FailIfGeneratedCodeChanges = Options.bFailIfGeneratedCodeChanges,
					NoOutput = Options.bNoOutput,
					IncludeDebugOutput = Options.bIncludeDebugOutput,
					NoDefaultExporters = Options.bNoDefaultExporters,
				};

				if (Options.bWriteRef)
				{
					Session.ReferenceMode = UhtReferenceMode.Reference;
				}
				else if (Options.bVerifyRef)
				{
					Session.ReferenceMode = UhtReferenceMode.Verify;
				}

				foreach(UhtExporter Exporter in Session.ExporterTable)
				{
					if (Arguments.HasOption($"-{Exporter.Name}"))
					{
						Session.SetExporterStatus(Exporter.Name, true);
					}
					else if (Arguments.HasOption($"-no{Exporter.Name}"))
					{
						Session.SetExporterStatus(Exporter.Name, false);
					}
				}

				// Read and parse
				Session.Run(ManifestPath!);
				Session.LogMessages();
				return (int)(Session.HasErrors ? CompilationResult.OtherCompilationError : CompilationResult.Succeeded);
			}
			catch (Exception Ex)
			{
				// Unhandled exception.
				Logger.LogError(Ex, "Unhandled exception: {Ex}", ExceptionUtils.FormatException(Ex));
				Logger.LogDebug(Ex, "Unhandled exception: {Ex}", ExceptionUtils.FormatExceptionDetails(Ex));
				return (int)CompilationResult.OtherCompilationError;
			}
		}
	}
}
