using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Diagnostics;

namespace UnitTests
{
   
    /// <summary>
    ///This is a test class for P4WorkspaceListTest and is intended
    ///to contain all P4WorkspaceListTest Unit Tests
    ///</summary>
    [TestClass()]
    public class P4WorkspaceListTest
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
        ///A test for CreateClientList
        ///</summary>
        [TestMethod()]
        public void CreateClientListTest()
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
                        P4WorkspaceList actual = new P4WorkspaceList(target, null, null, -1);
                        Assert.IsNotNull(actual);

                        if (unicode)
                            Assert.AreEqual(3, actual.Count);
                        else
                            Assert.AreEqual(2, actual.Count);

                        Assert.AreEqual( "admin", actual[0].Owner );

                        actual = new P4WorkspaceList(target, null, null, 1);
                        Assert.IsNotNull(actual);

                        Assert.AreEqual( 1, actual.Count );

                        actual = new P4WorkspaceList(target, "admin", null, -1);
                        Assert.IsNotNull(actual);

                        Assert.AreEqual(2, actual.Count);

                        Assert.AreEqual("admin", actual[0].Owner );

                        actual = new P4WorkspaceList(target, "admin", "*2", -1);
                        Assert.IsNotNull(actual);

                        Assert.AreEqual(1, actual.Count);

                        Assert.AreEqual("admin", actual[0].Owner );
                    }
                }
                catch (P4Exception ex)
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
