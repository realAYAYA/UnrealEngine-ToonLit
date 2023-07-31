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
		///A test for CreateGroup
		///</summary>
		[TestMethod()]
		public void CreateGroupTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			string targetGroup = "thenewguys";

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

						Group u = new Group();
						u.Id = targetGroup;
						u.UserNames = new List<string> { "Alex" };
						u.OwnerNames = new List<string> { "Alex" };
						u.MaxResults = 9999;

						//GroupOptions uFlags = new GroupOptions(GroupFlags.Force);
						Group newGuy = rep.CreateGroup(u, null);

						Assert.IsNotNull(newGuy);
						Assert.AreEqual(targetGroup, newGuy.Id);

                        // create a group connected as admin user with owner Alex and 
                        // users Alice and Alex (no -A needed)
                        Group group = new Group();
                        string targetGroup2 = "Mygroup";
                        group.Id = targetGroup2;
                        group.UserNames = new List<string> { "Alice", "Alex" };
                        group.OwnerNames = new List<string> { "Alex" };
                        
                        Group newGuy2 = rep.CreateGroup(group, null);
                        Assert.IsNotNull(newGuy2);
                        Assert.AreEqual(targetGroup2, newGuy2.Id);

                        bool disconnected = con.Disconnect(null);
                        Assert.IsTrue(disconnected);

                        // connect as admin level user Alex
                        con.UserName = "Alex";
                        con.Client = new Client();
                        con.Client.Name = "Alex_space";

                        connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        // create a group as Alex with owner Alex using -A 
                        Group Alex_group = new Group();
                        string targetGroup3 = "Alex_group";
                        Alex_group.Id = targetGroup3;
                        Alex_group.UserNames = new List<string> { "Alice", "Alex" };
                        Alex_group.OwnerNames = new List<string> { "Alex" };

                        Group newGuy3 = rep.CreateGroup(Alex_group, new Options(GroupCmdFlags.AdminAdd));
                        Assert.IsNotNull(newGuy3);
                        Assert.AreEqual(targetGroup3, newGuy3.Id);
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
		///A test for DeleteGroup
		///</summary>
		[TestMethod()]
		public void DeleteGroupTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			string targetGroup = "deleteme";

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

                        Group u = new Group();
                        u.Id = targetGroup;
                        u.UserNames = new List<string> { "Alex" };
                        u.OwnerNames = new List<string> { "Alex" };
                        u.MaxResults = 9999;

                        Group newGuy = rep.CreateGroup(u, null);

                        IList<Group> u2 = rep.GetGroups(new Options(GroupsCmdFlags.IncludeAllValues, 2));

                        Assert.IsNotNull(u2);
                        Assert.AreEqual(2, u2.Count);

                        rep.DeleteGroup(u, null);

                        u2 = rep.GetGroups(new Options(GroupsCmdFlags.IncludeAllValues, 2));

                        Assert.IsNotNull(u2);
                        Assert.AreEqual(1, u2.Count);

                        Group u3 = new Group();
                        u3.Id = targetGroup;
                        u3.UserNames = new List<string> { "Alex" };
                        u3.OwnerNames = new List<string> { "admin" };
                        u3.MaxResults = 9999;

                        newGuy = rep.CreateGroup(u3, null);
                        try
                        {
                            Assert.Inconclusive("NewGuy {0}", newGuy.Id);
                        }
                        catch { }
                        if (newGuy == null)
                        {
                            P4CommandResult res = con.LastResults;

                            Assert.IsNotNull(res.ErrorList);
                            foreach (P4ClientError e in res.ErrorList)
                            {
                                Assert.Fail("CreateGroup returned null: {0}:{1}", e.ErrorCode, e.ErrorMessage);
                            }
                        }
                        Assert.IsNotNull(newGuy);

                        IList<Group> u4 = rep.GetGroups(new Options(GroupsCmdFlags.IncludeAllValues, 2));

                        Assert.IsNotNull(u4);
                        if (con.ApiLevel >= 38)
                        {
                            Assert.AreEqual(2, u4.Count);
                        }
                        else
                        {
                            Assert.AreEqual(1, u4.Count);
                        }
                        // delete the group when the user is an owner but not a superuser
                        GroupCmdOptions opts = new GroupCmdOptions(GroupCmdFlags.OwnerAccess);

                        rep.DeleteGroup(u3, opts);

                        u4 = rep.GetGroups(new Options(GroupsCmdFlags.IncludeAllValues, 2));

                        Assert.IsNotNull(u4);
                        Assert.AreEqual(1, u4.Count);

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
		///A test for DeleteGroup as non-owner
		///</summary>
		[TestMethod()]
		public void DeleteGroupNotOwnedByUserTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "Alex";
			string pass = string.Empty;
			string ws_client = "Alex_space";

			string targetGroup = "everyone";

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

                        Group u = rep.GetGroup(targetGroup, new Options(GroupsCmdFlags.None, -1));

						Assert.IsNotNull(u);
                        try
                        {
                            rep.DeleteGroup(u, null);
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(805705769, e.ErrorCode, "You don't have permission for this operation.\n");
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
		///A test for GetGroup
		///</summary>
		[TestMethod()]
		public void GetGroupTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "ws_client";

			string targetGroup = "everyone";

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

                        Group u = rep.GetGroup(targetGroup, null);

						Assert.IsNotNull(u);
						Assert.AreEqual(targetGroup, u.Id);

                        bool disconnected = con.Disconnect();
                        Assert.IsTrue(disconnected);

                        // connect as admin user Alex
                        con.UserName = "Alex";
                        con.Client = new Client();
                        con.Client.Name = "Alex_space";

                        connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        // create a group as admin user Alex
                        Group group = new Group();
                        string groupstring = "anotherGroup";
                        group.Id = groupstring;
                        group.UserNames = new List<string> { "Alice" };
                        group.OwnerNames = new List<string> { "Alex" };
                        group.MaxResults = 9999;
                        rep.CreateGroup(group, new Options(GroupCmdFlags.AdminAdd));
                        
                        // use the -A flag to get the group
                        GroupCmdOptions opts = new GroupCmdOptions(GroupCmdFlags.OwnerAccess);
                        Group u2 = rep.GetGroup(groupstring, opts);

                        Assert.IsNotNull(u2);
                        Assert.AreEqual(groupstring, u2.Id);
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
		///A test for GetGroups
		///</summary>
		[TestMethod()]
		public void GetGroupsTest()
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

						// add 2 groups to the depot
                        Group group = new Group();
                        group.Id = "anotherGroup";
                        group.UserNames = new List<string> { "Alice" };
                        group.OwnerNames = new List<string> { "Alice" };
                        group.MaxResults = 9999;
                        rep.CreateGroup(group);

                        Group group1 = new Group();
                        group1.Id = "anotherGroup2";
                        group1.UserNames = new List<string> { "Alice" };
                        group1.OwnerNames = new List<string> { "Alice" };
                        rep.CreateGroup(group1);

                        // get the first 2 groups (out of 3)
                        IList<Group> u = rep.GetGroups(new Options(GroupsCmdFlags.None, 2));
                        Assert.IsNotNull(u);
                        Assert.AreEqual(2, u.Count);
                       
                        // add another group with a subgroup
                        Group group2 = new Group();
                        group2.Id = "superGroup";
                        group2.UserNames = new List<string> { "Alex" };
                        group2.OwnerNames = new List<string> { "Alex" };
                        group2.MaxResults = 9999;
                        group2.SubGroups = new List<string> { "everyone" };
                        rep.CreateGroup(group2);

                        // get all groups that include "everyone" as a subgroup
                        string[] subGroups = new string[1] { "everyone" };
                        IList<Group> u2 = rep.GetGroups(new Options(GroupsCmdFlags.IncludeIndirect, -1), subGroups);

                        Assert.IsNotNull(u2);
                        Assert.AreEqual(1, u2.Count);

                        // get all groups that include "Alice"
                        IList<Group> u3 = rep.GetGroups(new Options(GroupsCmdFlags.IncludeIndirect, -1), "Alice");
                        Assert.IsNotNull(u3);
                        Assert.AreEqual(4, u3.Count);

                        // use the -v flag to get the MaxResults, MaxScanRows, MaxLockTime, and
                        // Timeout values for the specified group. 
                        IList<Group> u4 = rep.GetGroups(new Options(GroupsCmdFlags.IncludeAllValues, 1));

                        Assert.IsNotNull(u4);
                        Assert.AreEqual(1, u4.Count);
                        foreach (Group g in u4)
                        {
                            if (g.Id == "superGroup")
                            {
                                Assert.AreEqual(9999, g.MaxResults);
                                Assert.AreEqual("everyone", g.SubGroups[0]);
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
        ///A test for UpdateGroup
        ///</summary>
        [TestMethod()]
        public void UpdateGroupTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            string targetGroup = "everyone";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, 13, unicode);
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

                        Group u = new Group();
						u.Id = targetGroup;
						u.UserNames = new List<string> { "admin" };
						u.OwnerNames = new List<string> { "admin" };
						u.MaxResults = 9999;
                        u.PasswordTimeout = 1111;
                     
                        Group newGuy = rep.CreateGroup(u, null);

						Assert.IsNotNull(newGuy);
						Assert.AreEqual(targetGroup, newGuy.Id);
                     
                        newGuy.UserNames.Add("Alice");
                        newGuy.UserNames.Add("Alex");
                        Group u2 = rep.UpdateGroup(newGuy);

                        Assert.IsNotNull(u2);
                        Assert.AreEqual(targetGroup, u2.Id);
                        Assert.AreEqual(3, u2.UserNames.Count);
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
        ///A test for UpdateGroupByOwner
        ///</summary>
        [TestMethod()]
        public void UpdateGroupByOwnerTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            string targetGroup = "everyone";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, 13, unicode);
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

                        Group u = new Group();
                        u.Id = targetGroup;
                        u.UserNames = new List<string> { "admin" };
                        u.UserNames.Add("Alice");
                        u.UserNames.Add("Alex");
                        u.OwnerNames = new List<string> { "Alex" };
                        u.MaxResults = 9999;

                        u = rep.UpdateGroup(u, null);

                        Assert.IsNotNull(u);
                        Assert.AreEqual(targetGroup, u.Id);

                        con.Disconnect();
                        con.UserName = "Alex";
                        connected = con.Connect(null);
                        Assert.IsTrue(connected);
                        con.Credential = rep.Connection.Login("pass");

                        u = rep.GetGroup(targetGroup);

                        u.MaxResults = 4444;
                        u.UserNames.Remove("Alice");
                        Options gFlags = new Options(GroupCmdFlags.OwnerAccess);
                        u = rep.UpdateGroup(u, gFlags);

                        u = rep.GetGroup(targetGroup);
                        Assert.IsNotNull(u);
                        Assert.AreEqual(u.MaxResults,4444);
                        Assert.IsFalse(u.UserNames.Contains("Alice"));
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
        ///A test for UpdateGroup using -A. This flag can only be used to add new groups, 
        ///not update existing ones.
        ///</summary>
        [TestMethod()]
        public void UpdateGroupWithAdminFlagTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            string targetGroup = "everyone";

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

                        Group newGuy = rep.GetGroup(targetGroup);

                        Assert.IsNotNull(newGuy);

                        newGuy.UserNames.Add("Alice");
                        newGuy.UserNames.Add("Alex");

                        try
                        {
                            Group u2 = rep.CreateGroup(newGuy, new Options(GroupCmdFlags.AdminAdd));
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(822090446, e.ErrorCode, "Error in group specification.\nNo permission to modify existing group everyone.\n");
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
        ///A test for UpdateGroup using -A. This flag can only be used to add new groups, 
        ///not update existing ones.
        ///</summary>
        [TestMethod()]
        public void CreateGroupWithOwnerFlagTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "Alice";
            string pass = string.Empty;
            string ws_client = "Alice_space";

            string targetGroup = "test";

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

                        Group u = new Group();
                        u.Id = targetGroup;
                        u.OwnerNames = new List<string> { "Alex" };
                       
                        try
                        {
                            Group u2 = rep.CreateGroup(u, new Options(GroupCmdFlags.OwnerAccess));
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(838867416, e.ErrorCode, "Error in group specification.\nUser 'Alex' is not an owner of group '%targetGroup'.\n");
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
        ///A test that creates and reads back the Group
        ///</summary>
        [TestMethod()]
        public void CreateVerifyGroupTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            string targetGroup = "newgroup1";

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

                        Group u = new Group();
                        u.Id = targetGroup;
                        u.UserNames = new List<string> { "Alex" };
                        u.OwnerNames = new List<string> { "Alex" };
                        u.MaxResults = 9999;
                        u.MaxScanRows = 101;
                        u.MaxLockTime = 102;
                        u.MaxOpenFiles = 103;
                        u.TimeOut = 104;
                        u.PasswordTimeout = 105;
                        
                        //GroupOptions uFlags = new GroupOptions(GroupFlags.Force);
                        Group createdGroup = rep.CreateGroup(u, null);

                        Assert.IsNotNull(createdGroup);
                        Assert.AreEqual(targetGroup, createdGroup.Id);

                        Group readGroup = rep.GetGroup(targetGroup, null);

                        Assert.AreEqual(createdGroup.MaxResults, readGroup.MaxResults);
                        Assert.AreEqual(createdGroup.MaxScanRows, readGroup.MaxScanRows);
                        Assert.AreEqual(createdGroup.MaxLockTime, readGroup.MaxLockTime);
                        Assert.AreEqual(createdGroup.MaxOpenFiles, readGroup.MaxOpenFiles);
                        Assert.AreEqual(createdGroup.TimeOut, readGroup.TimeOut);
                        Assert.AreEqual(createdGroup.PasswordTimeout, readGroup.PasswordTimeout);


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
