using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
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
		///A test for CreateDepot
		///</summary>
		[TestMethod()]
		public void CreateDepotTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

		    Process p4d = null;

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				try
				{
                    p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                    Server server = new Server(new ServerAddress(uri));
					Repository rep = new Repository(server);

					using (Connection con = rep.Connection)
					{
						con.UserName = user;
						con.Client = new Client();
						con.Client.Name = ws_client;

						bool connected = con.Connect(null);
						Assert.IsTrue(connected);

						Assert.AreEqual(con.Status, ConnectionStatus.Connected);

						Depot d = new Depot();
						d.Id = "NewDepot";
						d.Description = "created by perforce";
						d.Owner = "admin";
						d.Type = DepotType.Stream;
                        //TODO StreamDepth
						d.Map = "NewDepot/...";
                        d.StreamDepth = "//NewDepot/1";

						Depot newDepot = rep.CreateDepot(d, null);
						Assert.IsNotNull(newDepot);
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
        ///A test for CreateDepot with an invalid path
        ///</summary>
        [TestMethod()]
        public void CreateInvalidPathDepotTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                    Server server = new Server(new ServerAddress(uri));
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        Depot d = new Depot();
                        d.Id = "NewDepot";
                        d.Description = "created by perforce";
                        d.Owner = "admin";
                        d.Type = DepotType.Stream;
                        d.Map = "123";

                        try
                        {
                            Depot newDepot = rep.CreateDepot(d, null);
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(822153266, e.ErrorCode, "Error in depot specification. "                            
                            + "Map entry '123' must have only 1 wildcard which must be a trailing '/...' or '\\...'.");
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
        ///A test for CreateDepot attempting to add another spec depot
        ///</summary>
        [TestMethod()]
        public void CreateExtraSpecDepotTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                    Server server = new Server(new ServerAddress(uri));
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        Depot d = new Depot();
                        d.Id = "NewDepot";
                        d.Description = "created by perforce";
                        d.Owner = "admin";
                        d.Type = DepotType.Spec;//.Local;
                        d.Map = "123";
                        d.StreamDepth = "//NewDepot/1";

                        try
                        {
                            Depot newDepot = rep.CreateDepot(d, null);
                        }
                        catch (P4Exception e)
                        {                           
                            Assert.AreEqual(839064437, e.ErrorCode, e.Message);
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
		///A test for DeleteDepot
		///</summary>
		[TestMethod()]
		public void DeleteDepotTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

		    Process p4d = null;

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				try
				{
                    p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                    Server server = new Server(new ServerAddress(uri));
					Repository rep = new Repository(server);

					using (Connection con = rep.Connection)
					{
						con.UserName = user;
						con.Client = new Client();
						con.Client.Name = ws_client;

						bool connected = con.Connect(null);
						Assert.IsTrue(connected);

						Assert.AreEqual(con.Status, ConnectionStatus.Connected);

						Depot d = new Depot();
						d.Id = "NewDepot";
						d.Description = "created by perforce";
						d.Owner = "admin";
						d.Type = DepotType.Local;
						d.Map = "NewDepot/...";
                        d.StreamDepth = "//NewDepot/1";

						Depot newDepot = rep.CreateDepot(d, null);

						Assert.IsNotNull(newDepot);

						rep.DeleteDepot(newDepot, null);

						IList<Depot> dlist = rep.GetDepots();

						Assert.IsFalse(dlist.Contains(newDepot));

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
        ///A test for DeleteDepot for a depot with files in it
        ///</summary>
        [TestMethod()]
        public void DeleteDepotWithFilesTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                    Server server = new Server(new ServerAddress(uri));
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);
                       
                        String depotName = "flow";

                        Depot depot = rep.GetDepot(depotName, null);

                        Assert.IsNotNull(depot);
                        try
                        {
                            rep.DeleteDepot(depot, null);
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(822417475, e.ErrorCode, 
                                "Depot flow isn't empty. To delete a depot, all file revisions must be removed " 
                                + "and all lazy copy references from other depots must be severed. Use 'p4 obliterate'"
                                + "or 'p4 snap' to break file linkages from other depots, then clear this depot with "
                                + "'p4 obliteror 'p4 snap' to break file linkages from other depots, then clear this depot "
                                + "with 'p4 obliterate', then retry the deletion.");
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
		///A test for GetDepot
		///</summary>
		[TestMethod()]
		public void GetDepotTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			string targetDepot = "flow";

            Process p4d = null;

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				try
				{
                    p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                    Server server = new Server(new ServerAddress(uri));
					Repository rep = new Repository(server);

					using (Connection con = rep.Connection)
					{
						con.UserName = user;
						con.Client = new Client();
						con.Client.Name = ws_client;

						bool connected = con.Connect(null);
						Assert.IsTrue(connected);

						Assert.AreEqual(con.Status, ConnectionStatus.Connected);

						Depot d = rep.GetDepot(targetDepot, null);

						Assert.IsNotNull(d);
						Assert.AreEqual(targetDepot, d.Id);
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
		///A test for GetDepots
		///</summary>
		[TestMethod()]
		public void GetDepotsTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

		    Process p4d = null;

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				try
				{
                    p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                    Server server = new Server(new ServerAddress(uri));
					Repository rep = new Repository(server);

					using (Connection con = rep.Connection)
					{
						con.UserName = user;
						con.Client = new Client();
						con.Client.Name = ws_client;

						bool connected = con.Connect(null);
						Assert.IsTrue(connected);

						Assert.AreEqual(con.Status, ConnectionStatus.Connected);

						IList<Depot> dlist = rep.GetDepots();

						Assert.IsTrue(dlist[0].Id.Equals("depot"));
						Assert.IsTrue(dlist[1].Map.Equals("flow/..."));
						Assert.IsTrue(dlist[2].Type.Equals(DepotType.Stream));
						Assert.IsTrue(dlist[3].Description.Equals("Depot For 'Rocket' project\n\nEVENTS/new_stream_events/events0100_create_depots.pl-Event_001-perforce-CREATE_DEPOTS-Creating depots...\n"));
						//DateTime modified = new DateTime(2010, 10, 19, 10, 40, 3);
						//Assert.AreEqual(modified, dlist[0].Modified);
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
        ///A test for CheckDepotTypesA
        ///</summary>
        [TestMethod()]
        public void CheckDepotTypesTestA()
        {
            CheckDepotTypesTest(false);
        }

        /// <summary>
        ///A test for CheckDepotTypesU
        ///</summary>
        [TestMethod()]
        public void CheckDepotTypesTestU()
        {
            CheckDepotTypesTest(true);
        }

        /// <summary>
        ///A test for CheckDepotTypes
        ///</summary>
        public void CheckDepotTypesTest(bool unicode)
        {
            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            Process p4d = null;

            try
            {
                p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                Server server = new Server(new ServerAddress(uri));
                Repository rep = new Repository(server);

                using (Connection con = rep.Connection)
                {
                    con.UserName = user;
                    con.Client = new Client();
                    con.Client.Name = ws_client;

                    bool connected = con.Connect(null);
                    Assert.IsTrue(connected);

                    Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                    Depot d = new Depot();
                    d.Id = "LocalDepot";
                    d.Description = "created by perforce";
                    d.Owner = "admin";
                    d.Type = DepotType.Local;
                    d.Map = "LocalDepot/...";
                    d.StreamDepth = "//StreamDepot/1";

                    rep.CreateDepot(d);

                    d = new Depot();
                    d.Id = "RemoteDepot";
                    d.Description = "created by perforce";
                    d.Owner = "admin";
                    d.Type = DepotType.Remote;
                    d.Map = "RemoteDepot/...";
                    d.StreamDepth = "//StreamDepot/1";

                    rep.CreateDepot(d);

                    d = new Depot();
                    d.Id = "StreamDepot";
                    d.Description = "created by perforce";
                    d.Owner = "admin";
                    d.Type = DepotType.Stream;
                    d.Map = "StreamDepot/...";
                    d.StreamDepth = "//StreamDepot/1";

                    rep.CreateDepot(d);

                    d = new Depot();
                    d.Id = "ArchiveDepot";
                    d.Description = "created by perforce";
                    d.Owner = "admin";
                    d.Type = DepotType.Archive;
                    d.Map = "ArchiveDepot/...";
                    d.StreamDepth = "//StreamDepot/1";

                    rep.CreateDepot(d);

                    d = new Depot();
                    d.Id = "UnloadDepot";
                    d.Description = "created by perforce";
                    d.Owner = "admin";
                    d.Type = DepotType.Unload;
                    d.Map = "UnloadDepot/...";
                    d.StreamDepth = "//StreamDepot/1";

                    rep.CreateDepot(d);

                    d = new Depot();
                    d.Id = "TangentDepot";
                    d.Description = "created by perforce";
                    d.Owner = "admin";
                    d.Type = DepotType.Tangent;
                    d.Map = "TangentDepot/...";
                    d.StreamDepth = "//StreamDepot/1";

                    rep.CreateDepot(d);

                    rep.CreateDepot(d);

                    d = new Depot();
                    d.Id = "GraphDepot";
                    d.Description = "created by perforce";
                    d.Owner = "admin";
                    d.Type = DepotType.Graph;
                    d.Map = "GraphDepot/...";
                    d.StreamDepth = "//StreamDepot/1";

                    rep.CreateDepot(d);
                    
                    IList<Depot> dlist = rep.GetDepots();

                    Assert.IsTrue(dlist[0].Type.Equals(DepotType.Archive));
                    Assert.IsTrue(dlist[1].Type.Equals(DepotType.Local));
                    Assert.IsTrue(dlist[2].Type.Equals(DepotType.Stream));
                    Assert.IsTrue(dlist[3].Type.Equals(DepotType.Graph));
                    Assert.IsTrue(dlist[4].Type.Equals(DepotType.Local));
                    Assert.IsTrue(dlist[5].Type.Equals(DepotType.Stream));
                    Assert.IsTrue(dlist[6].Type.Equals(DepotType.Remote));
                    Assert.IsTrue(dlist[7].Type.Equals(DepotType.Stream));
                    Assert.IsTrue(dlist[8].Type.Equals(DepotType.Spec));
                    Assert.IsTrue(dlist[9].Type.Equals(DepotType.Stream));
                    Assert.IsTrue(dlist[10].Type.Equals(DepotType.Tangent));
                    Assert.IsTrue(dlist[11].Type.Equals(DepotType.Stream));
                    Assert.IsTrue(dlist[12].Type.Equals(DepotType.Unload));

                    Options opts = new Options();
                    opts["-t"] = "extension";
                    dlist = rep.GetDepots(opts);

                    Assert.IsTrue(dlist[0].Type.Equals(DepotType.Extension));
                }
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }
        }
    }
}
