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
		///A test for CreateLabel
		///</summary>
		[TestMethod()]
		public void CreateLabelTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
				Process p4d = new Process();

				p4d = Utilities.DeployP4TestServer(TestDir, 9, unicode);

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

						Label l = new Label();
						l.Id = "newLabel";
						l.Owner = "admin";
						l.Description = "created by admin";
#pragma warning disable 618
                  l.Options = "locked";
#pragma warning restore 618
						l.ViewMap = new ViewMap();
						string v0 = "//depot/main/...";
						string v1 = "//depot/rel1/...";
						string v2 = "//depot/rel2/...";
						string v3 = "//depot/dev/...";
						l.ViewMap.Add(v0);
						l.ViewMap.Add(v1);
						l.ViewMap.Add(v2);
						l.ViewMap.Add(v3);

						Label newLabel = rep.CreateLabel(l, null);

						Assert.IsNotNull(newLabel);
						Assert.AreEqual("newLabel", newLabel.Id);

                        string v4 = "\"//depot/rel2/a file with spaces\"";
                        newLabel.ViewMap.Add(v4);

                        newLabel = rep.UpdateLabel(newLabel);

                        Assert.IsNotNull(newLabel);
                        Assert.AreEqual(newLabel.ViewMap.Count,5);

                        Label l2 = new Label();
                        l2.Id = "newLabel2";

                        // create a label from a template
                        Label newLabel2 = rep.CreateLabel(newLabel, new LabelCmdOptions(LabelCmdFlags.None, newLabel.Id));
                        Assert.IsNotNull(newLabel2);
                        Assert.AreEqual(newLabel2.ViewMap.Count, 5);

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
        ///A test for UpdateLabelascii
        ///</summary>
        [TestMethod()]
        public void UpdateLabelTestjob096467A()
        {
            UpdateLabelTestjob096467(false);
        }

        /// <summary>
        ///A test for UpdateLabelunicode
        ///</summary>
        [TestMethod()]
        public void UpdateLabelTestjob096467U()
        {
            UpdateLabelTestjob096467(true);
        }

        /// <summary>
        ///A test for UpdateLabel
        ///</summary>
        public void UpdateLabelTestjob096467(bool unicode)
        {
            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

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

                    string targetLabel = "admin_label";

                    Label l = rep.GetLabel(targetLabel);

                    // change the viewmap to have an exclusionary line
                    ViewMap vm = l.ViewMap;
                    MapEntry original = vm[0];
                    MapEntry newExclude = new MapEntry(MapType.Exclude, original.Left, original.Right);
                    l.ViewMap.Remove(original);
                    l.ViewMap.Add(newExclude);
                    l = rep.UpdateLabel(l);

                    // now update the label without any changes, to
                    // confirm that the exclusionary line does not break
                    // the attempted update
                    l = rep.GetLabel(targetLabel);
                    l = rep.UpdateLabel(l);

                    Assert.IsNotNull(l);
                    Assert.AreEqual(targetLabel, l.Id);
                    Assert.AreEqual(l.ViewMap.Count, 1);
                    Assert.IsTrue(l.ViewMap[0].Type.Equals(MapType.Exclude));
                    Assert.IsTrue(l.ViewMap[0].Left.Path.Equals("//depot/..."));
                }
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }
        }

        /// <summary>
        /// A test for DeleteLabel
        ///</summary>
        [TestMethod()]
		public void DeleteLabelTest()
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

						IList<Label> llist = rep.GetLabels(null);

						Assert.IsNotNull(llist);

						Label deleteTarget = new Label();
						deleteTarget.Id = "admin_label";
						rep.DeleteLabel(deleteTarget, null);

						llist = rep.GetLabels(null);

						Assert.IsNull(llist);

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
        /// A test for LockLabel
        ///</summary>
        [TestMethod()]
        public void LockLabelTest()
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

                        Label l = rep.GetLabel("admin_label");
                        l.Locked = true;
                        rep.UpdateLabel(l);
                        Label l2 = rep.GetLabel("admin_label");
                        Assert.IsTrue(l2.Locked);
                        l2.Locked = false;
                        rep.UpdateLabel(l2);
                        Label l3 = rep.GetLabel("admin_label");
                        Assert.IsFalse(l3.Locked);
                        
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
		///A test for GetLabel
		///</summary>
		[TestMethod()]
		public void GetLabelTest()
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

						string targetLabel = "admin_label";

						Label l = rep.GetLabel(targetLabel);

						Assert.IsNotNull(l);

						Assert.AreEqual(targetLabel, l.Id);

						Assert.AreEqual(l.ViewMap.Count, 1);
						Assert.IsTrue(l.ViewMap[0].Type.Equals(MapType.Include));
						Assert.IsTrue(l.ViewMap[0].Left.Path.Equals("//depot/..."));

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
        ///A test for GetLabelWithRevision
        ///</summary>
        [TestMethod()]
        public void GetLabelWithRevisionTest()
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

                        string targetLabel = "newLabel2";

                        Label l = new Label();
                        l.Id = targetLabel;
                        l.Owner = "admin";
                        l.Description = "created by admin";
#pragma warning disable 618
                        l.Options = "unlocked";
#pragma warning restore 618
                        l.ViewMap = new ViewMap();
                        string v0 = "//depot/main/...";
                        string v1 = "//depot/rel1/...";
                        string v2 = "//depot/rel2/...";
                        string v3 = "//depot/dev/...";
                        l.ViewMap.Add(v0);
                        l.ViewMap.Add(v1);
                        l.ViewMap.Add(v2);
                        l.ViewMap.Add(v3);
                        l.Revision = "2";

                        rep.CreateLabel(l);

                        

                        l = rep.GetLabel(targetLabel);

                        Assert.IsNotNull(l);

                        Assert.AreEqual(targetLabel, l.Id);

                        Assert.AreEqual(l.Revision, "2");
                        Assert.AreEqual(l.ViewMap.Count, 4);
                        Assert.IsTrue(l.ViewMap[0].Type.Equals(MapType.Include));
                        Assert.IsTrue(l.ViewMap[0].Left.Path.Equals("//depot/main/..."));

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
		///A test for GetLabels
		///</summary>
		[TestMethod()]
		public void GetLabelsTest()
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
                       
 						Options ops = new Options();
						ops["-m"] = "1";
                        IList<Label> l = rep.GetLabels(ops);


						Assert.IsNotNull(l);
						Assert.AreEqual(1, l.Count);
						Assert.AreEqual("Created by admin.\n", l[0].Description);

                        //now test for options set for a label that does not exist
                        ops = new Options();
                        ops["-u"] = "nonexistantuser";
                        l = rep.GetLabels(ops);
                        Assert.IsNull(l);

                        // associate a file with a label
                        FileSpec fs = new FileSpec(new DepotPath("//depot/Modifiers/ReadMe.txt"), null);

                        IList<FileSpec> lfs = new List<FileSpec>();
                        lfs.Add(fs);

                        Options ops2 = new Options();
                        IList<FileSpec> target = rep.TagFiles(lfs, "admin_label", ops2);
                        Assert.IsNotNull(target);

                        // get labels which include files in the path //depot/Modifiers/...
                        FileSpec path = new FileSpec(new DepotPath("//depot/Modifiers/..."), null);
                        Options ops3 = new Options();
                        IList<Label> l2 = rep.GetLabels(ops3, path);

                        Assert.IsNotNull(l2);
                        Assert.AreEqual(1, l2.Count);
                        Assert.AreEqual("Created by admin.\n", l2[0].Description);


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
        ///A test for GetLabelsPreServerID
        ///</summary>
        [TestMethod()]
        public void GetLabelsPreServerIDTest()
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

                        // tests to ensure old labels options work, prior
                        // to ServerID argument

                        Options opts = new Options(LabelsCmdFlags.None,null,null,
                            50,null);

                        IList<Label> labels = rep.GetLabels(opts);

                        Assert.IsNotNull(labels);
                        Assert.AreEqual(1, labels.Count);

                        LabelsCmdOptions opts2 = new LabelsCmdOptions(LabelsCmdFlags.None, null, null,
                            50, null);

                        labels = rep.GetLabels(opts2);

                        Assert.IsNotNull(labels);
                        Assert.AreEqual(1, labels.Count);

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
        ///A test for GetLabelsWithOptions
        ///</summary>
        [TestMethod()]
        public void GetLabelsWithOptionsTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {

                // TODO update test with Edge Server installation
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

                        LabelsCmdOptions opts = new LabelsCmdOptions(LabelsCmdFlags.None,
                           null, null,50,null,null);
                        IList<Label> labels = rep.GetLabels(opts);
                        Assert.AreEqual("Created by admin.\n", labels[0].Description);

                        opts = new LabelsCmdOptions(LabelsCmdFlags.Unloaded,
                           null, null, 50, null, null);
                        labels = rep.GetLabels(opts);
                        Assert.IsNull(labels);

                        opts = new LabelsCmdOptions(LabelsCmdFlags.All,
                           null, null, 50, null, null);
                        labels = rep.GetLabels(opts);
                        Assert.AreEqual("Created by admin.\n", labels[0].Description);

                        opts = new LabelsCmdOptions(LabelsCmdFlags.None,
                           null, null, 50, null, "1666-master");
                        labels = rep.GetLabels(opts);
                        Assert.IsNull(labels);
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
        ///A test for GetLabelWithGlobalOption
        ///</summary>
        [TestMethod()]
        public void GetLabelWithGlobalOptionTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                // TODO update test with Edge Server installation
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

                        LabelCmdOptions opts = new LabelCmdOptions(LabelCmdFlags.Global,null);

                        Label label = rep.GetLabel("admin_label", null, opts);
                        Assert.IsNull(opts["-g"]);
                        Assert.IsTrue(opts.ContainsKey("-g"));
                        label.Description = "fetched label with the global option";
                        label = rep.UpdateLabel(label);
                        label = rep.GetLabel("admin_label", null, null);
                        Assert.IsTrue(label.Description.Contains("global option"));
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
