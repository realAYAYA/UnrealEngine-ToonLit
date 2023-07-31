using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Diagnostics;

namespace UnitTests
{
    
    
    /// <summary>
    ///This is a test class for P4SpecificationTest and is intended
    ///to contain all P4SpecificationTest Unit Tests
    ///</summary>
    [TestClass()]
    public class P4SpecificationTest
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

            for( int i = 0; i < 2; i++ ) // run once for ascii, once for unicode
            {
                Process p4d = Utilities.DeployP4TestServer( TestDir, unicode );
                try
                {
                    using (P4Server target = new P4Server(server, user, pass, ws_client))
                    {
                        P4Specification actual = P4Specification.Fetch("client", target, "admin_space");
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
        ///A test for Parse
        ///</summary>
        [TestMethod()]
        public void ParseTest()
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
                        P4Specification client1 = P4Specification.Fetch("client", target, "admin_space");
                        Assert.IsNotNull(client1);

                        String spec = client1.ToString();

                        P4Specification actual = new P4Specification();
                        actual.Parse(spec);

                        Assert.AreEqual(client1["Owner"], actual["Owner"]);
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
        ///A test for Save
        ///</summary>
        [TestMethod()]
        public void FetchTest1()
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
                        P4Specification actual = new P4Specification(target, "admin", "user");
                        actual.Fetch();
                        Assert.IsNotNull(actual);
                        Assert.AreEqual("admin", actual["User"]);
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
                        P4Specification actual = new P4Specification(target, "admin_space2", "client");
                        actual.Fetch();
                        Assert.IsNotNull(actual);
                        Assert.AreEqual("admin_space2", actual["Client"]);

                        actual.Name = "admin_space3";
                        actual["Client"] = actual.Name;

                        actual.Save();

                        Assert.AreEqual("Client admin_space3 saved.", actual.LastCommandResults.InfoOutput[0].Info);
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
                        P4Specification actual = new P4Specification(target, "admin_space2", "client");
                        actual.Fetch();
                        Assert.IsNotNull(actual);
                        Assert.AreEqual("admin_space2", actual["Client"]);

                        actual.Delete();

                        Assert.AreEqual("Client admin_space2 deleted.", actual.LastCommandResults.InfoOutput[0].Info);
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
    }
}
