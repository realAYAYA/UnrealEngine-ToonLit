using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net;
using System.IO;

namespace p4api.net.unit.test
{


	/// <summary>
	///This is a test class for RepositoryTest and is intended
	///to contain RepositoryTest Unit Tests
	///</summary>
	public partial class RepositoryTest
	{

		/// <summary>
		///A test for GetServerMetaData
		///</summary>
		[TestMethod()]
		public void GetServerMetaDataTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                var serverRoot = Utilities.TestServerRoot(TestDir, unicode);
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

#pragma warning disable 612
                  ServerMetaData s = rep.GetServerMetaData();
#pragma warning restore 612

						Assert.IsNotNull(s);

						if (unicode == true)
						{
							Assert.AreEqual(s.UnicodeEnabled, true);
						}

						Assert.AreEqual(s.Root, serverRoot);

						Assert.AreEqual(s.CaseSensitive, false);

						Assert.AreEqual(s.MoveEnabled, true);

						
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
		///A test for GetServerMetaDatajob076750A
		///</summary>
		[TestMethod()]
        public void GetServerMetaDataTestjob076750A()
        {
            GetServerMetaDataTestjob076750(false);
        }

        /// <summary>
		///A test for GetServerMetaDatajob076750U
		///</summary>
		[TestMethod()]
        public void GetServerMetaDataTestjob076750U()
        {
            GetServerMetaDataTestjob076750(true);
        }
        /// <summary>
        ///A test for GetServerMetaDatajob076750
        ///</summary>
        public void GetServerMetaDataTestjob076750(bool unicode)
        {
            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

                Process p4d = Utilities.DeployP4TestServer(TestDir, 8, unicode);
                var serverRoot = Utilities.TestServerRoot(TestDir, unicode);
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

#pragma warning disable 612
                    ServerMetaData s = rep.GetServerMetaData();
#pragma warning restore 612

                    Assert.IsNotNull(s);
                    DateTime dt = new DateTime(1, 1, 1, 0, 0, 0); //(1 / 1 / 0001 12:00:00 AM)
                    Assert.AreNotEqual(s.Version.Date, dt);

                    if (unicode == true)
                    {
                        Assert.AreEqual(s.UnicodeEnabled, true);
                    }

                    Assert.AreEqual(s.Root, serverRoot);

                    Assert.AreEqual(s.CaseSensitive, false);

                    Assert.AreEqual(s.MoveEnabled, true);


                }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
        }

    }
}
