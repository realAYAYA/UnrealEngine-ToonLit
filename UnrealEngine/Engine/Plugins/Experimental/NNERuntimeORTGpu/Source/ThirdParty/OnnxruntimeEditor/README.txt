- How to compile ONNXRuntime for Dml and Cuda provider -

**********************
**** Prerequisite ****
**********************

visual studio 2019
cuda toolkit v11.6
cudnn v8.5.0.96
Python 3.x
cmake >=3.24

***************
**** Steps ****
***************

Developer Command Prompt for VS 2019

cd <SomePath>
git clone --recursive https://github.com/Microsoft/onnxruntime.git
cd onnxruntime
git switch rel-1.13.1

build.bat --config Release --build_shared_lib --parallel --cmake_generator "Visual Studio 16 2019" --use_full_protobuf --use_cuda --cudnn_home "C:\Program Files\NVIDIA\CUDNN\v8.5.0.96" --cuda_home "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.6" --cuda_version 11.6 --use_dml --skip_tests --use_full_protobuf

copy <SomePath>\onnxruntime\include to Engine\Plugins\Experimental\NNE\Source\ThirdParty\OnnxruntimeEditor\include

copy onnxruntime.lib, onnxruntime_providers_shared.lib, onnxruntime_providers_cuda.lib 
from <SomePath>\onnxruntime\onnxruntime\build\Windows\Release\Release
to Engine\Plugins\Experimental\NNE\Source\ThirdParty\OnnxruntimeEditor\lib

copy onnxruntime.dll, onnxruntime_providers_shared.dll, onnxruntime_providers_cuda.dll 
from <SomePath>\onnxruntime\onnxruntime\build\Windows\Release\Release
to Engine\Plugins\Experimental\NNE\Source\ThirdParty\OnnxruntimeEditor\lib

adjust version numbers in NNEOnnxruntimeEditor.build.cs