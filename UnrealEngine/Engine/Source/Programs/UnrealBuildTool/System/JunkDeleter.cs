// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	class JunkDeleter
	{
		/// <summary>
		/// Loads JunkManifest.txt file and removes all junk files/folders defined in it.
		/// </summary>
		public static void DeleteJunk(ILogger Logger)
		{
			List<string> JunkManifest = LoadJunkManifest();
			DeleteAllJunk(JunkManifest, Logger);
		}

		/// <summary>
		/// Loads JunkManifest.txt file.
		/// </summary>
		/// <returns>Junk manifest file contents.</returns>
		private static List<string> LoadJunkManifest()
		{
			string ManifestPath = ".." + Path.DirectorySeparatorChar + "Build" + Path.DirectorySeparatorChar + "JunkManifest.txt";
			List<string> JunkManifest = new List<string>();
			if (File.Exists(ManifestPath))
			{
				string MachineName = Unreal.MachineName;
				using (StreamReader reader = new StreamReader(ManifestPath))
				{
					string CurrentToRootDir = ".." + Path.DirectorySeparatorChar + "..";
					string? LineRead;
					while ((LineRead = reader.ReadLine()) != null)
					{
						string JunkEntry = LineRead.Trim();
						if (String.IsNullOrEmpty(JunkEntry) == false)
						{
							string[] Tokens = JunkEntry.Split(":".ToCharArray());
							bool bIsValidJunkLine = true;
							foreach (string Token in Tokens)
							{
								if (Token.StartsWith("Machine=", StringComparison.InvariantCultureIgnoreCase) == true)
								{
									string[] InnerTokens = Token.Split("=".ToCharArray());
									// check if the machine name on the line matches the current machine name, if not, we don't apply this junk
									if (InnerTokens.Length == 2 && MachineName.StartsWith(InnerTokens[1]) == false)
									{
										// Not meant for this machine
										bIsValidJunkLine = false;
									}
								}
								else if (Token.StartsWith("Platform=", StringComparison.InvariantCultureIgnoreCase) == true)
								{
									string[] InnerTokens = Token.Split("=".ToCharArray());
									// check if the machine name on the line matches the current machine name, if not, we don't apply this junk
									if (InnerTokens.Length == 2)
									{
										UnrealTargetPlatform ParsedPlatform;
										// if the platform is valid, then we want to keep the files, which means that we don't want to apply the junk line
										if (UnrealTargetPlatform.TryParse(InnerTokens[1], out ParsedPlatform))
										{
											if (UEBuildPlatform.TryGetBuildPlatform(ParsedPlatform, out _))
											{
												// this is a good platform, so don't delete any files!
												bIsValidJunkLine = false;
											}
										}
									}
								}
							}

							// All paths within the manifest are Unreal root directory relative.
							// UBT's working directory is Engine\Source so add "..\..\" to each of the entires.
							if (bIsValidJunkLine)
							{
								// the entry is always the last element in the token array (after the final :)
								string FixedPath = Path.Combine(CurrentToRootDir, Tokens[Tokens.Length - 1]);
								FixedPath = FixedPath.Replace('\\', Path.DirectorySeparatorChar);
								JunkManifest.Add(FixedPath);
							}
						}
					}
				}
			}
			return JunkManifest;
		}

		/// <summary>
		/// Goes through each entry from the junk manifest and deletes it.
		/// </summary>
		/// <param name="JunkManifest">JunkManifest.txt entries.</param>
		/// <param name="Logger">Logger for output</param>
		private static void DeleteAllJunk(List<string> JunkManifest, ILogger Logger)
		{
			foreach (string Junk in JunkManifest)
			{
				if (IsFile(Junk))
				{
					string FileName = Path.GetFileName(Junk);
					if (FileName.Contains('*'))
					{
						// Wildcard search and delete
						string DirectoryToLookIn = Path.GetDirectoryName(Junk)!;
						if (Directory.Exists(DirectoryToLookIn))
						{
							// Delete all files within the specified folder
							string[] FilesToDelete = Directory.GetFiles(DirectoryToLookIn, FileName, SearchOption.TopDirectoryOnly);
							foreach (string JunkFile in FilesToDelete)
							{
								DeleteFile(JunkFile, Logger);
							}

							// Delete all subdirectories with the specified folder
							string[] DirectoriesToDelete = Directory.GetDirectories(DirectoryToLookIn, FileName, SearchOption.TopDirectoryOnly);
							foreach (string JunkFolder in DirectoriesToDelete)
							{
								DeleteDirectory(JunkFolder, Logger);
							}
						}
					}
					else
					{
						// Delete single file
						DeleteFile(Junk, Logger);
					}
				}
				else if (Directory.Exists(Junk))
				{
					// Delete the selected folder and all its contents
					DeleteDirectory(Junk, Logger);
				}
			}
		}

		private static bool IsFile(string PathToCheck)
		{
			string FileName = Path.GetFileName(PathToCheck);
			if (String.IsNullOrEmpty(FileName) == false)
			{
				if (FileName.Contains('*'))
				{
					// Assume wildcards are file because the path will be searched for files and directories anyway.
					return true;
				}
				else
				{
					return File.Exists(PathToCheck);
				}
			}
			else
			{
				return false;
			}
		}

		/// <summary>
		/// Deletes a directory recursively gracefully handling all exceptions.
		/// </summary>
		/// <param name="DirectoryPath">Path.</param>
		/// <param name="Logger">Logger for output</param>
		private static void DeleteDirectory(string DirectoryPath, ILogger Logger)
		{
			try
			{
				Logger.LogInformation("Deleting junk directory: \"{Dir}\".", DirectoryPath);
				Directory.Delete(DirectoryPath, true);
			}
			catch (Exception Ex)
			{
				// Ignore all exceptions
				Logger.LogInformation("Unable to delete junk directory: \"{Dir}\". Error: {Ex}", DirectoryPath, Ex.Message.TrimEnd());
			}
		}

		/// <summary>
		/// Deletes a file gracefully handling all exceptions.
		/// </summary>
		/// <param name="Filename">Filename.</param>
		/// <param name="Logger">Logger for output</param>
		private static void DeleteFile(string Filename, ILogger Logger)
		{
			try
			{
				Logger.LogInformation("Deleting junk file: \"{File}\".", Filename);
				File.Delete(Filename);
			}
			catch (Exception Ex)
			{
				// Ingore all exceptions
				Logger.LogInformation("Unable to delete junk file: \"{File}\". Error: {Ex}", Filename, Ex.Message.TrimEnd());
			}
		}
	}
}
