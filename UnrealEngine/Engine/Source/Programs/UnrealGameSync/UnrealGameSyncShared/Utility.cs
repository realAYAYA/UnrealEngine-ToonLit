// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Drawing;
using System.IO;
using System.Linq;	
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	public sealed class UserErrorException : Exception
	{
		public int Code { get; }

		public UserErrorException(string message, int code = 1) : base(message)
		{
			this.Code = code;
		}
	}

	public class PerforceChangeDetails
	{
		public string Description;
		public bool ContainsCode;
		public bool ContainsContent;

		public PerforceChangeDetails(DescribeRecord describeRecord)
		{
			Description = describeRecord.Description;

			// Check whether the files are code or content
			foreach (DescribeFileRecord file in describeRecord.Files)
			{
				if (PerforceUtils.CodeExtensions.Any(extension => file.DepotFile.EndsWith(extension, StringComparison.OrdinalIgnoreCase)))
				{
					ContainsCode = true;
				}
				else
				{
					ContainsContent = true;
				}

				if (ContainsCode && ContainsContent)
				{
					break;
				}
			}
		}
	}

	public static class Utility
	{
		static JsonSerializerOptions GetDefaultJsonSerializerOptions()
		{
			JsonSerializerOptions options = new JsonSerializerOptions();
			options.AllowTrailingCommas = true;
			options.ReadCommentHandling = JsonCommentHandling.Skip;
			options.PropertyNameCaseInsensitive = true;
			options.Converters.Add(new JsonStringEnumConverter());
			return options;
		}

		public static JsonSerializerOptions DefaultJsonSerializerOptions { get; } = GetDefaultJsonSerializerOptions();

		public static bool TryLoadJson<T>(FileReference file, [NotNullWhen(true)] out T? obj) where T : class
		{
			if (!FileReference.Exists(file))
			{
				obj = null;
				return false;
			}

			try
			{
				obj = LoadJson<T>(file);
				return true;
			}
			catch
			{
				obj = null;
				return false;
			}
		}

		public static T LoadJson<T>(FileReference file)
		{
			byte[] data = FileReference.ReadAllBytes(file);
			return JsonSerializer.Deserialize<T>(data, DefaultJsonSerializerOptions)!;
		}

		public static void SaveJson<T>(FileReference file, T obj)
		{
			JsonSerializerOptions options = new JsonSerializerOptions { IgnoreNullValues = true, WriteIndented = true };
			options.Converters.Add(new JsonStringEnumConverter());

			byte[] buffer;
			using (MemoryStream stream = new MemoryStream())
			{
				using (Utf8JsonWriter writer = new Utf8JsonWriter(stream, new JsonWriterOptions { Indented = true }))
				{
					JsonSerializer.Serialize(writer, obj, options);
				}
				buffer = stream.ToArray();
			}

			FileReference.WriteAllBytes(file, buffer);
		}

		public static string GetPathWithCorrectCase(FileInfo info)
		{
			DirectoryInfo parentInfo = info.Directory;
			if(info.Exists)
			{
				return Path.Combine(GetPathWithCorrectCase(parentInfo), parentInfo.GetFiles(info.Name)[0].Name); 
			}
			else
			{
				return Path.Combine(GetPathWithCorrectCase(parentInfo), info.Name);
			}
		}

		public static string GetPathWithCorrectCase(DirectoryInfo info)
		{
			DirectoryInfo parentInfo = info.Parent;
			if(parentInfo == null)
			{
				return info.FullName.ToUpperInvariant();
			}
			else if(info.Exists)
			{
				return Path.Combine(GetPathWithCorrectCase(parentInfo), parentInfo.GetDirectories(info.Name)[0].Name);
			}
			else
			{
				return Path.Combine(GetPathWithCorrectCase(parentInfo), info.Name);
			}
		}

		public static void ForceDeleteFile(string fileName)
		{
			if(File.Exists(fileName))
			{
				File.SetAttributes(fileName, File.GetAttributes(fileName) & ~FileAttributes.ReadOnly);
				File.Delete(fileName);
			}
		}

		public static bool SpawnProcess(string fileName, string commandLine)
		{
			using(Process childProcess = new Process())
			{
				childProcess.StartInfo.FileName = fileName;
				childProcess.StartInfo.Arguments = String.IsNullOrEmpty(commandLine) ? "" : commandLine;
				childProcess.StartInfo.UseShellExecute = false;
				return childProcess.Start();
			}
		}

		public static bool SpawnHiddenProcess(string fileName, string commandLine)
		{
			using(Process childProcess = new Process())
			{
				childProcess.StartInfo.FileName = fileName;
				childProcess.StartInfo.Arguments = String.IsNullOrEmpty(commandLine) ? "" : commandLine;
				childProcess.StartInfo.UseShellExecute = false;
				childProcess.StartInfo.RedirectStandardOutput = true;
				childProcess.StartInfo.RedirectStandardError = true;
				childProcess.StartInfo.CreateNoWindow = true;
				try
				{
					return childProcess.Start();
				}
				catch
				{
					return false;
				}
			}
		}

		public static async Task<int> ExecuteProcessAsync(string fileName, string? workingDir, string commandLine, Action<string> outputLine, CancellationToken cancellationToken)
		{
			using (ManagedProcess newProcess = new ManagedProcess(null, fileName, commandLine, workingDir, null, null, ProcessPriorityClass.Normal))
			{
				for (; ; )
				{
					string? line = await newProcess.ReadLineAsync(cancellationToken);
					if (line == null)
					{
						newProcess.WaitForExit();
						return newProcess.ExitCode;
					}
					outputLine(line);
				}
			}
		}

		public static bool SafeIsFileUnderDirectory(string fileName, string directoryName)
		{
			try
			{
				string fullDirectoryName = Path.GetFullPath(directoryName).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
				string fullFileName = Path.GetFullPath(fileName);
				return fullFileName.StartsWith(fullDirectoryName, StringComparison.InvariantCultureIgnoreCase);
			}
			catch(Exception)
			{
				return false;
			}
		}

		/// <summary>
		/// Expands variables in $(VarName) format in the given string. Variables are retrieved from the given dictionary, or through the environment of the current process.
		/// Any unknown variables are ignored.
		/// </summary>
		/// <param name="inputString">String to search for variable names</param>
		/// <param name="additionalVariables">Lookup of variable names to values</param>
		/// <returns>String with all variables replaced</returns>
		public static string ExpandVariables(string inputString, Dictionary<string, string>? additionalVariables = null)
		{
			string result = inputString;
			for (int idx = result.IndexOf("$("); idx != -1; idx = result.IndexOf("$(", idx))
			{
				// Find the end of the variable name
				int endIdx = result.IndexOf(')', idx + 2);
				if (endIdx == -1)
				{
					break;
				}

				// Extract the variable name from the string
				string name = result.Substring(idx + 2, endIdx - (idx + 2));

				// Strip the format from the name
				string? format = null;
				int formatIdx = name.IndexOf(':');
				if(formatIdx != -1)
				{ 
					format = name.Substring(formatIdx + 1);
					name = name.Substring(0, formatIdx);
				}

				// Find the value for it, either from the dictionary or the environment block
				string? value;
				if (additionalVariables == null || !additionalVariables.TryGetValue(name, out value))
				{
					value = Environment.GetEnvironmentVariable(name);
					if (value == null)
					{
						idx = endIdx + 1;
						continue;
					}
				}

				// Encode the variable if necessary
				if(format != null)
				{
					if(String.Equals(format, "URI", StringComparison.InvariantCultureIgnoreCase))
					{
						value = Uri.EscapeDataString(value);
					}
				}

				// Replace the variable, or skip past it
				result = result.Substring(0, idx) + value + result.Substring(endIdx + 1);
			}
			return result;
		}

		class ProjectJson
		{
			public bool Enterprise { get; set; }
		}

		/// <summary>
		/// Determines if a project is an enterprise project
		/// </summary>
		/// <param name="FileName">Path to the project file</param>
		/// <returns>True if the given filename is an enterprise project</returns>
		public static bool IsEnterpriseProjectFromText(string text)
		{
			try
			{
				JsonSerializerOptions options = new JsonSerializerOptions();
				options.PropertyNameCaseInsensitive = true;
				options.Converters.Add(new JsonStringEnumConverter());

				ProjectJson project = JsonSerializer.Deserialize<ProjectJson>(text, options)!;

				return project.Enterprise;
			}
			catch
			{
				return false;
			}
		}

		/******/

		private static void AddLocalConfigPaths_WithSubFolders(DirectoryInfo baseDir, string fileName, List<FileInfo> files)
		{
			if(baseDir.Exists)
			{
				FileInfo baseFileInfo = new FileInfo(Path.Combine(baseDir.FullName, fileName));
				if(baseFileInfo.Exists)
				{
					files.Add(baseFileInfo);
				}

				foreach (DirectoryInfo subDirInfo in baseDir.EnumerateDirectories())
				{
					FileInfo subFile = new FileInfo(Path.Combine(subDirInfo.FullName, fileName));
					if (subFile.Exists)
					{
						files.Add(subFile);
					}
				}
			}
		}

		private static void AddLocalConfigPaths_WithExtensionDirs(DirectoryInfo baseDir, string relativePath, string fileName, List<FileInfo> files)
		{
			if (baseDir.Exists)
			{
				AddLocalConfigPaths_WithSubFolders(new DirectoryInfo(Path.Combine(baseDir.FullName, relativePath)), fileName, files);

				DirectoryInfo platformExtensionsDir = new DirectoryInfo(Path.Combine(baseDir.FullName, "Platforms"));
				if (platformExtensionsDir.Exists)
				{
					foreach (DirectoryInfo platformExtensionDir in platformExtensionsDir.EnumerateDirectories())
					{
						AddLocalConfigPaths_WithSubFolders(new DirectoryInfo(Path.Combine(platformExtensionDir.FullName, relativePath)), fileName, files);
					}
				}

				DirectoryInfo restrictedBaseDir = new DirectoryInfo(Path.Combine(baseDir.FullName, "Restricted"));
				if (restrictedBaseDir.Exists)
				{
					foreach (DirectoryInfo restrictedDir in restrictedBaseDir.EnumerateDirectories())
					{
						AddLocalConfigPaths_WithSubFolders(new DirectoryInfo(Path.Combine(restrictedDir.FullName, relativePath)), fileName, files);
					}
				}
			}
		}

		public static List<FileInfo> GetLocalConfigPaths(DirectoryInfo engineDir, FileInfo projectFile)
		{
			List<FileInfo> searchPaths = new List<FileInfo>();
			AddLocalConfigPaths_WithExtensionDirs(engineDir, "Programs/UnrealGameSync", "UnrealGameSync.ini", searchPaths);

			if (projectFile.Name.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase))
			{
				AddLocalConfigPaths_WithExtensionDirs(projectFile.Directory, "Build", "UnrealGameSync.ini", searchPaths);
			}
			else
			{
				AddLocalConfigPaths_WithExtensionDirs(engineDir, "Programs/UnrealGameSync", "DefaultEngine.ini", searchPaths);
			}
			return searchPaths;
		}

		/******/

		private static void AddDepotConfigPaths_PlatformFolders(string basePath, string fileName, List<string> searchPaths)
		{
			searchPaths.Add(String.Format("{0}/{1}", basePath, fileName));
			searchPaths.Add(String.Format("{0}/*/{1}", basePath, fileName));
		}

		private static void AddDepotConfigPaths_PlatformExtensions(string basePath, string relativePath, string fileName, List<string> searchPaths)
		{
			AddDepotConfigPaths_PlatformFolders(basePath + relativePath, fileName, searchPaths);
			AddDepotConfigPaths_PlatformFolders(basePath + "/Platforms/*" + relativePath, fileName, searchPaths);
			AddDepotConfigPaths_PlatformFolders(basePath + "/Restricted/*" + relativePath, fileName, searchPaths);
		}

		public static List<string> GetDepotConfigPaths(string enginePath, string projectPath)
		{
			List<string> searchPaths = new List<string>();
			AddDepotConfigPaths_PlatformExtensions(enginePath, "/Programs/UnrealGameSync", "UnrealGameSync.ini", searchPaths);

			if (projectPath.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase))
			{
				AddDepotConfigPaths_PlatformExtensions(projectPath.Substring(0, projectPath.LastIndexOf('/')), "/Build", "UnrealGameSync.ini", searchPaths);
			}
			else
			{
				AddDepotConfigPaths_PlatformExtensions(enginePath, "/Programs/UnrealGameSync", "DefaultEngine.ini", searchPaths);
			}
			return searchPaths;
		}

		/******/

		public static async Task<string[]?> TryPrintFileUsingCacheAsync(IPerforceConnection perforce, string depotPath, DirectoryReference cacheFolder, string? digest, ILogger logger, CancellationToken cancellationToken)
		{
			if(digest == null)
			{
				PerforceResponse<PrintRecord<string[]>> response = await perforce.TryPrintLinesAsync(depotPath, cancellationToken);
				if (response.Succeeded)
				{
					return response.Data.Contents;
				}
				else
				{
					return null;
				}
			}

			FileReference cacheFile = FileReference.Combine(cacheFolder, digest);
			if(FileReference.Exists(cacheFile))
			{
				logger.LogDebug("Reading cached copy of {DepotFile} from {LocalFile}", depotPath, cacheFile);
				string[] lines = FileReference.ReadAllLines(cacheFile);
				try
				{
					FileReference.SetLastWriteTimeUtc(cacheFile, DateTime.UtcNow);
				}
				catch(Exception ex)
				{
					logger.LogWarning(ex, "Exception touching cache file {LocalFile}", cacheFile);
				}
				return lines;
			}
			else
			{
				DirectoryReference.CreateDirectory(cacheFolder);

				FileReference tempFile = new FileReference(String.Format("{0}.{1}.temp", cacheFile.FullName, Guid.NewGuid()));
				PerforceResponseList<PrintRecord> response = await perforce.TryPrintAsync(tempFile.FullName, depotPath, cancellationToken);
				if (!response.Succeeded)
				{
					return null;
				}
				else
				{
					string[] lines = await FileReference.ReadAllLinesAsync(tempFile);
					try
					{
						FileReference.SetAttributes(tempFile, FileAttributes.Normal);
						FileReference.SetLastWriteTimeUtc(tempFile, DateTime.UtcNow);
						FileReference.Move(tempFile, cacheFile);
					}
					catch
					{
						try
						{
							FileReference.Delete(tempFile);
						}
						catch
						{
						}
					}
					return lines;
				}
			}
		}

		public static void ClearPrintCache(DirectoryReference cacheFolder)
		{
			DirectoryInfo cacheDir = cacheFolder.ToDirectoryInfo();
			if(cacheDir.Exists)
			{
				DateTime deleteTime = DateTime.UtcNow - TimeSpan.FromDays(5.0);
				foreach(FileInfo cacheFile in cacheDir.EnumerateFiles())
				{
					if(cacheFile.LastWriteTimeUtc < deleteTime || cacheFile.Name.EndsWith(".temp", StringComparison.OrdinalIgnoreCase))
					{
						try
						{
							cacheFile.Attributes = FileAttributes.Normal;
							cacheFile.Delete();
						}
						catch
						{
						}
					}
				}
			}
		}

		public static Color Blend(Color first, Color second, float T)
		{
			return Color.FromArgb((int)(first.R + (second.R - first.R) * T), (int)(first.G + (second.G - first.G) * T), (int)(first.B + (second.B - first.B) * T));
		}

		public static PerforceSettings OverridePerforceSettings(IPerforceSettings defaultConnection, string? serverAndPort, string? userName)
		{
			PerforceSettings newSettings = new PerforceSettings(defaultConnection);
			if(!String.IsNullOrWhiteSpace(serverAndPort))
			{
				newSettings.ServerAndPort = serverAndPort;
			}
			if (!String.IsNullOrWhiteSpace(userName))
			{
				newSettings.UserName = userName;
			}
			return newSettings;
		}

		public static string FormatRecentDateTime(DateTime date)
		{
			DateTime now = DateTime.Now;

			DateTime midnight = new DateTime(now.Year, now.Month, now.Day);
			DateTime midnightTonight = midnight + TimeSpan.FromDays(1.0);

			if(date > midnightTonight)
			{
				return String.Format("{0} at {1}", date.ToLongDateString(), date.ToShortTimeString());
			}
			else if(date >= midnight)
			{
				return String.Format("today at {0}", date.ToShortTimeString());
			}
			else if(date >= midnight - TimeSpan.FromDays(1.0))
			{
				return String.Format("yesterday at {0}", date.ToShortTimeString());
			}
			else if(date >= midnight - TimeSpan.FromDays(5.0))
			{
				return String.Format("{0:dddd} at {1}", date, date.ToShortTimeString());
			}
			else
			{
				return String.Format("{0} at {1}", date.ToLongDateString(), date.ToShortTimeString());
			}
		}

		public static string FormatDurationMinutes(TimeSpan duration)
		{
			return FormatDurationMinutes((int)(duration.TotalMinutes + 1));
		}

		public static string FormatDurationMinutes(int totalMinutes)
		{
			if(totalMinutes > 24 * 60)
			{
				return String.Format("{0}d {1}h", totalMinutes / (24 * 60), (totalMinutes / 60) % 24);
			}
			else if(totalMinutes > 60)
			{
				return String.Format("{0}h {1}m", totalMinutes / 60, totalMinutes % 60);
			}
			else
			{
				return String.Format("{0}m", totalMinutes);
			}
		}

		public static string FormatUserName(string userName)
		{
			StringBuilder normalUserName = new StringBuilder();
			for(int idx = 0; idx < userName.Length; idx++)
			{
				if(idx == 0 || userName[idx - 1] == '.')
				{
					normalUserName.Append(Char.ToUpper(userName[idx]));
				}
				else if(userName[idx] == '.')
				{
					normalUserName.Append(' ');
				}
				else
				{
					normalUserName.Append(Char.ToLower(userName[idx]));
				}
			}
			return normalUserName.ToString();
		}

		public static void OpenUrl(string url)
		{
			ProcessStartInfo startInfo = new ProcessStartInfo();
			startInfo.FileName = url;
			startInfo.UseShellExecute = true;
			using Process _ = Process.Start(startInfo);
		}
	}
}
