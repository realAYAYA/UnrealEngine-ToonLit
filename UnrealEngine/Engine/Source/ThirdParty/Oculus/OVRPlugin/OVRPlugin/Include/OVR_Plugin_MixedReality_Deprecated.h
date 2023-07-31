/************************************************************************************

Copyright (c) Facebook Technologies, LLC and its affiliates.  All rights reserved.

Your use of this SDK or tool is subject to the Oculus SDK License Agreement, available at
https://developer.oculus.com/licenses/oculussdk/

Unless required by applicable law or agreed to in writing, the Oculus SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_Plugin_MixedReality_Deprecated_h
#define OVR_Plugin_MixedReality_Deprecated_h

#include "OVR_Plugin_MixedReality.h"

#ifdef __cplusplus
extern "C" {
#endif

OVRP_EXPORT ovrpBool ovrp_IsCameraDeviceAvailable(ovrpCameraDevice camera);
OVRP_EXPORT ovrpBool ovrp_HasCameraDeviceOpened(ovrpCameraDevice camera);
OVRP_EXPORT ovrpBool ovrp_IsCameraDeviceColorFrameAvailable(ovrpCameraDevice camera);

#ifdef __cplusplus
}
#endif

#endif
