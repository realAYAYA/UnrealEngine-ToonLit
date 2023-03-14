// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Console;
using P4VUtils.Commands;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using System.Xml;

namespace P4VUtils
{
	class CustomToolInfo
	{
		public string Name { get; set; }
		public string Arguments { get; set; }
		public bool AddToContextMenu { get; set; } = true;
		public bool ShowConsole { get; set; }
		public bool RefreshUI { get; set; } = true;
		public string Shortcut { get; set; } = "";
		public bool PromptForArgument { get; set; }
		public string PromptText { get; set; } = "";

		public CustomToolInfo(string Name, string Arguments)
		{
			this.Name = Name;
			this.Arguments = Arguments;
		}
	}

	abstract class Command
	{
		public abstract string Description { get; }

		public abstract CustomToolInfo CustomTool { get; }

		public abstract Task<int> Execute(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger);
	}

	class Program
	{
		// UEHelpersInRoot - commands that help with common but simple operations
		public static IReadOnlyDictionary<string, Command> RootHelperCommands { get; } = new Dictionary<string, Command>(StringComparer.OrdinalIgnoreCase)
		{
			["describe"] = new DescribeCommand(),
			["copyclnum"] = new CopyCLCommand(),
			["findlastedit"] = new FindLastEditCommand(),
			["findlasteditbyline"] = new P4BlameCommand(),
		};

		// UESubmit - commands that help with submitting files/changelists
		public static IReadOnlyDictionary<string, Command> SubmissionCommands { get; } = new Dictionary<string, Command>(StringComparer.OrdinalIgnoreCase)
		{
			["submitandvirtualize"] = new SubmitAndVirtualizeCommand(),
		};

		// UEHelpers - commands that help with common but simple operations
		public static IReadOnlyDictionary<string, Command> ToolboxCommands { get; } = new Dictionary<string, Command>(StringComparer.OrdinalIgnoreCase)
		{
			["backout"] = new BackoutCommand(),
			["snapshot"] = new SnapshotCommand(),
			["reconcilecode"] = new FastReconcileCodeEditsCommand(),
			["reconcileall"] = new FastReconcileAllEditsCommand(),
			["unshelvetocurrentrevision"] = new UnshelveToCurrentRevision(),
			["unshelvemakedatawritable"] = new UnshelveMakeDataWritable(),
			["convertcldatatolocalwritable"] = new ConvertCLDataToLocalWritable(),
			["convertdatatolocalwritable"] = new ConvertDataToLocalWritable(),
		};

		// UEIntegrate Folder commands - complex commands to facilitate integrations/backout
		public static IReadOnlyDictionary<string, Command> IntegrateCommands { get; } = new Dictionary<string, Command>(StringComparer.OrdinalIgnoreCase)
		{
			["cherrypick"] = new CherryPickCommand(),
			["converttoedit"] = new ConvertToEditCommand(),
			["edigrate"] = new EdigrateCommand(),
		};

		// UEHorde Folder - local build and horde preflights
		public static IReadOnlyDictionary<string, Command> HordeCommands { get; } = new Dictionary<string, Command>(StringComparer.OrdinalIgnoreCase)
		{
			["compile"] = new CompileCommand(),
			["preflight"] = new PreflightCommand(),
			["preflightandsubmit"] = new PreflightAndSubmitCommand(),
			["movewriteablepreflightandsubmit"] = new MoveWriteableFilesthenPreflightAndSubmitCommand(),
		};

		public static IDictionary<string, Command> Commands = SubmissionCommands.Concat(RootHelperCommands).Concat(ToolboxCommands).Concat(IntegrateCommands).Concat(HordeCommands).ToDictionary(p => p.Key, p => p.Value, StringComparer.OrdinalIgnoreCase);

		static void PrintHelp(ILogger Logger)
		{
			Logger.LogInformation("P4VUtils");
			Logger.LogInformation("Provides useful shortcuts for working with P4V");
			Logger.LogInformation("");
			Logger.LogInformation("Usage:");
			Logger.LogInformation("  P4VUtils [Command] [Arguments...]");
			Logger.LogInformation("");

			List<KeyValuePair<string, string>> Table = new List<KeyValuePair<string, string>>();
			foreach (KeyValuePair<string, Command> Pair in Commands)
			{
				Table.Add(new KeyValuePair<string, string>(Pair.Key, Pair.Value.Description));
			}

			Logger.LogInformation("Commands:");
			HelpUtils.PrintTable(Table, 2, 15, ConsoleUtils.WindowWidth - 1, Logger);
		}

		static async Task<int> Main(string[] Args)
		{
			using ILoggerFactory Factory = LoggerFactory.Create(Builder => Builder.AddEpicDefault());//.AddSimpleConsole(Options => { Options.SingleLine = true; Options.IncludeScopes = false; }));
			ILogger Logger = Factory.CreateLogger<Program>();
			Log.SetInnerLogger(Logger);

			try
			{
				return await InnerMain(Args, Logger);
			}
			catch (Exception Ex)
			{
				Logger.LogError(Ex, "Unhandled exception: {Ex}", Ex.ToString());
				return 1;
			}
		}

		static async Task<int> InnerMain(string[] Args, ILogger Logger)
		{
			if (Args.Length == 0 || Args[0].Equals("-help", StringComparison.OrdinalIgnoreCase))
			{
				PrintHelp(Logger);
				return 0;
			}
			else if (Args[0].StartsWith("-", StringComparison.Ordinal))
			{
				Logger.LogInformation("Missing command name");
				PrintHelp(Logger);
				return 1;
			}
			else if (Args[0].Equals("install", StringComparison.OrdinalIgnoreCase))
			{
				Logger.LogInformation("Adding custom tools...");
				return await UpdateCustomToolRegistration(true, Logger);
			}
			else if (Args[0].Equals("uninstall", StringComparison.OrdinalIgnoreCase))
			{
				Logger.LogInformation("Removing custom tools...");
				return await UpdateCustomToolRegistration(false, Logger);
			}
			else if (Commands.TryGetValue(Args[0], out Command? Command))
			{
				if (Args.Any(x => x.Equals("-help", StringComparison.OrdinalIgnoreCase)))
				{
					List<KeyValuePair<string, string>> Parameters = CommandLineArguments.GetParameters(Command.GetType());
					Logger.LogInformation("{Title}", Args[0]);
					Logger.LogInformation("{Description}", Command.GetType());
					Logger.LogInformation("Parameters:");
					HelpUtils.PrintTable(Parameters, 4, 24, HelpUtils.WindowWidth - 1, Logger);
					return 0;
				}

				Dictionary<string, string> ConfigValues = ReadConfig();
				return await Command.Execute(Args, ConfigValues, Logger);
			}
			else
		{
				Logger.LogError("Unknown command: {Command}", Args[0]);
				PrintHelp(Logger);
				return 1;
			}
		}

		static Dictionary<string, string> ReadConfig()
		{
			Dictionary<string, string> ConfigValues = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

			string BasePath = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location)!;
			AppendConfig(Path.Combine(BasePath, "P4VUtils.ini"), ConfigValues);
			AppendConfig(Path.Combine(BasePath, "NotForLicensees", "P4VUtils.ini"), ConfigValues);

			return ConfigValues;
		}

		static void AppendConfig(string SourcePath, Dictionary<string, string> ConfigValues)
		{
			if (File.Exists(SourcePath))
			{
				string[] Lines = File.ReadAllLines(SourcePath);
				foreach (string Line in Lines)
				{
					int EqualsIdx = Line.IndexOf('=', StringComparison.Ordinal);
					if (EqualsIdx != -1)
					{
						string Key = Line.Substring(0, EqualsIdx).Trim();
						string Value = Line.Substring(EqualsIdx + 1).Trim();
						ConfigValues[Key] = Value;
					}
				}
			}
		}

		public static bool TryLoadXmlDocument(FileReference Location, XmlDocument Document)
		{
			if (FileReference.Exists(Location))
			{
				try
				{
					Document.Load(Location.FullName);
					return true;
				}
				catch
				{
				}
			}
			return false;
		}

		static string GetToolName(XmlElement ToolNode)
		{
			return ToolNode.SelectSingleNode("Definition")?.SelectSingleNode("Name")?.InnerText ?? string.Empty;
		}

		// returns true if all tools were removed
		static bool RemoveCustomToolsFromNode(XmlElement RootNode, FileReference DotNetLocation, FileReference AssemblyLocation, ILogger Logger)
		{
			int ToolsChecked = 0;
			int ToolsRemoved = 0;

			XmlNodeList? CustomToolDefList = RootNode.SelectNodes("CustomToolDef");
			if (CustomToolDefList == null)
			{
				return false;
			}

			// Removes tools explicitly calling the assembly location identified above - i assume as a way to "filter" only those we explicitly added (@Ben.Marsh) - nochecking, remove this comment once verified.
			foreach (XmlNode? ChildNode in CustomToolDefList)
			{
				XmlElement? ChildElement = ChildNode as XmlElement;
				if (ChildElement != null)
				{
					ToolsChecked++;
					XmlElement? CommandElement = ChildElement.SelectSingleNode("Definition/Command") as XmlElement;


					// In a recent change we started to output the Command element as a quoted argument if the path contains spaces.
					// FileReference does not resolve quoted string properly which was causing the comparisons here to fail.
					// We can strip the quotes before creating a FileReference to compare with DotNetLocation to ensure that the comparsion
					// is correct.
					String CommandPath = (CommandElement?.InnerText ?? String.Empty).StripQuoteArgument();

					if (new FileReference(CommandPath) == DotNetLocation)
					{
						XmlElement? ArgumentsElement = ChildElement.SelectSingleNode("Definition/Arguments") as XmlElement;
						if (ArgumentsElement != null)
						{
							string[] Arguments = CommandLineArguments.Split(ArgumentsElement.InnerText);
							if (Arguments.Length > 0 && new FileReference(Arguments[0]) == AssemblyLocation)
							{
								Logger.LogInformation("Removing Tool {ToolName}", GetToolName(ChildElement));
								RootNode.RemoveChild(ChildElement);
								ToolsRemoved++;
							}
						}
					}
				}
			}

			return ToolsChecked == ToolsRemoved;
		}

		static void InstallCommandsListInFolder(string FolderName, bool AddFolderToContextMenu, IReadOnlyDictionary<string, Command> InputCommmands, XmlDocument Document, FileReference DotNetLocation, FileReference AssemblyLocation, ILogger Logger)
		{
			// <CustomToolDefList>				// list of custom tools (top level)
			//  < CustomToolDef >				// loose custom tool in top level
			//	< CustomToolFolder>				// folder containing custom tools
			//		< Name > Test </ Name >
			//		< CustomToolDefList >		// list of custom tools in folder
			//		< CustomToolDef >			// definition of tool

			// This is the top level node, there will also be a per folder node added of same name
			XmlElement? Root = Document.SelectSingleNode("CustomToolDefList") as XmlElement;

			if (Root != null)
			{
				XmlElement FolderDefinition = Document.CreateElement("CustomToolFolder");

				XmlElement FolderDescription = Document.CreateElement("Name");
				FolderDescription.InnerText = FolderName;
				FolderDefinition.AppendChild(FolderDescription);

				XmlElement FolderToContextMenu = Document.CreateElement("AddToContext");
				FolderToContextMenu.InnerText = AddFolderToContextMenu ? "true" : "false";
				FolderDefinition.AppendChild(FolderToContextMenu);

				XmlElement FolderDefList = Document.CreateElement("CustomToolDefList");

				foreach (KeyValuePair<string, Command> Pair in InputCommmands)
				{
					CustomToolInfo CustomTool = Pair.Value.CustomTool;

					XmlElement ToolDef = Document.CreateElement("CustomToolDef");
					{
						XmlElement Definition = Document.CreateElement("Definition");
						{
							XmlElement Description = Document.CreateElement("Name");
							Description.InnerText = CustomTool.Name;
							Definition.AppendChild(Description);

							XmlElement Command = Document.CreateElement("Command");
							Command.InnerText = DotNetLocation.FullName.QuoteArgument();
							Definition.AppendChild(Command);

							XmlElement Arguments = Document.CreateElement("Arguments");
							Arguments.InnerText = $"{AssemblyLocation.FullName.QuoteArgument()} {Pair.Key} {CustomTool.Arguments}";
							Definition.AppendChild(Arguments);

							if (CustomTool.Shortcut.Length > 1)
							{
								XmlElement Shortcut = Document.CreateElement("Shortcut");
								Shortcut.InnerText = CustomTool.Shortcut;
								Definition.AppendChild(Shortcut);
							}
						}
						ToolDef.AppendChild(Definition);

						if (CustomTool.ShowConsole)
						{
							XmlElement Console = Document.CreateElement("Console");
							{
								XmlElement CloseOnExit = Document.CreateElement("CloseOnExit");
								CloseOnExit.InnerText = "false";
								Console.AppendChild(CloseOnExit);
							}
							ToolDef.AppendChild(Console);
						}

						if (CustomTool.RefreshUI)
						{
							XmlElement Refresh = Document.CreateElement("Refresh");
							Refresh.InnerText = CustomTool.RefreshUI ? "true" : "false";
							ToolDef.AppendChild(Refresh);
						}

						if (CustomTool.PromptForArgument)
						{
							XmlElement Prompt = Document.CreateElement("Prompt");
							{
								XmlElement PromptText = Document.CreateElement("PromptText");
								PromptText.InnerText = CustomTool.PromptText.Length > 0 ? CustomTool.PromptText : "Argument";
								Prompt.AppendChild(PromptText);
							}
							ToolDef.AppendChild(Prompt);
						}

						XmlElement AddToContext = Document.CreateElement("AddToContext");
						AddToContext.InnerText = CustomTool.AddToContextMenu ? "true" : "false";
						ToolDef.AppendChild(AddToContext);
					}
					FolderDefList.AppendChild(ToolDef);
				}

				FolderDefinition.AppendChild(FolderDefList);

				Root.AppendChild(FolderDefinition);
			}
		}
		static void RemoveCustomToolsFromFolders(XmlElement RootNode, FileReference DotNetLocation, FileReference AssemblyLocation, ILogger Logger)
		{
			XmlNodeList? CustomToolFolderList = RootNode.SelectNodes("CustomToolFolder");
			if(CustomToolFolderList == null)
			{
				return;
			}

			foreach (XmlNode? ChildNode in CustomToolFolderList)
			{
				if (ChildNode != null)
				{
					bool RemoveFolder = false;
					XmlElement? FolderRoot = ChildNode.SelectSingleNode("CustomToolDefList") as XmlElement;
					if (FolderRoot != null)
					{
						XmlElement? FolderNameNode = ChildNode.SelectSingleNode("Name") as XmlElement;
						string FolderNameString = "";
						if (FolderNameNode != null)
						{
							FolderNameString = FolderNameNode.InnerText;
						}
						Logger.LogInformation("Removing Tools from folder {Folder}", FolderNameString);
						RemoveFolder = RemoveCustomToolsFromNode(FolderRoot, DotNetLocation, AssemblyLocation, Logger);
					}

					if (RemoveFolder)
					{
						// remove the folder itself.
						RootNode.RemoveChild(ChildNode);
					}
				}
			}
		}
		public static async Task<int> UpdateCustomToolRegistration(bool bInstall, ILogger Logger)
		{
			DirectoryReference? ConfigDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile);
			if (ConfigDir == null)
			{
				Logger.LogError("Unable to find config directory.");
				return 1;
			}

			FileReference ConfigFile = FileReference.Combine(ConfigDir, ".p4qt", "customtools.xml");

			XmlDocument Document = new XmlDocument();
			if (!TryLoadXmlDocument(ConfigFile, Document))
			{
				DirectoryReference.CreateDirectory(ConfigFile.Directory);
				using (StreamWriter Writer = new StreamWriter(ConfigFile.FullName))
				{
					await Writer.WriteLineAsync(@"<?xml version=""1.0"" encoding=""UTF-8""?>");
					await Writer.WriteLineAsync(@"<!--perforce-xml-version=1.0-->");
					await Writer.WriteLineAsync(@"<CustomToolDefList varName=""customtooldeflist"">");
					await Writer.WriteLineAsync(@"</CustomToolDefList>");
				}
				Document.Load(ConfigFile.FullName);
			}


			FileReference DotNetLocation = FileReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.ProgramFiles)!, "dotnet", "dotnet.exe");
			FileReference AssemblyLocation = new FileReference(Assembly.GetExecutingAssembly().GetOriginalLocation());

			XmlElement? Root = Document.SelectSingleNode("CustomToolDefList") as XmlElement;
			if (Root == null)
			{
				Logger.LogError("Unknown schema for {ConfigFile}", ConfigFile);
				return 1;
			}

			// Remove Custom tools at the root
			RemoveCustomToolsFromNode(Root, DotNetLocation, AssemblyLocation, Logger);

			// Remove Custom tools in folders, and the folders
			RemoveCustomToolsFromFolders(Root, DotNetLocation, AssemblyLocation, Logger);

			// Insert new entries
			if (bInstall)
			{
				InstallCommandsListInFolder("UE RootHelpers", false/*AddFolderToContextMenu*/, RootHelperCommands, Document, DotNetLocation, AssemblyLocation, Logger);
				InstallCommandsListInFolder("UE Submit", true/*AddFolderToContextMenu*/, SubmissionCommands, Document, DotNetLocation, AssemblyLocation, Logger);
				InstallCommandsListInFolder("UE Toolbox", true/*AddFolderToContextMenu*/, ToolboxCommands, Document, DotNetLocation, AssemblyLocation, Logger);
				InstallCommandsListInFolder("UE Integrate", true/*AddFolderToContextMenu*/, IntegrateCommands, Document, DotNetLocation, AssemblyLocation, Logger);
				InstallCommandsListInFolder("UE Horde", true/*AddFolderToContextMenu*/, HordeCommands, Document, DotNetLocation, AssemblyLocation, Logger);
			}

			// Save the new document
			Document.Save(ConfigFile.FullName);
			Logger.LogInformation("Written {ConfigFile}", ConfigFile.FullName);
			return 0;
		}
	}
}
