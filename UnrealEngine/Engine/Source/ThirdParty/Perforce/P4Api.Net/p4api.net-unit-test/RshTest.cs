using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Perforce.P4;
using System.IO;
using NLog;
using System.Diagnostics;

namespace p4api.net.unit.test
{
    [TestClass]
    public class RshTest
    {
        private static Logger logger = LogManager.GetCurrentClassLogger();
        String TestDir = @"c:\MyTestDir";

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


        [TestMethod]
        public void RshConnectionTest()
        {
            bool unicode = false;

            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            var p4d = Utilities.DeployP4TestServer(TestDir, unicode, TestContext.TestName);
            Server server = new Server(new ServerAddress("localhost:6666"));
            Repository rep = new Repository(server);

            using (Connection con = rep.Connection)
            {
                con.UserName = user;
                con.Client = new Client();
                con.Client.Name = ws_client;
                bool connected = con.Connect(null);
                Assert.IsTrue(connected);
                Assert.AreEqual(con.Status, ConnectionStatus.Connected);
                uint cmdID = 7;
                string[] args = new string[] { "stop" };
                Assert.IsTrue(con.getP4Server().RunCommand("admin", cmdID, false, args, args.Length));
                logger.Debug("Stopped launched server");
            }

            string uri = Utilities.TestRshServerPort(TestDir, unicode);
            server = new Server(new ServerAddress(uri));
            rep = new Repository(server);
            logger.Debug("Created new server");
            try
            {
                using (Connection con = rep.Connection)
                {
                    con.UserName = user;
                    con.Client = new Client();
                    con.Client.Name = ws_client;

                    logger.Debug("About to connect");
                    Assert.AreEqual(con.Status, ConnectionStatus.Disconnected);
                    Assert.IsTrue(con.Connect(null));
                    Assert.AreEqual(con.Status, ConnectionStatus.Connected);
                    logger.Debug("Connected");
                    Utilities.SetClientRoot(rep, TestDir, unicode, ws_client);

                    FileSpec fs = new FileSpec(new DepotPath("//depot/MyCode/ReadMe.txt"), null);
                    rep.Connection.Client.EditFiles(null, fs);
                    logger.Debug("File edited");
                }
            } finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }
        }
    }
}
