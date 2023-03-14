# MetricsDiscoveryHelper

MetricsDiscovery is the Intel graphics driver's API providing access to GPU
metrics and override controls.  More information on the MetricsDiscovery API is
available here:
[https://github.com/intel/metrics-discovery](https://github.com/intel/metrics-discovery)

MetricsDiscoveryHelper is a runtime library that helps configure and collect MetricsDiscovery metrics from an application.  The intent is that this is a fairly thin layer that
- implements some of the common functionality requried to access the MetricsDiscovery API,
- adds some abstractions that simplify use in real-time applications,
- provides an interface that can be implemented by Intel MetricsFramework,
- and has as low overhead as possible for use in real-time analysis scenarios.

Please see [CONTRIBUTING](CONTRIBUTING.md) for information on how to request
features, report issues, or contribute code changes.

See [metrics_discovery_helper.h](source/metrics_discovery_helper.h) for documentation, and for sample usage:
- [samples/periodic_sample](samples/periodic_sample/periodic_sample.cpp)
- [samples/range_sample_dx11](samples/range_sample_dx11/range_sample_dx11.cpp)

## License

Copyright 2018 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Building

Use VisualStudio 2017 to build the desired configuration of metrics_discovery_helper.vcxproj.

The resulting includes and library will be copied to ```build\include\``` and ```build\lib\```.

