// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "../common/tutorial/tutorial.h"
#include "../common/tutorial/statistics.h"
#include <set>
#include "../../common/sys/mutex.h"
#include "../common/core/ray.h"
#include "../../kernels/geometry/triangle_triangle_intersector.h"

namespace embree
{
  RTCDevice g_device = nullptr;
  RTCScene g_scene = nullptr;
  struct Tutorial : public TutorialApplication
  {
    bool pause;
  
    Tutorial()
      : TutorialApplication("collide",FEATURE_RTCORE), pause(false)//, use_user_geometry(false)
    {
      camera.from = Vec3fa(-2.5f,2.5f,-2.5f);
      camera.to   = Vec3fa(0.0f,0.0f,0.0f);
    }
    
  };

}

int main(int argc, char** argv) {
  return embree::Tutorial().main(argc,argv);
}
