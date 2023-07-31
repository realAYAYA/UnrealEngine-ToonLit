using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.IO;
using NLog;
using System.Diagnostics;
using System.Collections.Generic;

namespace p4api.net.unit.test
{
	
	/// <summary>
	///This is a test class for ConnectionTest and is intended
	///to contain all ConnectionTest Unit Tests
	///</summary>
	[TestClass()]
	public class ConnectionTest
	{
        private static Logger logger = LogManager.GetCurrentClassLogger();
        String TestDir = @"c:\MyTestDir";

		/// <summary>
		///Gets or sets the test context which provides
		///information about and functionality for the current test run.
		///</summary>
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
        ///A test for Connect
        ///</summary>
        [TestMethod()]
        public void ConnectTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
                Server server = new Server(new ServerAddress(uri));
                try
                {
                    using (Connection target = new Connection(server))
                    {

                        target.UserName = user;
                        target.Client = new Client();
                        target.Client.Name = ws_client;

                        Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);

                        Assert.IsTrue(target.Connect(null));

                        Assert.AreEqual(target.Status, ConnectionStatus.Connected);
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        /// <summary>
        ///A test for Connectjob084160
        /// testing for attempting to connect
        /// with a user that does not exist, and
        /// auto user creation disabled.
        ///</summary>
        [TestMethod()]
        public void ConnectTestjob084160()
        {
            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            Process p4d = Utilities.DeployP4TestServer(TestDir, false, TestContext.TestName);
            Server server = new Server(new ServerAddress(uri));

            try
            {
                using (Connection target = new Connection(server))
                {

                    target.UserName = user;
                    target.Client = new Client();
                    target.Client.Name = ws_client;

                    Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
                    Assert.IsTrue(target.Connect(null));

                    // Tell the server to disallow auto user creation
                    string[] args = { "set", "dm.user.noautocreate=2" };
                    var cmd = new P4Command(target, "configure", false, args);

                    // Reconnect with non exitent user
                    // with default settings this would 
                    // create a user. Catch the error.
                    target.Disconnect();
                    target.UserName = "notinprotects";
                    target.Connect(null);
                }
            }
            catch (P4Exception p4ex)
            {
                // we're not getting an error code, so look for the message
                Assert.IsTrue(p4ex.Message.Contains("Access for user"));
                Assert.IsTrue(p4ex.Message.Contains("has not been enabled by 'p4 protect'."));
                Assert.AreEqual(p4ex.ErrorLevel, ErrorSeverity.E_FAILED);
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }
        }
        /// <summary>
        ///A test for Connect
        ///</summary>
        [TestMethod()]
        public void ConnectBadCredTest()
        {
            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";


            Process p4d = Utilities.DeployP4TestServer(TestDir, false, TestContext.TestName);
            Server server = new Server(new ServerAddress(uri));
            try
            {
                using (Connection target = new Connection(server))
                {

                    target.UserName = user;
                    target.Client = new Client();
                    target.Client.Name = ws_client;

                    Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);

                    Assert.IsTrue(target.Connect(null));

                    Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                    // now add a null crededntial
                    bool failed = false;
                    try
                    {
                        target.Credential = null;
                    }
                    catch (Exception ex)
                    {
                        failed = true;
                    }
                    Assert.IsFalse(failed);

                    string pw = target.getP4Server().Password;
                    Assert.IsNull(pw);
                }
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }

        }

        /// <summary>
        ///A test for Connect
        ///</summary>
        [TestMethod()]
		public void ConnectUsingP4ConfigTest()
		{
			bool unicode = false;
		    Process p4d = null;

			string oldConfig = P4Server.Get("P4CONFIG");
			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
			    try
			    {
			        p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
			        var clientRoot = Utilities.TestClientRoot(TestDir, unicode);

			        // write a config file in the workspace root 
			        string expected = Path.Combine(clientRoot, "admin_space", "myP4Config.txt");
			        Directory.CreateDirectory(Path.Combine(clientRoot, "admin_space"));
			        P4Server.Set("P4CONFIG", "myP4Config.txt");
			   
			        try
				    {
					    if (System.IO.File.Exists(expected))
					    {
						    System.IO.File.Delete(expected);
					    }
					    using (System.IO.StreamWriter sw = new System.IO.StreamWriter(expected))
					    {
						    sw.WriteLine("P4PORT=localhost:6666");
						    sw.WriteLine("P4USER=admin");
						    sw.WriteLine("P4CLIENT=admin_space");
					    }
					    string actual = P4Server.GetConfig(Path.Combine(clientRoot, "admin_space", "MyCode"));
					    Assert.AreEqual(actual, expected);
				    }
				    catch (Exception ex) {
                        logger.Error("Failed to write config file: {0}", ex.Message);
                        Assert.Fail("Could not write config file");
                    }

				    Server server = new Server(null);
				
					using (Connection target = new Connection(null))
					{
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);

						Options opts = new Perforce.P4.Options();
						opts["cwd"] = Path.Combine(clientRoot, "admin_space", "MyCode");
						Assert.IsTrue(target.Connect(opts));

						Assert.AreEqual(ConnectionStatus.Connected, target.Status);

                        P4Server p4server = target.getP4Server();
						if (unicode)
							Assert.IsTrue(p4server.UseUnicode, "Unicode server detected as not supporting Unicode");
						else
							Assert.IsFalse(p4server.UseUnicode, "Non Unicode server detected as supporting Unicode");

						string actual = p4server.Config;
						Assert.AreEqual(expected, actual, true); // ignore case

						Assert.AreEqual("admin", p4server.User);
						Assert.AreEqual("admin_space", p4server.Client);
						Assert.AreEqual("localhost:6666", p4server.Port);
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
					P4Server.Set("P4CONFIG", oldConfig);
				}
				unicode = !unicode;
			}
		}

		/// <summary>
		///A test for Connect
		///</summary>
		[TestMethod()]
		public void ConnectUsingP4EnviroTest()
		{
			bool unicode = false;

			//string uri = "localhost:6666";
			//string user = "admin";
			//string pass = string.Empty;
			//string ws_client = "admin_space";

			string oldConfig = P4Server.Get("P4CONFIG");

            // No local overrides
			P4Server.Update("P4CONFIG", null);

		    Process p4d = null;

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
			    try
			    {
				    p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
                    var serverRoot = Utilities.TestServerRoot(TestDir, unicode);

				    // set environment variables for this server configuration 
				    try
				    {
					    Environment.SetEnvironmentVariable("P4PORT", "localhost:6666");
					    Environment.SetEnvironmentVariable("P4USER", "admin");
					    Environment.SetEnvironmentVariable("P4CLIENT", "admin_space");
				    }
				    catch { Assert.Fail("Could not set P4 Environment"); }

				    Server server = new Server(null);
			
					using (Connection target = new Connection(null))
					{
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);

                        target.CharacterSetName = unicode ? "utf8" : "none";

						Options opts = new Perforce.P4.Options();
						opts["cwd"] = Path.Combine(serverRoot, "admin_space", "MyCode");
                        Assert.IsTrue(target.Connect(opts));

                        Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                        P4Server p4server = target.getP4Server();

                        if (unicode)
							Assert.IsTrue(p4server.UseUnicode, "Unicode server detected as not supporting Unicode");
						else
							Assert.IsFalse(p4server.UseUnicode, "Non Unicode server detected as supporting Unicode");

						Assert.AreEqual(p4server.User, "admin");
						Assert.AreEqual(p4server.Client, "admin_space");
						Assert.AreEqual(p4server.Port, "localhost:6666");

						string actual = p4server.Config;
						Assert.AreEqual(actual, "noconfig");
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
                    P4Server.Update("P4CONFIG", oldConfig);
                }
				unicode = !unicode;
			}
		}

        /// <summary>
        ///A test for Connecting with env vars
        ///making sure P4CLIENT is not set to null
        ///</summary>
        [TestMethod()]
        public void ConnectUsingP4TestHostEnviroTest()
        {
            bool unicode = false;

            // Environment settings on the test host need to be
            // set as follows:
            // P4USER=admin
            // P4CLIENT=admin_space
            // P4PORT=localhost:6666
            P4Server.Set("P4USER", "admin");
            P4Server.Set("P4CLIENT", "admin_space");
            P4Server.Set("P4PORT", "localhost:6666");

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
                    var serverRoot = Utilities.TestServerRoot(TestDir, unicode);

                    Server server = new Server(null);

                    using (Connection target = new Connection(null))
                    {
                        Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);

                        target.CharacterSetName = unicode ? "utf8" : "none";

                        Options opts = new Perforce.P4.Options();
                        opts["cwd"] = Path.Combine(serverRoot, "admin_space", "MyCode");
                        Assert.IsTrue(target.Connect(opts));

                        Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                        P4Server p4server = target.getP4Server();
                        Assert.AreEqual(p4server.User, "admin");
                        Assert.AreEqual(p4server.Client, "admin_space");
                        Assert.AreEqual(p4server.Port, "localhost:6666");

                        string p4client = target.GetP4EnvironmentVar("P4CLIENT");
                        // make sure P4CLIENT has not changed
                        Assert.AreEqual(p4client, "admin_space");
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        /// <summary>
        ///A test for Connecting with PUC, making sure not to
        ///change P4CLIENT env var
        ///</summary>
        [TestMethod()]
        public void ConnectUsingPUCTest()
        {
            bool unicode = false;

            // Environment settings on the test host need to be
            // set as follows:
            P4Server.Set("P4USER", "admin");
            P4Server.Set("P4CLIENT", "admin_space");
            P4Server.Set("P4PORT", "localhost:6666");

            // if they are set in the test as they are in
            // ConnectUsingP4EnviroTest, the checks for server
            // PUC and env PUC will pass, even though
            // running p4 set on the host machine will show
            // that P4CLIENT has been changed

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
                    var serverRoot = Utilities.TestServerRoot(TestDir, unicode);

                    string uri = "localhost:6666";
                    string user = "admin";
                    string pass = string.Empty;
                    // connecting with a different workspace than
                    // what is set in the env var P4CLIENT
                    // this should not change that var to admin_space2
                    string ws_client = "admin_space2";

                    Server server = new Server(new ServerAddress(uri));


                    using (Connection target = new Connection(server))
                    {
                        Client cli = new Client();
                        cli.Name = ws_client;
                        target.Client = cli;
                        target.UserName = user;
                        Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);

                        target.CharacterSetName = unicode ? "utf8" : "none";

                        Options opts = new Perforce.P4.Options();
                        Assert.IsTrue(target.Connect(opts));

                        Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                        P4Server p4server = target.getP4Server();
                        Assert.AreEqual(p4server.User, "admin");
                        Assert.AreEqual(target.Client.Name, "admin_space2");
                        Assert.AreEqual(p4server.Port, "localhost:6666");
                        
                        string p4client = target.GetP4EnvironmentVar("P4CLIENT");   
                        // make sure P4CLIENT has not changed
                        Assert.AreEqual(p4client, "admin_space");
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        internal String BridgeStringHelper(IntPtr bridgeString)
        {
            String retval = P4Server.MarshalPtrToStringUtf8_Int(bridgeString);
            P4Bridge.ReleaseString(bridgeString);
            return retval;
        }

        /// <summary>
        ///A test for Connecting with env vars
        ///making sure P4CLIENT is not set to null
        ///</summary>
        [TestMethod()]
        public void ConnectUsingPathTest()
        {
            bool unicode = false;
            // create two p4config files, put them in a path, make sure that ConnectionFromPath returns
            // a server connection with the correct settings
            var config1Root = Path.Combine(Utilities.TestClientRoot(TestDir, unicode), "config1");
            var config2Root = Path.Combine(config1Root, "config2");

            Directory.CreateDirectory(config1Root);
            Directory.CreateDirectory(config2Root);

            using (System.IO.StreamWriter sw = new StreamWriter(Path.Combine(config1Root, ".testconfig")))
            {
                sw.WriteLine("P4PORT=testport1");
                sw.WriteLine("P4USER=testuser1");
                sw.WriteLine("P4CLIENT=testclient1");
            }

            using (System.IO.StreamWriter sw = new StreamWriter(Path.Combine(config2Root, ".testconfig")))
            {
                sw.WriteLine("P4PORT=testport2");
                sw.WriteLine("P4USER=testuser2");
                sw.WriteLine("P4CLIENT=testclient2");
            }

            // set the P4CONFIG to .testconfig
            P4Server.Set("P4CONFIG", ".testconfig");

            // attempt to grab some servers based on the path
            IntPtr bridge1 = P4Bridge.ConnectionFromPath(config1Root);
            IntPtr bridge2 = P4Bridge.ConnectionFromPath(config2Root);

            // get the port/user/client info from the bridge
            Assert.AreEqual("testuser1", BridgeStringHelper(P4Bridge.get_user(bridge1)));
            Assert.AreEqual("testport1", BridgeStringHelper(P4Bridge.get_port(bridge1)));
            Assert.AreEqual("testclient1", BridgeStringHelper(P4Bridge.get_client(bridge1)));

            // get the second bridge's settings
            Assert.AreEqual("testuser2", BridgeStringHelper(P4Bridge.get_user(bridge2)));
            Assert.AreEqual("testport2", BridgeStringHelper(P4Bridge.get_port(bridge2)));
            Assert.AreEqual("testclient2", BridgeStringHelper(P4Bridge.get_client(bridge2)));

            // make sure to clean up the bridge servers
            P4Bridge.ReleaseConnection(bridge1);
            P4Bridge.ReleaseConnection(bridge2);
        }

        //private static bool IsHexDigit(char c)
        //{
        //    if (char.IsDigit(c))
        //    {
        //        return true;
        //    }
        //    switch (c)
        //    {
        //        case 'A':
        //        case 'a':
        //        case 'B':
        //        case 'b':
        //        case 'C':
        //        case 'c':
        //        case 'D':
        //        case 'd':
        //        case 'E':
        //        case 'e':
        //        case 'F':
        //        case 'f':
        //            return true;
        //    }

        //    return false;
        //}

        /// <summary>
        /// A test for connecting to IPv6 server address
        /// </summary>
        //[TestMethod()]
        //public void ConnectIPv6Test()
        //{
        //    bool unicode = false;

        //    string tcp = "tcp6";
        //    string uri = tcp + ":::1:6666";
        //    string user = "admin";
        //    string pass = string.Empty;
        //    string ws_client = "admin_space";

        //    for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
        //    {
        //        Process p4d = Utilities.DeployIPv6P4TestServer(TestDir, tcp, unicode);
        //        Server server = new Server(new ServerAddress(uri));
        //        try
        //        {
        //            using (Connection target = new Connection(server))
        //            {
        //                target.UserName = user;
        //                target.Client = new Client();
        //                target.Client.Name = ws_client;

        //                Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
        //                Assert.IsTrue(target.Connect(null));
        //                Assert.AreEqual(target.Status, ConnectionStatus.Connected);
        //            }
        //        }
        //        finally
        //        {
        //            Utilities.RemoveTestServer(p4d, TestDir);
        //        }
        //        unicode = !unicode;
        //    }
        //}

        /// <summary>
        /// A test for connecting to IPv6 or 4 server address
        /// </summary>
        //[TestMethod()]
        //public void ConnectIPv6or4Test()
        //{
        //    bool unicode = false;

        //    string tcp = "tcp64";
        //    string uri = tcp + ":::1:6666";
        //    string user = "admin";
        //    string pass = string.Empty;
        //    string ws_client = "admin_space";

        //    for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
        //    {
        //        Process p4d = Utilities.DeployIPv6P4TestServer(TestDir, tcp, unicode);
        //        Server server = new Server(new ServerAddress(uri));
        //        try
        //        {
        //            using (Connection target = new Connection(server))
        //            {
        //                target.UserName = user;
        //                target.Client = new Client();
        //                target.Client.Name = ws_client;

        //                Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
        //                Assert.IsTrue(target.Connect(null));
        //                Assert.AreEqual(target.Status, ConnectionStatus.Connected);
        //            }
        //        }
        //        finally
        //        {
        //            Utilities.RemoveTestServer(p4d, TestDir);
        //        }
        //        unicode = !unicode;
        //    }
        //}

        /// <summary>
        /// A test for connecting to IPv4 or 6 server address
        /// </summary>
        //[TestMethod()]
        //public void ConnectIPv4or6Test()
        //{
        //    bool unicode = false;

        //    string tcp = "tcp46";
        //    string uri = tcp + ":::1:6666";
        //    string user = "admin";
        //    string pass = string.Empty;
        //    string ws_client = "admin_space";

        //    for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
        //    {
        //        Process p4d = Utilities.DeployIPv6P4TestServer(TestDir, tcp, unicode);
        //        Server server = new Server(new ServerAddress(uri));
        //        try
        //        {
        //            using (Connection target = new Connection(server))
        //            {
        //                target.UserName = user;
        //                target.Client = new Client();
        //                target.Client.Name = ws_client;

        //                Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
        //                Assert.IsTrue(target.Connect(null));
        //                Assert.AreEqual(target.Status, ConnectionStatus.Connected);
        //            }
        //        }
        //        finally
        //        {
        //            Utilities.RemoveTestServer(p4d, TestDir);
        //        }
        //        unicode = !unicode;
        //    }
        //}

        bool IsFingerprint(String msg)
        {
            // must be like this: 
            // FB:4A:4C:06:FB:53:3F:EE:39:75:E9:5E:07:1A:11:FE:46:91:68:11
            // 20 bytes separated by colons
            if (msg.Length != 20 * 2 + 19)
                return false;
            String[] bytes = msg.Split(':');
            if (bytes.Length != 20)
                return false;
            // check each byte for funsies
            foreach (String b in bytes)
            {
                try
                {
                    Int32.Parse(b, System.Globalization.NumberStyles.HexNumber);
                }
                catch (Exception)
                {
                    return false;
                }
            }
            return true;
        }

        /// <summary>
        ///A test for ConnectSSL with trust -y
        ///</summary>
        [TestMethod()]
        public void ConnectSSLTestMinusY()
        {
            bool unicode = false;

            string uri = "ssl:localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeploySSLP4TestServer(TestDir, unicode);
                // export a P4TRUST variable to our TestDir for easy cleanup
                P4Server.Set("P4TRUST", Path.Combine(TestDir, String.Format("ConnectSSLTest.{0}.txt", i)));

                Server server = new Server(new ServerAddress(uri));
                try
                {
                    using (Connection target = new Connection(server))
                    {
                        string trustFlag = "-y";
                        target.UserName = user;
                        target.Client = new Client();
                        target.Client.Name = ws_client;

                        Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
                        Assert.IsTrue(target.TrustAndConnect(null, trustFlag, null));
                        Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                        // check to see if server metadata was retrieved
                        Assert.IsNotNull(target.Server.Metadata);
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        /// <summary>
        ///A test for ConnectSSL with trust -i
        ///</summary>
        [TestMethod()]
        public void ConnectSSLTest()
        {
            bool unicode = false;

            string uri = "ssl:localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeploySSLP4TestServer(TestDir, unicode);
                // export a P4TRUST variable to our TestDir for easy cleanup
                // create random name
                string random = Path.GetRandomFileName();
                P4Server.Set("P4TRUST", Path.Combine(TestDir, String.Format("ConnectSSLTest.{0}.txt", random)));

                Server server = new Server(new ServerAddress(uri));
                try
                {
                    using (Connection target = new Connection(server))
                    {
                        string trustFlag = "-i";
                        string fingerprint = string.Empty;
                        target.UserName = user;
                        target.Client = new Client();
                        target.Client.Name = ws_client;

                        Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
                        try
                        {
                            target.Connect(null);
                        }
                        catch (Exception ex)
                        {
                            string[] sslMsg = ex.Message.Split(new char[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);

                            for (int idx = 1; idx < sslMsg.Length; idx++)
                            {
                                if (string.IsNullOrEmpty(sslMsg[idx]) == false && IsFingerprint(sslMsg[idx]))
                                {
                                    fingerprint = sslMsg[idx];
                                    break;
                                }
                            }
                        }

                        Assert.IsTrue(target.TrustAndConnect(null, trustFlag, fingerprint));
                        Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                        // check to see if server metadata was retrieved
                        Assert.IsNotNull(target.Server.Metadata);
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        /// <summary>
        ///A test for Connect and check server version
        ///</summary>
        [TestMethod()]
        public void ConnectTestCheckServerVersion()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string pass = string.Empty;

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
                    Server server = new Server(new ServerAddress(uri));
              
                    using (Connection target = new Connection(server))
                    {
                        target.Connect(null);
                        switch (target.ApiLevel)
                        {
                            case 28:
                                {
                                    Assert.AreEqual(target.Server.Metadata.Version.Major,
                                    "2009.2");
                                    break;
                                }
                            case 29:
                                {
                                    Assert.AreEqual(target.Server.Metadata.Version.Major,
                                    "2010.1");
                                    break;
                                }
                            case 30:
                                {
                                    Assert.AreEqual(target.Server.Metadata.Version.Major,
                                    "2010.2");
                                    break;
                                }
                            case 31:
                                {
                                    Assert.AreEqual(target.Server.Metadata.Version.Major,
                                    "2011.1");
                                    break;
                                }
                            case 32:
                                {
                                    Assert.AreEqual(target.Server.Metadata.Version.Major,
                                    "2011.2");
                                    break;
                                }
                            case 33:
                                {
                                    //Assert.AreEqual(target.Server.Metadata.Version.Major,
                                    //"2012.1");
                                    break;
                                }
                            case 34:
                                {
                                    Assert.AreEqual(target.Server.Metadata.Version.Major,
                                    "2012.2");
                                    break;
                                }
                            case 40:
                                {
                                    Assert.AreEqual(target.Server.Metadata.Version.Major,
                                     "2015.2");
                                    break;
                                }
                            default:
                                {
                                    Trace.WriteLine(string.Format("ApiLevel: {0}, MajorVersion: {1}",
                                        target.ApiLevel, target.Server.Metadata.Version.Major));
                                    break;
                                }
                        }
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

		/// <summary>
		///A test for Connect
		///</summary>
		[TestMethod()]
		public void ContinualConnectTest()
		{
            bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			Random rdm = new Random();

			int cointoss = rdm.Next(0, 1);
			if (cointoss != 0)
			{
				unicode = true;
			}
			for (int i = 0; i < 1; i++) // run only once for ascii or unicode (randomly), it's a long test
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
				Server server = new Server(new ServerAddress(uri));
				try
				{
					DateTime start = DateTime.Now;
					while (true)
					{
						using (Connection target = new Connection(server))
						{
							string[] args = new string[] { "-m", "1", "//depot/*." };

							uint cmdID = 7;
							using (P4Server _P4Server = new P4Server("localhost:6666", null, null, null))
							{
								string val = P4Server.Get("P4IGNORE");
								int _p4IgnoreSet = string.IsNullOrEmpty(val) ? 0 : 1;

								target.UserName = user;
								target.Client = new Client();
								target.Client.Name = ws_client;
								Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
								Assert.IsTrue(target.Connect(null));
								Assert.AreEqual(target.Status, ConnectionStatus.Connected);
								Assert.IsTrue(target.getP4Server().RunCommand("fstat", cmdID, false, args, args.Length));
							}

							Assert.IsTrue(target.getP4Server().RunCommand("fstat", ++cmdID, false, args, args.Length));
							Assert.IsTrue(target.ApiLevel > 0);
							int delay = rdm.Next(0, 11);
							if (delay > 0)
							{
								System.Threading.Thread.Sleep(TimeSpan.FromSeconds(delay));
							}
						}

						if ((DateTime.Now - start) > TimeSpan.FromSeconds(158))
							break;
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}

		/// <summary>
		///A test for Connect
		///</summary>
		[TestMethod()]
		public void ConnectAndRunCommandsTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
				Server server = new Server(new ServerAddress(uri));
				try
				{
					using (Connection target = new Connection(server))
					{
						target.UserName = user;
						target.Client = new Client();
						target.Client.Name = ws_client;
                        target.CharacterSetName = unicode ? "utf8" : "none";

						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.IsTrue(target.Connect(null));
						Assert.AreEqual(target.Status, ConnectionStatus.Connected);
						string[] args = new string[] {"-m", "1", "//depot/*."};
						uint cmdID = 7; 
						Assert.IsTrue(target.getP4Server().RunCommand("fstat", cmdID, false, args, args.Length));
						Assert.IsTrue(target.getP4Server().RunCommand("fstat", ++cmdID, false, args, args.Length));
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}


#if _TEST_P4AUTH
		/// <summary>
		///A test for Connect using a bad auth server
		///</summary>
		[TestMethod()]
		public void ConnectWithBadP4AuthTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			for (int i = 0; i < 3; i++) // run once for ascii, once for unicode
			{
				String zippedFile = "a.exe";
				if (i == 1)
				{
					zippedFile = "u.exe";
				}
				if (i == 2)
				{
					zippedFile = "s3.exe";
					pass = "Password";
				}

				Process p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, "P4AuthTest.bat");
				Server server = new Server(new ServerAddress(uri));
				try
				{
					using (Connection target = new Connection(server))
					{
						target.UserName = user;
						target.Client = new Client();
						target.Client.Name = ws_client;

						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);

						try
						{
							Assert.IsFalse(target.Connect(null));
						}
						catch (Exception ex)
						{
							Assert.IsTrue(ex is P4Exception);
						}
						Assert.AreNotEqual(target.Status, ConnectionStatus.Connected);
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}
#endif

		/// <summary>
		///A test for Connect
		///</summary>
		[TestMethod()]
		public void ConnectBadTest()
		{
			bool unicode = false;

			string uri = "locadhost:77777";
			string pass = string.Empty;

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
				Server server = new Server(new ServerAddress(uri));
				try
				{
					using (Connection target = new Connection(server))
					{
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.AreEqual(target.Server.State, ServerState.Unknown);

						try
						{
							Assert.IsFalse(target.Connect(null));
						}
						catch (AssertFailedException) 
						{ 
							throw; 
						}
						catch (P4Exception ex)
						{
							Trace.WriteLine(string.Format("ConnectBadTest throw an exception: {0}", ex.Message));
							Trace.WriteLine(string.Format("Stacktrace:\r\n{0}", ex.StackTrace));
						}
						catch (Exception ex)
						{
							Trace.WriteLine(string.Format("ConnectBadTest throw an exception: {0}", ex.Message));
							Trace.WriteLine(string.Format("Stacktrace:\r\n{0}", ex.StackTrace));
						}
						Assert.AreEqual(target.Server.State, ServerState.Offline);
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}

		/// <summary>
		///A test for Disconnect
		///</summary>
		[TestMethod()]
		public void DisconnectTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";
			

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
				Server server = new Server(new ServerAddress(uri));
				try
				{
					using (Connection target = new Connection(server))
					{
						target.UserName = user;
						target.Client = new Client();
						target.Client.Name = ws_client;

						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.AreEqual(target.Server.State, ServerState.Unknown);
						Assert.IsTrue(target.Connect(null));
						Assert.AreEqual(target.Server.State, ServerState.Online);
						Assert.AreEqual(target.Status, ConnectionStatus.Connected);
						Assert.IsTrue(target.Disconnect(null));
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.IsFalse(target.Disconnect(null));
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}

		/// <summary>
		///A test for Client
		///</summary>
		[TestMethod()]
		public void ClientTestA()
        {
            ClientTest(false);
        }

        [TestMethod()]
        public void ClientTestU()
        {
            ClientTest(true);
        }

        private void ClientTest(bool unicode)
		{
			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

		    Process p4d = null;

            try
            {
				p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
				Server server = new Server(new ServerAddress(uri));
				
				Repository rep = new Repository(server);

				using (Connection con = rep.Connection)
				{
					con.UserName = user;
					con.Client = new Client();
					con.Client.Name = ws_client;

					Assert.AreEqual(con.Status, ConnectionStatus.Disconnected);
					Assert.AreEqual(con.Server.State, ServerState.Unknown);
					Assert.IsTrue(con.Connect(null));
					Assert.AreEqual(con.Server.State, ServerState.Online);
					Assert.AreEqual(con.Status, ConnectionStatus.Connected);
					P4Command syncCmd = new P4Command(con.getP4Server(), "sync", false);
					P4CommandResult r = syncCmd.Run();
					Assert.AreEqual(r.ErrorList[0].ErrorMessage, "File(s) up-to-date.\n");
				}

				using (Connection con = rep.Connection)
				{
					con.UserName = user;
					con.Client = new Client();
					ws_client = "ws_bad_client";
					con.Client.Name = ws_client;	
					
					bool failed = false;
					Assert.IsTrue(con.Connect(null));
						
					try
					{
						P4Command syncCmd = new P4Command(con.getP4Server(), "sync", false);
						P4CommandResult r = syncCmd.Run();
					}
					catch
					{
						failed = true;
					}

					Assert.IsTrue(failed);
					ws_client = "admin_space";
				}
			}
			finally
			{
				Utilities.RemoveTestServer(p4d, TestDir);
			}
		}


        /// <summary>
        /// A fail test for Login (Ascii)
        ///</summary>
        [TestMethod()]
        public void LoginFailTestA()
        {
            LoginFailTest("a.exe", "Alex", "alex_space", false);
        }

        /// <summary>
        /// A fail test for Login (Unicode)
        ///</summary>
        [TestMethod()]
        public void LoginFailTestU()
        {
            LoginFailTest("u.exe", "Алексей", "alex_space", true);
        }

        /// <summary>
        /// A fail test for Login (security 3)
        ///</summary>
        [TestMethod()]
        public void LoginFailTestS()
        {
            LoginFailTest("s3.exe", "Alex", "alex_space", false);
        }

        private void LoginFailTest(string exeName, string user, string client, bool unicode)
        {
            string uri = "localhost:6666";
            string ticketfile = "c:\\notaticketfile";

            System.IO.File.Delete(ticketfile);

            Process p4d = null;
            try
            {
                P4Server.Set("P4TICKETS", ticketfile);
                p4d = Utilities.DeployP4TestServer(TestDir, 10, exeName, unicode);
                Server server = new Server(new ServerAddress(uri));

                using (Connection target = new Connection(server))
                {
                    Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
                    Assert.AreEqual(target.Server.State, ServerState.Unknown);

                    target.UserName = user;

                    bool threwLoginException = false;
                    try
                    {
                        Assert.IsTrue(target.Connect(null));
                        Client clientObj = new Client();
                        clientObj.Name = client;
                        target.Client = clientObj;
                    }
                    catch (P4Exception e)
                    {
                        if (e.ErrorCode == P4ClientError.MsgServer_BadPassword)
                            threwLoginException = true;
                    }

                    Assert.IsTrue(threwLoginException);
                }
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }
        }

        /// <summary>
        ///A test for Login
        ///</summary>
        [TestMethod()]
		public void LoginTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = "pass";

		    Process p4d = null;

			for (int i = 0; i < 3; i++) // run once for ascii, once for unicode, once for the security level 3 server
			{
			    try
			    {
				    String zippedFile = "a.exe";
				    if (i == 1)
				    {
					    zippedFile = "u.exe";
				    }
				    if (i == 2)
				    {
					    zippedFile = "s3.exe";
					    pass = "Password";
				    }

				    p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, unicode);
				    Server server = new Server(new ServerAddress(uri));
				
					using (Connection target = new Connection(server))
					{
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.AreEqual(target.Server.State, ServerState.Unknown);

						target.UserName = user;
						Options options = new Options();
						options["Password"] = pass;

						Assert.IsTrue(target.Connect(options));
						Assert.AreEqual(target.Server.State, ServerState.Online);
						Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                        Credential cred = target.Login(pass);
						Assert.IsNotNull(cred);
						Assert.AreEqual(user, cred.UserName);
						Assert.IsTrue(target.Logout(null));
						Assert.IsTrue(target.Disconnect(null));
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.IsFalse(target.Disconnect(null));
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
			    unicode = !unicode;
			}
		}
		/// <summary>
		///Another test for Login
		///</summary>
		[TestMethod()]
		public void LoginTest2()
		{
			bool unicode = false;

			string uri = "localhost:6666";   
			string user = "admin";		    
			string pass = "pass";
			string user2 = "Alex";

		    Process p4d = null;
			
			for (int i = 0; i < 3; i++) // run once for ascii, once for unicode, once for the security level 3 server
			{
				String zippedFile = "a.exe";
				if (i == 1)
				{
					zippedFile = "u.exe";
					user2 = "Алексей";
				}
				if (i == 2)
				{
					zippedFile = "s3.exe";
					user2 = "Alex";
					pass = "Password";
				}
                try
                {
				    p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, unicode);
				    Server server = new Server(new ServerAddress(uri));
                    
                    // Override any Test machine P4Config or Environment settings
				    P4Server.Update("P4CLIENT", "");

                    string myclient = P4Server.Get("P4CLIENT");

					using (Connection target = new Connection(server))
					{
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.AreEqual(target.Server.State, ServerState.Unknown);

						target.UserName = user;        

						Options options = new Options();
						options["Password"] = pass;

						Assert.IsTrue(target.Connect(options));
						Assert.AreEqual(target.Server.State, ServerState.Online);
						Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                        // login as admin
                        Credential cred = target.Login(pass);
						Assert.IsNotNull(cred);
						Assert.AreEqual(user, cred.UserName);

                        // Make sure user2 is logged out
                        //target.Logout(new LogoutCmdOptions(LogoutCmdFlags.AllHosts), user2);

                        // Log myself out also
						target.Logout(null);

						target.UserName = user2;

						options = new Options();
						options["Password"] = pass;

						Assert.IsTrue(target.Connect(options));
						Assert.AreEqual(target.Server.State, ServerState.Online);
						Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                        // login as alex/alexei
                        Credential cred2 = target.Login(pass);
						Assert.IsNotNull(cred2);

						Assert.AreEqual(user2, cred2.UserName);

						if (zippedFile != "s3.exe")
						{ Assert.IsTrue(target.Logout(null)); }

						Assert.IsTrue(target.Disconnect(null));
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.IsFalse(target.Disconnect(null));
					}
				}
				finally
				{
                    P4Bridge.ReloadEnviro();
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}
		/// <summary>
		///A test for Login
		///</summary>
		[TestMethod()]
		public void LoginTest3()
		{
			bool unicode = false;

			string uri = "127.0.0.1:6666";
			string user = "admin";
			string pass = "pass";

			for (int i = 2; i < 3; i++) // run once for ascii, once for unicode, once for the security level 3 server
			{
				String zippedFile = "a.exe";
				if (i == 1)
				{
					zippedFile = "u.exe";
				}
				if (i == 2)
				{
					zippedFile = "s3.exe";
					pass = "Password";
				}

				Process p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, unicode);
				Server server = new Server(new ServerAddress(uri));
				try
				{
					using (Connection target = new Connection(server))
					{
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.AreEqual(target.Server.State, ServerState.Unknown);

						target.UserName = user;
						Options options = new Options();
						options["Password"] = pass;

						Assert.IsTrue(target.Connect(options));
						Assert.AreEqual(target.Server.State, ServerState.Online);
						Assert.AreEqual(target.Status, ConnectionStatus.Connected);

						Credential cred = target.Login(pass,true);//,true);
						Assert.IsNotNull(cred);
						Assert.AreEqual(user, cred.UserName);
						Assert.IsTrue(target.Logout(null));

						cred = target.Login(pass,true); //,true);
						Assert.IsNotNull(cred);
						Assert.AreEqual(user, cred.UserName);
						Assert.IsTrue(target.Logout(null));
						Assert.IsTrue(target.Disconnect(null));
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.IsFalse(target.Disconnect(null));
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}
        /// <summary>
        ///A test for Loginjob080984
        ///</summary>
        [TestMethod()]
        public void LoginTestjob080984()
        {
            string uri = "127.0.0.1:6666";
            string user = "admin";
            string pass = "Password";

            String zippedFile = "s3.exe";

            Process p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, false);
            Server server = new Server(new ServerAddress(uri));
            try
            {
                using (Connection target = new Connection(server))
                {
                    // taken from customer submitted Test_P4Api.Net
                    // BUG #1) this won't work.
                    target.SetP4EnvironmentVar("P4TICKETS", @"C:\Users\testUser\p4tickets.txt");

                    // this failed for user, as they tried to set it before
                    // establishing a connection
                    string envVar = target.GetP4EnvironmentVar("P4TICKETS");
                    Assert.IsNull(envVar);

                    Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
                    Assert.AreEqual(target.Server.State, ServerState.Unknown);

                    target.UserName = user;
                    target.Client = new Client();
                    target.Client.Name = "admin_space";

                    Assert.IsTrue(target.Connect(null));
                    Assert.AreEqual(target.Server.State, ServerState.Online);
                    Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                    // now this will work after connected
                    target.SetP4EnvironmentVar("P4TICKETS", @"C:\Users\testUser\p4tickets.txt");
                    envVar = target.GetP4EnvironmentVar("P4TICKETS");
                    Assert.AreEqual(envVar, @"C:\Users\testUser\p4tickets.txt");

                    // now remove it
                    target.SetP4EnvironmentVar("P4TICKETS", "");

                    // BUG #2) No other way to pass the ticket to the api
                    // but setting the environment variable through .Net,
                    // the other api like the python and the C++ one as mean
                    // to do that without modifying the environment variables.

                    // This is correct in that P4API.NET currently does not
                    // allow passing -P for password or ticekt when running
                    // commands. A better workaround than setting the P4TICKETS
                    // p4 environment variable (and assumingly needing to edit it
                    // with the recently obtained ticket), is to set the 
                    // P4PASSWD environment variable with the ticket.

                    // login to get credential. This will return a ticket value
                    // in cred.Ticket as well as creating the p4tickets.txt file
                    // containing that ticket
                    Credential cred = target.Login(pass, true);
                    Assert.IsNotNull(cred);
                    Assert.AreEqual(user, cred.UserName);

                    target.Disconnect();
                    target.Connect(null);

                    // this should work as we currently have a valid ticket
                    FileSpec fs = new FileSpec(new DepotPath("//depot..."),
                        null, null, null);
                    IList<FileSpec> fsl = target.Client.SyncFiles(null, fs);
                    
                    // delete the tickets file
                    var pathWithEnv = @"%USERPROFILE%\p4tickets.txt";
                    var filePath = Environment.ExpandEnvironmentVariables(pathWithEnv);
                    System.IO.File.SetAttributes(filePath, FileAttributes.Normal);
                    System.IO.File.Delete(filePath);

                    target.Disconnect();
                    target.Connect(null);

                    // this should not work. Re-connected, but not logged in
                    // since the ticket is deleted
                    try
                    {
                        fsl = target.Client.SyncFiles(null, fs);
                    }
                    catch (P4Exception p4ex)
                    {
                        // Perforce password (P4PASSWD) invalid or unset.
                        Assert.AreEqual(p4ex.ErrorCode, P4ClientError.MsgServer_BadPassword);
                    }

                    // set the password env var and reconnect
                    Environment.SetEnvironmentVariable("P4PASSWD", cred.Ticket);
                    target.Disconnect();
                    target.Connect(null);

                    // this will now work
                    fsl = target.Client.SyncFiles(null, fs);

                    // BUG #3) The following line will crash 100%, and it's impossible
                    // to catch the exception! The error is provided by Windows : the
                    // program has stopped working. Uncommenting line #34 will crashs
                    // 100 % whatever if line #21 is commented or not or if you use
                    // P4TICKETS instead of P4PORT.
                    //Console.WriteLine(p4Rep.Connection.GetP4EnvironmentVar("P4PORT"));

                    // This does seem to have been the case in 2015.1, when running the
                    // test app. Unable to repro with current version

                    string P4PORT = target.GetP4EnvironmentVar("P4PORT");
                    string P4TICKETS = target.GetP4EnvironmentVar("P4TICKETS");
                    // these both are null as the vars do not exist, but there
                    // is no crash.

                    // BUG #4) While the ticket is not stored on the machine which is
                    // fine because DisplayTicket represents a p4 login using -p flag,
                    // the cred.Ticket property is corrupted, without the fix we sent
                    // you previously, it has the status message of the attempt (success)
                    // and not the Ticket hashstring like it should have, rendering the
                    // Credential object totally useless because it can be used with
                    // future commands since the cred. Ticket property value is erroneous.

                    // unable to repro with current version
                    string host = System.Net.Dns.GetHostName();
                    LoginCmdOptions opts = new LoginCmdOptions(LoginCmdFlags.DisplayTicket,
                        host);
                    cred = target.Login(pass, opts);
                    string ticket = cred.Ticket;
                    // a valid ticket should always have a length of 32
                    Assert.AreEqual(ticket.Length, 32);
                    // customer reported this text in the ticket field
                    Assert.IsFalse(ticket.Contains("Success: Password verified"));

                    Environment.SetEnvironmentVariable("P4PASSWD", null);
                }
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }
        }

        /// <summary>
        ///A test for Login with null Client
        ///</summary>
        [TestMethod()]
        public void LoginTestWithNullClient()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = "pass";

            for (int i = 0; i < 3; i++) // run once for ascii, once for unicode, once for the security level 3 server
            {
                String zippedFile = "a.exe";
                if (i == 1)
                {
                    zippedFile = "u.exe";
                }
                if (i == 2)
                {
                    zippedFile = "s3.exe";
                    pass = "Password";
                }

                Process p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, unicode);
                Server server = new Server(new ServerAddress(uri));
                try
                {
                    using (Connection target = new Connection(server))
                    {
                        Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
                        Assert.AreEqual(target.Server.State, ServerState.Unknown);

                        target.UserName = user;
                        Options options = new Options();
                        options["Password"] = pass;

                        Assert.IsTrue(target.Connect(options));
                        Assert.AreEqual(target.Server.State, ServerState.Online);
                        Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                        // set the connection Client to null
                        target.Client = null;
                        Credential cred = target.Login(pass);
                        // even if Client is null, credential should not be returned
                        // as a null
                        Assert.IsNotNull(cred);
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }
		/// <summary>
		///Another test for Login
		///</summary>
		[TestMethod()]
		public void TrustTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			//			string user = "admin";
			//			string pass = "pass";
			//			string ws_client = "alex_space";

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				String zippedFile = "a.exe";
				if (i == 1)
				{
					zippedFile = "u.exe";
				}

				Process p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, unicode);
				Server server = new Server(new ServerAddress(uri));
				try
				{
					using (Connection target = new Connection(server))
					{
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.AreEqual(target.Server.State, ServerState.Unknown);
						Assert.IsTrue(target.Connect(null));
						Assert.AreEqual(target.Server.State, ServerState.Online);
						Assert.AreEqual(target.Status, ConnectionStatus.Connected);
						TrustCmdOptions options = new TrustCmdOptions(TrustCmdFlags.AutoAccept);
						Assert.IsTrue(target.Trust(options, null));
						Assert.IsTrue(target.Disconnect(null));
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}
        /// <summary>
        ///Another test for Login
        ///</summary>
        [TestMethod()]
        public void CharacterSetNameTestA()
        {
            CharacterSetNameTest(false);
        }

        [TestMethod()]
        public void CharacterSetNameTestU()
        {
            CharacterSetNameTest(true);
        }

        private void CharacterSetNameTest(bool unicode)
		{
            Environment.SetEnvironmentVariable("P4CHARSET", "");
            string uri = "localhost:6666";

			String zippedFile = "a.exe";
			if (unicode)
			{
				zippedFile = "u.exe";
			}

			Process p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, unicode);
			Server server = new Server(new ServerAddress(uri));
			try
			{
				using (Connection target = new Connection(server))
				{
					Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
					Assert.AreEqual(target.Server.State, ServerState.Unknown);
					Assert.IsTrue(target.Connect(null));
					Assert.AreEqual(target.Server.State, ServerState.Online);
					Assert.AreEqual(target.Status, ConnectionStatus.Connected);
					string actual = target.CharacterSetName;
                    string p4charset = target.GetP4EnvironmentVar("P4CHARSET");
					if ((p4charset != null) && (p4charset != "none"))
                    {
                        Assert.AreEqual(p4charset, actual);
                    }
                    else if (unicode)
					{
						// should have been automatically detected if the server is 
						// unicode based on this systems codepage
						Assert.IsFalse(string.IsNullOrEmpty(actual) || (actual == "none"));
					}
					else
					{
						// no charset needed on on non unicode servers
						Assert.IsTrue(string.IsNullOrEmpty(actual) || (actual == "none"));
					}
					Assert.IsTrue(target.Disconnect(null));
				}
			}
			finally
			{
				Utilities.RemoveTestServer(p4d, TestDir);
			}
		}

		/// <summary>
		///A test for SetPassword
		///</summary>
		[TestMethod()]
		public void SetPasswordTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = "pass";

			for (int i = 0; i < 3; i++) // run once for ascii, once for unicode, once for the security level 3 server
			{
				String zippedFile = "a.exe";
				if (i == 1)
				{
					zippedFile = "u.exe";
				}
				if (i == 2)
				{
					zippedFile = "s3.exe";
					pass = "Password";
				}

				Process p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, unicode);
				Server server = new Server(new ServerAddress(uri));
				try
				{
					using (Connection target = new Connection(server))
					{
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.AreEqual(target.Server.State, ServerState.Unknown);

						target.UserName = user;
						Options options = new Options();
						options["Password"] = pass;

						Assert.IsTrue(target.Connect(options));
						Assert.AreEqual(target.Server.State, ServerState.Online);
						Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                        Credential cred = target.Login(pass);
						Assert.IsNotNull(cred);
						Assert.AreEqual(user, cred.UserName);
						Assert.IsTrue(target.SetPassword(pass, pass + "2"));
						Assert.IsTrue(target.Logout(null));
						Assert.IsTrue(target.Disconnect(null));
						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
						Assert.IsFalse(target.Disconnect(null));
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}

        /// <summary>
        ///A test for SetTicketFile
        ///</summary>
        [TestMethod()]
        public void SetTicketFileTest()
        {
            string uri = "localhost:6666";
            string user = "admin";
            string pass = "Password";

            String zippedFile = "s3.exe";
            Process p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, false);
            Server server = new Server(new ServerAddress(uri));
            try
            {
                using (Connection target = new Connection(server))
                {
                    Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
                    Assert.AreEqual(target.Server.State, ServerState.Unknown);

                    target.UserName = user;
                    Options options = new Options();
                    options["Password"] = pass;

                    Assert.IsTrue(target.Connect(options));
                    Assert.AreEqual(target.Server.State, ServerState.Online);
                    Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                    P4Server pServer = target.getP4Server();
                    pServer.SetTicketFile(TestDir + "\\p4tickets.txt");

                    Credential cred = target.Login(pass);
                    Assert.IsNotNull(cred);
                    Assert.AreEqual(user, cred.UserName);

                    string ticket = pServer.GetTicketFile();
                    Assert.IsTrue(System.IO.File.Exists(ticket));
                }
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }
        }

        /// <summary>
        ///A test for GetTicket
        ///</summary>
        [TestMethod()]
        public void GetTicketTest()
        {
            string uri = "localhost:6666";
            string user = "admin";
            string pass = "Password";

            String zippedFile = "s3.exe";
            Process p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile, false);
            Server server = new Server(new ServerAddress(uri));
            try
            {
                using (Connection target = new Connection(server))
                {
                    Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);
                    Assert.AreEqual(target.Server.State, ServerState.Unknown);

                    target.UserName = user;
                    Options options = new Options();
                    options["Password"] = pass;

                    Assert.IsTrue(target.Connect(options));
                    Assert.AreEqual(target.Server.State, ServerState.Online);
                    Assert.AreEqual(target.Status, ConnectionStatus.Connected);

                    P4Server pServer = target.getP4Server();
                    string random = TestDir + "\\" + Path.GetRandomFileName();
                    pServer.SetTicketFile(random);

                    // ticket file just set, should not exist yet
                    Assert.IsFalse(System.IO.File.Exists(random));

                    Credential cred = target.Login(pass);
                    Assert.IsNotNull(cred);
                    Assert.AreEqual(user, cred.UserName);

                    // after login, ticket file should exist
                    Assert.IsTrue(System.IO.File.Exists(random));

                    // it can be challenging to get the port for the server, so
                    // read it directly from the ticket file
                    string ticketFile = pServer.GetTicketFile();
                    string ticketFromFile = System.IO.File.ReadAllText(ticketFile);
                    string port = ticketFromFile.Remove(ticketFromFile.IndexOf('='));

                    // get the ticket using GetTicket(ticketFile, port, user)
                    string ticket = pServer.GetTicket(ticketFile, port, user);

                    // confirm that the ticket has been obtained, and that it
                    // is the same as the one returned in login.
                    Assert.IsNotNull(ticket);
                    Assert.AreEqual(ticket, cred.Ticket);
                }
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }
        }

        /// <summary>
        ///A test for GetExistingTicket
        ///</summary>
        [TestMethod()]
		public void GetTicketFileTest()
		{
			string actual = P4Server.GetEnvironmentTicketFile();

			Assert.IsTrue(actual.Contains("p4tickets.txt"));
		}

		/// <summary>
		///A test for GetExistingTicket
		///</summary>
		[TestMethod()]
		public void GetExistingTicketTest()
		{
#if DEBUG_GET_TICKET
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = "pass";
			string ws_client = "admin_space";

			// run once for the security level 3 server, user has a password and a ticket will get generated
			for (int i = 2; i < 3; i++) 
			{
				String zippedFile = "a.exe";
				if (i == 1)
				{
					zippedFile = "u.exe";
				}
				if (i == 2)
				{
					zippedFile = "s3.exe";
					pass = "Password";
				}

				Process p4d = Utilities.DeployP4TestServer(TestDir, 10, zippedFile);
				Server server = new Server(new ServerAddress(uri));
				try
				{
					using (Connection target = new Connection(server))
					{

						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);

						Assert.AreEqual(target.Server.State, ServerState.Unknown);

						target.UserName = user;
						Options options = new Options();
						options["Password"] = pass;

						Assert.IsTrue(target.Connect(options));

						Assert.AreEqual(target.Server.State, ServerState.Online);

						Assert.AreEqual(target.Status, ConnectionStatus.Connected);

						Credential cred = target.Login(pass, null, null);
						Assert.IsNotNull(cred);

						Assert.AreEqual(user, cred.UserName);

						string ticket = target.GetExistingTicket(user);

						Assert.IsNotNull(ticket);

						Assert.IsTrue(target.Logout(null));

						Assert.IsTrue(target.Disconnect(null));

						Assert.AreEqual(target.Status, ConnectionStatus.Disconnected);

						Assert.IsFalse(target.Disconnect(null));
					}
				}
	finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
			
#endif
		}
        /// <summary>
        ///Another test for Login
        ///</summary>
        [TestMethod()]
        public void GetP4ConfigP4EnvVarWithCWDTest()
        {
            bool unicode = false;

            string server = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            Process p4d = null;
            string spaceRoot = "";

            string oldConfig = P4Server.Get("P4CONFIG");

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
                    var serverRoot = Utilities.TestClientRoot(TestDir, unicode);
                    spaceRoot = Path.Combine(serverRoot, "admin_space");

                    using (Repository rep = new Repository(new Server(new ServerAddress(string.Empty))))
                    {
                        string expected = Path.Combine(spaceRoot, "MyCode", "myP4Config.txt");
                        P4Server.Set("P4CONFIG", "myP4Config.txt");

                        // Delete the config file if it exists
                        if (System.IO.File.Exists(expected))
                        {
                            System.IO.File.Delete(expected);
                        }

                        //make sure it returns null if no config file
                        string actual = P4Server.GetConfig(Path.Combine(spaceRoot, "MyCode"));
                        if (actual != null)
                        {
                            Assert.AreEqual(actual, "noconfig", true);
                        }
                        using (System.IO.StreamWriter sw = new StreamWriter(expected))
                        {
                            sw.WriteLine(string.Format("P4PORT={0}", server));
                            sw.WriteLine(string.Format("P4USER={0}", user));
                            sw.WriteLine(string.Format("P4CLIENT={0}", ws_client));
                        }

                        actual = P4Server.GetConfig(Path.Combine(spaceRoot, "MyCode"));
                        Assert.AreEqual(actual, expected, true);

                        System.Environment.CurrentDirectory = Path.Combine(spaceRoot, "MyCode");

                        Options opts = new Options();
                        opts.Add("ProgramName", "P4UnitTest");
                        opts.Add("ProgramVersion", "1234");
                        opts.Add("cwd", System.Environment.CurrentDirectory);

                        using (Connection con = rep.Connection)
                        {
                            Assert.IsTrue(rep.Connection.Connect(opts));

                            actual = rep.Connection.GetP4EnvironmentVar("P4CONFIG");
                            Assert.AreEqual(actual, "myP4Config.txt", true);

                            actual = rep.Connection.GetP4ConfigFile();
                            Assert.AreEqual(actual, expected, true);

                            actual = rep.Connection.GetP4ConfigFile(System.Environment.CurrentDirectory);
                            Assert.AreEqual(actual, expected, true);

                        }
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                    P4Server.Set("P4CONFIG", oldConfig);
                    if (System.IO.File.Exists(Path.Combine(spaceRoot, "myP4Config.txt")))
                    {
                        System.IO.File.Delete(Path.Combine(spaceRoot, "myP4Config.txt"));
                    }
                }
                unicode = !unicode;
            }
        }

        /// <summary>
        ///A test for mapped CWD job098063
        ///</summary>
        [TestMethod()]
        public void ConnectWithCWDTestA()
        {
            ConnectWithCWDTest(false);
        }

        /// <summary>
        ///A test for mapped CWD job098063
        ///</summary>
        [TestMethod()]
        public void ConnectWithCWDTestU()
        {
            ConnectWithCWDTest(true);
        }
        /// <summary>
        ///A test for mapped CWD job098063
        ///</summary>
        public void ConnectWithCWDTest(bool unicode)
        {
            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            Process p4d = null;

                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
                    var clientRoot = Utilities.TestClientRoot(TestDir, unicode);
                    var adminSpace = Path.Combine(clientRoot, "admin_space");
                    Directory.CreateDirectory(adminSpace);
                    Server server = new Server(new ServerAddress(uri));
                    Repository rep = new Repository(server);

                    Process proc = new Process();
                    proc.StartInfo.FileName = "subst.exe";
                    proc.StartInfo.Arguments = "X: " + adminSpace;
                    proc.Start();

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;
                        List<string> altRoots = new List<string>();
                        altRoots.Add("X:\\");
                        con.Client.AltRoots = altRoots;
                        Assert.AreEqual(con.Status, ConnectionStatus.Disconnected);
                        Assert.AreEqual(con.Server.State, ServerState.Unknown);

                        // set the CWD prior to establishing a connection
                        con.CurrentWorkingDirectory = "X:\\";

                        Assert.IsTrue(con.Connect(null));

                        // need to update the client since we
                        // added an AltRoot for the mapped drive
                        rep.UpdateClient(con.Client);

                        Assert.AreEqual(con.Server.State, ServerState.Online);
                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);
                        Assert.AreEqual("admin", con.Client.OwnerName);
                        Utilities.SetClientRoot(rep, TestDir, unicode, ws_client);

                        FileSpec toFile = new FileSpec(new LocalPath(Path.Combine("X:\\", "MyCode", "ReadMe.txt")),
                            null);
                        Options options = new Options(EditFilesCmdFlags.None, -1, null);
                        IList<FileSpec> oldfiles = con.Client.EditFiles(options, toFile);
                        Assert.AreEqual(1, oldfiles.Count);
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                    Process proc = new Process();
                    proc.StartInfo.FileName = "subst.exe";
                    proc.StartInfo.Arguments = "X: /D";
                    proc.Start();
                }
        }

    }
}