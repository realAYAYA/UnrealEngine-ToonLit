using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Diagnostics;

namespace UnitTests
{
    
    
    /// <summary>
    ///This is a test class for P4JobTest and is intended
    ///to contain all P4JobTest Unit Tests
    ///</summary>
    [TestClass()]
    public class P4JobTest
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
                        //P4JobList expected = null; 
                        P4Job actual;
                        actual = new P4Job(target, "job000001");
                        Assert.IsTrue(actual.Delete(false));
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
                        //P4JobList expected = null; 
                        P4Job actual;
                        actual = new P4Job(target, "job000001");
                        actual.Fetch();
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
        ///A test for Fetch
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
                        //P4JobList expected = null; 
                        P4Job actual;
                        actual = P4Job.Fetch(target, "job000001");
                        actual.Fetch();
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
        ///A test for Parse
        ///</summary>
        [TestMethod()]
        public void ParseTest()
        {
            P4Server pserver = new P4Server(false);
            P4Job target = new P4Job(pserver, "new");
            bool expected = true; 
            bool actual;
            actual = target.Parse(spec);
            Assert.AreEqual(expected, actual);
            Assert.AreEqual("job042236", target.JobId);
            Assert.AreEqual("ksawicki", target["OwnedBy"]);
            // currently, the base parser doesn't save tags without values
            //Assert.IsTrue(String.IsNullOrEmpty(target["P4Blog"])); 
        }

        private static String spec = 
@"Job:	job042236

Status:	closed

Type:	SIR

Severity:	C

Subsystem:	p4ws

Dependency:

OwnedBy:	ksawicki

ReportedBy:	ksawicki

ModifiedBy:	ksawicki

ReportedDate:	2010/12/03 09:40:10

ModifiedDate:	2010/12/03 09:42:19

CommitRelease:	2011.1

CallNumbers:

Customers:

Description:
	Add view resolver provider extension point for contributing view registered for a given
	Content-Type and format query parameter.
	
	http://computer.perforce.com/newwiki/index.php?title=Web_Services_Work_Group/Spec/Extension_Points#View_Resolver_Provider

Categories:

P4Blog:

UIDetails:

JIRAWorkLog:
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
                        //P4JobList expected = null; 
                        P4Job actual;
                        actual = new P4Job(target, "job000001");
                        actual.Fetch();
                        actual.JobId = "new";
                        actual.Save(false);
                        Assert.AreEqual("admin", actual["User"]);
                        Assert.AreNotEqual("job000001", actual.JobId);
                        Assert.AreNotEqual("new", actual.JobId);
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
        //tested by SaveTest()
        //[TestMethod()]
        //public void ToStringTest()
        //{
        //}

        /// <summary>
        ///A test for JobId
        ///</summary>
        [TestMethod()]
        public void JobIdTest()
        {
            P4Server pserver = new P4Server(false);
            string expected = "Job1";
            P4Job target = new P4Job(pserver, expected); 
            string actual;
            actual = target.JobId;
            Assert.AreEqual(expected, actual);

            expected = "Job2";
            target.JobId = expected;
            actual = target.JobId;
            Assert.AreEqual(expected, actual);
        }
    }
}
