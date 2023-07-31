using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Diagnostics;

namespace UnitTests
{
    /// <summary>
    ///This is a test class for P4UserTest and is intended
    ///to contain all P4UserTest Unit Tests
    ///</summary>
    [TestClass()]
    public class P4UserTest
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
                        P4User actual = P4User.Fetch(target, "admin");
                        Assert.IsNotNull(actual);

                        Assert.AreEqual("admin",actual.User);
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
                        P4User actual = P4User.Fetch(target, "admin");
                        Assert.IsNotNull(actual);

                        actual.Delete(true);
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
        public void FetchTest1()
        {
            bool unicode = false;

            string server = "localhost:6666";
            string pass = string.Empty;
            string ws_client = "admin_space";

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer(TestDir, unicode);
                try
                {
                    String user = "Alex";
                    if (unicode)
                        user = "Алексей";
                    using (P4Server target = new P4Server(server, "admin", pass, ws_client))
                    {
                        P4User actual = new P4User(target, user);
                        actual.Fetch();
                        Assert.IsNotNull(actual);
                        Assert.AreEqual(actual.User, user);
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
        ///A test for FromTaggedOutput
        ///</summary>
        ///
        // Tested by the Fetch tests
        //[TestMethod()]
        //public void FromTaggedOutputTest()
        //{
        //}

        /// <summary>
        ///A test for Parse
        ///</summary>
        [TestMethod()]
        public void ParseTest()
        {
            P4Server pserver = new P4Server(false);
            P4User target = new P4User(pserver, "fred"); // TODO: Initialize to an appropriate value
            target.Parse(spec);
            Assert.AreEqual(target.User, "fred");
        }
        private String spec =
@"# A Perforce User Specification.
#
#  User:        The user's user name.
#  Type:        Either 'service' or 'standard'. Default is 'standard'.
#  Email:       The user's email address; for email review.
#  Update:      The date this specification was last modified.
#  Access:      The date this user was last active.  Read only.
#  FullName:    The user's real name.
#  JobView:     Selects jobs for inclusion during changelist creation.
#  Password:    If set, user must have matching $P4PASSWD on client.
#  PasswordChange:
#               The date this password was last changed.  Read only.
#  Reviews:     Listing of depot files to be reviewed by user.

User:	fred

Email:	fred@unittests.com

Update:	2010/07/30 16:17:35

Access:	2010/12/01 15:18:55

FullName:	Fred Farnsworth

JobView: type=bug & ^status=closed

Reviews:
    //depot/poetry\r\n
    //depot/movies\r\n

Password:	******";

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
                        P4User actual = P4User.Fetch(target, "admin");
                        Assert.IsNotNull(actual);

                        actual.FullName = "Steady Freddy";

                        actual.Save( false );

                        actual = P4User.Fetch(target, "admin");
                        Assert.IsNotNull(actual);

                        Assert.AreEqual(actual.FullName, "Steady Freddy");
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
        
        // ToString is invoke during the Save Test, so no separate test is needed.
        
        //[TestMethod()]
        //public void ToStringTest()
        //{
        //}

        /// <summary>
        ///A test for Access
        ///</summary>
        //[TestMethod()]
        //public void AccessTest()
        //{
        //    P4Server pserver = null;
        //    P4User target = new P4User(pserver, "Fred");
        //    DateTime expected = DateTime.Now;
        //    DateTime actual;
        //    target.LastAccess = expected;
        //    actual = target.LastAccess;
        //    Assert.AreEqual(expected, actual);
        //}

        /// <summary>
        ///A test for Email
        ///</summary>
        [TestMethod()]
        public void EmailTest()
        {
            P4Server pserver = null;
            P4User target = new P4User(pserver, "Fred");
            string expected = "HandyManny@sheetrockhills.dc";
            string actual;
            target.Email = expected;
            actual = target.Email;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for FullName
        ///</summary>
        [TestMethod()]
        public void FullNameTest()
        {
            P4Server pserver = null;
            P4User target = new P4User(pserver, "Fred");
            string expected = "Handy Manny";
            string actual;
            target.FullName = expected;
            actual = target.FullName;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for JobView
        ///</summary>
        [TestMethod()]
        public void JobViewTest()
        {
            P4Server pserver = null;
            P4User target = new P4User(pserver, "Fred");
            string expected = "type=bug & ^status=closed";
            string actual;
            target.JobView = expected;
            actual = target.JobView;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Password
        ///</summary>
        [TestMethod()]
        public void PasswordTest()
        {
            P4Server pserver = null;
            P4User target = new P4User(pserver, "Fred");
            string expected = "Password";
            string actual;
            target.Password = expected;
            actual = target.Password;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Reviews
        ///</summary>
        [TestMethod()]
        public void ReviewsTest()
        {
            P4Server pserver = null;
            P4User target = new P4User(pserver, "Fred");
            StringList expected = new StringList();
            expected.Add("//depot/poetry");
            expected.Add("//depot/movies");
            StringList actual;
            target.Reviews = expected;
            actual = target.Reviews;
            Assert.AreEqual(expected, actual);
        }

        /// <summary>
        ///A test for Update
        ///</summary>
        //[TestMethod()]
        //public void UpdateTest()
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
        ///A test for User
        ///</summary>
        [TestMethod()]
        public void UserTest()
        {
            P4Server pserver = null;
            string expected = "Charlie";
            P4User target = new P4User(pserver, expected);
            string actual;
            // target.User = expected; set in constructor
            actual = target.User;
            Assert.AreEqual(expected, actual);
        }
    }
}
