// Copyright Epic Games, Inc.All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using AutomationTool;
using EpicGame;
using EpicGames.Core;

namespace LyraTest
{
	[Help("Updates the Audit_InCook collections.")]
	[RequireP4]
	public class Lyra_UpdateAuditCollections : BuildCommand
	{
		public override void ExecuteBuild()
		{
			LogInformation("************************* UpdateAuditCollections");

			// Now update what is in the InCook audit collection
			string CollectionName = "Audit_InCook";
			string ManifestFilename = "Manifest_UFSFiles_Win64.txt";

			// Attempt to find the UFS file list. Default to the log folder
			string UFSFilename = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, ManifestFilename);
			if (!CommandUtils.FileExists_NoExceptions(UFSFilename))
			{
				UFSFilename = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, "..", "..", ManifestFilename);
			}

			LogInformation("Attempting to use UFS manifest {0} (exists={1})", UFSFilename, CommandUtils.FileExists_NoExceptions(UFSFilename));

			UpdateInCookAuditCollection(CollectionName, UFSFilename);
		}


		static string AssetExtension = ".uasset";
		static string MapExtention = ".umap";

		static string EngineFolderName = "Engine/Content";
		static string EnginePluginFolderName = "Engine/Plugins/";

		//@TODO: Should derive these, otherwise it will break as soon as someone clones the projects
		static string GameFolderName = "Lyra/Content";
		static string GamePluginFolderName = "Lyra/Plugins/";
		static string GameProjectDirectory = "Samples/Games/Lyra";

		static public void UpdateInCookAuditCollection(string CollectionName, string UFSFilename)
		{
			if (!CommandUtils.FileExists_NoExceptions(UFSFilename))
			{
				LogWarning("Could not update audit collection, missing file: " + UFSFilename);
				return;
			}

			int WorkingCL = -1;
			if (CommandUtils.P4Enabled)
			{
				WorkingCL = CommandUtils.P4.CreateChange(CommandUtils.P4Env.Client, String.Format("Updated " + CollectionName + " collection using CL {0}", P4Env.Changelist));
			}

			var CollectionFilenameLocal = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, GameProjectDirectory, "Content", "Collections", CollectionName + ".collection");
			if (!InternalUtils.SafeCreateDirectory(Path.GetDirectoryName(CollectionFilenameLocal)))
			{
				LogWarning("Could not create directory {0}", Path.GetDirectoryName(CollectionFilenameLocal));
				return;
			}

			if (WorkingCL > 0)
			{
				var CollectionFilenameP4 = CommandUtils.CombinePaths(PathSeparator.Slash, CommandUtils.P4Env.Branch, GameProjectDirectory, "Content", "Collections", CollectionName + ".collection");
				if (!CommandUtils.FileExists_NoExceptions(CollectionFilenameLocal))
				{
					CommandUtils.P4.Add(WorkingCL, CollectionFilenameP4);
				}
				else
				{
					CommandUtils.P4.Edit(WorkingCL, CollectionFilenameP4);
				}
			}


			StreamReader ManifestFile = null;
			StreamWriter CollectionFile = null;
			try
			{
				CollectionFile = new StreamWriter(CollectionFilenameLocal);
				CollectionFile.WriteLine("FileVersion:1");
				CollectionFile.WriteLine("Type:Static");
				CollectionFile.WriteLine("");

				string Line = "";
				ManifestFile = new StreamReader(UFSFilename);
				while ((Line = ManifestFile.ReadLine()) != null)
				{
					string[] Tokens = Line.Split('\t');
					if (Tokens.Length > 1)
					{
						string UFSPath = Tokens[0];
						UFSPath = UFSPath.Trim('\"');
						bool bIsAsset = UFSPath.EndsWith(AssetExtension, StringComparison.InvariantCultureIgnoreCase);
						bool bIsMap = !bIsAsset && UFSPath.EndsWith(MapExtention, StringComparison.InvariantCultureIgnoreCase);
						if (bIsAsset || bIsMap)
						{
							bool bIsGame = UFSPath.StartsWith(GameFolderName);
							bool bIsEngine = UFSPath.StartsWith(EngineFolderName);
							bool bIsGamePlugin = UFSPath.StartsWith(GamePluginFolderName);
							bool bIsEnginePlugin = UFSPath.StartsWith(EnginePluginFolderName);
							if (bIsGame || bIsEngine || bIsGamePlugin || bIsEnginePlugin)
							{
								string ObjectPath = UFSPath;

								bool bValidPath = true;
								if (bIsGame)
								{
									ObjectPath = "/Game" + ObjectPath.Substring(GameFolderName.Length);
								}
								else if (bIsEngine)
								{
									ObjectPath = "/Engine" + ObjectPath.Substring(EngineFolderName.Length);
								}
								else if (bIsGamePlugin || bIsEnginePlugin)
								{
									int ContentIdx = ObjectPath.IndexOf("/Content/");
									if (ContentIdx != -1)
									{
										int PluginIdx = ObjectPath.LastIndexOf("/", ContentIdx - 1);
										if (PluginIdx == -1)
										{
											PluginIdx = 0;
										}
										else
										{
											// Skip the leading "/"
											PluginIdx++;
										}

										DirectoryReference PluginRoot = new DirectoryReference(ObjectPath.Substring(0, ContentIdx));
										string PluginName = "";
										foreach (FileReference PluginFile in CommandUtils.FindFiles("*.uplugin", false, PluginRoot))
										{
											PluginName = PluginFile.GetFileNameWithoutAnyExtensions();
											break;
										}
										if (string.IsNullOrEmpty(PluginName))
										{
											// Fallback to the directory name if the .uplugin file doesn't exist
											PluginName = ObjectPath.Substring(PluginIdx, ContentIdx - PluginIdx);
										}
										if (PluginName.Length > 0)
										{
											int PathStartIdx = ContentIdx + "/Content/".Length;
											ObjectPath = "/" + PluginName + "/" + ObjectPath.Substring(PathStartIdx);
										}
										else
										{
											LogWarning("Could not add asset to collection. No plugin name. Path:" + UFSPath);
											bValidPath = false;
										}
									}
									else
									{
										LogWarning("Could not add asset to collection. No content folder. Path:" + UFSPath);
										bValidPath = false;
									}
								}

								if (bValidPath)
								{
									string ObjectName = Path.GetFileNameWithoutExtension(ObjectPath);
									ObjectPath = Path.GetDirectoryName(ObjectPath) + "/" + ObjectName + "." + ObjectName;

									ObjectPath = ObjectPath.Replace('\\', '/');

									CollectionFile.WriteLine(ObjectPath);
								}
							}
						}
					}
				}

			}
			catch (Exception Ex)
			{
				CommandUtils.LogInformation("Did not update InCook collection. {0}", Ex.Message);
			}
			finally
			{
				if (ManifestFile != null)
				{
					ManifestFile.Close();
				}

				if (CollectionFile != null)
				{
					CollectionFile.Close();
				}
			}

			if (WorkingCL > 0)
			{
				// Check in the collection
				int SubmittedCL;
				CommandUtils.P4.Submit(WorkingCL, out SubmittedCL, true, true);
			}

		}
	}
}