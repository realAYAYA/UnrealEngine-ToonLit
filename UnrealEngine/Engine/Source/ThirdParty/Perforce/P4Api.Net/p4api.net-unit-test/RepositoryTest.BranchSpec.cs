using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Collections.Generic;
using System.Diagnostics;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for RepositoryTest and is intended
    ///to contain RepositoryTest Unit Tests
    ///</summary>
    public partial class RepositoryTest
    {
        /// <summary>
        ///A test for CreateBranchSpec
        ///</summary>
        [TestMethod()]
        public void CreateBranchSpecTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = new Process();

                p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);

                Server server = new Server(new ServerAddress(uri));
                try
                {
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        BranchSpec b = new BranchSpec();
                        b.Id = "newDepotBranch";
                        b.Owner = "admin";
                        b.Description = "created by perforce";
#pragma warning disable 618
                        b.Options = "locked";
#pragma warning restore 618
                        b.ViewMap = new ViewMap();
                        string v0 = "//depot/main/... //depot/rel1/...";
                        string v1 = "//depot/main/... //depot/rel2/...";
                        string v2 = "//depot/dev/... //depot/main/...";
                        b.ViewMap.Add(v0);
                        b.ViewMap.Add(v1);
                        b.ViewMap.Add(v2);

                        BranchSpec newBranchSpec = rep.CreateBranchSpec(b, null);

                        Assert.IsNotNull(newBranchSpec);
                        Assert.AreEqual("newDepotBranch", newBranchSpec.Id);


                        string v3 = "\"//depot/main/a file with spaces.txt\" \"//depot/rel1/a file with spaces.txt\"";
                        newBranchSpec.ViewMap.Add(v3);
                        newBranchSpec = rep.UpdateBranchSpec(newBranchSpec);
                        Assert.IsNotNull(newBranchSpec);
                        Assert.AreEqual(newBranchSpec.ViewMap.Count, 4);
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
        /// A test for DeleteStream
        ///</summary>
        [TestMethod()]
        public void DeleteBranchSpecTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                Server server = new Server(new ServerAddress(uri));
                try
                {
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

						IList<BranchSpec> blist = rep.GetBranchSpecs(null);

						Assert.IsNotNull(blist);

                        BranchSpec deleteTarget = new BranchSpec();
                        deleteTarget.Id = "MyCode->MyCode2";
                        rep.DeleteBranchSpec (deleteTarget, null);

						blist = rep.GetBranchSpecs(null);

						Assert.IsNull(blist);

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
        /// A test for LockBranchSpec
        ///</summary>
        [TestMethod()]
        public void LockBranchSpecTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                Server server = new Server(new ServerAddress(uri));
                try
                {
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        BranchSpec b = rep.GetBranchSpec("MyCode->MyCode2");
                        b.Locked = true;
                        rep.UpdateBranchSpec(b);
                        BranchSpec b2 = rep.GetBranchSpec("MyCode->MyCode2");
                        Assert.IsTrue(b2.Locked);
                        b2.Locked = false;
                        rep.UpdateBranchSpec(b2);
                        BranchSpec b3 = rep.GetBranchSpec("MyCode->MyCode2");
                        Assert.IsFalse(b3.Locked);

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
        ///A test for GetBranchSpec
        ///</summary>
        [TestMethod()]
        public void GetBranchSpecTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                Server server = new Server(new ServerAddress(uri));
                try
                {
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        string targetBranchSpec = "MyCode->MyCode2";

                        BranchSpec b = rep.GetBranchSpec(targetBranchSpec);

                        Assert.IsNotNull(b);

                        Assert.AreEqual(targetBranchSpec, b.Id);

						BranchSpec b1 = rep.GetBranchSpec("StreamBranchMapWithD3", "//flow/D3", null, null);

						Assert.IsNotNull(b1);

						Assert.IsTrue(b1.ViewMap[0].Left.Path.Contains("flow/D3/..."));

						BranchSpec b2 = rep.GetBranchSpec("StreamBranchMapWithD3OtherParent", "//flow/D3", "//flow/D2", null);

						Assert.IsNotNull(b2);

						Assert.AreEqual(b2.ViewMap.Count, 6);
						Assert.IsTrue(b2.ViewMap[0].Type.Equals(MapType.Exclude));



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
        ///A test for GetBranchSpecs
        ///</summary>
		[TestMethod()]
		public void GetBranchSpecsTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
				Server server = new Server(new ServerAddress(uri));
				try
				{
					Repository rep = new Repository(server);

					using (Connection con = rep.Connection)
					{
						con.UserName = user;
						con.Client = new Client();
						con.Client.Name = ws_client;

						bool connected = con.Connect(null);
						Assert.IsTrue(connected);

						Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        Options bFlags = new Options(BranchSpecsCmdFlags.None, "admin", "", -1);

                        IList<BranchSpec> b = rep.GetBranchSpecs(bFlags);


						Assert.IsNotNull(b);
						Assert.AreEqual(1, b.Count);
						Assert.AreEqual("Created by admin.\n", b[0].Description);

					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}
    }
}
