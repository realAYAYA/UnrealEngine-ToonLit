using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Diagnostics;

namespace UnitTests
{
    /// <summary>
    ///This is a test class for P4ChangeTest and is intended
    ///to contain all P4ChangeTest Unit Tests
    ///</summary>
    [TestClass()]
    public class P4ChangeTest
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

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, unicode);
                try
                {
                    using (P4Server target = new P4Server(server, user, pass, ws_client))
                    {
                        P4Change actual = P4Change.Fetch(target, 6);
                        Assert.IsNotNull(actual);

                        Assert.IsTrue(actual.Delete(false, true));
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

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, unicode);
                try
                {
                    using (P4Server target = new P4Server(server, user, pass, ws_client))
                    {
                        P4Change actual = P4Change.Fetch(target, 1);
                        Assert.IsNotNull(actual);

                        Assert.AreEqual("admin", actual.User);
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
            P4Change target = new P4Change(pserver, -1);
            string spec = ChangeSpec;
            bool expected = true; // TODO: Initialize to an appropriate value
            bool actual;
            actual = target.Parse(spec);
            Assert.AreEqual(expected, actual);
            Assert.AreEqual(168750, target.ChangeNumber);
        }

        private static String ChangeSpec =
@"# A Perforce Change Specification.
#
#  Change:      The change number. 'new' on a new changelist.
#  Date:        The date this specification was last modified.
#  Client:      The client on which the changelist was created.  Read-only.
#  User:        The user who created the changelist.
#  Status:      Either 'pending' or 'submitted'. Read-only.
#  Type:        Either 'public' or 'restricted'. Default is 'public'.
#  Description: Comments about the changelist.  Required.
#  Jobs:        What opened jobs are to be closed by this changelist.
#               You may delete jobs from this list.  (New changelists only.)
#  Files:       What opened files from the default changelist are to be added
#               to this changelist.  You may delete files from this list.
#               (New changelists only.)

Change:	168750

Date:	2008/10/15 16:42:12

Client:	ksawicki_perforce_1666

User:	ksawicki

Status:	submitted

Description:
    Refactor to centralized open action that handles showing a dialog for which changelist things should be opened into.
";
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
                        P4Change newChange = new P4Change(target, "admin", "admin_space");
                        newChange.Restricted = false;
                        newChange.Description = "New change list for Unit tests";

                        Assert.IsTrue(newChange.Save());

                        Assert.AreNotEqual(-1, newChange.ChangeNumber);

                        P4Change actual = P4Change.Fetch(target, newChange.ChangeNumber);
                        Assert.IsNotNull(actual);

                        Assert.AreEqual("admin", actual.User);
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
        ///A test for ToString
        ///</summary>
        // ToString is validated in SaveTest()
        //[TestMethod()]
        //public void ToStringTest()
        //{
        //}

        /// <summary>
        ///A test for User
        ///</summary>
        [TestMethod()]
        public void UserTest()
        {
            P4Server pserver = new P4Server(false);
            P4Change target = new P4Change(pserver, 42);
            string expected = "Charlie";
            string actual;
            target.User = expected;
            actual = target.User;
            Assert.AreEqual(expected, actual);

            expected = "Charlie";
            target = new P4Change(pserver, expected, "Ws");
            actual = target.User;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Restricted
        ///</summary>
        [TestMethod()]
        public void RestrictedTest()
        {
            P4Server pserver = new P4Server(false);
            P4Change target = new P4Change(pserver, 42);
            bool expected = false;
            target.Restricted = expected;
            bool actual = target.Restricted;
            Assert.AreEqual(expected, actual);

            expected = true;
            target.Restricted = expected;
            actual = target.Restricted;
            Assert.AreEqual(expected, actual); // can only test default value of false
        }

        /// <summary>
        ///A test for Pending
        ///</summary>
        [TestMethod()]
        public void PendingTest()
        {
            P4Server pserver = new P4Server(false);
            P4Change target = new P4Change(pserver, "fred", "FredClient");
            bool actual;
            actual = target.Pending;
            Assert.AreEqual(true, actual); // can only test default value of false
        }

        /// <summary>
        ///A test for Modified
        ///</summary>
        //[TestMethod()]
        //public void ModifiedTest()
        //{
        //    P4Server pserver = new P4Server(false);
        //    P4Change target = new P4Change(pserver, "fred", "FredClient");
        //    DateTime expected = DateTime.Now;
        //    DateTime actual;
        //    target.Modified = expected;
        //    actual = target.Modified;
        //    Assert.AreEqual(expected, actual);
        //}

        /// <summary>
        ///A test for Jobs
        ///</summary>
        [TestMethod()]
        public void JobsTest()
        {
            P4Server pserver = new P4Server(false);
            P4Change target = new P4Change(pserver, "fred", "FredClient");

            StringList expected = new String[] { "12", "42" };
            StringList actual;
            target.Jobs = expected;
            actual = target.Jobs;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Files
        ///</summary>
        [TestMethod()]
        public void FilesTest()
        {
            P4Server pserver = new P4Server(false);
            P4Change target = new P4Change(pserver, "fred", "FredClient");

            StringList expected = new String[] { "//depot/code/stuff/i.c", "//depot/code/stuff/j.c" };
            StringList actual;
            target.Files = expected;
            actual = target.Files;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Description
        ///</summary>
        [TestMethod()]
        public void DescriptionTest()
        {
            P4Server pserver = new P4Server(false);
            P4Change target = new P4Change(pserver, "fred", "FredClient");
            string expected = "This is a test";
            string actual;
            target.Description = expected;
            actual = target.Description;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Client
        ///</summary>
        [TestMethod()]
        public void ClientTest()
        {
            String expected = "FredClient";
            P4Server pserver = new P4Server(false);
            P4Change target = new P4Change(pserver, "fred", expected);
            string actual;
            actual = target.Client;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for ChangeNumber
        ///</summary>
        [TestMethod()]
        public void ChangeNumberTest()
        {
            P4Server pserver = new P4Server(false);
            P4Change target = new P4Change(pserver, "fred", "FredClient");
            long actual;
            actual = target.ChangeNumber;
            Assert.AreEqual(-1, actual);

            target = new P4Change(pserver, 42);
            actual = target.ChangeNumber;
            Assert.AreEqual(42, actual);
        }
    }
}
