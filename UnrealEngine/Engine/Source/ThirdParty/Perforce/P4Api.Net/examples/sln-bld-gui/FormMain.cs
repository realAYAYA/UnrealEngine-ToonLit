using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;
using System.Diagnostics;
using System.Threading;

using Perforce.P4;

namespace Perforce.sln.bld.gui
{
	public partial class FormMain : Form
	{
		int lastChange = 0;
		internal Repository rep = null;

		private void checkConnect()
		{
			if (rep != null)
			{
				//release any old connection
				rep.Dispose();
			}

			String conStr = mServerConnection.Text;
			String user = mUserText.Text;
			String password = mPaswordTxt.Text;
			try
			{
				Server server = new Server(new ServerAddress(conStr));

				rep = new Repository(server);

				rep.Connection.UserName = user;
				Options options = new Options();
				options["Password"] = password;

				rep.Connection.Client = new Client();

				rep.Connection.Connect(options);

			}
			catch (Exception ex)
			{
				rep = null;
				MessageBox.Show(ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
			}
		}




		public FormMain()
		{
			InitializeComponent();
		}

		private void mShowPasswordChk_CheckedChanged(object sender, EventArgs e)
		{
			mPaswordTxt.UseSystemPasswordChar = !mShowPasswordChk.Checked;
		}

		Thread MonitorChangesThread = null;

		private void mBrowseDepotBtn_Click(object sender, EventArgs e)
		{
			checkConnect();
			
				DepotPathDlg dlg = new DepotPathDlg(rep);
				if (dlg.ShowDialog() == DialogResult.OK)
				{
					mSolutionPath.Text = dlg.SelectedFile;

					int lastChange = GetLastChange();
					mLastChangeLbl.Text = lastChange.ToString();
					changeAtLastBuild = lastChange;

					// start the monitor
					MonitorChangesThread = new Thread(new ThreadStart(MonitorThreadProc));
					MonitorChangesThread.IsBackground = true;
					MonitorChangesThread.Start();
				}
			
		}

		private void mSelectBuildDir_Click(object sender, EventArgs e)
		{
			folderBrowseDlg.SelectedPath = mBuildFolderTxt.Text;
			if (folderBrowseDlg.ShowDialog() != DialogResult.Cancel)
			{
				mBuildFolderTxt.Text = folderBrowseDlg.SelectedPath;
			}
		}

		private void OnInfoResults(int level, String data)
		{
			String spaces = String.Empty;
			for (int idx = 0; idx < level; idx++)
			{
				spaces += ".";
			}
			AsynchAddLineToLog(String.Format("{0}{1}", spaces, data));
		}

		StreamWriter log;

		int changeAtLastBuild = 0;

		private void mBuildNowBtn_Click(object sender, EventArgs e)
		{
			RunBuild(true);
		}

		private void RunBuild(bool async)
		{
			DateTime buildTime = DateTime.Now;
			String buildId = buildTime.ToString("MMddyyHHmmss");
			String buildFolder = buildTime.ToString("MM-dd-yy_HHmmss");
			String buildPath = Path.Combine(mBuildFolderTxt.Text, buildFolder);

			int idx = 0;
			while ((Directory.Exists(buildPath)) && (idx < 26))
			{
				buildPath = Path.Combine(mBuildFolderTxt.Text, buildTime.ToString("MM-dd-yy HHmmss") + ((char)((int)'a' + idx)));
			}
			if (idx >= 26)
				return;

			string logFile = Path.Combine(buildPath, "BuildLog.txt");

			Directory.CreateDirectory(buildPath);

			String ConStr = mServerConnection.Text;
			String User = mUserText.Text;
			String Password = mPaswordTxt.Text;
			String Target = mSolutionPath.Text;
			Server pServer = new Server(new ServerAddress(ConStr));
			rep = new Repository(pServer);

			

			Connection con = rep.Connection;

			con.Connect(null);
			log = new StreamWriter(logFile);

			Client buildClient = new Client();

			buildClient.Name = "p4apinet_solution_builder_sample_application_client";
			buildClient.OwnerName = con.UserName;
			buildClient.ViewMap = new ViewMap();
			buildClient.Root = buildPath;
			buildClient.Options = ClientOption.AllWrite;
			buildClient.LineEnd = LineEnd.Local;
			buildClient.SubmitOptions = new ClientSubmitOptions(false, SubmitType.SubmitUnchanged);

			string depotPath = mSolutionPath.Text;

			IList<FileMetaData> fmd = null;

			try
			{
				fmd = rep.GetFileMetaData(null, FileSpec.DepotSpec(depotPath));
			}

			catch{}

			if (( fmd == null) || (fmd.Count !=1))
			{
				string message = string.Format("The solution file \"{0}\" does not exist in the depot.", depotPath);
				MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
				return;
			}


			if (mSolutionPath.Text.EndsWith(".sln"))
			{
				depotPath = mSolutionPath.Text.Substring(0, mSolutionPath.Text.LastIndexOf('/'));
			}
				string depotFolder = depotPath.Substring(depotPath.LastIndexOf('/') + 1);

				depotPath += "/...";

				String clientPath = String.Format("//{0}/{1}/...", buildClient.Name, depotFolder);

				MapEntry entry = new MapEntry(MapType.Include, new DepotPath(depotPath), new ClientPath(clientPath));

				buildClient.ViewMap.Add(entry);

				rep.CreateClient(buildClient);
				con.Client = rep.GetClient(buildClient.Name);

				string localPath = clientPath;
				localPath = localPath.TrimStart('/');
				localPath = localPath.TrimEnd('.');
				localPath = localPath.Substring(localPath.IndexOf('/') + 1);
				localPath = localPath.Replace('/', '\\');
				string solutionName = Target.Substring(depotPath.LastIndexOf('/'));
				solutionName = solutionName.TrimStart('/');
				localPath = Path.Combine(buildPath, localPath, solutionName);


				int lastChange = GetLastChange();
				AsynchSetLastChange(lastChange);

				IList<Changelist> changes = GetChangesAfter(changeAtLastBuild, lastChange);

				 changeAtLastBuild = lastChange;

				if (changes != null)
				{
					for (idx = 0; idx < changes.Count; idx++)
					{
						AsynchAddLineToLog(changes[idx].ToString(true));
					}
				}


				if (async)
				{
					Thread buildThread = new Thread(new ParameterizedThreadStart(RunBuildProc));
					buildThread.IsBackground = true;
					buildThread.Start(localPath);
				}
				else
				{
					RunBuildProc(localPath);
				}

				con.Disconnect(null);
		}

		private void mSelectMSBuildLoactionBtn_Click(object sender, EventArgs e)
		{
			fileBrowseDlg.FileName = mMsBuildPathTxt.Text;
			if (fileBrowseDlg.ShowDialog() != DialogResult.Cancel)
			{
				mMsBuildPathTxt.Text = fileBrowseDlg.FileName;
			}
		}

		private int GetLastChange()
		{
			string depotPath = mSolutionPath.Text;
			if (mSolutionPath.Text.EndsWith(".sln"))
			{
				depotPath = mSolutionPath.Text.Substring(0, mSolutionPath.Text.LastIndexOf('/'));
			}

			String ConStr = mServerConnection.Text;
			String User = mUserText.Text;
			String Password = mPaswordTxt.Text;
			String Target = mSolutionPath.Text;
			Server pServer = new Server(new ServerAddress(ConStr));
			rep = new Repository(pServer);
			Connection con = rep.Connection;
			con.Connect(null);

			Options opts = new Options();
			opts["-m"]="1";
			IList<Changelist> changes = rep.GetChangelists(opts, null);

			if (changes != null)
				return (int) changes[0].Id;

			return -1;
		}

		private IList<Changelist> GetChangesAfter(int previous, int current)
		{
			if (current <= previous)
				return null;

			string depotPath = mSolutionPath.Text;
			if (mSolutionPath.Text.EndsWith(".sln"))
			{
				depotPath = mSolutionPath.Text.Substring(0, mSolutionPath.Text.LastIndexOf('/'));
			}

			FileSpec fs = new DepotPath(depotPath);
			Options opts = new Options();
			opts["-m"] = (current - previous).ToString();
			IList<Changelist> changes = rep.GetChangelists(opts, fs);

			return changes;
		}

		Process buildProc = null;

		bool buildFailed = false;

		private void RunBuildProc(object oSolutionFilePath)
		{
			checkConnect();
			
			Connection con = rep.Connection;

			Client buildClient = new Client();
			buildClient.Name = "p4apinet_solution_builder_sample_application_client";
			con.Client = rep.GetClient(buildClient.Name);

			Options sFlags = new Options((SyncFilesCmdFlags.Force|SyncFilesCmdFlags.PopulateClient), -1);

			IList<FileSpec> rFiles = con.Client.SyncFiles(sFlags, null);

			Options opts = new Options();
			opts["-f"] = null;
			rep.DeleteClient(buildClient,opts);
			
			buildFailed = false;

			String SolutionFilePath = (String)oSolutionFilePath;

			String MSBuildPath = (String)mMsBuildPathTxt.Text;

			buildProc = new Process();

			string quotedPath = string.Format("\"{0}\"", SolutionFilePath);

			ProcessStartInfo si = new ProcessStartInfo(MSBuildPath, quotedPath);
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

			log.Dispose();
			log = null;

			AsynchSetLastChange(buildFailed);

			return;
		}

		private delegate void AddLineDelegate(String Line);

		public void AddLineToLog(string line)
		{
			if (mLogText.TextLength > 0)
				mLogText.Text += "\r\n";

			mLogText.Text += line;

			mLogText.Select(mLogText.Text.Length, 0);
			mLogText.ScrollToCaret(); 
		}

		private void AsynchAddLineToLog( String line)
		{
			if (AddLine == null)
				AddLine = new AddLineDelegate(AddLineToLog);

			if (log != null)
			{
				log.WriteLine(line);
			}

			mLogText.BeginInvoke(AddLine, line);
		}

		AddLineDelegate AddLine = null;

		private delegate void SetLastChangeDelegate(int Line);

		public void SetLastChange(int change )
		{
			mLastChangeLbl.Text = change.ToString();
		}

		SetLastChangeDelegate SetLast = null;

		private void AsynchSetLastChange( int change )
		{
			if (SetLast == null)
				SetLast = new SetLastChangeDelegate(SetLastChange);

			mLastChangeLbl.BeginInvoke(SetLast, change);
		}

		private delegate void ShowBuildFailedDelegate(bool show);

		public void ShowBuildFailed(bool show)
		{
			mBuildFailedLbl.Visible = show;
		}

		ShowBuildFailedDelegate ShowFail = null;

		private void AsynchSetLastChange( bool show )
		{
			if (ShowFail == null)
				ShowFail = new ShowBuildFailedDelegate(ShowBuildFailed);

			mLastChangeLbl.BeginInvoke(ShowFail, show);
		}

		private void ReadStandardOutThreadProc()  //Process p, TextBox t)
		{
			try
			{
				String line;
				while ((line = buildProc.StandardOutput.ReadLine()) != null)
				{
					AsynchAddLineToLog(line);

					if (line.Contains("FAILED."))
						buildFailed = true;
				}
				return;
			}
			catch (ThreadAbortException) { return; }
		}

		private void ReadStandardErrorThreadProc()  //Process p, TextBox t)
		{
			try
			{
				String line;
				while ((line = buildProc.StandardError.ReadLine()) != null)
				{
					AsynchAddLineToLog(line);
				}
				return;
			}
			catch (ThreadAbortException) { return; }
		}

		int mBuildInterval = 2;

		private void MonitorThreadProc()
		{
			try
			{
				while (true)
				{
					int last = GetLastChange();

					mLogText.BeginInvoke(new AddLineDelegate(AddLineToLog), 
						String.Format("[{0} {1}] Checking for changes....", 
						DateTime.Now.ToShortDateString(), DateTime.Now.ToShortTimeString()));
					if (last > changeAtLastBuild)
					{
						mLogText.BeginInvoke(new AddLineDelegate(AddLineToLog), "Changes found");
						RunBuild(false);
					}

					AsynchAddLineToLog( String.Format("Next Check at {0}", 
						DateTime.Now.AddMinutes((double)mBuildInterval).ToShortTimeString()));
					Thread.Sleep(TimeSpan.FromMinutes(mBuildInterval));
				}
			}
			catch (ThreadAbortException) { }
		}

		private void mBuildIntervalCmb_SelectedIndexChanged(object sender, EventArgs e)
		{
			int.TryParse((String)mBuildIntervalCmb.SelectedItem, out mBuildInterval);
		}

		private void FormMain_Load(object sender, EventArgs e)
		{
			ReadSettings();

			if (String.IsNullOrEmpty(mSolutionPath.Text))
				return;

			lastChange = GetLastChange();
			mLastChangeLbl.Text = lastChange.ToString();
			changeAtLastBuild = lastChange;

			// start the monitor
			MonitorChangesThread = new Thread(new ThreadStart(MonitorThreadProc));
			MonitorChangesThread.IsBackground = true;
			MonitorChangesThread.Start();

		}

		private void FormMain_FormClosing(object sender, FormClosingEventArgs e)
		{
			SaveSettings();
		}

		private void SaveSettings()
		{
			using (StreamWriter sw = new StreamWriter("buildProc.config"))
			{
				//Credentials

				if (mShowPasswordChk.Checked)
					sw.Write('1');
				else
					sw.Write('0');

				sw.WriteLine(mPaswordTxt.Text);
				sw.WriteLine(mUserText.Text);
				sw.WriteLine(mServerConnection.Text);

				sw.WriteLine(mBuildFolderTxt.Text);

				sw.WriteLine(mMsBuildPathTxt.Text);

				sw.WriteLine(mBuildIntervalCmb.SelectedIndex.ToString());

				sw.WriteLine(mSolutionPath.Text);
			}
	   }
		private void ReadSettings()
		{
			if (!System.IO.File.Exists("buildProc.config"))
				return;
			using (StreamReader sr = new StreamReader("buildProc.config"))
			{
				//Credentials
				String line = sr.ReadLine();
				mShowPasswordChk.Checked = (line[0] == '1');
				mPaswordTxt.Text = line.Substring(1);

				mUserText.Text = sr.ReadLine();
				mServerConnection.Text = sr.ReadLine();

				mBuildFolderTxt.Text = sr.ReadLine();

				mMsBuildPathTxt.Text = sr.ReadLine();

				line = sr.ReadLine();
				int idx = 1;
				int.TryParse(line, out idx);
				mBuildIntervalCmb.SelectedIndex = idx;

				mSolutionPath.Text = sr.ReadLine();
			}
		}

		private void mUserText_TextChanged(object sender, EventArgs e)
		{

		}

		private void mSolutionPath_TextChanged(object sender, EventArgs e)
		{
			string path = mSolutionPath.Text;

			mBuildNowBtn.Enabled = !string.IsNullOrEmpty(path);
		}


	}
}
