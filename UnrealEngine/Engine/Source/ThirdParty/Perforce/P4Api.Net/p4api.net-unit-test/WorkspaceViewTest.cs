using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Collections.Generic;

namespace UnitTests
{
    
    
    /// <summary>
    ///This is a test class for WorkspaceViewTest and is intended
    ///to contain all WorkspaceViewTest Unit Tests
    ///</summary>
    [TestClass()]
    public class WorkspaceViewTest
    {


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
        ///A test for WorkspaceView Constructor
        ///</summary>
        [TestMethod()]
        public void WorkspaceViewConstructorTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                P4Server pserver = new P4Server(unicode);
                StringList text = new StringList();
                text.Add("//depot/... //workspace/depot/...");
                text.Add("//usr/... //workspace/usr/...");

                WorkspaceView target = new WorkspaceView(pserver, text);

                Assert.AreEqual(target.Count, 2);
            }
        }

        /// <summary>
        ///A test for SplitViewLine
        ///</summary>
        [TestMethod()]
        public void SplitViewLineTest()
        {
            P4Server pserver = new P4Server(false);
            string line = "//depot/... //workspace/depot/...";
            string[] expected = new string[] { "//depot/...", "//workspace/depot/..." };
            string[] actual;
            actual = WorkspaceView.SplitViewLine(line);
            Assert.AreEqual(expected.Length, actual.Length);
            Assert.AreEqual(expected[0], actual[0]);
            Assert.AreEqual(expected[1], actual[1]);

            line = "\"//depot/spaced out/...\" //workspace/depot/nospace/...";
            expected = new string[] { "//depot/spaced out/...", "//workspace/depot/nospace/..." };
            actual = WorkspaceView.SplitViewLine(line);
            Assert.AreEqual(expected.Length, actual.Length);
            Assert.AreEqual(expected[0], actual[0]);
            Assert.AreEqual(expected[1], actual[1]);

            line = "//workspace/depot/nospace/... \"//depot/spaced out/...\"";
            expected = new string[] { "//workspace/depot/nospace/...", "//depot/spaced out/..." };
            actual = WorkspaceView.SplitViewLine(line);
            Assert.AreEqual(expected.Length, actual.Length);
            Assert.AreEqual(expected[0], actual[0]);
            Assert.AreEqual(expected[1], actual[1]);

            line = "\"//workspace/depot/spaced out/...\" \"//depot/spaced out/...\"";
            expected = new string[] { "//workspace/depot/spaced out/...", "//depot/spaced out/..." };
            actual = WorkspaceView.SplitViewLine(line);
            Assert.AreEqual(expected.Length, actual.Length);
            Assert.AreEqual(expected[0], actual[0]);
            Assert.AreEqual(expected[1], actual[1]);

            line = "-//depot/... //workspace/depot/...";
            expected = new string[] { "//depot/...", "//workspace/depot/..." };
            actual = WorkspaceView.SplitViewLine(line);
            Assert.AreEqual(expected.Length, actual.Length);
            Assert.AreEqual(expected[0], actual[0]);
            Assert.AreEqual(expected[1], actual[1]);

            line = "\"-//depot/spaced out/...\" //workspace/depot/nospace/...";
            expected = new string[] { "//depot/spaced out/...", "//workspace/depot/nospace/..." };
            actual = WorkspaceView.SplitViewLine(line);
            Assert.AreEqual(expected.Length, actual.Length);
            Assert.AreEqual(expected[0], actual[0]);
            Assert.AreEqual(expected[1], actual[1]);

            line = "-//workspace/depot/nospace/... \"//depot/spaced out/...\"";
            expected = new string[] { "//workspace/depot/nospace/...", "//depot/spaced out/..." };
            actual = WorkspaceView.SplitViewLine(line);
            Assert.AreEqual(expected.Length, actual.Length);
            Assert.AreEqual(expected[0], actual[0]);
            Assert.AreEqual(expected[1], actual[1]);

            line = "\"-//workspace/depot/spaced out/...\" \"//depot/spaced out/...\"";
            expected = new string[] { "//workspace/depot/spaced out/...", "//depot/spaced out/..." };
            actual = WorkspaceView.SplitViewLine(line);
            Assert.AreEqual(expected.Length, actual.Length);
            Assert.AreEqual(expected[0], actual[0]);
            Assert.AreEqual(expected[1], actual[1]);
        }

        /// <summary>
        ///A test for ToString
        ///</summary>
        [TestMethod()]
        public void ToStringTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                P4Server pserver = new P4Server(unicode);
                string target = "//depot/... //workspace/depot/...\r\n//usr/... //workspace/usr/...";
                StringList text = new StringList();
                text.Add("//depot/... //workspace/depot/...");
                text.Add("//usr/... //workspace/usr/...");

                WorkspaceView testView = new WorkspaceView(pserver, text);

                string actual = testView.ToString();

                Assert.AreEqual(target.Replace("\r\n",""), actual.Replace("\r\n",""));
            }
        }

        /// <summary>
        ///A test for FromDepotDirectory
        ///</summary>
        [TestMethod()]
        public void FromDepotDirectoryTest()
        {
            bool unicode=  false;
            for (int i = 0; i < 2; i++)
            {
                P4Server pserver = new P4Server(unicode);
                string target = "//workspace/depot/code";
                StringList text = new StringList();
                text.Add("//depot/... //workspace/depot/...");
                text.Add("//usr/... //workspace/usr/...");

                WorkspaceView testView = new WorkspaceView(pserver, text);

                string actual = testView.FromDepotDirectory("//depot/code");

                Assert.AreEqual(target, actual);
            }
        }

        /// <summary>
        ///A test for ToDepotDirectory
        ///</summary>
        [TestMethod()]
        public void ToDepotDirectoryTest()
        {
            bool unicode = false;
            for (int i = 0; i < 2; i++)
            {
                P4Server pserver = new P4Server(unicode);
                string target = "//depot/code";
                StringList text = new StringList();
                text.Add("//depot/... //workspace/depot/...");
                text.Add("//usr/... //workspace/usr/...");

                WorkspaceView testView = new WorkspaceView(pserver, text);

                string actual = testView.ToDepotDirectory("//workspace/depot/code");

                Assert.AreEqual(target, actual);
            }
        }
    }
}
