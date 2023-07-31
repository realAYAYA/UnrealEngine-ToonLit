using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Diagnostics;

namespace UnitTests
{
    /// <summary>
    ///This is a test class for P4JobListTest and is intended
    ///to contain all P4JobListTest Unit Tests
    ///</summary>
    [TestClass()]
    public class P4JobListTest
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
        ///A test for CreateUsersList
        ///</summary>
        [TestMethod()]
        public void CreateJobListTest()
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
                        //P4JobList expected = null; 
                        P4JobList actual;
                        actual = new P4JobList(target);
                        Assert.AreEqual(1, actual.Count);
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
        ///A test for CreateJobList
        ///</summary>
        [TestMethod()]
        public void CreateJobListTest1()
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
                        string jobView = "user=admin";
                        bool includeIntegratedFixes = true;
                        bool longOutput = true;
                        bool reverseOrder = true;
                        int maxItems = 1;
                        string[] files = new String[] { "//depot/..." };
                        P4JobList actual;
                        actual = new P4JobList(target, jobView, includeIntegratedFixes, longOutput, reverseOrder, maxItems, files);

                        Assert.AreEqual(1, actual.Count);
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
