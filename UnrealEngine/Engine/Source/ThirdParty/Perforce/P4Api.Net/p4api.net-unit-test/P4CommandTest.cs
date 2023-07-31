// #define _LOG_TO_FILE

using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Diagnostics;
using System.Threading;
using Perforce.P4;
using NLog;

namespace p4api.net.unit.test
{      
	/// <summary>
	///This is a test class for P4CommandTest and is intended
	///to contain all P4CommandTest Unit Tests
	///</summary>
	[TestClass()]
	public class P4CommandTest
	{
		String TestDir = "c:\\MyTestDir";
        private static Logger logger = LogManager.GetCurrentClassLogger();
        public TestContext TestContext { get; set; }

        [TestInitialize]
        public void SetupTest()
        {
            Utilities.LogTestStart(TestContext);
        }
        [TestCleanup]
        public void CleanupTest()
        {
            Utilities.LogTestFinish(TestContext);
        }

        #region Additional test attributes
        // 
        //You can use the following additional attributes as you write your tests:
        //
        //Use ClassInitialize to run code before running the first test in the class
        //[ClassInitialize()]
        //public static void MyClassInitialize(TestContext testContext)
        //{
        //}
        //
        //Use ClassCleanup to run code after all tests in a class have run
        //[ClassCleanup()]
        //public static void MyClassCleanup()
        //{
        //}
        //
        //Use TestInitialize to run code before running each test
        //[TestInitialize()]
        //public void MyTestInitialize()
        //{
        //}
        //
        //Use TestCleanup to run code after each test has run
        //[TestCleanup()]
        //public void MyTestCleanup()
        //{
        //}
        //
        #endregion


        /// <summary>
        ///A test for Args
        ///</summary>
        [TestMethod()]
		public void ArgsTest()
		{
			bool unicode = false;

			string serverAddr = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			// turn off exceptions for this test
			ErrorSeverity oldExceptionLevel = P4Exception.MinThrowLevel;
			P4Exception.MinThrowLevel = ErrorSeverity.E_NOEXC;

		    Process p4d = null;

			for( int i = 0; i < 2; i++ ) // run once for ascii, once for unicode
			{
				try
				{
                    p4d = Utilities.DeployP4TestServer(TestDir, unicode);
					using( P4Server server = new P4Server( serverAddr, user, pass, ws_client ) )
					{
						if( unicode )
							Assert.IsTrue( server.UseUnicode, "Unicode server detected as not supporting Unicode" );
						else
							Assert.IsFalse( server.UseUnicode, "Non Unicode server detected as supporting Unicode" );

						P4Command target = new P4Command( server );

						StringList expected = new StringList(new string[]{ "a", "b", "c" });
						target.Args = expected;

						StringList actual = target.Args;

						Assert.AreEqual( expected, actual );
					}
				}
				finally
				{
					Utilities.RemoveTestServer( p4d, TestDir );
				}
				unicode = !unicode;
			}
			// reset the exception level
			P4Exception.MinThrowLevel = oldExceptionLevel;
		}

		/// <summary>
		///A test for Run
		///</summary>
		[TestMethod()]
		public void RunTest()
		{
			bool unicode = false;

			string serverAddr = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

		    Process p4d = null;

			// turn off exceptions for this test
			ErrorSeverity oldExceptionLevel = P4Exception.MinThrowLevel;
			P4Exception.MinThrowLevel = ErrorSeverity.E_NOEXC;

			for( int i = 0; i < 2; i++ ) // run once for ascii, once for unicode
			{
				try
				{
                    p4d = Utilities.DeployP4TestServer(TestDir, unicode);
					using( P4Server server = new P4Server( serverAddr, user, pass, ws_client ) )
					{
						if( unicode )
							Assert.IsTrue( server.UseUnicode, "Unicode server detected as not supporting Unicode" );
						else
							Assert.IsFalse( server.UseUnicode, "Non Unicode server detected as supporting Unicode" );

						P4Command target = new P4Command( server, "help", false, null );

						P4CommandResult results = target.Run();
						Assert.IsTrue( results.Success );
					}
				}
				finally
				{
					Utilities.RemoveTestServer( p4d, TestDir );
				}
				unicode = !unicode;
			}
			// reset the exception level
			P4Exception.MinThrowLevel = oldExceptionLevel;
		}

		/// <summary>
		///A test for Run
		///</summary>
		[TestMethod()]
		public void RunTest1()
		{
			bool unicode = false;

			string serverAddr = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			// turn off exceptions for this test
			ErrorSeverity oldExceptionLevel = P4Exception.MinThrowLevel;
			P4Exception.MinThrowLevel = ErrorSeverity.E_NOEXC;

		    Process p4d = null;

			for( int i = 0; i < 2; i++ ) // run once for ascii, once for unicode
			{
				try
				{
                    p4d = Utilities.DeployP4TestServer(TestDir, unicode);
					using( P4Server server = new P4Server( serverAddr, user, pass, ws_client ) )
					{
						if( unicode )
							Assert.IsTrue( server.UseUnicode, "Unicode server detected as not supporting Unicode" );
						else
							Assert.IsFalse( server.UseUnicode, "Non Unicode server detected as supporting Unicode" );

						P4Command target = new P4Command( server, "help", false, null );

						P4CommandResult results = target.Run(new String[] { "print" });
						Assert.IsTrue( results.Success );

						P4ClientInfoMessageList helpTxt = target.InfoOutput;

						Assert.IsNotNull( helpTxt );
					}
				}
				finally
				{
					Utilities.RemoveTestServer( p4d, TestDir );
				}
				unicode = !unicode;
			}
			// reset the exception level
			P4Exception.MinThrowLevel = oldExceptionLevel;
		}

		P4Command cmd1 = null;
		P4Command cmd2 = null;
		P4Command cmd3 = null;
		P4Command cmd4 = null;
		P4Command cmd5 = null;
		P4Command cmd6 = null;

		bool run = true;

		TimeSpan delay = TimeSpan.FromMilliseconds(5);

        private void ReportCommandStart(P4Command cmd, DateTime time)
        {
            WriteLine(string.Format("Thread {2} starting command: {0:X8}, at {1}",
                cmd1.CommandId, time.ToLongTimeString(), Thread.CurrentThread.ManagedThreadId));
        }
        private void ReportCommandStop(P4Command cmd, DateTime time)
        {
            WriteLine(string.Format("Thread {3} Finished command: {0:X8}, at {1}, run time {2} Milliseconds",
                cmd1.CommandId, time.ToLongTimeString(), (DateTime.Now - time).TotalMilliseconds, Thread.CurrentThread.ManagedThreadId));
        }

        private void cmdThreadProc1()
		{
			try
			{
                WriteLine(System.Reflection.MethodBase.GetCurrentMethod().Name);
                P4Server server = serverMT.getServer();
                while (run)
				{
					cmd1 = new P4Command(server, "fstat", false, "//depot/...");

					DateTime StartedAt = DateTime.Now;

                    ReportCommandStart(cmd1, StartedAt);
                    P4CommandResult result = cmd1.Run();
                    ReportCommandStop(cmd1, StartedAt);

					P4CommandResult lastResult = server.LastResults;

					Assert.AreEqual(result.Success, lastResult.Success);
					if (result.InfoOutput != null)
					{
						Assert.AreEqual(result.InfoOutput.Count, lastResult.InfoOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.InfoOutput);
					}
					if (result.ErrorList != null)
					{
						Assert.AreEqual(result.ErrorList.Count, lastResult.ErrorList.Count);
					}
					else
					{
						Assert.IsNull(result.ErrorList);
					}
					if (result.TextOutput != null)
					{
						Assert.AreEqual(result.TextOutput, lastResult.TextOutput);
					}
					else
					{
						Assert.IsNull(lastResult.TextOutput);
					}
					if (result.TaggedOutput != null)
					{
						Assert.AreEqual(result.TaggedOutput.Count, lastResult.TaggedOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.TaggedOutput);
					}
					Assert.AreEqual(result.Cmd, lastResult.Cmd);
					if (result.CmdArgs != null)
					{
						Assert.AreEqual(result.CmdArgs.Length, lastResult.CmdArgs.Length);
					}
					else
					{
						Assert.IsNull(lastResult.CmdArgs);
					}

					if (!result.Success)
					{
						WriteLine(string.Format("Thread 1, fstat failed:{0}",(result.ErrorList!=null && result.ErrorList.Count>0)?result.ErrorList[0].ErrorMessage :"<unknown error>"));
					}
					else
					{
						WriteLine(string.Format("Thread 1, fstat Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine("Thread 1 cleanly exited");
				return;
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
				Assert.Fail(ex.Message);
			}
		}

		private void cmdThreadProc2()
		{
			try
			{
                WriteLine(System.Reflection.MethodBase.GetCurrentMethod().Name);
                P4Server server = serverMT.getServer();
                while (run)
				{
                    cmd2 = new P4Command(server, "dirs", false, "//depot/*");

					DateTime StartedAt = DateTime.Now;

                    ReportCommandStart(cmd2, StartedAt);
                    P4CommandResult result = cmd2.Run();
                    ReportCommandStop(cmd2, StartedAt);

                    P4CommandResult lastResult = server.LastResults;

					Assert.AreEqual(result.Success, lastResult.Success);
					if (result.InfoOutput!=null)
					{
						Assert.AreEqual(result.InfoOutput.Count, lastResult.InfoOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.InfoOutput);
					}
					if (result.ErrorList!=null)
					{
						Assert.AreEqual(result.ErrorList.Count, lastResult.ErrorList.Count);
					}
					else
					{
						Assert.IsNull(result.ErrorList);
					}
					if (result.TextOutput!=null)
					{
						Assert.AreEqual(result.TextOutput, lastResult.TextOutput);
					}
					else
					{
						Assert.IsNull(lastResult.TextOutput);
					}
					if (result.TaggedOutput!=null)
					{
						Assert.AreEqual(result.TaggedOutput.Count, lastResult.TaggedOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.TaggedOutput);
					}
					Assert.AreEqual(result.Cmd, lastResult.Cmd);
					if (result.CmdArgs!=null)
					{
						Assert.AreEqual(result.CmdArgs.Length, lastResult.CmdArgs.Length);
					}
					else
					{
						Assert.IsNull(lastResult.CmdArgs);
					}

					if (!result.Success)
					{
						WriteLine(string.Format("Thread 2, dirs failed:{0}", (result.ErrorList != null && result.ErrorList.Count > 0) ? result.ErrorList[0].ErrorMessage : "<unknown error>"));
					}
					else
					{
						WriteLine(string.Format("Thread 2, dirs Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine("Thread 2 cleanly exited");
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
				Assert.Fail(ex.Message);
			}
		}

		private void cmdThreadProc3()
		{
			try
			{
                WriteLine(System.Reflection.MethodBase.GetCurrentMethod().Name);
                P4Server server = serverMT.getServer();
                while (run)
				{
                    cmd3 = new P4Command(server, "edit", false, "-n", @"//depot/...");

					DateTime StartedAt = DateTime.Now;

                    ReportCommandStart(cmd3, StartedAt);
                    P4CommandResult result = cmd3.Run();
                    ReportCommandStop(cmd3, StartedAt);

                    P4CommandResult lastResult = server.LastResults;

					Assert.AreEqual(result.Success, lastResult.Success);
					if (result.InfoOutput != null)
					{
						Assert.AreEqual(result.InfoOutput.Count, lastResult.InfoOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.InfoOutput);
					}
					if (result.ErrorList != null)
					{
						Assert.AreEqual(result.ErrorList.Count, lastResult.ErrorList.Count);
					}
					else
					{
						Assert.IsNull(result.ErrorList);
					}
					if (result.TextOutput != null)
					{
						Assert.AreEqual(result.TextOutput, lastResult.TextOutput);
					}
					else
					{
						Assert.IsNull(lastResult.TextOutput);
					}
					if (result.TaggedOutput != null)
					{
						Assert.AreEqual(result.TaggedOutput.Count, lastResult.TaggedOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.TaggedOutput);
					}
					Assert.AreEqual(result.Cmd, lastResult.Cmd);
					if (result.CmdArgs != null)
					{
						Assert.AreEqual(result.CmdArgs.Length, lastResult.CmdArgs.Length);
					}
					else
					{
						Assert.IsNull(lastResult.CmdArgs);
					}

					if (!result.Success)
					{
						WriteLine(string.Format("Thread 3, edit failed:{0}", (result.ErrorList != null && result.ErrorList.Count>0) ? result.ErrorList[0].ErrorMessage : "<unknown error>"));
					}
					else
					{
						WriteLine(string.Format("Thread 3, edit Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
                    if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine("Thread 3 cleanly exited");
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
				Assert.Fail(ex.Message);
			}
		}

		private void cmdThreadProc4()
		{
			try
			{
                WriteLine(System.Reflection.MethodBase.GetCurrentMethod().Name);
                P4Server server = serverMT.getServer();
                while (run)
				{
                    using (P4Server _P4Server = new P4Server("localhost:6666", null, null, null))
					{
						string val = P4Server.Get("P4IGNORE");
						bool _p4IgnoreSet = !string.IsNullOrEmpty(val);

						if (_p4IgnoreSet)
						{
							WriteLine(string.Format("P4Ignore is set, {0}", val));
						}
						else
						{
							WriteLine("P4Ignore is not set");
						}

						Assert.IsTrue(_P4Server.ApiLevel > 0);
					}
					cmd4 = new P4Command(server, "fstat", false, "//depot/...");

					DateTime StartedAt = DateTime.Now;

                    ReportCommandStart(cmd4, StartedAt);
                    P4CommandResult result = cmd4.Run();
                    ReportCommandStop(cmd4, StartedAt);

                    P4CommandResult lastResult = server.LastResults;

					Assert.AreEqual(result.Success, lastResult.Success);
					if (result.InfoOutput != null)
					{
						Assert.AreEqual(result.InfoOutput.Count, lastResult.InfoOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.InfoOutput);
					}
					if (result.ErrorList != null)
					{
						Assert.AreEqual(result.ErrorList.Count, lastResult.ErrorList.Count);
					}
					else
					{
						Assert.IsNull(result.ErrorList);
					}
					if (result.TextOutput != null)
					{
						Assert.AreEqual(result.TextOutput, lastResult.TextOutput);
					}
					else
					{
						Assert.IsNull(lastResult.TextOutput);
					}
					if (result.TaggedOutput != null)
					{
						Assert.AreEqual(result.TaggedOutput.Count, lastResult.TaggedOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.TaggedOutput);
					}
					Assert.AreEqual(result.Cmd, lastResult.Cmd);
					if (result.CmdArgs != null)
					{
						Assert.AreEqual(result.CmdArgs.Length, lastResult.CmdArgs.Length);
					}
					else
					{
						Assert.IsNull(lastResult.CmdArgs);
					}
					if (!result.Success)
					{
						WriteLine(string.Format("Thread 4, fstat failed:{0}",(result.ErrorList!=null && result.ErrorList.Count>0)?result.ErrorList[0].ErrorMessage :"<unknown error>"));
					}
					else
					{
						WriteLine(string.Format("Thread 4, fstat Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine("Thread 4 cleanly exited");
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
				Assert.Fail(ex.Message);
			}
		}

		private void cmdThreadProc5()
		{
			try
			{
                WriteLine(System.Reflection.MethodBase.GetCurrentMethod().Name);
                P4Server server = serverMT.getServer();
                while (run)
				{
                    cmd5 = new P4Command(server, "dirs", false, "//depot/*");

					DateTime StartedAt = DateTime.Now;

                    ReportCommandStart(cmd5, StartedAt);
                    P4CommandResult result = cmd5.Run();
                    ReportCommandStop(cmd5, StartedAt);

                    P4CommandResult lastResult = server.LastResults;

					Assert.AreEqual(result.Success, lastResult.Success);
					if (result.InfoOutput != null)
					{
						Assert.AreEqual(result.InfoOutput.Count, lastResult.InfoOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.InfoOutput);
					}
					if (result.ErrorList != null)
					{
						Assert.AreEqual(result.ErrorList.Count, lastResult.ErrorList.Count);
					}
					else
					{
						Assert.IsNull(result.ErrorList);
					}
					if (result.TextOutput != null)
					{
						Assert.AreEqual(result.TextOutput, lastResult.TextOutput);
					}
					else
					{
						Assert.IsNull(lastResult.TextOutput);
					}
					if (result.TaggedOutput != null)
					{
						Assert.AreEqual(result.TaggedOutput.Count, lastResult.TaggedOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.TaggedOutput);
					}
					Assert.AreEqual(result.Cmd, lastResult.Cmd);
					if (result.CmdArgs != null)
					{
						Assert.AreEqual(result.CmdArgs.Length, lastResult.CmdArgs.Length);
					}
					else
					{
						Assert.IsNull(lastResult.CmdArgs);
					}

					if (!result.Success)
					{
						WriteLine(string.Format("Thread 5, dirs failed:{0}", (result.ErrorList != null && result.ErrorList.Count > 0) ? result.ErrorList[0].ErrorMessage : "<unknown error>"));
					}
					else
					{
						WriteLine(string.Format("Thread 5, dirs Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine("Thread 5 cleanly exited");
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
				Assert.Fail(ex.Message);
			}
		}

		private void cmdThreadProc6()
		{
			try
			{
                WriteLine(System.Reflection.MethodBase.GetCurrentMethod().Name);
                P4Server server = serverMT.getServer();
                while (run)
				{
					cmd6 = new P4Command(server, "edit", false, "-n", "//depot/...");

					DateTime StartedAt = DateTime.Now;
                    ReportCommandStart(cmd6, StartedAt);
                    P4CommandResult result = cmd6.Run();
                    ReportCommandStop(cmd6, StartedAt);

                    P4CommandResult lastResult = server.LastResults;

					Assert.AreEqual(result.Success, lastResult.Success);
					if (result.InfoOutput != null)
					{
						Assert.AreEqual(result.InfoOutput.Count, lastResult.InfoOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.InfoOutput);
					}
					if (result.ErrorList != null)
					{
						Assert.AreEqual(result.ErrorList.Count, lastResult.ErrorList.Count);
					}
					else
					{
						Assert.IsNull(result.ErrorList);
					}
					if (result.TextOutput != null)
					{
						Assert.AreEqual(result.TextOutput, lastResult.TextOutput);
					}
					else
					{
						Assert.IsNull(lastResult.TextOutput);
					}
					if (result.TaggedOutput != null)
					{
						Assert.AreEqual(result.TaggedOutput.Count, lastResult.TaggedOutput.Count);
					}
					else
					{
						Assert.IsNull(lastResult.TaggedOutput);
					}
					Assert.AreEqual(result.Cmd, lastResult.Cmd);
					if (result.CmdArgs != null)
					{
						Assert.AreEqual(result.CmdArgs.Length, lastResult.CmdArgs.Length);
					}
					else
					{
						Assert.IsNull(lastResult.CmdArgs);
					}

					if (!result.Success)
					{
						WriteLine(string.Format("Thread 6, edit failed:{0}", (result.ErrorList != null && result.ErrorList.Count>0) ? result.ErrorList[0].ErrorMessage : "<unknown error>"));
					}
					else
					{
						WriteLine(string.Format("Thread 6, edit Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine("Thread 6 cleanly exited");
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
				Assert.Fail(ex.Message);
			}
		}

		P4ServerMT serverMT = null;

#if _LOG_TO_FILE
		static System.IO.StreamWriter sw = null;

		public static void WriteLine(string msg)
		{
			lock (sw)
			{
				sw.WriteLine(msg);
				sw.Flush();
			}
		}

        public static void LogBridgeMessage( int log_level,String source,String message )
		{
			WriteLine(string.Format("[{0}] {1}:{2}", source, log_level, message));
		}

        private static LogFile.LogMessageDelegate LogFn = new LogFile.LogMessageDelegate(LogBridgeMessage);

#else
		public void WriteLine(string msg)
		{
			Trace.WriteLine(msg);
		}
#endif
        /// <summary>
        ///A test for Running multiple command concurrently
        ///</summary>
        [TestMethod()]
        public void RunAsyncTestA()
        {
            RunAsyncTest(false);
        }

        /// <summary>
        ///A test for Running multiple command concurrently
        ///</summary>
        [TestMethod()]
        public void RunAsyncTestU()
        {
            RunAsyncTest(true);
        }

        public void RunAsyncTest(bool unicode)
		{
#if _LOG_TO_FILE
			using (sw = new System.IO.StreamWriter("C:\\Logs\\RunAsyncTestLog.Txt", true))
			{
				LogFile.SetLoggingFunction(LogFn);
#endif
				string serverAddr = "localhost:6666";
				string user = "admin";
				string pass = string.Empty;
				string ws_client = "admin_space";

				// turn off exceptions for this test
				ErrorSeverity oldExceptionLevel = P4Exception.MinThrowLevel;
				P4Exception.MinThrowLevel = ErrorSeverity.E_NOEXC;

		        Process p4d = null;

				try
				{
                    p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
					using (serverMT = new P4ServerMT(serverAddr, user, pass, ws_client))
					{
                        P4Server server = serverMT.getServer();
						if (unicode)
							Assert.IsTrue(server.UseUnicode, "Unicode server detected as not supporting Unicode");
						else
							Assert.IsFalse(server.UseUnicode, "Non Unicode server detected as supporting Unicode");
                            
						run = true;

						Thread t1 = new Thread(new ThreadStart(cmdThreadProc1));
						t1.Name = "RunAsyncTest Thread t1";
						Thread t2 = new Thread(new ThreadStart(cmdThreadProc2));
						t2.Name = "RunAsyncTest Thread t2";
						Thread t3 = new Thread(new ThreadStart(cmdThreadProc3));
						t3.Name = "RunAsyncTest Thread t3";

						Thread t4 = new Thread(new ThreadStart(cmdThreadProc4));
						t4.Name = "RunAsyncTest Thread t4";
						Thread t5 = new Thread(new ThreadStart(cmdThreadProc5));
						t5.Name = "RunAsyncTest Thread t5";
						Thread t6 = new Thread(new ThreadStart(cmdThreadProc6));
						t6.Name = "RunAsyncTest Thread t6";

						t1.Start();
						Thread.Sleep(TimeSpan.FromSeconds(5)); // wait to start a 4th thread
						t2.Start();
						t3.Start();
						Thread.Sleep(TimeSpan.FromSeconds(5)); // wait to start a 4th thread

						run = false;

						if (t1.Join(1000) == false)
						{
							WriteLine("Thread 1 did not cleanly exit");
							t1.Abort();
						}
						if (t2.Join(1000) == false)
						{
							WriteLine("Thread 2 did not cleanly exit");
							t2.Abort();
						}
						if (t3.Join(1000) == false)
						{
							WriteLine("Thread 3 did not cleanly exit");
							t3.Abort();
						}

						Thread.Sleep(TimeSpan.FromSeconds(15)); // wait 15 seconds so will disconnect

						run = true; ;

						t1 = new Thread(new ThreadStart(cmdThreadProc1));
						t1.Name = "RunAsyncTest Thread t1b";
						t2 = new Thread(new ThreadStart(cmdThreadProc2));
						t2.Name = "RunAsyncTest Thread t2b";
						t3 = new Thread(new ThreadStart(cmdThreadProc3));
						t3.Name = "RunAsyncTest Thread t3b";

						t1.Start();
						t2.Start();
						t3.Start();
						Thread.Sleep(TimeSpan.FromSeconds(1)); // wait to start a 4th thread

						t4.Start();
						Thread.Sleep(TimeSpan.FromSeconds(2)); // wait to start a 5th thread
						t5.Start();
						Thread.Sleep(TimeSpan.FromSeconds(3)); // wait to start a 6th thread
						t6.Start();

						Thread.Sleep(TimeSpan.FromSeconds(15)); // run all threads for 15 seconds

						run = false;

						if (t1.Join(1000) == false)
						{
							WriteLine("Thread 1 did not cleanly exit");
							t1.Abort();
						}
						if (t2.Join(1000) == false)
						{
							WriteLine("Thread 2 did not cleanly exit");
							t2.Abort();
						}
						if (t3.Join(1000) == false)
						{
							WriteLine("Thread 3 did not cleanly exit");
							t3.Abort();
						}
						if (t4.Join(1000) == false)
						{
							WriteLine("Thread 4 did not cleanly exit");
							t4.Abort();
						}
						if (t5.Join(1000) == false)
						{
							WriteLine("Thread 5 did not cleanly exit");
							t5.Abort();
						}
						if (t6.Join(1000) == false)
						{
							WriteLine("Thread 6 did not cleanly exit");
							t6.Abort();
						}
					}
				}
				catch (Exception ex)
				{
					Assert.Fail("Test threw an exception: {0}\r\n{1}", ex.Message, ex.StackTrace);
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}

                // reset the exception level
				P4Exception.MinThrowLevel = oldExceptionLevel;
#if _LOG_TO_FILE
			}
#endif
		}
	}
}
