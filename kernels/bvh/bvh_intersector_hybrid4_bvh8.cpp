// ======================================================================== //
// Copyright 2009-2018 Intel Corporation                                    //
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

#include "bvh_intersector_hybrid.cpp"

namespace embree
{
  namespace isa
  {
    ////////////////////////////////////////////////////////////////////////////////
    /// BVH8Intersector4 Definitions
    ////////////////////////////////////////////////////////////////////////////////

    IF_ENABLED_TRIS(DEFINE_INTERSECTOR4(BVH8Triangle4Intersector4HybridMoeller,         BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA false COMMA ArrayIntersectorK_1<4 COMMA TriangleMIntersectorKMoeller  <SIMD_MODE(4) COMMA 4 COMMA true> > >));
    IF_ENABLED_TRIS(DEFINE_INTERSECTOR4(BVH8Triangle4Intersector4HybridMoellerNoFilter, BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA false COMMA ArrayIntersectorK_1<4 COMMA TriangleMIntersectorKMoeller  <SIMD_MODE(4) COMMA 4 COMMA false> > >));
    IF_ENABLED_TRIS(DEFINE_INTERSECTOR4(BVH8Triangle4iIntersector4HybridMoeller,        BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA false COMMA ArrayIntersectorK_1<4 COMMA TriangleMiIntersectorKMoeller <SIMD_MODE(4) COMMA 4 COMMA true> > >));
    IF_ENABLED_TRIS(DEFINE_INTERSECTOR4(BVH8Triangle4vIntersector4HybridPluecker,       BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA true  COMMA ArrayIntersectorK_1<4 COMMA TriangleMvIntersectorKPluecker<SIMD_MODE(4) COMMA 4 COMMA true> > >));
    IF_ENABLED_TRIS(DEFINE_INTERSECTOR4(BVH8Triangle4iIntersector4HybridPluecker,       BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA true  COMMA ArrayIntersectorK_1<4 COMMA TriangleMiIntersectorKPluecker<SIMD_MODE(4) COMMA 4 COMMA true> > >));

    IF_ENABLED_TRIS(DEFINE_INTERSECTOR4(BVH8Triangle4vMBIntersector4HybridMoeller,  BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN2_AN4D COMMA false COMMA ArrayIntersectorK_1<4 COMMA TriangleMvMBIntersectorKMoeller <SIMD_MODE(4) COMMA 4 COMMA true> > >));
    IF_ENABLED_TRIS(DEFINE_INTERSECTOR4(BVH8Triangle4iMBIntersector4HybridMoeller,  BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN2_AN4D COMMA false COMMA ArrayIntersectorK_1<4 COMMA TriangleMiMBIntersectorKMoeller <SIMD_MODE(4) COMMA 4 COMMA true> > >));
    IF_ENABLED_TRIS(DEFINE_INTERSECTOR4(BVH8Triangle4vMBIntersector4HybridPluecker, BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN2_AN4D COMMA true  COMMA ArrayIntersectorK_1<4 COMMA TriangleMvMBIntersectorKPluecker<SIMD_MODE(4) COMMA 4 COMMA true> > >));
    IF_ENABLED_TRIS(DEFINE_INTERSECTOR4(BVH8Triangle4iMBIntersector4HybridPluecker, BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN2_AN4D COMMA true  COMMA ArrayIntersectorK_1<4 COMMA TriangleMiMBIntersectorKPluecker<SIMD_MODE(4) COMMA 4 COMMA true> > >));

    IF_ENABLED_QUADS(DEFINE_INTERSECTOR4(BVH8Quad4vIntersector4HybridMoeller,        BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA false COMMA ArrayIntersectorK_1<4 COMMA QuadMvIntersectorKMoeller <4 COMMA 4 COMMA true> > >));
    IF_ENABLED_QUADS(DEFINE_INTERSECTOR4(BVH8Quad4vIntersector4HybridMoellerNoFilter,BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA false COMMA ArrayIntersectorK_1<4 COMMA QuadMvIntersectorKMoeller <4 COMMA 4 COMMA false> > >));
    IF_ENABLED_QUADS(DEFINE_INTERSECTOR4(BVH8Quad4iIntersector4HybridMoeller,        BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA false COMMA ArrayIntersectorK_1<4 COMMA QuadMiIntersectorKMoeller <4 COMMA 4 COMMA true> > >));
    IF_ENABLED_QUADS(DEFINE_INTERSECTOR4(BVH8Quad4vIntersector4HybridPluecker,       BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA true  COMMA ArrayIntersectorK_1<4 COMMA QuadMvIntersectorKPluecker<4 COMMA 4 COMMA true> > >));
    IF_ENABLED_QUADS(DEFINE_INTERSECTOR4(BVH8Quad4iIntersector4HybridPluecker,       BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA true  COMMA ArrayIntersectorK_1<4 COMMA QuadMiIntersectorKPluecker<4 COMMA 4 COMMA true> > >));

    IF_ENABLED_QUADS(DEFINE_INTERSECTOR4(BVH8Quad4iMBIntersector4HybridMoeller,      BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN2_AN4D COMMA false COMMA ArrayIntersectorK_1<4 COMMA QuadMiMBIntersectorKMoeller <4 COMMA 4 COMMA true> > >));
    IF_ENABLED_QUADS(DEFINE_INTERSECTOR4(BVH8Quad4iMBIntersector4HybridPluecker,     BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN2_AN4D COMMA true  COMMA ArrayIntersectorK_1<4 COMMA QuadMiMBIntersectorKPluecker<4 COMMA 4 COMMA true> > >));

    IF_ENABLED_CURVES(DEFINE_INTERSECTOR4(BVH8Line4iIntersector4,  BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA false COMMA ArrayIntersectorK_1<4 COMMA LineMiIntersectorK<SIMD_MODE(4) COMMA 4 COMMA true> > >));
    IF_ENABLED_CURVES(DEFINE_INTERSECTOR4(BVH8Line4iMBIntersector4,BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN2_AN4D COMMA false COMMA ArrayIntersectorK_1<4 COMMA LineMiMBIntersectorK<SIMD_MODE(4) COMMA 4 COMMA true> > >));
 
    IF_ENABLED_CURVES(DEFINE_INTERSECTOR4(BVH8OBBVirtualCurveIntersector4Hybrid, BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1_UN1 COMMA false COMMA VirtualCurveIntersectorK<4> >));

    IF_ENABLED_USER(DEFINE_INTERSECTOR4(BVH8VirtualIntersector4Chunk, BVHNIntersectorKChunk<8 COMMA 4 COMMA BVH_AN1 COMMA false COMMA ArrayIntersectorK_1<4 COMMA ObjectIntersector4> >));
    IF_ENABLED_USER(DEFINE_INTERSECTOR4(BVH8VirtualMBIntersector4Chunk, BVHNIntersectorKChunk<8 COMMA 4 COMMA BVH_AN2_AN4D COMMA false COMMA ArrayIntersectorK_1<4 COMMA ObjectIntersector4MB> >));

    IF_ENABLED_GRIDS(DEFINE_INTERSECTOR4(BVH8GridIntersector4HybridMoellerNoFilter, BVHNIntersectorKHybrid<8 COMMA 4 COMMA BVH_AN1 COMMA false COMMA SubGridIntersectorKMoeller <8 COMMA 4 COMMA false> >));


  }
}

