using System;
using System.IO;
using System.Diagnostics;
using System.Reflection;
using System.Collections.Generic;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    /// MainTest is for some assembly-global initialization/finalization calls
    /// </summary>
    [TestClass]
    public class MainTest
    {
        private static Logger logger = null;    // Initialized in constructor for whole DLL
        private static List<string> dirsToCleanup = new List<string>();
        private static List<Process> procsToCleanup = new List<Process>();

        static void InitLogging()
        {
            string assemblyFolder = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            System.Diagnostics.Trace.WriteLine(string.Format("TestP4 folder: {0}", assemblyFolder));
            try
            {
                // Useful for debugging logging. Note name of config needs to agree with vsixmanifest file which installs it.
                NLog.LogManager.Configuration = new NLog.Config.XmlLoggingConfiguration(assemblyFolder + "\\NLog.config", true);
                LogManager.ThrowExceptions = true;
                LogManager.EnableLogging();
                logger = LogManager.GetCurrentClassLogger();
                logger.Info("@@@@@@ p4api.net-unit-test run start");
                LogManager.ThrowExceptions = false;
            } catch (Exception ex)
            {
                System.Diagnostics.Trace.WriteLine(string.Format("TestP4 Exception: {0}", ex.Message));
                System.Diagnostics.Trace.WriteLine(string.Format("TestP4 Stack: {0}", ex.StackTrace));
                LogManager.ThrowExceptions = false;
                if (logger == null)
                {
                    logger = LogManager.GetCurrentClassLogger();
                    System.Diagnostics.Trace.WriteLine(string.Format("TestP4 Logging will default to disabled"));
                }
            }
        }

        [AssemblyInitialize()]
        public static void AssemblyInit(TestContext context)
        {
            InitLogging();
        }

        [AssemblyCleanup()]
        public static void AssemblyCleanup()
        {
            Utilities.LogAllTestsFinish();
            foreach (var dir in dirsToCleanup)
            {
                logger.Info("Removing {0}", dir);
                //test directory exists
                try
                {
                    // try to delete it
                    Directory.Delete(dir, true);
                }
                catch
                {
                    // simple delete failed, try to clobber it (can be slow so last resort)
                    Utilities.ClobberDirectory(dir);
                }
            }
        }

        public static void RememberToCleanup(string dirname)
        {
            if (!dirsToCleanup.Contains(dirname))
                dirsToCleanup.Add(dirname);
        }

        public static void RememberToCleanup(Process proc)
        {
            if (!procsToCleanup.Contains(proc))
                procsToCleanup.Add(proc);
        }

        public static void CleanupTest(string testDir)
        {
            foreach (var proc in procsToCleanup)
                Utilities.RemoveTestServer(proc, testDir);
        }

    }
}
