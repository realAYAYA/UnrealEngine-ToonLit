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
		private static string testClientSpec =
@"# A Perforce Client Specification.
#
#  Client:      The client name.
#  Update:      The date this specification was last modified.
#  Access:      The date this client was last used in any way.
#  Owner:       The user who created this client.
#  Host:        If set, restricts access to the named host.
#  Description: A short description of the client (optional).
#  Root:        The base directory of the client workspace.
#  AltRoots:    Up to two alternate client workspace roots.
#  Options:     Client options:
#                      [no]allwrite [no]clobber [no]compress
#                      [un]locked [no]modtime [no]rmdir
#  SubmitOptions:
#                      submitunchanged/submitunchanged+reopen
#                      revertunchanged/revertunchanged+reopen
#                      leaveunchanged/leaveunchanged+reopen
#  LineEnd:     Text file line endings on client: local/unix/mac/win/share.
#  View:        Lines to map depot files into the client workspace.
#
# Use 'p4 help client' to see more about client views and options.

Client:	XP1_usr

Update:	2010/11/29 15:30:32

Access:	2010/11/23 08:26:17

Owner:	XP1

Host:	XPPro001

Description:
	Created by xp1.

Root:	c:\XP1_dev

AltRoots:
	c:\XP1_dev_A1
	c:\XP1_dev_A2

Options:	noallwrite noclobber nocompress unlocked nomodtime normdir

SubmitOptions:	submitunchanged

LineEnd:	local

View:
	//depot/dev/xp1/... //XP1_usr/depot/dev/xp1/...
";

		/// <summary>
		///A test for CreateClient
		///</summary>
		[TestMethod()]
		public void CreateClientTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, 10, unicode);
				Server server = new Server(new ServerAddress(uri));
				try
				{
					Repository rep = new Repository(server);

					using (Connection con = rep.Connection)
					{
						con.UserName = user;
						con.Client = new Client();
						con.Client.Name = ws_client;
					    if (unicode)
					        con.CharacterSetName = "utf8";

						bool connected = con.Connect(null);
						Assert.IsTrue(connected);

						Assert.AreEqual(con.Status, ConnectionStatus.Connected);

						Client c = new Client();
						c.Parse(testClientSpec);
						c.OwnerName = "Alice";

						MapEntry me = new MapEntry(MapType.Include,
							new DepotPath("//depot/dev/xp1/..."),
							new ClientPath("//XP1_usr/depot/dev/xp1/..."));

						c.ViewMap.Add(me);

						c.Stream = "//Rocket/GUI";
						c.StreamAtChange = "82";
						
						Client newGuy = rep.CreateClient(c, null);

						Assert.IsNotNull(newGuy);
						Assert.AreEqual("Alice", newGuy.OwnerName);

						//create another one without stream fields or server id
						Client b = new Client();
						b.Parse(testClientSpec);
						b.Name = "XP2_usr";
						b.OwnerName = "Alice";
						
						b.AltRoots = null;
						MapEntry me2 = new MapEntry(MapType.Include,
							new DepotPath("//depot/dev/xp1/..."),
							new ClientPath("//XP2_usr/depot/dev/xp1/..."));

						b.ViewMap.Clear();
						b.ViewMap.Add(me2);

						Client anotherNewGuy = rep.CreateClient(b, null);

						Assert.IsNotNull(anotherNewGuy);
						Assert.AreEqual("Alice", anotherNewGuy.OwnerName);

                        // add lines with spaces in the view
                        
                        MapEntry me3 = new MapEntry(MapType.Include,
                            new DepotPath("//depot/dev/xp1/file with spaces.txt"),
                            new ClientPath("//XP2_usr/dev/xp1/file with spaces.txt"));

                        b.ViewMap.Clear();
                        b.ViewMap.Add(me3);

					    anotherNewGuy = rep.UpdateClient(b);

                        Assert.IsNotNull(anotherNewGuy);

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
		///A test for DeleteClient
		///</summary>
		[TestMethod()]
		public void DeleteClientTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			string targetClient = "admin_space2";

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
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

						Client c = new Client();
						c.Name = targetClient;

						Options uFlags = new Options(ClientCmdFlags.Force);
						rep.DeleteClient(c, uFlags);

						IList<Client> u2 = rep.GetClients(
							new Options(ClientsCmdFlags.None, null, "XP1_usr", 2, null));

						Assert.IsNull(u2);
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
		///A test for GetClient
		///</summary>
		[TestMethod()]
		public void GetClientTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, 6, unicode);
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

						Client c = rep.GetClient("alex_space", null);

						Assert.IsNotNull(c);
						Assert.AreEqual("alex_space", c.Name);
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
		///A test for GetClient - get streams client
		///</summary>
		[TestMethod()]
		public void GetClientTest2()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, 10, unicode);
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

						Client c = rep.GetClient("bob-main", null);

						Assert.IsNotNull(c);
						Assert.AreEqual("bob-main", c.Name);
						Assert.AreEqual("//Parents/MAIN", c.Stream);
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
		///A test for GetClients
		///</summary>
		[TestMethod()]
		public void GetClientsTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = Utilities.DeployP4TestServer(TestDir, 6, unicode);
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

						IList<Client> u = rep.GetClients(
							new Options(ClientsCmdFlags.IncludeTime, null, null, 2, null));

						Assert.IsNotNull(u);
						Assert.AreEqual(2, u.Count);
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
