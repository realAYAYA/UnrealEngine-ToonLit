# Introduction 
Extension of the Unreal Engine FAutomationTestBase to provide test fixtures and common automation testing commands.

# Why CQTest?
There are other valid ways of testing in Unreal engine.  One option is to use the provided macros from Unreal Engine: [docs](https://docs.unrealengine.com/5.1/en-US/automation-technical-guide/)
```cpp
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMinimalTest, "Game.Test", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FMinmalTest::RunTest(const FString& Parameters) 
	{
		TestTrue(TEXT("True should be true"), true);
		return true;
	}
```

Unreal has also developed their [spec test framework](https://docs.unrealengine.com/4.27/en-US/TestingAndOptimization/Automation/AutomationSpec/), which is inspired by Behavior Driven Design
```cpp
    DEFINE_SPEC(FMinimalTest, "Game.Test", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
	void FMinimalTest::Define() 
	{
		Describe("Assertions", [this]() 
		{
			It("Should pass when testing that true is true", [this]() 
			{
				TestTrue(TEXT("True should be true"), true);
			});
		});
	}
```
With the spec tests, be careful about capturing state

```cpp
    BEGIN_DEFINE_SPEC(FMinimalTest, "Game.Test", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
		uint32 SomeValue = 3;
	END_DEFINE_SPEC

	void FMinimalTest::Define() 
	{
		Describe("Assertions", [this]() 
		{
			uint describeValue = 42;
			It("Has access to members defined on the spec", [this]() 
			{
				TestEqual(TEXT("Class value should be set"), SomeValue, 3);
			});
			
			xIt("Does not capture variables described inside of lambdas", [this]() 
			{
				TestEqual(TEXT("DescribeValue will now be garbage as it went out of scope"), describeValue, 42);
			});
		});
	}
```

The inspiration for CQTest was to add the before/after test abilities, while resetting state between tests automatically.  One of the guiding principles is to make easy things easy.

```cpp
	TEST(MinimalTest, "Game.Test") 
	{
		ASSERT_THAT(IsTrue(true));
	}
	
	TEST_CLASS(MinimalFixture, "Game.Test") 
	{
		uint32 SomeNumber = 0;
		BEFORE_EACH() 
		{
			SomeNumber++;
		}

		TEST_METHOD(MinimalFixture, CanAccessMembers) 
		{
			ASSERT_THAT(AreEqual(1, SomeNumber));  // passes every time
		}
	};
```

# Installation
Inside the project that you want to test, you'll need to change 2 things:
In the .uproject file of the project you want to test, add the following to the Plugins section

```json
		{
			"Name": "CQTest",
			"Enabled": true
		}
```

Then in the project's .Build.cs file, you'll want to add the following to the PrivateDependencyModuleNames.  Something like

```csharp
		PrivateDependencyModuleNames.AddRange(
			new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"CQTest"  <----
				 }
			);
```

	
# Build
No special steps are required to build the plugin project, it should build with the rest of the project.

# Test
This plugin has a set of tests to validate and document the behavior.  To run tests in Unreal
- Launch the editor
- Find the Tools drop down and select Session Frontend
- Navigate to the Automation tab
- By default, the tests should be listed first under " Product.Plugins.CQTest"
- Select the tests you would like to run and press 'Start Tests'

# Examples

Tests can be as simple as

```cpp
    #include "CQTest.h"
	
	TEST(MinimalTest, "Game.MyGame") 
	{
		ASSERT_THAT(IsTrue(true));
	}
```

For setup and teardown, or common state between multiple tests, or to group related tests, use the TEST_CLASS macro.

```cpp
    #include "CQTest.h"
	TEST_CLASS(MyNeatTest, "Game.MyGame") 
	{
		bool SetupCalled = false;
		uint32 SomeNumber = 0;
		Thing* Thing = nullptr;
		
		BEFORE_EACH() 
		{
			SetupCalled = true;
			SomeNumber++;
			Thing = new Thing();
		}
		
		AFTER_EACH() 
		{
			delete Thing; //Should normally use RAII for things like this
		}
		
	protected:
		bool UsefulHelperMethod() const 
		{
			return true; 
		}
		
		TEST_METHOD(BeforeRunTest_CallsSetup) 
		{
			ASSERT_THAT(IsTrue(SetupCalled));
		}
		
		TEST_METHOD(ProtectedMembers_AreAccessible) 
		{
			ASSERT_THAT(IsTrue(UsefulHelperMethod()));
		}
		
		TEST_METHOD(DataMembers_BetweenTestRuns_AreReset) 
		{
			ASSERT_THAT(AreEqual(1, SomeNumber));
		}
	};
```

Test Directory determines where in the Automation tab the tests appear.  In the example above, we specify "Game.MyGame", but you may also have an auto-generated test directory based on the folder structure.

```cpp
	TEST_CLASS(MyNeatTest, GenerateTestDirectory)
	{
	};

	TEST_CLASS(MyNeatTest, "Game.Test.[GenerateTestDirectory].Validation")
	{
	};
```

Constructors (and destructors) are available.  Destructors shouldn't throw, and you shouldn't put assertions in them (as they are called after the testing framework is done with the test).

```cpp
	TEST_CLASS(SomeTestClass, "Game.Test")
	{
		bool bConstructed = false;
		SomeTestClass()
			: bConstructed(true)
		{
		}
		
		TEST_METHOD(ConstructorIsCalled)
		{
			ASSERT_THAT(IsTrue(bConstructed));
		}
	};
```	

Latent actions are supported with the TEST_CLASS macro.  Each step will complete all latent actions before moving to the next.  If an assertion is raised during a latent action, then no further latent actions will be processed.  The AFTER_EACH method will still be invoked though.

```cpp
    TEST_CLASS(LatentActionTest, "Game.Test") 
	{
		uint32 calls = 0;
		BEFORE_EACH() 
		{
			AddCommand(new FExecute([&]() { calls++; }));
		}
		
		AFTER_EACH() 
		{
			AddCommand(new FExecute([&]() { calls++; })); // executed after the next line, as it is a latent action
			ASSERT_THAT(AreEqual(2, calls));
		}
		
		TEST_METHOD(PerformLatentAction) 
		{
			ASSERT_THAT(AreEqual(1, calls));
			AddCommand(new FExecute([&]() { calls++; }));
		}
	};
```

Also available for commands is a fluent command builder

```cpp
	TEST_METHOD(SomeTest) 
	{
		TestCommandBuilder
			.Do([&]() { StepOne(); })
			.Then([&]() { StepTwo(); })
			.Until([&]() { return StepThreeComplete(); })
			.Then([&]() { ASSERT_THAT(IsTrue(SomethingImportant)); });
	}
```

The framework will ensure that all of those commands happen in order using a future pattern.
Similarly, the framework will ensure that a test can await a ticking object.  See GameObjectsTickTest for an example
One word of caution, the framework does not currently support adding latent actions from within latent actions.
Instead, it is better to add the actions as a series of self-contained steps.
 

# Extending the framework
The framework has been designed to allow for extensions in a couple areas.  See ExtensionTests.cpp for in-code examples.

## Test Components
This testing framework embraces composition over inheritence.  Creating new components should be the default mechanism for extending the framework.  Some of the components available to you are:
  ActorTestSpawner - Allows a test to spawn actors, and manages their despawning.
  MapTestSpawner - Creates a map and opens a level.  Allows tests to spawn actors in that world.
  BlueprintHelper - Eases the ability for a test to spawn Blueprint objects, intended to be used with MapTestSpawner.
  PIENetworkComponent - Allows tests to create a server and a collection of clients.  Good for testing replication.

## Assertions
Not all platforms support exceptions, and so the assertions are unable to rely on them.
There are a few options here:
  We could just throw exceptions, and only run tests on platforms which support exceptions
  We could return a [[nodiscard]] bool to encourage checking each assertion and returning if it fails
  We could return a normal bool and rely on people to check it when it's important.
Exceptions have the advantage of working in helper functions and lambdas, as well as not depending on human diligence.
A normal bool is less noisy, and allows developers to use intellisense, but is more error prone
The default implementation used is the [[nodiscard]] bool, with a helper macro ASSERT_THAT which does the early return check for you.

You can use your own types within the Assert.AreEqual and Assert.AreNotEqual methods assuming you have the == and != operators defined as needed.
In addition, the error message will print out the string version of your type, assuming you have a ToString method defined as well.  The framework will complain if it doesn't know how to print your value.
You can find examples of providing a string to the framework in CQTestConvertTests.cpp, but below is a simple example.
```cpp
struct MyCustomType
{
	int32 Value;
	bool operator==(const MyCustomType& other) const
	{
		return Value == other.Value;
	}
	bool operator!=(const MyCustomType& other) const
	{
		return !(*this == other);
	}
	
	FString ToString() const
	{
		//your to string logic
		return FString();
	}
};
enum struct MyCustomEnum
{
	Red, Green, Blue
};
template<>
FString CQTestConvert::ToString(const MyCustomEnum&)
{
	//your to string logic
	return FString();
}
```

You are able to customize the assertions which are available, and how they behave.
See CQTestTests/Private/ExtensionTests.cpp for an example
Below is some untested example code to inspire ideas

```cpp
	struct FluentAsserter
	{
	private:
		int CurrentIntValue = 0;
		TArray<FString> Errors;
		FAutomationTestBase& TestRunner;
		
	public:
		FluentAsserter(FAutomationTestBase& InTestRunner)
			: TestRunner(InTestRunner)
		{
		}
		
		~FluentAsserter()
		{
			for(const auto& error : Errors)
			{
				TestRunner.AddError(error);
			}
		}
		
		FluentAsserter& That(int value)
		{
			CurrentIntValue = value;
			return *this;
		}
		
		FluentAsserter& Equals(int value)
		{
			if(CurrentIntValue != value)
			{
				Errors.Add(FString::Printf("%d != %d", CurrentIntValue, value));
			}
			return *this;
		}
	};
```

From here, you could create macros your studio uses to create tests

```cpp
	#define MY_STUDIO_TEST_CLASS(_ClassName, _TestDir) TEST_CLASS_WITH_ASSERTS(_ClassName, _TestDir, FluentAsserter)
	#define MY_STUDIO_TEST(_TestName, _TestDir) \
	MY_STUDIO_TEST_CLASS(_TestName, _TestDir) \
	{ \
		TEST_METHOD(_TestName##_Method); \
	};\
	void _TestName::_TestName##_Method()
```

## Base test class
Similarly there may be a use case to create many tests which have the same member variables or helper methods.  This can be implemented by extending the test class

```cpp
	template<typename Derived, typename AsserterType>
	struct ActorTest : public Test<Derived, AsserterType>
	{
		SpawnHelper Spawner;
	};
```

And creating a macro which uses it

```cpp
	#define ACTOR_TEST(_ClassName, _TestDir) TEST_CLASS_WITH_BASE(_ClassName, _TestDir, ActorTest)
```

# Contribute
Improvements like bug fixes and extensions are welcome when accompanied by unit tests.
