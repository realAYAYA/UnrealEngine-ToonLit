using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Diagnostics;
using System.Threading;
using Perforce.P4;

namespace sln.bld.cmd
{
	class Program
	{
		static String ServerSpec;
		static String User;
		static String Password;
		static String Workspace;

		static String Target;

		static String MsBuildPathTxt = "C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319\\msbuild.exe";

		private static void Usage()
		{
			Console.WriteLine(@"sln-bld-cmd -s host:port -u user [-p password] [-c workspace] <depot path of solution file to build>");


		}
		private static void ParseArgs(string[] args)
		{
			for (int idx = 0; idx < args.Length; idx++)
			{
				switch (args[idx].ToLower())
				{
					default:
						Target = args[idx];
						break;
					case "-h":
						Usage();
						Environment.Exit(0);
						break;
					case "/?":
						Usage();
						Environment.Exit(0);
						break;
					case "-s":
						ServerSpec = args[++idx];
						break;
					case "-u":
						User = args[++idx];
						break;
					case "-p":
						Password = args[++idx];
						break;
					case "-c":
						Workspace = args[++idx];
						break;
				}
			}
		}

		private static void ReadSettings()
		{

			if (!System.IO.File.Exists("config.txt"))
				return;

			using (StreamReader sr = new StreamReader("config.txt"))
			{
				ServerSpec = sr.ReadLine();
				User = sr.ReadLine();
				Password = sr.ReadLine();
				Workspace = sr.ReadLine();
			}
		}

		static void Main(string[] args)
		{
			ReadSettings();

			ParseArgs(args);

			if (String.IsNullOrWhiteSpace(ServerSpec))
			{
				Console.WriteLine("Must specify a server connection (host:port)!");
				Environment.Exit(1);
			}
			if (String.IsNullOrWhiteSpace(User))
			{
				Console.WriteLine("Must specify a user!");
				Environment.Exit(1);
			}

			DateTime buildTime = DateTime.Now;
			String buildId = buildTime.ToString("MMddyyHHmmss");
			String buildFolder = buildTime.ToString("MM-dd-yy_HHmmss");
			String buildPath = Path.Combine(Environment.CurrentDirectory, buildFolder);

			int idx = 0;
			while ((Directory.Exists(buildPath)) && (idx < 26))
			{
				buildPath = Path.Combine(Environment.CurrentDirectory, buildTime.ToString("MM-dd-yy HHmmss") + ((char)((int)'a' + idx)));
			}
			if (idx >= 26)
				Environment.Exit(1);

			Directory.CreateDirectory(buildPath);

			Server pServer = new Server(new ServerAddress(ServerSpec));
			Repository rep = new Repository(pServer);

			using (Connection con = rep.Connection)
			{
				con.UserName = User;
				con.Client = new Client();
				con.Client.Name = Workspace;
				Options options = new Options();
				options["Password"] = Password;
				
				con.Connect(options);

				Client buildClient = new Client();

				buildClient.Name = "p4apinet_solution_builder_sample_application_client";
				buildClient.OwnerName = User;
				buildClient.ViewMap = new ViewMap();
				buildClient.Root = buildPath;
				buildClient.Options = ClientOption.AllWrite;
				buildClient.LineEnd = LineEnd.Local;
				buildClient.SubmitOptions = new ClientSubmitOptions(false, SubmitType.SubmitUnchanged);

				string depotPath = Target;
				if (Target.EndsWith(".sln"))
				{
					depotPath = Target.Substring(0, Target.LastIndexOf('/'));
				}
				string depotFolder = depotPath.Substring(depotPath.LastIndexOf('/') + 1);

				depotPath += "/...";

				String clientPath = String.Format("//{0}/{1}/...", buildClient.Name, depotFolder);

				MapEntry entry = new MapEntry(MapType.Include, new DepotPath(depotPath), new ClientPath(clientPath));

				buildClient.ViewMap.Add(entry);

				Console.Write("Creating "+buildClient.Name+"...\r\n");

				rep.CreateClient(buildClient);
				
				con.Disconnect(null);
				con.Client = new Client();
				con.Client.Name = buildClient.Name;
				con.Connect(options);

				Console.Write("Running command 'p4 sync -p...\r\n");
				
				Options sFlags = new Options(SyncFilesCmdFlags.PopulateClient, -1);
				
				IList<FileSpec> rFiles = con.Client.SyncFiles(sFlags, null);

				Console.Write("Deleting " + buildClient.Name + "...\r\n");
				rep.DeleteClient(buildClient, null);
				
				string localPath = clientPath;
				localPath = localPath.TrimStart('/');
				localPath = localPath.TrimEnd('.');
				localPath = localPath.Substring(localPath.IndexOf('/') + 1);
				localPath = localPath.Replace('/', '\\');
				string solutionName = Target.Substring(depotPath.LastIndexOf('/'));
				solutionName = solutionName.TrimStart('/');
				localPath = Path.Combine(buildPath, localPath, solutionName);

				RunBuidProc(localPath);
			}
		}
		static Process buildProc;

		private static void RunBuidProc(String SolutionFilePath)
		{
			buildProc = new Process();

			string quotedPath = string.Format("\"{0}\"", SolutionFilePath);

			ProcessStartInfo si = new ProcessStartInfo(MsBuildPathTxt, quotedPath);
			si.UseShellExecute = false;
			si.RedirectStandardOutput = true;
			si.RedirectStandardError = true;
			si.WorkingDirectory = Path.GetDirectoryName(SolutionFilePath);
			si.CreateNoWindow = false;

			buildProc.StartInfo = si;

			Thread stdoutThread = new Thread(new ThreadStart(ReadStandardOutThreadProc));
			stdoutThread.IsBackground = true;

			Thread stderrThread = new Thread(new ThreadStart(ReadStandardErrorThreadProc));
			stderrThread.IsBackground = true;

			buildProc.Start();
			stdoutThread.Start();
			stderrThread.Start();

			buildProc.WaitForExit();

			if (stdoutThread.IsAlive)
				stdoutThread.Abort();

			if (stderrThread.IsAlive)
				stderrThread.Abort();

			buildProc = null;

			return;
		}

		private static void ReadStandardOutThreadProc()  //Process p, TextBox t)
		{
			try
			{
				String line;
				while ((line = buildProc.StandardOutput.ReadLine()) != null)
				{
					Console.WriteLine(line);
				}
				return;
			}
			catch (ThreadAbortException) { return; }
		}

		private static void ReadStandardErrorThreadProc()  //Process p, TextBox t)
		{
			try
			{
				String line;
				while ((line = buildProc.StandardError.ReadLine()) != null)
				{
					Console.WriteLine(line);
				}
				return;
			}
			catch (ThreadAbortException) { return; }
		}

	}
}
