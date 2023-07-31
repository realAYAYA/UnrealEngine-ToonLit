// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using UnrealGameSync;

namespace UnrealGameSyncLauncher
{
	static class Program
	{
		[STAThread]
		static int Main(string[] args)
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);

			bool firstInstance;
			using(Mutex instanceMutex = new Mutex(true, "UnrealGameSyncRunning", out firstInstance))
			{
				if(!firstInstance)
				{
					using(EventWaitHandle activateEvent = new EventWaitHandle(false, EventResetMode.AutoReset, "ActivateUnrealGameSync"))
					{
						activateEvent.Set();
					}
					return 0;
				}

				// Figure out if we should sync the unstable build by default
				bool preview = args.Contains("-unstable", StringComparer.InvariantCultureIgnoreCase) || args.Contains("-preview", StringComparer.InvariantCultureIgnoreCase);

				// Read the settings
				string? serverAndPort = null;
				string? userName = null;
				string? depotPath = DeploymentSettings.DefaultDepotPath;
				GlobalPerforceSettings.ReadGlobalPerforceSettings(ref serverAndPort, ref userName, ref depotPath, ref preview);

				// If the shift key is held down, immediately show the settings window
				SettingsWindow.SyncAndRunDelegate syncAndRunWrapper = (perforce, depotParam, previewParam, logWriter, cancellationToken) => SyncAndRun(perforce, depotParam, previewParam, args, instanceMutex, logWriter, cancellationToken);
				if ((Control.ModifierKeys & Keys.Shift) != 0)
				{
					// Show the settings window immediately
					SettingsWindow updateError = new SettingsWindow(null, null, serverAndPort, userName, depotPath, preview, syncAndRunWrapper);
					if(updateError.ShowDialog() == DialogResult.OK)
					{
						return 0;
					}
				}
				else
				{
					// Try to do a sync with the current settings first
					CaptureLogger logger = new CaptureLogger();

					IPerforceSettings settings = new PerforceSettings(PerforceSettings.Default) { PreferNativeClient = true }.MergeWith(newServerAndPort: serverAndPort, newUserName: userName);

					ModalTask? task = PerforceModalTask.Execute(null, "Updating", "Checking for updates, please wait...", settings, (p, c) => SyncAndRun(p, depotPath, preview, args, instanceMutex, logger, c), logger);
					if (task == null)
					{
						logger.LogInformation("Canceled by user");
					}
					else if (task.Succeeded)
					{
						return 0;
					}

					SettingsWindow updateError = new SettingsWindow("Unable to update UnrealGameSync from Perforce. Verify that your connection settings are correct.", logger.Render(Environment.NewLine), serverAndPort, userName, depotPath, preview, syncAndRunWrapper);
					if(updateError.ShowDialog() == DialogResult.OK)
					{
						return 0;
					}
				}
			}
			return 1;
		}

		public static async Task SyncAndRun(IPerforceConnection perforce, string? baseDepotPath, bool preview, string[] args, Mutex instanceMutex, ILogger logger, CancellationToken cancellationToken)
		{
			try
			{
				if (String.IsNullOrEmpty(baseDepotPath))
				{
					throw new UserErrorException($"Invalid setting for sync path");
				}

				string baseDepotPathPrefix = baseDepotPath.TrimEnd('/');

				// Find the most recent changelist
				string syncPath = baseDepotPathPrefix + (preview ? "/UnstableRelease.zip" : "/Release.zip");
				List<ChangesRecord> changes = await perforce.GetChangesAsync(ChangesOptions.None, 1, ChangeStatus.Submitted, syncPath, cancellationToken);
				if (changes.Count == 0)
				{
					syncPath = baseDepotPathPrefix + (preview ? "/UnstableRelease/..." : "/Release/...");
					changes = await perforce.GetChangesAsync(ChangesOptions.None, 1, ChangeStatus.Submitted, syncPath, cancellationToken);
					if (changes.Count == 0)
					{
						throw new UserErrorException($"Unable to find any UGS binaries under {syncPath}");
					}
				}

				int requiredChangeNumber = changes[0].Number;
				logger.LogInformation("Syncing from {SyncPath}", syncPath);

				// Create the target folder
				string applicationFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UnrealGameSync", "Latest");
				if (!SafeCreateDirectory(applicationFolder))
				{
					throw new UserErrorException($"Couldn't create directory: {applicationFolder}");
				}

				// Read the current version
				string syncVersionFile = Path.Combine(applicationFolder, "SyncVersion.txt");
				string requiredSyncText = String.Format("{0}\n{1}@{2}", perforce.Settings.ServerAndPort ?? "", syncPath, requiredChangeNumber);

				// Check the application exists
				string applicationExe = Path.Combine(applicationFolder, "UnrealGameSync.exe");

				// Check if the version has changed
				string? syncText;
				if (!File.Exists(syncVersionFile) || !File.Exists(applicationExe) || !TryReadAllText(syncVersionFile, out syncText) || syncText != requiredSyncText)
				{
					// Try to delete the directory contents. Retry for a while, in case we've been spawned by an application in this folder to do an update.
					for (int numRetries = 0; !SafeDeleteDirectoryContents(applicationFolder); numRetries++)
					{
						if (numRetries > 20)
						{
							throw new UserErrorException($"Couldn't delete contents of {applicationFolder} (retried {numRetries} times).");
						}
						Thread.Sleep(500);
					}

					// Find all the files in the sync path at this changelist
					List<FStatRecord> fileRecords = await perforce.FStatAsync(FStatOptions.None, $"{syncPath}@{requiredChangeNumber}", cancellationToken).ToListAsync(cancellationToken);
					if (fileRecords.Count == 0)
					{
						throw new UserErrorException($"Couldn't find any matching files for {syncPath}@{requiredChangeNumber}");
					}

					// Sync all the files in this list to the same directory structure under the application folder
					string depotPathPrefix = syncPath.Substring(0, syncPath.LastIndexOf('/') + 1);
					foreach (FStatRecord fileRecord in fileRecords)
					{
						if (fileRecord.DepotFile == null)
						{
							throw new UserErrorException("Missing depot path for returned file");
						}

						string localPath = Path.Combine(applicationFolder, fileRecord.DepotFile.Substring(depotPathPrefix.Length).Replace('/', Path.DirectorySeparatorChar));
						if (!SafeCreateDirectory(Path.GetDirectoryName(localPath)!))
						{
							throw new UserErrorException($"Couldn't create folder {Path.GetDirectoryName(localPath)}");
						}

						await perforce.PrintAsync(localPath, fileRecord.DepotFile, cancellationToken);
					}

					// If it was a zip file, extract it
					if (syncPath.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
					{
						string localPath = Path.Combine(applicationFolder, syncPath.Substring(depotPathPrefix.Length).Replace('/', Path.DirectorySeparatorChar));
						ZipFile.ExtractToDirectory(localPath, applicationFolder);
					}

					// Check the application exists
					if (!File.Exists(applicationExe))
					{
						throw new UserErrorException($"Application was not synced from Perforce. Check that UnrealGameSync exists at {syncPath}/UnrealGameSync.exe, and you have access to it.");
					}

					// Update the version
					if (!TryWriteAllText(syncVersionFile, requiredSyncText))
					{
						throw new UserErrorException("Couldn't write sync text to {SyncVersionFile}");
					}
				}
				logger.LogInformation("");

				// Build the command line for the synced application, including the sync path to monitor for updates
				string originalExecutable = Assembly.GetEntryAssembly()!.Location;
                if (Path.GetExtension(originalExecutable).Equals(".dll", StringComparison.OrdinalIgnoreCase))
                {
                    string newExecutable = Path.ChangeExtension(originalExecutable, ".exe");
                    if (File.Exists(newExecutable))
                    {
                        originalExecutable = newExecutable;
                    }
                }

				StringBuilder newCommandLine = new StringBuilder(String.Format("-updatepath=\"{0}@>{1}\" -updatespawn=\"{2}\"{3}", syncPath, requiredChangeNumber, originalExecutable, preview ? " -unstable" : ""));
				foreach (string arg in args)
				{
					newCommandLine.AppendFormat(" {0}", QuoteArgument(arg));
				}

				// Release the mutex now so that the new application can start up
				instanceMutex.Close();

				// Spawn the application
				logger.LogInformation("Spawning {App} with command line: {CmdLine}", applicationExe, newCommandLine.ToString());
				using (Process childProcess = new Process())
				{
					childProcess.StartInfo.FileName = applicationExe;
					childProcess.StartInfo.Arguments = newCommandLine.ToString();
					childProcess.StartInfo.UseShellExecute = false;
					childProcess.StartInfo.CreateNoWindow = false;
					if (!childProcess.Start())
					{
						throw new UserErrorException("Failed to start process");
					}
				}
			}
			catch (UserErrorException ex)
			{
				logger.LogError("{Message}", ex.Message);
				throw;
			}
			catch (Exception ex)
			{
				logger.LogError(ex, "Error while syncing application.");
				foreach (string line in ex.ToString().Split('\n'))
				{
					logger.LogError("{Line}", line);
				}
				throw;
			}
		}

		static string QuoteArgument(string arg)
		{
			if(arg.IndexOf(' ') != -1 && !arg.StartsWith("\""))
			{
				return String.Format("\"{0}\"", arg);
			}
			else
			{
				return arg;
			}
		}

		static bool TryReadAllText(string fileName, [NotNullWhen(true)] out string? text)
		{
			try
			{
				text = File.ReadAllText(fileName);
				return true;
			}
			catch(Exception)
			{
				text = null;
				return false;
			}
		}

		static bool TryWriteAllText(string fileName, string text)
		{
			try
			{
				File.WriteAllText(fileName, text);
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}

		static bool SafeCreateDirectory(string directoryName)
		{
			try
			{
				Directory.CreateDirectory(directoryName);
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}

		static bool SafeDeleteDirectory(string directoryName)
		{
			try
			{
				Directory.Delete(directoryName, true);
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}

		static bool SafeDeleteDirectoryContents(string directoryName)
		{
			try
			{
				DirectoryInfo directory = new DirectoryInfo(directoryName);
				foreach(FileInfo childFile in directory.EnumerateFiles("*", SearchOption.AllDirectories))
				{
					childFile.Attributes = childFile.Attributes & ~FileAttributes.ReadOnly;
					childFile.Delete();
				}
				foreach(DirectoryInfo childDirectory in directory.EnumerateDirectories())
				{
					SafeDeleteDirectory(childDirectory.FullName);
				}
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}
	}
}
