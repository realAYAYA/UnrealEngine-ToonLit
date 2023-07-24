using System;
using System.IO;
using System.Diagnostics;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using NLog;
using Perforce.P4;
using System.Net.Sockets;
using System.Net;

namespace p4api.net.unit.test
{
    class ResetServerType
    {
        private bool unicodeReset = false;
        private bool nonUnicodeReset = false;
        public bool IsResetRequired(bool unicode)
        {
            if (unicode)
                return unicodeReset;
            return nonUnicodeReset;
        }
        public void SetResetRequired(bool unicode, bool value)
        {
            if (unicode)
                unicodeReset = value;
            else
                nonUnicodeReset = value;
        }

    }

    class Utilities
    {
        private static Logger logger = LogManager.GetCurrentClassLogger();
        private static ResetServerType ResetServer = new ResetServerType();
        private static Stopwatch stopWatchAllTests = null;
        private static Stopwatch stopWatch = null;
        private static int testCount = 0;
        private static string[] settings = new string[] { "P4CLIENT", "P4CONFIG", "P4IGNORE", "P4PASSWD", "P4PORT", "P4TRUST", "P4TICKETS", "P4USER" };
        private static string[] settingValues;
        private static string[] envValues;
        private static string cwd;
        private static int[] preTestObjectCount;
        private static P4CallBacks.LogMessageDelegate logDelegate = null;
        private static LogFile.LogMessageDelgate logFileDelegate = null;
        private static long allocs = 0;
        private static long frees = 0;

        private static void LogFunction(int log_level, string file, int line, string message)
        {
            logger.Debug(String.Format("{0}({1}): {2}", file, line, message));
        }

        private static void LogFileFunction(int log_level, string src, string message)
        {
            logger.Debug(String.Format("{0}: {1}", src, message));
        }

        private static void SaveP4Prefs()
        {
            settingValues = new string[settings.Length];
            envValues = new string[settings.Length];
            for (int i = 0; i < settings.Length; i++)
            {
                settingValues[i] = P4Server.Get(settings[i]);
                // also clear the value
                P4Server.Set(settings[i], "");
            }
        }

        private static void RestoreP4Prefs()
        {
            P4Bridge.ReloadEnviro();

            for (int i = 0; i < settings.Length; i++)
            {
                P4Server.Set(settings[i], settingValues[i]);
            }
        }

        public static void LogTestStart(TestContext testContext)
        {
            // save and restore the p4config env variable and the cwd
            SaveP4Prefs();
            allocs = P4Debugging.GetStringAllocs();
            frees = P4Debugging.GetStringReleases();

            cwd = System.IO.Directory.GetCurrentDirectory();

            logDelegate = new P4CallBacks.LogMessageDelegate(LogFunction);
            logFileDelegate = new LogFile.LogMessageDelgate(LogFileFunction);

            LogFile.SetLoggingFunction(logFileDelegate);

            P4Debugging.SetBridgeLogFunction(logDelegate);
            preTestObjectCount = new int[P4Debugging.GetAllocObjectCount()];

            for (int i = 0; i < preTestObjectCount.Length; i++)
            {
                preTestObjectCount[i] = P4Debugging.GetAllocObject(i);
            }

            // reset the p4d_cmd variable to the default
            p4d_cmd = _p4d_cmd;

            testCount++;
            logger.Info("====== TestName: {0}", testContext.TestName);
            stopWatch = Stopwatch.StartNew();
            if (stopWatchAllTests == null)
                stopWatchAllTests = Stopwatch.StartNew();
        }

        public static void LogTestFinish(TestContext testContext)
        {
            System.IO.Directory.SetCurrentDirectory(cwd);
            RestoreP4Prefs();

            logger.Info("------ {0}: {1}, {2} ms", testContext.TestName, testContext.CurrentTestOutcome, stopWatch.ElapsedMilliseconds);

            int iExtraObjects = 0;
            for (int i = 0; i < P4Debugging.GetAllocObjectCount(); i++)
            {
                int postTest = P4Debugging.GetAllocObject(i);
                if (preTestObjectCount[i] != postTest)
                {
                    iExtraObjects += postTest - preTestObjectCount[i];
                    logger.Info(String.Format("<<<<*** Item count for {0} mismatch: {1}/{2}",
                        P4Debugging.GetAllocObjectName(i), preTestObjectCount[i], postTest));
                }
            }

            long postAllocs = P4Debugging.GetStringAllocs();
            long postFrees = P4Debugging.GetStringReleases();

            if (postAllocs - allocs != postFrees - frees)
            {
                logger.Info(String.Format("<<<<*** String alloc mismatch: {0}/{1}", postAllocs - allocs, postFrees - frees));
                Assert.AreEqual(postAllocs - allocs, postFrees - frees);
            }

            Assert.AreEqual(0, iExtraObjects);
        }

        public static void LogAllTestsFinish()
        {
            logger.Info("@@@@@@ Time for tests: {0} - {1} s", testCount, stopWatchAllTests.Elapsed);
        }

        public static void ClobberDirectory(String path)
        {
            DirectoryInfo di = new DirectoryInfo(path);

            ClobberDirectory(di);
        }

        public static void ClobberDirectory(DirectoryInfo di)
        {
            string comSpec = Environment.GetEnvironmentVariable("ComSpec");
            Process Zapper = new Process();
            ProcessStartInfo si = new ProcessStartInfo(comSpec, "/c rd /S /Q " + di.FullName);
            si.WorkingDirectory = Path.GetDirectoryName(di.FullName);
            si.UseShellExecute = true;

            Zapper.StartInfo = si;
            try
            {
                Zapper.Start();
            }
            catch (Exception ex)
            {
                logger.Info("In ClobberDirectory, Zapper.Start() failed: {0}", ex.Message);
            }
            if (Zapper.HasExited == false)
            {
                Zapper.WaitForExit();
            }
            if (Directory.Exists(di.FullName))
            {
                bool worked = false;
                int retries = 0;
                do
                {
                    if (!di.Exists)
                        return;

                    try
                    {
                        FileInfo[] files = di.GetFiles();

                        foreach (FileInfo fi in files)
                        {
                            if (fi.IsReadOnly)
                                fi.IsReadOnly = false;
                            fi.Delete();
                        }
                        DirectoryInfo[] subDirs = di.GetDirectories();
                        foreach (DirectoryInfo sdi in subDirs)
                        {
                            ClobberDirectory(sdi);
                        }

                        di.Delete();

                        worked = true;
                    }
                    catch (Exception)
                    {
                        System.Threading.Thread.Sleep(100);
                    }
                    retries++;
                }
                while (!worked && retries < 2);
            }
        }

        static string p4d_exe = System.Environment.GetEnvironmentVariable("P4DPATH");
        //const String _p4d_cmd = "-vrpc=3 -vnet=9 -p localhost:6666 -Id {1} -r {0} -L p4d.log -vserver=3";
        const String _p4d_cmd = "-p localhost:6666 -Id {1} -r {0} -L p4d.log";
        static String p4d_cmd = null;
        static String rsh_p4d_cmd = "rsh:" + p4d_exe + " -r {0} -L p4d.log -vserver=3 -i";
        static String restore_cmd = "-r {0} -jr checkpoint.{1}";
        static String upgrade_cmd = "-r {0} -xu";
        static String generate_key_cmd = string.Empty;
        static string rubbishBin = "c:\\MyTestDir-rubbish-bin";




        public static Process DeploySSLP4TestServer(string path, bool Unicode)
        {
            System.Environment.SetEnvironmentVariable("P4SSLDIR", path);
            string test = System.Environment.GetEnvironmentVariable("P4SSLDIR");
            p4d_cmd = "-p ssl:6666 -Id UnitTestServer -r {0}";
            generate_key_cmd = "-Gc";
            return DeployP4TestServer(path, 1, Unicode);
        }

        public static Process DeployIPv6P4TestServer(string path, string tcp, bool Unicode)
        {
            p4d_cmd = "-p " + tcp + ":::1:6666 -Id UnitTestServer -r {0}";
            return DeployP4TestServer(path, 1, Unicode);
        }

		public static Process DeployP4TestServer(string path, bool unicode, string testName="")
		{
            return DeployP4TestServer(path, 1, unicode, testName);
		}

        public static string TestServerRoot(string baseRoot, bool Unicode)
		{
            String extra = "a";
			if (Unicode)
			{
                extra = "u";
			}
            var root = Path.Combine(baseRoot, extra, "server");
            logger.Debug("Server root: {0}", root);
            return root;
		}

        public static string TestRshServerPort(string baseRoot, bool Unicode)
		{
            String extra = "a";
            if (Unicode)
            {
                extra = "u";
            }
            var root = Path.Combine(baseRoot, extra, "server");
            root = String.Format(rsh_p4d_cmd, root);
            logger.Debug("Server root: {0}", root);
            return root;
		}

        public static string TestClientRoot(string baseRoot, bool Unicode)
		{
            String extra = "a";
            if (Unicode)
			{
                extra = "u";
            }
            var root = Path.Combine(baseRoot, extra, "clients");
            logger.Debug("Client root: {0}", root);
            return root;
			}

        public static void SetClientRoot(Repository rep, string TestDir, bool unicode, string ws_client)
			{
            Client c = rep.GetClient(ws_client, null);
            c.Root = Path.Combine(TestClientRoot(TestDir, unicode), ws_client);
            rep.UpdateClient(c);
            var syncOpts = new SyncFilesCmdOptions(SyncFilesCmdFlags.Force, 0);
            rep.Connection.Client.SyncFiles(syncOpts, new FileSpec(new DepotPath("//...")));
					}


        public static Process DeployP4TestServer(string path, int checkpointRev, bool unicode, string testName = "")
		{
			String zippedFile = "a.exe";
            string currentPath = Directory.GetCurrentDirectory();
            if (unicode)
			{
				zippedFile = "u.exe";
			}
			return DeployP4TestServer(path, checkpointRev, zippedFile, unicode, testName);
		}

		public static Process DeployP4TestServer(string path, int checkpointRev, string zippedFile, bool unicode, string testName = "")
		{
            return DeployP4TestServer(path, checkpointRev, zippedFile, null, unicode, testName);
		}

	    public static Process DeployP4TestServer(string testRoot, int checkpointRev, string zippedFile, string P4DCmd,
	        bool unicode, string testName = "")
	    {
	        logger.Info("DeployP4TestServer");
	        if (!Directory.Exists(rubbishBin))
	        {
	            Directory.CreateDirectory(rubbishBin);
	        }

	        string assemblyFile = typeof(Utilities).Assembly.CodeBase;
	        String unitTestDir = Path.GetDirectoryName(assemblyFile);

	        String EnvPath = Environment.GetEnvironmentVariable("path");
	        String CurWDir = Environment.CurrentDirectory;

	        var zipBase = Path.GetFileNameWithoutExtension(zippedFile); // a or u
	        var unzippedContentsDir = Path.Combine(rubbishBin, zipBase);
	        var testServerRoot = Path.Combine(testRoot, zipBase, "server");
	        var testClientsRoot = Path.Combine(testRoot, zipBase, "clients");
	        CreateDir(testServerRoot, 10);
	        try
	        {
	            Environment.CurrentDirectory = testServerRoot;
	        }
	        catch (Exception ex)
	        {
	            bool dirExists = Directory.Exists(testServerRoot);
	            logger.Error("Can't cd to {0}, {1}", testServerRoot, ex.Message);
	            return null;
	        }
	        using (StreamWriter sw = new StreamWriter("CmdLog.txt", false))
	        {
	            int idx;
	            if (unitTestDir.ToLower().StartsWith("file:\\"))
	            {
	                // cut off the file:\\
	                idx = unitTestDir.IndexOf("\\") + 1;
	                unitTestDir = unitTestDir.Substring(idx);
	            }
	           // if ((idx = unitTestDir.IndexOf("TestResults")) > 0)
	           // {
	           //     unitTestDir = unitTestDir.Substring(0, idx);
	                //if (unitTestDir.ToLower().Contains("bin\\debug") == false) // is this needed?
	                //{
	                //    unitTestDir = Path.Combine(unitTestDir, "bin\\debug");
	                //}
	            //}

	            // Decide whether to reset server
	            bool resetServerDir = ResetServer.IsResetRequired(unicode);
	            ResetServer.SetResetRequired(unicode, false);

	            ProcessStartInfo si;
	            String msg;
	            string unitTestZip = Path.Combine(unitTestDir, zippedFile);
	            string targetTestZip = Path.Combine(unzippedContentsDir, zippedFile);
	            if (!Directory.Exists(unzippedContentsDir))
	            {
	                // Copy file and unzip it
	                CreateDir(unzippedContentsDir, 5);
	                CopyFile(unitTestZip, targetTestZip);
	                FileInfo fi = new FileInfo(targetTestZip);

	                Process Unzipper = new Process();

	                // unpack the zip
	                si = new ProcessStartInfo(zippedFile);
	                si.WorkingDirectory = unzippedContentsDir;
	                si.Arguments = "-y";

                    logger.Info("Unzipping {0} {1}", si.FileName, si.Arguments);

	                Unzipper.StartInfo = si;
	                Unzipper.Start();
	                Unzipper.WaitForExit();

	                // Copy unzipped directory tree
	                CopyDirTree(unzippedContentsDir, testServerRoot);
	            }
	            else if (resetServerDir)
	            {
	                CopyDirTree(unzippedContentsDir, testServerRoot);
	            }

	            // Make sure no db.* files present
	            logger.Info("Removing db.* from {0}", testServerRoot);
	            try
	            {
	                foreach (string fname in Directory.GetFiles(testServerRoot, "db.*", SearchOption.TopDirectoryOnly))
	                {
	                    System.IO.File.Delete(fname);
	                }
	            }
	            catch (Exception ex)
	            {
	                logger.Error(ex.Message);
	                logger.Error(ex.StackTrace);
	            }
	            // Reset client directories
	            DelDir(testClientsRoot);
	            CreateDir(testClientsRoot, 5);
	            foreach (
	                string fname in Directory.GetDirectories(unzippedContentsDir, "*space*", SearchOption.TopDirectoryOnly)
	            )
	            {
	                var dirOnly = Path.GetFileName(fname);
	                CopyDirTree(fname, Path.Combine(testClientsRoot, dirOnly));
	            }

	            if (p4d_cmd.Contains("ssl:"))
	            {
	                Process GenKeyandCert = new Process();

                    // generate private key and certificate
                    si = new ProcessStartInfo(p4d_exe);
                    si.Arguments = generate_key_cmd;
	                si.WorkingDirectory = testServerRoot;
	                si.UseShellExecute = false;
                    si.CreateNoWindow = true;
                    si.RedirectStandardOutput = true;

                    msg = si.Arguments;
	                logger.Info(msg);

	                GenKeyandCert.StartInfo = si;
	                GenKeyandCert.Start();
	                GenKeyandCert.WaitForExit();
	            }

	            // restore the checkpoint
	            Process RestoreCheckPoint = new Process();
                si = new ProcessStartInfo(p4d_exe);
                si.Arguments = String.Format(restore_cmd, testServerRoot, checkpointRev);
                //si.FileName = "p4d.exe";
	            si.WorkingDirectory = testServerRoot;
	            si.UseShellExecute = false;
                si.RedirectStandardOutput = true;
                si.CreateNoWindow = true;

                logger.Info("{0} {1}", si.FileName, si.Arguments);

	            RestoreCheckPoint.StartInfo = si;
	            RestoreCheckPoint.Start();
	            RestoreCheckPoint.WaitForExit();

                if (RestoreCheckPoint.ExitCode != 0)
                    return null;

	            // upgrade the db tables
	            Process UpgradeTables = new Process();
	            si = new ProcessStartInfo(p4d_exe);
	            si.Arguments = String.Format(upgrade_cmd, testServerRoot);
	            si.WorkingDirectory = testServerRoot;
	            si.UseShellExecute = false;
                si.CreateNoWindow = true;
                si.RedirectStandardOutput = true;

                logger.Info("{0} {1}", si.FileName, si.Arguments);

	            UpgradeTables.StartInfo = si;
	            UpgradeTables.Start();
	            UpgradeTables.WaitForExit();

                if (UpgradeTables.ExitCode != 0)
                    return null;

                Process p4d = new Process();

	            if (P4DCmd != null)
	            {
	                string P4DCmdSrc = Path.Combine(unitTestDir, P4DCmd);
	                string P4DCmdTarget = Path.Combine(testServerRoot, P4DCmd);
	                System.IO.File.Copy(P4DCmdSrc, P4DCmdTarget);

	                // run the command to start p4d
	                si = new ProcessStartInfo(P4DCmdTarget);
	                si.Arguments = String.Format(testServerRoot);
	                si.WorkingDirectory = testServerRoot;
	                si.UseShellExecute = false;
                    si.CreateNoWindow = true;
                    si.RedirectStandardOutput = true;

                    logger.Info("{0} {1}", si.FileName, si.Arguments);

	                p4d.StartInfo = si;
	                p4d.Start();
	            }
	            else
	            {
                    //start p4d
                    si = new ProcessStartInfo(p4d_exe);
                    if (string.IsNullOrEmpty(testName))
	                    testName = "UnitTestServer";
	                si.Arguments = String.Format(p4d_cmd, testServerRoot, testName);
	                si.WorkingDirectory = testServerRoot;
	                si.UseShellExecute = false;
                    si.RedirectStandardOutput = true;
                    si.CreateNoWindow = true;

                    logger.Info("{0} {1}", si.FileName, si.Arguments);

	                p4d.StartInfo = si;
	                p4d.Start();
	            }
	            Environment.CurrentDirectory = CurWDir;

                // Give p4d time to start up
                DateTime started = DateTime.UtcNow;
                TcpClient client = new TcpClient();
                while (DateTime.UtcNow.Subtract(started).Milliseconds < 500)
                {
                    client.Connect("localhost", 6666);
                    if (client.Connected)
                        return p4d;
                }

                // that didn't work
	            return p4d;
	        }
	    }

	    private static void CopyDirTree(string srcDir, string targDir)
        {
            logger.Info("Copying directory {0} to {1}", srcDir, targDir);
            // First Create all of the directories
            foreach (string dirPath in Directory.GetDirectories(srcDir, "*",
                SearchOption.AllDirectories))
                Directory.CreateDirectory(dirPath.Replace(srcDir, targDir));

            // Then copy all the files & Replaces any files with the same name
            foreach (string fname in Directory.GetFiles(srcDir, "*.*", SearchOption.AllDirectories))
		{
                var target = fname.Replace(srcDir, targDir);
                try
			{
                    System.IO.File.Copy(fname, target, true);
                }
                catch (Exception ex)
                {
                    logger.Debug(ex.Message);
                }
            }
            logger.Info("Finished copying {0} to {1}", srcDir, targDir);
			}

        private static void CopyFile(string srcFile, string targetFile)
        {
            int retries = 3;
            int delay = 1000; // initial delay 1 second
            while (retries > 0)
			{
				try
				{
                    System.IO.File.Copy(srcFile, targetFile);
                    break; //success
                } catch (Exception)
                {
                    System.Threading.Thread.Sleep(delay);
                    delay *= 2; // wait twice as long next time
                    retries--;
                }
            }
				}

        private static void CreateDir(string path, int retries)
        {
            while ((Directory.Exists(path) == false) && (retries > 0))
				{
					try
					{
                    Directory.CreateDirectory(path);
                    if (Directory.Exists(path))
                    {
                        break;
					}
                    retries--;
                    System.Threading.Thread.Sleep(1000);
                } catch (Exception ex)
                {
                    retries--;
                    bool dirExists = Directory.Exists(path);
                    Trace.WriteLine(ex.Message);
                    if (dirExists)
					{
                        break;
                    }
                    System.Threading.Thread.Sleep(200);
					}
				}
			}


        private static void DelDir(string dirPath)
        {
            if (!Directory.Exists(dirPath))
                return;
            try
            {
                Directory.Delete(dirPath, true);
            } catch
            {
                try
			{
                    // delete failed, try to rename it
                    Directory.Move(dirPath, string.Format("{0}-{1}", dirPath, DateTime.Now.Ticks));
                } catch
			{
                    // rename failed, try to clobber it (can be slow so last resort)
                    Utilities.ClobberDirectory(dirPath);
			}
			}
		}

        public static void RemoveTestServer(Process p, String testRoot, bool resetDepot=false, bool unicode=false)
		{
            logger.Info("RemoveTestServer");
			if (p != null)
			{
				if (!p.HasExited)
					p.Kill();
				p.WaitForExit();
				// sleep for a second to let the system clean up
				// System.Threading.Thread.Sleep(100);
			}
            MainTest.RememberToCleanup(rubbishBin);
            MainTest.RememberToCleanup(testRoot);
            // Flag for next unit test to reset depot - see DeployP4TestServer
            ResetServer.SetResetRequired(unicode, resetDepot);
        }

        private static void MoveDir(string srcDir, string targDir)
        {
            logger.Info("Moving {0} to {1}", srcDir, targDir);
			try
			{
				int retries = 60;
                while ((Directory.Exists(srcDir)) && (retries > 0))
				{
					try
					{
						// Try to rename it
                        Directory.Move(srcDir, targDir);
						//must have worked
						break;
                    } catch
					{
						retries--;
						System.Threading.Thread.Sleep(1000);
						if (retries <= 0)
						{
							throw;
						}
					}
				}
            } catch (Exception ex)
			{
                logger.Info("In DeployP4TestServer, Directory.Move failed: {0}", ex.Message);
				// rename failed, try to clobber it (can be slow so last resort)
                Utilities.ClobberDirectory(srcDir);
			}
				}
	}
}
