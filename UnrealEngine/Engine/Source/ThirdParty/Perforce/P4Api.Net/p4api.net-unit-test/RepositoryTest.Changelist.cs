using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using NLog;
using System.Globalization;

namespace p4api.net.unit.test
{
	/// <summary>
	///This is a test class for RepositoryTest and is intended
	///to contain RepositoryTest Unit Tests
	///</summary>
	public partial class RepositoryTest
	{
		/// <summary>
		///A test for CreateChangelist
		///</summary>
		[TestMethod()]
		public void CreateChangelistTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

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

						Changelist c = new Changelist();
						c.Description = "New changelist for unit test";

						Options uFlags = new Options(ChangeCmdFlags.IncludeJobs);
						Changelist newGuy = rep.CreateChangelist(c, null);

						Assert.IsNotNull(newGuy);
						Assert.AreNotEqual(-1, newGuy.Id);

						Assert.AreEqual("admin", newGuy.OwnerName);
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
        ///A test for NewChangelist
        ///</summary>
        [TestMethod()]
        public void NewChangelistTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

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

                        Changelist c = rep.NewChangelist();

                        c.Description = "adding a description";
                        Changelist newOne = rep.SaveChangelist(c, null);

                        Assert.IsNotNull(newOne);

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
		///A test for UpdateChangelist
		///</summary>
		[TestMethod()]
		public void UpdateChangelistTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

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

						Changelist c = rep.GetChangelist(5);
						c.Description = "new desc\r\nline 2\nline 3";
						rep.UpdateChangelist(c);

						Changelist d = rep.GetChangelist(5);
                        Assert.AreEqual(d.Description, "new desc\r\nline 2\r\nline 3");
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
        ///A test for UpdateChangelist with another user's pending changelist
        ///</summary>
        [TestMethod()]
        public void UpdateChangelistOtherUserTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

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
                        int changeID = 120;
                        if(unicode)
                        {
                            changeID = 87;
                        }
                        Changelist c = new Changelist();
                        try
                        {
                            c = rep.GetChangelist(changeID);
                            c.Description = "new desc\r\nline 2\nline 3";
                            rep.UpdateChangelist(c);
                        }
                        catch (P4Exception ex)
                        {
                            Assert.AreEqual(838932948, ex.ErrorCode, ("can only be updated by user harold"));
                        }

                        Options opts = new Options();
                        opts["-f"] = null;

                        c = rep.GetChangelist(changeID);
                        c.Description = "new desc\r\nline 2\nline 3";
                        rep.UpdateChangelist(c,opts);

                        Changelist d = rep.GetChangelist(changeID);
                        Assert.AreEqual(d.Description, "new desc\r\nline 2\r\nline 3");
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
        ///A test for UpdateChangelistWithMultilineDescription
        ///</summary>
        [TestMethod()]
        public void UpdateChangelistWithMultilineDescTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

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

                        Changelist c = rep.GetChangelist(5);
                        c.Description = "new desc";
                        rep.UpdateChangelist(c);

                        Changelist d = rep.GetChangelist(5);
                        Assert.AreEqual(d.Description, "new desc");
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
        ///A test for SubmitShelvedFromChangelist
        ///</summary>
        [TestMethod()]
        public void SubmitShelvedFromChangelist()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir,13, unicode);
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
                        Utilities.SetClientRoot(rep, TestDir, unicode, ws_client);

                        Changelist change = new Changelist(5, true);
                        change.initialize(con);

                        // shelve the files in changelist 5
                        con.Client.ShelveFiles(new ShelveFilesCmdOptions(ShelveFilesCmdFlags.None,
                            null,change.Id));

                        // revert the checked out file that was shelved
                        FileSpec file = new FileSpec(new DepotPath("//..."), null, null, null);
                        con.Client.RevertFiles(new RevertCmdOptions(RevertFilesCmdFlags.None, change.Id),
                            file);
                        
                        // submit the shelved file
                        SubmitCmdOptions submitOptions = new 
                            SubmitCmdOptions(Perforce.P4.SubmitFilesCmdFlags.SubmitShelved,
                            5, null, null, null);
                        change.Submit(submitOptions);

                        P4CommandResult last = rep.Connection.LastResults;
                        Assert.IsTrue(last.Success);
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
        ///A test for UpdateSubmittedChangelist
        ///</summary>
        [TestMethod()]
        public void UpdateSubmittedChangelistTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

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

                        Changelist c = rep.GetChangelist(12);
                        c.Description += "\n\tModified!";
                        rep.UpdateSubmittedChangelist(c,null);

                        Changelist d = rep.GetChangelist(12);
                        Assert.IsTrue(d.Description.Contains("Modified!"));

                        // on the non-unicode server edit the description
                        // of Alex's changelist 8 as an admin
                        if (!unicode)
                        {
                            c = rep.GetChangelist(8);
                            c.Description += "\n\tModified!";
                            Options opts = new Options();
                            opts["-f"] = null;
                            rep.UpdateSubmittedChangelist(c, opts);

                            d = rep.GetChangelist(8);
                            Assert.IsTrue(d.Description.Contains("Modified!"));
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
		///A test for DeleteChangelist
		///</summary>
		[TestMethod()]
		public void DeleteChangelistTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

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

						Changelist c = new Changelist();
						c.Description = "New changelist for unit test";

						Changelist newGuy = rep.CreateChangelist(c, null);

						rep.DeleteChangelist(newGuy, null);

						Changelist deadGuy = null;
						try
						{
							deadGuy = rep.GetChangelist(newGuy.Id);
						}
						catch { }

						Assert.IsNull(deadGuy);
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
        ///A test for GetChangelistjob080718
        ///</summary>
        [TestMethod()]
        public void GetChangelistTestjob080718()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";
            Process p4d = Utilities.DeployP4TestServer(TestDir, 17, unicode);
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

                    Changelist c = rep.GetChangelist(156, null);

                    IList<FileSpec> fs = new List<FileSpec>();
                    fs.Add(c.Files[0]);

                    IList<FileMetaData> fmd = rep.GetFileMetaData(fs, null);

                    IList<FileIntegrationRecord> i = rep.GetSubmittedIntegrations(fs ,null);
                    Assert.IsNotNull(i);
                    Assert.AreEqual(i[0].ToFile.DepotPath, c.Files[0].DepotPath);
                    Assert.AreNotEqual(i[0].FromFile.DepotPath, c.Files[0].DepotPath);
                }
            }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
        }

        /// <summary>
        ///A test for GetChangelist
        ///</summary>
        [TestMethod()]
		public void GetChangelistTest()
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

						Changelist c = rep.GetChangelist(5, null);

						Assert.IsNotNull(c);
						Assert.AreEqual("admin", c.OwnerName);
						Assert.AreEqual(c.Files.Count, 1);
						Assert.AreEqual(c.Jobs.Count, 1);
						Assert.IsTrue(c.Files[0].DepotPath.Path.Contains("//depot/MyCode/NewFile.txt"));

						if (unicode == false)
						{
							c = rep.GetChangelist(4, null);
							Assert.AreEqual("admin", c.OwnerName);
							Assert.AreEqual(c.Files.Count, 2);
							Assert.AreEqual(c.Jobs.Count, 1);
							Assert.IsTrue(c.Files[0].DepotPath.Path.Contains("//depot/TheirCode/ReadMe.txt"));
							Assert.AreEqual(c.Files[0].Digest, "C7DECE3DB80A73F3F53AF4BCF6AC0576");
							Assert.AreEqual(c.Files[0].FileSize, 30);
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
        ///A test for GetChangelistWithUTCConversion
        ///</summary>
        [TestMethod()]
        public void GetChangelistWithUTCConversionTest()
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
                        Utilities.SetClientRoot(rep, TestDir, unicode, ws_client);

                        P4Command cmd = new P4Command(rep, "change", true, "5");
                        Options opts = new Options();
                        opts["-o"] = null;
                        Changelist fromChangeCommand = new Changelist();

                        P4CommandResult results = cmd.Run(opts);
                        if (results.Success)
                        {
                            fromChangeCommand.initialize(rep.Connection);
                            fromChangeCommand.FromChangeCmdTaggedOutput((results.TaggedOutput[0]),
                                server.Metadata.DateTimeOffset, FormBase.DSTMismatch(server.Metadata));
                        }

                        SubmitResults sr = fromChangeCommand.Submit(null);
                        
                        int submitted = 17;
                        if (unicode)
                        {
                            submitted = 13;
                        }

                        cmd = new P4Command(rep, "change", true, submitted.ToString());
                        opts = new Options();
                        opts["-o"] = null;
                        fromChangeCommand = new Changelist();

                        results = cmd.Run(opts);
                        if (results.Success)
                        {
                            fromChangeCommand.initialize(rep.Connection);
                            fromChangeCommand.FromChangeCmdTaggedOutput((results.TaggedOutput[0]),
                                server.Metadata.DateTimeOffset, FormBase.DSTMismatch(server.Metadata));
                        }
                        
                        Changelist fromDescribeCommand = rep.GetChangelist(submitted, null);

                        Assert.AreEqual(fromDescribeCommand.ModifiedDate, fromChangeCommand.ModifiedDate);
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
        ///A test for GetChangelistWithUTCConversionNoTZDetails
        ///</summary>
        [TestMethod()]
        public void GetChangelistWithUTCConversionNoTZDetailsTest()
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

                        P4Command cmd = new P4Command(rep, "describe", true, "5");
                        Changelist fromDescribeCommand = new Changelist();

                        P4CommandResult results = cmd.Run(null);
                        if (results.Success)
                        {
                            fromDescribeCommand.FromChangeCmdTaggedOutput((results.TaggedOutput[0]),
                                string.Empty, FormBase.DSTMismatch(server.Metadata));
                        }

                        DateTime unconverted = fromDescribeCommand.ModifiedDate;

                        fromDescribeCommand = new Changelist();
                        results = cmd.Run(null);
                        if (results.Success)
                        {
                            fromDescribeCommand.FromChangeCmdTaggedOutput((results.TaggedOutput[0]),
                                "-200", FormBase.DSTMismatch(server.Metadata));
                        }

                        Assert.AreEqual(unconverted.AddHours(-2), fromDescribeCommand.ModifiedDate);

                        fromDescribeCommand = new Changelist();
                        results = cmd.Run(null);
                        if (results.Success)
                        {
                            fromDescribeCommand.FromChangeCmdTaggedOutput((results.TaggedOutput[0]),
                                "+200", FormBase.DSTMismatch(server.Metadata));
                        }

                        Assert.AreEqual(unconverted.AddHours(2), fromDescribeCommand.ModifiedDate);

                        fromDescribeCommand = new Changelist();
                        results = cmd.Run(null);
                        if (results.Success)
                        {
                            fromDescribeCommand.FromChangeCmdTaggedOutput((results.TaggedOutput[0]),
                                "200", FormBase.DSTMismatch(server.Metadata));
                        }

                        Assert.AreEqual(unconverted.AddHours(2), fromDescribeCommand.ModifiedDate);

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
		///A test for GetChangelists
		///</summary>
		[TestMethod()]
		public void GetChangelistsTest()
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

						IList<Changelist> u = rep.GetChangelists(
                            new Options(ChangesCmdFlags.LongDescription, "admin_space", 10, ChangeListStatus.Pending, null));

						
						Assert.IsNotNull(u);
						Assert.AreEqual(3, u.Count);

					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}

        ///A test for GetChangelistsjob094473
        ///</summary>
        [TestMethod()]
        public void GetChangelistsTestjob094473A()
        {
            GetChangelistsTestjob094473(false);
        }

        ///A test for GetChangelistsjob094473
        ///</summary>
        [TestMethod()]
        public void GetChangelistsTestjob094473U()
        {
            GetChangelistsTestjob094473(true);
        }

        ///A test for GetChangelistsjob094473
        ///</summary>
        public void GetChangelistsTestjob094473(bool unicode)
        {
            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";


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

                    ChangesCmdOptions opts = new ChangesCmdOptions(ChangesCmdFlags.None,null,
                        0,ChangeListStatus.Submitted,null);
                    IList<Changelist> changes = rep.GetChangelists(opts, null);

                    // ascii 122 unicode 126
                    Assert.IsNotNull(changes);
                    if (unicode)
                    {
                        Assert.AreEqual(126, changes.Count);
                    }
                    else
                    {
                        Assert.AreEqual(122, changes.Count);
                    }

                    // only get changes @ or above 100
                    opts = new ChangesCmdOptions(ChangesCmdFlags.None, null,
                        0, ChangeListStatus.Submitted, null, 100);
                    // ascii 39 (151->100) unicode 51 (150->100)
                    changes = rep.GetChangelists(opts, null);
                    Assert.IsNotNull(changes);
                    if (unicode)
                    {
                        Assert.AreEqual(51, changes.Count);
                        Assert.AreEqual(150, changes[0].Id);
                    }
                    else
                    {
                        Assert.AreEqual(39, changes.Count);
                        Assert.AreEqual(151, changes[0].Id);
                    }

                    // only get changes @ or above 100 and reverse the order
                    opts = new ChangesCmdOptions(ChangesCmdFlags.ReverseOrder, null,
                        0, ChangeListStatus.Submitted, null, 100);
                    // ascii 39 (100->151) unicode 51 (100->150)
                    changes = rep.GetChangelists(opts, null);
                    Assert.IsNotNull(changes);
                    if (unicode)
                    {
                        Assert.AreEqual(51, changes.Count);
                        Assert.AreEqual(100, changes[0].Id);
                    }
                    else
                    {
                        Assert.AreEqual(39, changes.Count);
                        Assert.AreEqual(100, changes[0].Id);
                    }
                }
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }

        }

        [TestMethod()]
        ///A test for GetChangelistsjob097882U
        ///</summary>
        public void GetChangelistsTestjob097882U()
        {
            GetChangelistsTestjob097882(true);
        }

        [TestMethod()]
        ///A test for GetChangelistsjob097882A
        ///</summary>
        public void GetChangelistsTestjob097882A()
        {
            GetChangelistsTestjob097882(false);
        }

        ///A test for GetChangelistsjob097882
        ///</summary>
        public void GetChangelistsTestjob097882(bool unicode)
        {
            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

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

                    ChangesCmdOptions opts = new ChangesCmdOptions(ChangesCmdFlags.None, null,
                        0, ChangeListStatus.Submitted, null);
                    FileSpec fs = new FileSpec();
                    fs.DepotPath = new DepotPath("//depot/...");

                    //// Creates and initializes a CultureInfo. This is to create
                    //// a date with - as the seperator
                    CultureInfo myCI = new CultureInfo("en-US", false);

                    //// Clones myCI and modifies the DTFI and NFI instances associated with the clone.
                    CultureInfo myCIclone = (CultureInfo)myCI.Clone();
                    myCIclone.DateTimeFormat.DateSeparator = "-";

                    System.Threading.Thread.CurrentThread.CurrentCulture = myCIclone;

                    DateTime dts = new DateTime(2011, 5, 23, 1, 19, 17);
                    DateTime dte = new DateTime(2011, 5, 25, 1, 19, 17);

                    // confirm that the dates have dashes as seperators
                    Assert.IsTrue(dts.ToString().Contains("5-23-2011"));
                    Assert.IsTrue(dte.ToString().Contains("5-25-2011"));

                    DateTimeVersion start = new DateTimeVersion(dts);
                    DateTimeVersion end = new DateTimeVersion(dte);

                    fs.Version = new VersionRange(start, end);
                    IList<Changelist> changes = rep.GetChangelists(opts, fs);
                    Assert.AreEqual(changes.Count, 4);
                }
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }
        }

    }
}
