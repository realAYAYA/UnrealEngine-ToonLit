# Low level tests with Catch2

**Quick links**
* If you want to write tests, see [Create a test module](#CreateTestModule) then [Add tests](#AddTests)
* If you want to build and run existing tests from Visual Studio, check [Build and run tests](#BuildAndRunTests)

## Motivation

**The goal of low level testing is to avoid loading the whole engine or editor, the need for cooking and the need for adding extra dependencies such as rendering. It provides a lightweight framework for writing tests that are fast, deterministic and designed to test very specific modules and nothing more.**

Traditionally we write unit, integration and functional tests that can live next to testable code or as a separate module.
Developing tests in the classic pyramid style of unit, integration and functional is ideal but it can be time consuming. We shouldn't be limited to a specific type of test and we can use different methodologies such as TDD or BDD for unit, functional or integration tests.

Catch2 is a modern C++ test framework designed with multi-platform support and it can run individual tests or a filtered subset of them using tags.

## <a name="CreateTestModule"/>Create a test module
Create a module that inherits from `TestModuleRules`.
If building a test module that doesn't use Catch2, call the base constructor `: base(Target, false)`.

Call UpdateBuildGraphPropertiesFile if intended to generate BuildGraph property metadata.

**MyModuleTests.Build.cs**
```csharp
using UnrealBuildTool;

public class MyModuleTests : TestModuleRules
{
	public MyModuleTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			// Any private include paths from test target module MyModule
		);

		PrivateDependencyModuleNames.AddRange(
			// Any private dependencies, including MyModule, to link against
		);
		
		// Other types of dependencies minimally required to test a specific module

		UpdateBuildGraphPropertiesFile(new Metadata("MyModule", "My Module"));
	}
}

```
---
Next add a target file with a class that inherits from `TestTargetRules` and override default compilation flags if necessary - aim for a minimal testable module free of default features that don't add value to low level testing.
You can specify the supported platforms individually, if not they default to Win64, Mac, Linux and Android.

Here is where you can enable project specific global definitions as well as `Catch2` macros such as benchmarking or compilation flags etc.

**MyModuleTests.Target.cs**
```csharp
using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class MyModuleTestsTarget : TestTargetRules
{
	public MyModuleTestsTarget(TargetInfo Target) : base(Target)
	{
		bCompileAgainstCoreUObject = true;
		bCompileAgainstApplicationCore = true;

		// Optionally add global definitions for Catch2 benchmarking etc.
		GlobalDefinitions.Add("CATCH_CONFIG_ENABLE_BENCHMARKING=1");
    }
}
```
---

**Make sure to checkout Engine\Build\LowLevelTests_GenProps.xml before moving on - this is where the BuildGraph test related metadata will be generated.**

Generate the solution w/ GenerateProjectFiles.bat - the test project will be placed in the Visual Studio solution folder `Programs/LowLevelTests`.

You can now build the generated project and the executable will be placed in the appropriate `Binaries` folder.

> **_NOTE:_** When compiling the test project the binaries will be produced into a sub-folder with the same name as the test module - this is intentional to keep all test and platform specific files grouped into one place ready to be deployed on a device. This applies to desktop platforms as well.

**Buildgraph**

The buildgraph file *Engine\Build\LowLevelTests.xml* has an option to run the new test module we just added.

This option can be used to selectively include or exclude the test when running via BuildGraph.

```xml
<Option Name="RunMyModuleTests" DefaultValue="false" Description="Run My Module Tests"/>
```

```shell 
.\RunUAT.bat BuildGraph -Script="Engine/Build/LowLevelTests.xml" -set:RunMyModuleTests=true
```


## <a name="AddTests"/>Add tests

### Overview of types of tests

**Unit tests**
- Unit tests test one unit of code, typically one method in a class.
- They rely heavily on mocking inputs and external dependencies such as servers or databases.
- Typically we write one unit test suite per tested class and the goal is to cover the public interface of a class as well as different code paths within a method.
- Most tests we write are not unit tests because they are very restrictive in the way they should be written and the target code might not be testable this way.
- Unit tests shouldn't require special global setup and teardown and they should be able to be run independently between test suites.
- Unit tests should not have order dependency on other unit tests.
- Unit tests are very fast - they take seconds or less to run.
 
**Integration tests**
- Integration tests test multiple units of code together, usually two or more classes or methods.
- They can require global setup such as loading modules or an external resource.
- They are less restrictive than unit tests and are more common but it can be harder to cover branches in code (if conditions etc.).
- Integration tests are usually slower than unit tests - they take seconds to a couple of minutes to run.

**Functional tests**
- Functional tests will test a specific functionality, this can be a specific feature or a use case.
- They often require global setup and teardown to manage external resources.
- These are the most common types of tests and they can vary a lot in complexity.
- Functional tests can take anywhere from seconds to minutes and rarely hours to run.
They are usually slower than integration tests.

### Conventions

**Files and folders**
* For best results, name the test files using a `XTest.cpp` or `XTests.cpp` convention where `X.cpp` is the testable file.
* Reflect the tested module's folder structure.

**Naming**

Avoid using the term `unit tests` if they aren't actual unit tests - this definition is restrictive and miss-naming can cause confusion.
A unit test class should run in a couple of seconds.

**Best practices**

* Provide meaningful tags to test cases and scenarios. These can be used to our advantage, for example we could choose to parallelize the run of categories of tests.
* Make sure each `SECTION` or `THEN` block includes at least one `REQUIRE` or a `CHECK` - tests that don't have expectations are useless.
* `REQUIRE` immediately stops on failure but `CHECK` doesn't, use them wisely.
* Design tests so that they are deterministic and they fit a certain type: unit, integration, functional, stress test etc. 
* Make sure test code is multi-platform supported when the tested module requires it. For example, when working with the platform file system, use the `FPlatformFileManager` class, don't assume the test will run exclusively on a desktop platform.

### Catch2 test sample

A basic Catch2 test can be written in both BDD or classic style.

Test code must be written to be compilable and executable on each of the target tested module's supported platforms.

Check out complete Catch2 documentation here https://github.com/catchorg/Catch2

```cpp

#include "CoreMinimal.h"
#include "TestHarness.h"

// Other includes must be placed after CoreMinimal.h and TestHarness.h

SCENARIO("Summary of test scenario", "[functional][feature][slow]")
{
	GIVEN("Setup phase")
	{
		// Initialize variables, setup test preconditions etc
		[...]

		WHEN("I perform an action")
		{
			// Change internal state
			[...]
			THEN("Check for expectations")
			{
				REQUIRE(Condition_1);
				REQUIRE(Condition_2);
				CHECK(Condition_3);
			}
		}
	}
}

TEST_CASE("Summary of test case", "[unit][feature][fast]")
{
	// Setup code for this test case
	[...]

	SECTION("Test #1")
	{
		REQUIRE(Condition_1);
	}
	...
	SECTION("Test #n")
	{
		REQUIRE(Condition_n);
	}

	// Teardown code for this test case
	[...]
}
```


## <a name="BuildAndRunTests"/>Build and run tests

### From Visual Studio

Tests can be built and run directly from Visual Studio on desktop platforms:
1. Download the [Test Adapter for Catch2](https://marketplace.visualstudio.com/items?itemName=JohnnyHendriks.ext01) and install it.
2. Build the test projects from Visual Studio to produce the executables. The test adapter will discover tests in the executables and they must contain the word **Test** e.g. *MyModuleTest.exe* or *MyModuleTests.exe*.
3. The tests will be displayed in the test explorer: select *Test -> Test Explorer* from the menu: from here you can run tests and navigate to their source code.
Running the tests will generate a *Test Detail Summary* and a Catch test report XML file in the same folder as the executable.
4. If there are no tests in Test Explorer it's likely because the `Build` at step 3. didn't update the executable. Run a `Rebuild` on the test project and you should be good to go.

> **If there are still no tests shown in the *Test Explorer*, perform a Rebuild on the test project and in the Output tab in VS select `Show output from: Tests` and look for `Catch2Adapter` in the log.
> You should see a `Started Catch2Adapter test discovery...` line and a `Finished Catch2Adapter test discovery.`: between these lines you will see any problems reported by the Catch2 test adapter.**

## From command line

Go to the binaries folder and run the executable with the extra flag `-- --wait` to wait for user input before exiting. By default the test app exits as soon as it's done, this is intentional to prevent easy hangs on automated systems and tools such as the `Test Adapter for Catch2` which runs the executable to list all the tests.

## With Gauntlet on non-desktop platforms

1. Reserve a shared device from https://horde.devtools.epicgames.com/devices by going into the Shared tab and Checking Out a device.
2. Copy the device’s IP address.
3. Run the BuildGraph command making sure to change the platform accordingly and set *-device* to the copied IP.
In this example *-set:RunAudioTests=true* will run the audio tests while *-set:TestPlatformSwitch=true* means we are running on a Switch device..
```shell 
.\RunUAT.bat BuildGraph -Script="Engine/Build/LowLevelTests.xml" -set:RunAudioTests=true -set:TestPlatformSwitch=true -set:TestArgs="-deviceurl=https://horde.devtools.epicgames.com -device=<RESERVED SHARED DEVICE IP>" -Target="Run Low Level Tests"
```

This will compile the test module, stage it and run the test app on the specified device.
nd run the test app on the specified device.