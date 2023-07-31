NVIDIA Video Codec SDK 10.0 Readme and Getting Started Guide

System Requirements

* NVIDIA Kepler/Maxwell/Pascal/Volta/Turing/GA100 GPU with hardware video accelerators - Refer to the NVIDIA Video SDK developer zone web page (https://developer.nvidia.com/nvidia-video-codec-sdk) for GPUs which support encoding and decoding acceleration.

Video Codec SDK 10.0
   * Windows: Driver version 445.87 or higher
   * Linux:   Driver version 450.51 or higher
   * CUDA 10.1 Toolkit (optional) (CUDA 11.0 or higher if GA100 GPU is used)

[Windows Configuration Requirements]
- DirectX SDK is needed. You can download the latest SDK from Microsoft's DirectX
  website.

- The Vulkan SDK needs to be installed in order to build and run the
  AppMotionEstimationVkCuda sample application.

- In Windows, the following environment variables must be set to build the sample
applications included with the SDKRead Me
  - DXSDK_DIR: pointing to the DirectX SDK root directory.
  - VULKAN_SDK: pointing to Vulkan SDK install directory.
  - The CUDA Toolkit and the related environment variables are optional to install
    if the client has Video Codec SDK 8.0. However, they are mandatory if client has
    Video Codec SDK 8.1 or above on his/her machine.

[Linux Configuration Requirements]    
- X11 and OpenGL, GLUT, GLEW libraries for video playback and display

- CUDA Toolkit is mandatory if client has Video Codec SDK 8.1 or above on his/her
  machine.

- Libraries and headers from the FFmpeg project which can be downloaded and
  installed using the distribution's package manager or compiled from source.
  - The sample applications have been compiled and tested against the libraries and
    headers from FFmpeg- 4.1. The source code of FFmpeg- 4.1 has been included in
    this SDK package. While configuring FFmpeg on Linux, it is recommended not to
    use 'disable-decoders' option. This configuration is known to have a channel error
    (XID 31) while executing sample applications with certain clips and/or result in
    an unexpected behavior.

- To build/use sample applications that depend on FFmpeg, users may need to
  - Add the directory (/usr/local/lib/pkgconfig by default) to the
    PKG_CONFIG_PATH environment variable. This is required by the Makefile to
    determine the include paths for the FFmpeg headers.
  - Add the directory where the FFmpeg libraries are installed to the
    LD_LIBRARY_PATH environment variable. This is required for resolving runtime
    dependencies on FFmpeg libraries.

- Stub libraries (libnvcuvid.so and libnvidia-encode.so) have been included as part of
  the SDK package, in order to aid development of applications on systems where
  the NVIDIA driver has not been installed. The sample applications in the SDK will
  link against these stub libraries as part of the build process. However, users need to
  ensure that the stub libraries are not referenced when running the sample applications.
  A driver compatible with this SDK needs to be installed in order for the sample
  applications to work correctly.

- The Vulkan SDK needs to be installed in order to build and run the
  AppMotionEstimationVkCuda sample application.

[Common to all OS platforms]
* CUDA toolkit can be downloaded from http://developer.nvidia.com/cuda/cudatoolkit
* Vulkan SDK can be downloaded from https://vulkan.lunarg.com/sdk/home.
Alternatively, it can be installed by using the distribution's package manager.

