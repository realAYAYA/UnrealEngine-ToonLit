# About the library

The Android Memory Assistance API is an experimental library to help
applications avoid exceeding safe limits of memory use on devices. It can be
considered an alternative to using
[onTrimMemory](https://developer.android.com/reference/android/content/ComponentCallbacks2#onTrimMemory\(int\))
events to manage memory limits.

It can also provide an estimate of how much memory is approximately available,
helping apps scale assets to make the best use of the device.

# Specifics and limitations

The library is compatible with API level 19 (Android 4.4 / KitKat) and higher.
The library gathers metrics from a few different sources but has no special
access to the Android OS. These sources already available to apps, but not all
will be familiar to developers. The library is standalone with no dependencies
beyond the Android SDK

The metric sources available are currently:

*   The file `/proc/meminfo` for values such as `Active`, `Active(anon)`.
    `Active(file)`, `AnonPages`, `MemAvailable`, `MemFree`, `VmData`, `VmRSS`,
    `CommitLimit`, `HighTotal`, `LowTotal` and `MemTotal`.

*   The file `/proc/(pid)/status` for values such as `VmRSS` and `VmSize`.

*   `ActivityManager.getMemoryInfo()` for values such as `totalMem`,
    `threshold`, `availMem` and `lowmemory`.

*   The file `/proc/(pid)/oom_score`

*   `Debug.getNativeHeapAllocatedSize()`

*   `ActivityManager.getProcessMemoryInfo()`. It is not recommended for
    real-time use due to the high cost of calls and being rate throttled to one
    call per five minutes on newer devices.

It also has access to a dictionary of readings taken from about 175 distinct
phones that have run a stress test application in the Firebase Test Lab. This
dictionary is bundled directly with the library.

The library is only intended for use while applications are running in the
foreground (currently operated by users).

"Memory" means specifically native heap memory allocated by malloc, and graphics
memory allocated by the OpenGL ES and Vulkan Graphics APIs. (Note: Memory is
only one limit affecting graphics use; for example, the number of active GL
allocations is limited to the value `vm.max_map_count` (65536 on most devices).
However, memory is the only resource that this library attempts to track.)

If the preferred metrics or suitable lab readings are not available on any
device for some reason, then alternatives will be used instead.

The advice is given on a best effort basis without guarantees. The methods used
to generate this advice are quite simple. The library was built having studied
all available signals to determine which ones act as reliable predictors of
memory overload. The value added by the library is that these methods have been
found in lab tests to be effective,

The library is experimental until the assumption is proven that devices will
behave similarly to other devices with the same hardware and version of Android.

The API is intended for use by native applications (i.e., applications that are
written primarily in C/C++) but is itself written in Java. (As long as Android
platform calls are needed, such as those based on ActivityManager, some Java
components will be required in the library.)

Currently the API will not work effectively for applications that run in 32 bit
mode on devices that have 64 bit mode available (in other words where 32 bit
mode is "forced"). This is because the stress tests that calibrate the library
are run in 64 bit mode wherever available.

# API capabilities

The API can be called at any time to discover:

*   A memory warning signal that indicates whether siginficant allocation should
    stop ("yellow"), or memory should be freed ("red").

*   An estimate of the number of bytes safely available for future allocation
    (in the case of a "yellow" warning signal, or no warning signal).

*   A collection of raw memory metrics that can be captured for diagnostic
    purposes.

The choice of polling rate is left to the developer, to strike the correct
balance between calling cost (this varies significantly by device but the
ballpark is between 5 and 20 ms per call) and the rate of memory allocation
performed by the app (higher rate allows a more timely reaction to warnings ).
The API does not cache or rate limit, nor does it use a timer or other thread
mechanism.

All this information is dispensed as a JSON object, for a few reasons:

*   To allow easy integration across language barriers, C++, Java, and in the
    case of Unity projects, C#.

*   To allow capture of all data using telemetry for diagnostic purposes without
    devising a separate schema for the telemetry.

*   To avoid solidifying a strict schema in the API when the metrics that are
    captured and synthesized are both not available on all devices, and liable
    to change.

There is no strict requirement for apps to read the JSON returned. Instead, the
library provides methods that can be called to get simple recommendations.
However, as long as the library is experimental, this interface is not
guaranteed to be stable.

# Limitations of estimates

Estimates are currently the most experimental part of the libarary, and
estimates may be inaccurate for combinations of devices that we have not seen in
lab testing.

They may suggest a lower limit than can be allocated by an application in
practice, because:

*   Different types of memory allocation (e.g. heap allocation vs allocation via
    graphics API) experience different limits, MB for MB. The library is
    pessimistic so will report the lower figure.

*   The memory advice library is pessimistic about the effets of zram
    compression on memory availability. Allocated memory that is both rarely
    used, and has compressible contents (e.g. contains repeated data) can be
    compressed by [zram](https://en.wikipedia.org/wiki/Zram) on Android. In
    extreme cases this could even result in apps apparently allocating more
    memory that was actually present on the device.

The estimate is of remaining memory to allocate. This may change over time
affected by other actvity on the device. After memory has been allocated by the
app, further calls may receive more accurate results than calls made at the
start of the app's lifetime.

# Recommended strategies

The library will only be of use if client applications can vary the amount of
memory used. The variation could take the form of model detail, texture detail,
optional enhancements such as particle effects or shadows. Audio assets could be
reduced in fidelity, or dropped entirely.

Games must be able to vary memory used at startup, at runtime, or both.

## Startup strategies

If the application has a range of increasing quality assets and can estimate the
memory footprint of each option, it can query the library for the estimated
amount of memory remaining and select the best quality options that fit. Models
of increasing detail could be loaded until the yellow signal was received. At
this point, the application could stop increasing the quality of assets to avoid
making further significant allocations.

## Runtime strategies

Games could react to the "red" signal by unloading assets such as audio,
particle effects, or shadows, reducing screen resolution, or reducing texture
resolution. These can be restored when no more memory warnings (including
"yellow" signal) are received.

# API specifics

## Getting the library

Get the [repo tool](https://gerrit.googlesource.com/git-repo/) and sync the
Games SDK project
[games-sdk project](https://android.googlesource.com/platform/frameworks/opt/gamesdk/+/refs/heads/master);

```bash
repo init -u https://android.googlesource.com/platform/manifest -b my-branch
```

## Adding the library to an Android project

The Memory Advice library is found in folder
[test/memoryadvice](https://android.googlesource.com/platform/frameworks/opt/gamesdk/+/refs/heads/master/test/memoryadvice/).

It is recommended to build the library from source as a dependency of your app,
using Android Studio and Gradle integration.

Add these lines to the settings.gradle file in the root of the project,
replacing `..` with the path containing the memoryadvice project.

```gradle
include ':app', ':memoryadvice'
project(':memoryadvice').projectDir = new File('../memoryadvice/memoryadvice')
```

In the main application `settings.gradle` file, add an `implementation` line to
the `dependencies` section:

```gradle
dependencies {
    // ..
    implementation project(':memoryadvice')
}
```

In your main activity, initialize a single memoryAdviser object for the lifetime
of your app.

```java
import android.app.Activity;
import android.os.Bundle;
import com.google.android.apps.internal.games.memoryadvice.MemoryAdvisor;

class MyActivity extends Activity {
  private MemoryAdvisor memoryAdvisor;
  @Override
  protected void onCreate(Bundle savedInstanceState) {
    // ...
    memoryAdvisor = new MemoryAdvisor(this);
  }
}
```

At some frequency, call the memory advisor to get recommendations. The choice of
frequency should reflect the cost of calling `getAdvice()`; approximately 5 to
20 milliseconds per call.

```java
import android.app.Activity;
import com.google.android.apps.internal.games.memoryadvice.MemoryAdvisor;
import org.json.JSONObject;

class MyActivity extends Activity {
  private MemoryAdvisor memoryAdvisor;
  // ...
  void myMethod() {
    JSONObject advice = memoryAdvisor.getAdvice();
    // ...
  }
}
```

One option is to call the library back with the object to interpret the
recommendations.

```java
import android.app.Activity;
import com.google.android.apps.internal.games.memoryadvice.MemoryAdvisor;
import org.json.JSONObject;

class MyActivity extends Activity {
  private MemoryAdvisor memoryAdvisor;
  // ...
  void myMethod() {
    JSONObject advice = memoryAdvisor.getAdvice();
    MemoryAdvisor.MemoryState memoryState = MemoryAdvisor.getMemoryState(advice);
    switch (memoryState) {
      case OK:
        // The application can safely allocate significant memory.
        break;
      case APPROACHING_LIMIT:
        // The application should not allocate significant memory.
        break;
      case CRITICAL:
        // The application should free memory as soon as possible, until the memory state changes.
        break;
    }
  }
}
```

Another options is to initialize a `MemoryWatcher` object. This will call back
the application when the memory warning changes.

To limit the time overhead introduced by the Memory Assistance API, the calling
application sets a budget in milliseconds per second of runtime to spend
collecting the memory metrics and prepare the advice. The callback rate will be
automatically adjusted to stay within this budget.

In this example, a limit of 10 milliseconds per second is applied, and a maximum
duration between iterations of three seconds.

```java
import android.app.Activity;
import com.google.android.apps.internal.games.memoryadvice.MemoryAdvisor;
import com.google.android.apps.internal.games.memoryadvice.MemoryWatcher;
import org.json.JSONObject;

class MyActivity extends Activity {
  private MemoryAdvisor memoryAdvisor;
  // ...
  void myMethod() {
    JSONObject advice = memoryAdvisor.getAdvice();
    MemoryWatcher memoryWatcher = new MemoryWatcher(memoryAdvisor, 10, 3000,
        new MemoryWatcher.Client(){
      @Override
      public void newState(MemoryAdvisor.MemoryState state) {
        switch (memoryState) {
          case OK:
            // The application can safely allocate significant memory.
            break;
          case APPROACHING_LIMIT:
            // The application should not allocate significant memory.
            break;
          case CRITICAL:
            // The application should free memory as soon as possible, until the memory state
            // changes.
            break;
        }
      }
    });
  }
}
```

Call the library back with the object to get an estimate in bytes of the memory
that can safely be allocated by the application.

```java
import android.app.Activity;
import com.google.android.apps.internal.games.memoryadvice.MemoryAdvisor;
import org.json.JSONObject;

class MyActivity extends Activity {
  private MemoryAdvisor memoryAdvisor;
  // ...
  void myMethod() {
    JSONObject advice = memoryAdvisor.getAdvice();
    long availabilityEstimate = MemoryAdvisor.availabilityEstimate(advice);
    // ...
  }
}
```

## Telemetry / debugging info

Logging and debugging telemetry can be obtained from the advisor object.
Information about the device itself:

```java
import android.app.Activity;
import android.os.Bundle;
import com.google.android.apps.internal.games.memoryadvice.MemoryAdvisor;

class MyActivity extends Activity {
  private MemoryAdvisor memoryAdvisor;
  @Override
  protected void onCreate(Bundle savedInstanceState) {
    // ...
    memoryAdvisor = new MemoryAdvisor(this);
    JSONObject deviceInfo = memoryAdvisor.getDeviceInfo();
    // Now convert the deviceInfo to a string and log it.
  }
}
```

The data returned by `getAdvice()`:

```java
import android.app.Activity;
import com.google.android.apps.internal.games.memoryadvice.MemoryAdvisor;
import org.json.JSONObject;

class MyActivity extends Activity {
  private MemoryAdvisor memoryAdvisor;
  // ...
  void myMethod() {
    JSONObject advice = memoryAdvisor.getAdvice();
    // Now convert the advice object to a string and log it.
  }
}
```

## Unity integration

1.  In Android Studio, build the library
    [memoryadvice](https://android.googlesource.com/platform/frameworks/opt/gamesdk/+/refs/heads/master/test/memoryadvice/).
2.  Drag the generated AAR (e.g.
    `gamesdk/test/memoryadvice/memoryadvice/build/outputs/aar/memoryadvice-debug.aar`)
    into the Assets folder of your Unity project. See
    [Unity instructions](https://docs.unity3d.com/Manual/AndroidAARPlugins.html)
    for including Android libraries in projects.
3.  Optional: add the
    [JSON Object package](https://assetstore.unity.com/packages/tools/input-management/json-object-710)
    to your Unity project.
4.  Copy
    [UnityMemoryHandler.cs](https://android.googlesource.com/platform/frameworks/opt/gamesdk/+/refs/heads/master/test/unitymemory/UnityMemoryHandler.cs)
    and drag the script into the Scripts folder of your Unity project.
5.  Create a new Game Object in your game scene, and drag the
    `UnityMemoryHandler.cs` script from your Assets folder to the new Game
    Object to associate it with the script.
6.  Customize the script as required.

## Reporting bugs

Email [jimblackler@google.com](mailto:jimblackler@google.com) Please include the
output from memoryAdvisor.getDeviceInfo().toString().
