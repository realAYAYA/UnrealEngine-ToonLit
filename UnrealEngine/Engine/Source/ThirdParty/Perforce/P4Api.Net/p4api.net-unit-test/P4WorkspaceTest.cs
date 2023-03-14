using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Diagnostics;
using System.Collections.Generic;

namespace UnitTests
{
    
    
    /// <summary>
    ///This is a test class for P4WorkspaceTest and is intended
    ///to contain all P4WorkspaceTest Unit Tests
    ///</summary>
    [TestClass()]
    public class P4WorkspaceTest
    {
        String TestDir = "c:\\MyTestDir";

        private TestContext testContextInstance;

        /// <summary>
        ///Gets or sets the test context which provides
        ///information about and functionality for the current test run.
        ///</summary>
        public TestContext TestContext
        {
            get
            {
                return testContextInstance;
            }
            set
            {
                testContextInstance = value;
            }
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
        ///A test for Delete
        ///</summary>
        [TestMethod()]
        public void DeleteTest()
        {
            bool unicode = false;

            string server = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for( int i = 0; i < 2; i++ ) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer( TestDir, unicode );
                try
                {
                    using (P4Server target = new P4Server(server, user, pass, ws_client))
                    {
                        P4Workspace actual = P4Workspace.Fetch(target, "admin_space");
                        Assert.IsNotNull(actual);

                        actual.Delete( true );
                    }
                }
                catch (Exception ex)
                {
                    Assert.Fail("Exception Thrown: {0} : {1}", ex.ToString(), ex.Message);
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        /// <summary>
        ///A test for Fetch
        ///</summary>
        [TestMethod()]
        public void FetchTest()
        {
            bool unicode = false;

            string server = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for( int i = 0; i < 2; i++ ) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer( TestDir, unicode );
                try
                {
                    using (P4Server target = new P4Server(server, user, pass, ws_client))
                    {
                        P4Workspace actual = P4Workspace.Fetch(target, "admin_space");
                        Assert.IsNotNull(actual);
                    }
                }
                catch (Exception ex)
                {
                    Assert.Fail("Exception Thrown: {0} : {1}", ex.ToString(), ex.Message);
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        /// <summary>
        ///A test for GetView
        ///</summary>
        [TestMethod()]
        public void GetViewTest()
        {
            bool unicode = false;

            string server = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, unicode);
                try
                {
                    using (P4Server target = new P4Server(server, user, pass, ws_client))
                    {
                        P4Workspace actual = P4Workspace.Fetch(target, "admin_space");
                        Assert.IsNotNull(actual);

                        StringList view = actual.GetView();

                        Assert.AreEqual(view[0], "//depot/... //admin_space/...");
                    }
                }
                catch (Exception ex)
                {
                    Assert.Fail("Exception Thrown: {0} : {1}", ex.ToString(), ex.Message);
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        /// <summary>
        ///A test for Parse
        ///</summary>
        [TestMethod()]
        public void ParseTest()
        {
            P4Server pserver = new P4Server(false);
            P4Workspace target = new P4Workspace(pserver, "temp");
            bool actual = target.Parse(spec);
            Assert.IsTrue(actual);
            Assert.AreEqual(target.Name, "XP1_usr");
        }

        /// <summary>
        ///A test for ToString
        ///</summary>
        [TestMethod()]
        public void ToStringTest()
        {
            P4Server pserver = new P4Server(false);
            P4Workspace target = new P4Workspace(pserver, "temp");
            bool result = target.Parse(spec);
            Assert.IsTrue(result);

            Assert.AreEqual(target.Name, "XP1_usr");

            String actual = target.ToString();

            Assert.IsTrue(actual.StartsWith("Client:\tXP1_usr"));
        }

        /// <summary>
        ///A test for Save
        ///</summary>
        [TestMethod()]
        public void SaveTest()
        {
            bool unicode = false;

            string server = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, unicode);
                try
                {
                    using (P4Server target = new P4Server(server, user, pass, ws_client))
                    {
                        P4Workspace actual = P4Workspace.Fetch(target, "admin_space");
                        Assert.IsNotNull(actual);

                        String newDescription = "This is an new description";
                        actual.Description = newDescription;
                         
                        actual.Save( false );

                        P4Workspace newActual = P4Workspace.Fetch(target, "admin_space");

                        Assert.AreEqual(newActual.Description.TrimEnd(' ','\r','\n'), newDescription);
                    }
                }
                catch (Exception ex)
                {
                    Assert.Fail("Exception Thrown: {0} : {1}", ex.ToString(), ex.Message);
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        string spec =
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

        // Brain dead property tests
        /// <summary>
        ///A test for AltRoots
        ///</summary>
        [TestMethod()]
        public void AltRootsTest()
        {
            P4Server pserver = null; 
            P4Workspace target = new P4Workspace(pserver, "Fred");
            StringList expected = new StringList();
            expected.Add("Pooh Bear");
            expected.Add("Smokey Bear");
            StringList actual;
            target.AltRoots = expected;
            actual = target.AltRoots;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for LastAccess
        ///</summary>
        //[TestMethod()]
        //[DeploymentItem("p4.net.dll")]
        //public void LastAccessTest()
        //{
        //    P4Server pserver = null;
        //    P4Workspace target = new P4Workspace(pserver, "Fred");
        //    DateTime expected = DateTime.Now;
        //    DateTime actual;
        //    target.LastAccess = expected;
        //    actual = target.LastAccess;
        //    Assert.AreEqual(expected, actual);
        //}

        /// <summary>
        ///A test for Description
        ///</summary>
        [TestMethod()]
        public void DescriptionTest()
        {
            P4Server pserver = null;
            P4Workspace target = new P4Workspace(pserver, "Fred");
            string expected = "Handy Manny";
            string actual;
            target.Description = expected;
            actual = target.Description;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Host
        ///</summary>
        [TestMethod()]
        public void HostTest()
        {
            P4Server pserver = null;
            P4Workspace target = new P4Workspace(pserver, "Fred");
            string expected = "Handy Manny";
            string actual;
            target.Host = expected;
            actual = target.Host;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for LastUpdate
        ///</summary>
        //[TestMethod()]
        //[DeploymentItem("p4.net.dll")]
        //public void LastUpdateTest()
        //{
        //    P4Server pserver = null;
        //    P4Workspace target = new P4Workspace(pserver, "Fred");
        //    DateTime expected = DateTime.Now;
        //    DateTime actual;
        //    target.LastUpdate = expected;
        //    actual = target.LastUpdate;
        //    Assert.AreEqual(expected, actual);
        //}

        /// <summary>
        ///A test for P4Workspace.LineEndOptions
        ///</summary>
        [TestMethod()]
        public void LineEndOptionsTest()
        {
            P4Server pserver = new P4Server(false);
            P4Workspace target = new P4Workspace(pserver, "Fred");

            // possible values: local, unix, uac, win, share
            String spec = "local";
            P4Workspace.LineEndFlags expected = new P4Workspace.LineEndFlags(spec); //
            P4Workspace.LineEndFlags actual;
            target.LineEndOptions = expected;
            actual = target.LineEndOptions;
            String formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);

            spec = "unix";
            expected = new P4Workspace.LineEndFlags(spec); //
            target.LineEndOptions = expected;
            actual = target.LineEndOptions;
            formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);

            spec = "mac";
            expected = new P4Workspace.LineEndFlags(spec); //
            target.LineEndOptions = expected;
            actual = target.LineEndOptions;
            formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);

            spec = "win";
            expected = new P4Workspace.LineEndFlags(spec); //
            target.LineEndOptions = expected;
            actual = target.LineEndOptions;
            formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);

            spec = "share";
            expected = new P4Workspace.LineEndFlags(spec); //
            target.LineEndOptions = expected;
            actual = target.LineEndOptions;
            formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);
        }

        /// <summary>
        ///A test for Name
        ///</summary>
        [TestMethod()]
        public void NameTest()
        {
            P4Server pserver = null;
            P4Workspace target = new P4Workspace(pserver, "Fred");
            string expected = "Handy Manny";
            string actual;
            target.Name = expected;
            actual = target.Name;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Options
        ///</summary>
        [TestMethod()]
        public void OptionsTest()
        {
            P4Server pserver = new P4Server(false);
            P4Workspace target = new P4Workspace(pserver, "Fred");
            String spec = "noallwrite noclobber nocompress unlocked nomodtime normdir";
            P4Workspace.ClientFlags expected =  new P4Workspace.ClientFlags(spec);
            P4Workspace.ClientFlags actual;
            target.Options = expected;
            actual = target.Options;
            String formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);

            spec = "allwrite clobber compress locked modtime rmdir";
            expected = new P4Workspace.ClientFlags(spec);
            target.Options = expected;
            actual = target.Options;
            formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);
        }

        /// <summary>
        ///A test for Owner
        ///</summary>
        [TestMethod()]
        public void OwnerTest()
        {
            P4Server pserver = null;
            P4Workspace target = new P4Workspace(pserver, "Fred");
            string expected = "Handy Manny";
            string actual;
            target.Owner = expected;
            actual = target.Owner;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Root
        ///</summary>
        [TestMethod()]
        public void RootTest()
        {
            P4Server pserver = null;
            P4Workspace target = new P4Workspace(pserver, "Fred");
            string expected = "Handy Manny";
            string actual;
            target.Root = expected;
            actual = target.Root;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for SubmitOptions
        ///</summary>
        [TestMethod()]
        public void SubmitOptionsTest()
        {
            P4Server pserver = new P4Server(false);
            P4Workspace target = new P4Workspace(pserver, "Fred");

            // possible values: "submitunchanged", "revertunchanged", "leaveunchanged", "submitunchanged+reopen", "revertunchanged+reopen", "leaveunchanged+reopen";

            String spec = "submitunchanged";
            P4Workspace.SubmitFlags expected = new P4Workspace.SubmitFlags(spec);
            P4Workspace.SubmitFlags actual;
            target.SubmitOptions = expected;
            actual = target.SubmitOptions;
            String formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);

            spec = "revertunchanged";
            expected = new P4Workspace.SubmitFlags(spec);
            target.SubmitOptions = expected;
            actual = target.SubmitOptions;
            formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);

            spec = "leaveunchanged";
            expected = new P4Workspace.SubmitFlags(spec);
            target.SubmitOptions = expected;
            actual = target.SubmitOptions;
            formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);

            spec = "submitunchanged+reopen";
            expected = new P4Workspace.SubmitFlags(spec);
            target.SubmitOptions = expected;
            actual = target.SubmitOptions;
            formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);

            spec = "revertunchanged+reopen";
            expected = new P4Workspace.SubmitFlags(spec);
            target.SubmitOptions = expected;
            actual = target.SubmitOptions;
            formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);

            spec = "leaveunchanged+reopen";
            expected = new P4Workspace.SubmitFlags(spec);
            target.SubmitOptions = expected;
            actual = target.SubmitOptions;
            formatted = actual.ToString();
            Assert.AreEqual(spec, formatted);
        }

        /// <summary>
        ///A test for View
        ///</summary>
        [TestMethod()]
        public void ViewTest()
        {
            P4Server pserver = new P4Server(false);
            P4Workspace target = new P4Workspace(pserver, "Fred");
            WorkspaceView expected = new WorkspaceView(pserver, new String[] { "//depot/a //workspace/a" });
            WorkspaceView actual;
            target.View = expected;
            actual = target.View;
            Assert.AreEqual(expected.Count, actual.Count);
            Assert.AreEqual(expected.GetLeft(0), actual.GetLeft(0));
            Assert.AreEqual(expected.GetRight(0), actual.GetRight(0));
            Assert.AreEqual(expected.GetType(0), actual.GetType(0));
        }
    }
}
