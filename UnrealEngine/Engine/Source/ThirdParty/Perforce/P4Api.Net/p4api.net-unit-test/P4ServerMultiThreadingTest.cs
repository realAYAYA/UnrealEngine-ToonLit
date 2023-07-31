// change false to true if you want to debug the threadp procs
// currently changes to 30 seconds to wait for a thread to exit 
// and 15 minutes to run the test (and debug), up from 1 second
// and 15 seconds
#if false && DEBUG 
#define SlowJoin
#endif

using System;
using System.Text;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Diagnostics;
using Microsoft.VisualStudio.TestTools.UnitTesting;

using Perforce.P4;
using NLog;

namespace p4api.net.unit.test
{
	[TestClass]
	public class P4ServerMultiThreadingTest
	{
		// When debuging often need to slow down time outs to keep from triggering unwanted thread aborts 
#if SlowJoin
		const int JoinTime = 30000;
#else
		const int JoinTime = 1000;
#endif

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

        P4Command cmd1 = null;
		P4Command cmd2 = null;
		P4Command cmd3 = null;
		P4Command cmd4 = null;
		P4Command cmd5 = null;
		P4Command cmd6 = null;

		int cmdCnt1 = 0;
		int cmdCnt2 = 0;
		int cmdCnt3 = 0;
		int cmdCnt4 = 0;
		int cmdCnt5 = 0;
		int cmdCnt6 = 0;

		bool run = true;

		TimeSpan delay = TimeSpan.FromMilliseconds(5);

        class RunThreadException
        {
            public RunThreadException(int threadNumber, Exception threadException)
            {
                ThreadNumber = threadNumber;
                ThreadException = threadException;
            }
            public int ThreadNumber { get; set; }
            public Exception ThreadException { get; set; }
        }
        List<RunThreadException> runThreadExeptions = new List<RunThreadException>();

		private void cmdThreadProc1()
		{
			try
			{
                P4Server server = serverMT.getServer();

				while (run)
				{
					cmdCnt1++;
					cmd1 = new P4Command(server, "fstat", false, "//depot/...");

					DateTime StartedAt = DateTime.Now;

					WriteLine(string.Format("Thread 1 starting command: {0:X8}, at {1}",
						cmd1.CommandId, StartedAt.ToLongTimeString()));

					P4CommandResult result = cmd1.Run();

					WriteLine(string.Format("Thread 1 Finished command: {0:X8}, at {1}, run time {2} Milliseconds",
						cmd1.CommandId, StartedAt.ToLongTimeString(), (DateTime.Now - StartedAt).TotalMilliseconds));

					P4CommandResult lastResult = server.LastResults;

					if (!result.Success)
					{
                        string msg = string.Format("Thread 1, fstat failed:{0}", (result.ErrorList != null && result.ErrorList.Count > 0) ? result.ErrorList[0].ErrorMessage : "<unknown error>");
                        WriteLine(msg);
                        throw new Exception(msg);
                    }
					else
					{
						WriteLine(string.Format("Thread 1, fstat Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
					//Assert.IsTrue(result.Success);
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine(string.Format("Thread 1 cleanly exited after running {0} commands", cmdCnt1));
				return;
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
                WriteLine(string.Format("cmdThreadProc1 failed with exception: {0}\r\n{1}", ex.Message, ex.StackTrace));
                runThreadExeptions.Add(new RunThreadException(1, ex));
                //Assert.Fail("cmdThreadProc1 failed with exception", ex.Message);
            }
		}

		private void cmdThreadProc2()
		{
			try
			{
                P4Server server = serverMT.getServer();

                while (run)
				{
					cmdCnt2++; 
					cmd2 = new P4Command(server, "dirs", false, "//depot/*");

					DateTime StartedAt = DateTime.Now;

					WriteLine(string.Format("Thread 2 starting command: {0:X8}, at {1}",
						cmd2.CommandId, StartedAt.ToLongTimeString()));

					P4CommandResult result = cmd2.Run();

					WriteLine(string.Format("Thread 2 Finished command: {0:X8}, at {1}, run time {2} Milliseconds",
						cmd2.CommandId, StartedAt.ToLongTimeString(), (DateTime.Now - StartedAt).TotalMilliseconds));

					P4CommandResult lastResult = server.LastResults;

                    // Assert is not allowed on the non primary thread
                    Assert.AreEqual(result.Success, lastResult.Success);

					if (!result.Success)
					{
                        string msg = string.Format("Thread 2, fstat failed:{0}", (result.ErrorList != null && result.ErrorList.Count > 0) ? result.ErrorList[0].ErrorMessage : "<unknown error>");
                        WriteLine(msg);
                        throw new Exception(msg);
                    }
					else
					{
						WriteLine(string.Format("Thread 2, dirs Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
					//Assert.IsTrue(result.Success);
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine(string.Format("Thread 2 cleanly exited after running {0} commands", cmdCnt2));
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
                WriteLine(string.Format("cmdThreadProc2 failed with exception: {0}\r\n{1}", ex.Message, ex.StackTrace));
                runThreadExeptions.Add(new RunThreadException(2, ex));
                //Assert.Fail("cmdThreadProc2 failed with exception", ex.Message);
            }
		}

		private void cmdThreadProc3()
		{
			try
			{
                P4Server server = serverMT.getServer();

                while (run)
				{
					cmdCnt3++;
					cmd3 = new P4Command(server, "edit", false, "-n", "//depot/...");

					DateTime StartedAt = DateTime.Now;

					WriteLine(string.Format("Thread 3 starting command: {0:X8}, at {1}",
						cmd3.CommandId, StartedAt.ToLongTimeString()));

					P4CommandResult result = cmd3.Run();

					WriteLine(string.Format("Thread 3 Finished command: {0:X8}, at {1}, run time {2} Milliseconds",
						cmd3.CommandId, StartedAt.ToLongTimeString(), (DateTime.Now - StartedAt).TotalMilliseconds));

					P4CommandResult lastResult = server.LastResults;

					if (!result.Success)
					{
                        string msg = string.Format("Thread 3, fstat failed:{0}", (result.ErrorList != null && result.ErrorList.Count > 0) ? result.ErrorList[0].ErrorMessage : "<unknown error>");
                        WriteLine(msg);
                        throw new Exception(msg);
                    }
					else
					{
						WriteLine(string.Format("Thread 3, edit Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
					//Assert.IsTrue(result.Success);
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine(string.Format("Thread 3 cleanly exited after running {0} commands", cmdCnt3));
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
                WriteLine(string.Format("cmdThreadProc3 failed with exception: {0}\r\n{1}", ex.Message, ex.StackTrace));
                runThreadExeptions.Add(new RunThreadException(3, ex));
            }
		}

		private void cmdThreadProc4()
		{
			try
			{
                P4Server server = serverMT.getServer();

                while (run)
				{
					cmdCnt4++;

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

					cmd4 = new P4Command(server, "fstat", false, "//depot/...");

					DateTime StartedAt = DateTime.Now;

					WriteLine(string.Format("Thread 4 starting command: {0:X8}, at {1}",
						cmd4.CommandId, StartedAt.ToLongTimeString()));

					P4CommandResult result = cmd4.Run();

					WriteLine(string.Format("Thread 4 Finished command: {0:X8}, at {1}, run time {2} Milliseconds",
						cmd4.CommandId, StartedAt.ToLongTimeString(), (DateTime.Now - StartedAt).TotalMilliseconds));

					P4CommandResult lastResult = server.LastResults;
					if (!result.Success)
                    {
                        string msg = string.Format("Thread 4, fstat failed:{0}", (result.ErrorList != null && result.ErrorList.Count > 0) ? result.ErrorList[0].ErrorMessage : "<unknown error>");
						WriteLine(msg);
                        throw new Exception(msg);

                    }
					else
					{
						WriteLine(string.Format("Thread 4, fstat Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
					//Assert.IsTrue(result.Success);
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine(string.Format("Thread 4 cleanly exited after running {0} commands", cmdCnt4));
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
                WriteLine(string.Format("cmdThreadProc4 failed with exception: {0}\r\n{1}", ex.Message, ex.StackTrace));
                runThreadExeptions.Add(new RunThreadException(4, ex));
                //Assert.Fail("cmdThreadProc4 failed with exception", ex.Message);
            }
		}

		private void cmdThreadProc5()
		{
			try
			{
                P4Server server = serverMT.getServer();

                while (run)
				{
					cmdCnt5++;
					cmd5 = new P4Command(server, "dirs", false, "//depot/*");

					DateTime StartedAt = DateTime.Now;

					WriteLine(string.Format("Thread 5 starting command: {0:X8}, at {1}",
						cmd5.CommandId, StartedAt.ToLongTimeString()));

					P4CommandResult result = cmd5.Run();

					WriteLine(string.Format("Thread 5 Finished command: {0:X8}, at {1}, run time {2} Milliseconds",
						cmd5.CommandId, StartedAt.ToLongTimeString(), (DateTime.Now - StartedAt).TotalMilliseconds));

					P4CommandResult lastResult = server.LastResults;

                    // Assert is not allowed on the non primary thread

                    //Assert.AreEqual(result.Success, lastResult.Success);
                    //if (result.InfoOutput != null)
                    //{
                    //    Assert.AreEqual(result.InfoOutput.Count, lastResult.InfoOutput.Count);
                    //}
                    //else
                    //{
                    //    Assert.IsNull(lastResult.InfoOutput);
                    //}
                    //if (result.ErrorList != null)
                    //{
                    //    Assert.AreEqual(result.ErrorList.Count, lastResult.ErrorList.Count);
                    //}
                    //else
                    //{
                    //    Assert.IsNull(result.ErrorList);
                    //}
                    //if (result.TextOutput != null)
                    //{
                    //    Assert.AreEqual(result.TextOutput, lastResult.TextOutput);
                    //}
                    //else
                    //{
                    //    Assert.IsNull(lastResult.TextOutput);
                    //}
                    //if (result.TaggedOutput != null)
                    //{
                    //    Assert.AreEqual(result.TaggedOutput.Count, lastResult.TaggedOutput.Count);
                    //}
                    //else
                    //{
                    //    Assert.IsNull(lastResult.TaggedOutput);
                    //}
                    //Assert.AreEqual(result.Cmd, lastResult.Cmd);
                    //if (result.CmdArgs != null)
                    //{
                    //    Assert.AreEqual(result.CmdArgs.Length, lastResult.CmdArgs.Length);
                    //}
                    //else
                    //{
                    //    Assert.IsNull(lastResult.CmdArgs);
                    //}

					if (!result.Success)
					{
                        string msg = string.Format("Thread 5, fstat failed:{0}", (result.ErrorList != null && result.ErrorList.Count > 0) ? result.ErrorList[0].ErrorMessage : "<unknown error>");
                        WriteLine(msg);
                        throw new Exception(msg);
                    }
					else
					{
						WriteLine(string.Format("Thread 5, dirs Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>"));
					}
					//Assert.IsTrue(result.Success);
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}
				}
				WriteLine(string.Format("Thread 5 cleanly exited after running {0} commands", cmdCnt5));
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
                WriteLine(string.Format("cmdThreadProc5 failed with exception: {0}\r\n{1}", ex.Message, ex.StackTrace));
                runThreadExeptions.Add(new RunThreadException(5, ex));
                //Assert.Fail("cmdThreadProc5 failed with exception", ex.Message);
            }
		}

		private void cmdThreadProc6a()
		{
			try
			{
                P4Server server = serverMT.getServer();

                    cmdCnt6++;
					cmd6 = new P4Command(server, "edit", false, "-n", "//depot/...");

					DateTime StartedAt = DateTime.Now;
					WriteLine(string.Format("{2} starting command: {0:X8}, at {1}",
						cmd6.CommandId, StartedAt.ToLongTimeString(), t6Name));

					P4CommandResult result = cmd6.Run();

					WriteLine(string.Format("{3} Finished command: {0:X8}, at {1}, run time {2} Milliseconds",
						cmd6.CommandId, StartedAt.ToLongTimeString(), (DateTime.Now - StartedAt).TotalMilliseconds, t6Name));

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
                        string msg = string.Format("Thread 6, fstat failed:{0}", (result.ErrorList != null && result.ErrorList.Count > 0) ? result.ErrorList[0].ErrorMessage : "<unknown error>");
                        WriteLine(msg);
                        throw new Exception(msg);
                    }
					else
					{
						WriteLine(string.Format("{1}, edit Success:{0}", (result.InfoOutput != null && result.InfoOutput.Count > 0) ? result.InfoOutput[0].Message : "<no output>", t6Name));
					}
					//Assert.IsTrue(result.Success);
					if (delay != TimeSpan.Zero)
					{
						Thread.Sleep(delay);
					}

					WriteLine(string.Format("{1} cleanly exited after running 1 of {0} commands", cmdCnt6, t6Name));
			}
			catch (ThreadAbortException)
			{
				Thread.ResetAbort();
				return;
			}
			catch (Exception ex)
			{
                WriteLine(string.Format("cmdThreadProc6 failed with exception: {0}\r\n{1}", ex.Message, ex.StackTrace));
                runThreadExeptions.Add(new RunThreadException(6, ex));
                //Assert.Fail("cmdThreadProc6a failed with exception", ex.Message);
            }
		}

		string t6Name = null;

		// thread 6 continulay spawns new threads to run commands
		// this generates large numbers of threads that are used only once
		// to test that we can handle large numbers of unique thread ids
		private void cmdThreadProc6()
		{
            try
            {
                int threadNum = 0;
                while (run)
                {
                    t6Name = string.Format("RunAsyncTest Thread t6a-{0}", threadNum.ToString());
                    Thread t6a = new Thread(new ThreadStart(cmdThreadProc6a));
                    t6a.Name = t6Name;
                    threadNum++;

                    WriteLine("Spawning " + t6Name);

                    t6a.Start();

                    if (t6a.Join(JoinTime) == false)
                    {
                        WriteLine(t6Name + " did not cleanly exit");
                        t6a.Abort();
                    }
                }
                WriteLine(string.Format("Thread 6 cleanly exited after running {0} commands", cmdCnt6));
            }
			catch (ThreadAbortException)
			{
                Thread.ResetAbort();
				return;
			}
            catch (Exception ex)
            {
                WriteLine(string.Format("cmdThreadProc6a failed with exception: {0}\r\n{1}", ex.Message,ex.StackTrace));
                runThreadExeptions.Add(new RunThreadException(6, ex));
                //Assert.Fail("cmdThreadProc6 failed with exception", ex.Message);
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
		public void RunAsyncTest()
		{
#if _LOG_TO_FILE
			using (sw = new System.IO.StreamWriter("C:\\Logs\\RunAsyncTestLog.Txt", true))
			{
				LogFile.SetLoggingFunction(LogFn);
#endif
			bool unicode = false;

			string serverAddr = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			// turn off exceptions for this test
			ErrorSeverity oldExceptionLevel = P4Exception.MinThrowLevel;
			P4Exception.MinThrowLevel = ErrorSeverity.E_NOEXC;

			for (int i = 0; i < 1; i++) // run once for ascii, change < 1 to < 2 to also run once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
				try
				{
					using (serverMT = new P4ServerMT(serverAddr, user, pass, ws_client))
					{
                        P4Server server = serverMT.getServer();

						if (unicode)
							Assert.IsTrue(server.UseUnicode, "Unicode server detected as not supporting Unicode");
						else
							Assert.IsFalse(server.UseUnicode, "Non Unicode server detected as supporting Unicode");

						cmdCnt1 = 0;
						cmdCnt2 = 0;
						cmdCnt3 = 0;
						cmdCnt4 = 0;
						cmdCnt5 = 0;
						cmdCnt6 = 0;

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
						Thread.Sleep(TimeSpan.FromSeconds(5)); // run a bit 

						run = false; //now stop running commands

						if (t1.Join(JoinTime) == false)
						{
							WriteLine("Thread 1 did not cleanly exit");
							t1.Abort();
						}
						if (t2.Join(JoinTime) == false)
						{
							WriteLine("Thread 2 did not cleanly exit");
							t2.Abort();
						}
						if (t3.Join(JoinTime) == false)
						{
							WriteLine("Thread 3 did not cleanly exit");
							t3.Abort();
						}
                        if (runThreadExeptions.Count > 0)
                        {
                            // one or more run threads threw an excaption
                            string msg = string.Empty;
                            foreach (RunThreadException runThreadException in runThreadExeptions)
                            {
                                msg += string.Format("Thread {0} threw exception: {1}",
                                    runThreadException.ThreadNumber,
                                    runThreadException.ThreadException.Message);
                            }

                            Assert.Fail(msg);
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

#if SlowJoin
						Thread.Sleep(TimeSpan.FromMinutes(15)); // run all threads for 15 sseconds
#else
						Thread.Sleep(TimeSpan.FromSeconds(15)); // run all threads for 15 sseconds
#endif
						run = false;

						if (t1.Join(JoinTime) == false)
						{
							WriteLine("Thread 1 did not cleanly exit");
							t1.Abort();
						}
						if (t2.Join(JoinTime) == false)
						{
							WriteLine("Thread 2 did not cleanly exit");
							t2.Abort();
						}
						if (t3.Join(JoinTime) == false)
						{
							WriteLine("Thread 3 did not cleanly exit");
							t3.Abort();
						}
						if (t4.Join(JoinTime) == false)
						{
							WriteLine("Thread 4 did not cleanly exit");
							t4.Abort();
						}
						if (t5.Join(JoinTime) == false)
						{
							WriteLine("Thread 5 did not cleanly exit");
							t5.Abort();
						}
						if (t6.Join(JoinTime) == false)
						{
							WriteLine("Thread 6 did not cleanly exit");
							t6.Abort();
						}

                        if (runThreadExeptions.Count > 0)
                        {
                            // one or more run threads threw an excaption
                            string msg = string.Empty;
                            foreach (RunThreadException runThreadException in runThreadExeptions)
                            {
                                msg += string.Format("Thread {0} threw exception: {1}\r\n{2}",
                                    runThreadException.ThreadNumber,
                                    runThreadException.ThreadException.Message,
                                    runThreadException.ThreadException.StackTrace);
                            }

                            Assert.Fail(msg);
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
				unicode = !unicode;
			}
			// reset the exception level
			P4Exception.MinThrowLevel = oldExceptionLevel;
#if _LOG_TO_FILE
			}
#endif
		}
	}
}
