// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
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

#include "../common/tutorial/tutorial_device.h"

namespace embree {

/* scene data */
RTCDevice g_device = nullptr;
RTCScene g_scene = nullptr;
Vec3fa face_colors[12];

/* accumulation buffer */
Vec3fa* g_accu = nullptr;
unsigned int g_accu_width = 0;
unsigned int g_accu_height = 0;
unsigned int g_accu_count = 0;
Vec3fa g_accu_vx;
Vec3fa g_accu_vy;
Vec3fa g_accu_vz;
Vec3fa g_accu_p;
extern "C" bool g_changed;
extern "C" float g_time;
extern "C" unsigned int g_num_time_steps;
extern "C" unsigned int g_num_time_steps2;


__aligned(16) float cube_vertices[8][4] =
{
  { -1.0f, -1.0f, -1.0f, 0.0f },
  {  1.0f, -1.0f, -1.0f, 0.0f },
  {  1.0f, -1.0f,  1.0f, 0.0f },
  { -1.0f, -1.0f,  1.0f, 0.0f },
  { -1.0f,  1.0f, -1.0f, 0.0f },
  {  1.0f,  1.0f, -1.0f, 0.0f },
  {  1.0f,  1.0f,  1.0f, 0.0f },
  { -1.0f,  1.0f,  1.0f, 0.0f }
};

unsigned int cube_triangle_indices[36] = {
  1, 5, 4,  0, 1, 4,
  2, 6, 5,  1, 2, 5,
  3, 7, 6,  2, 3, 6,
  4, 7, 3,  0, 4, 3,
  5, 6, 7,  4, 5, 7,
  3, 2, 1,  0, 3, 1
};

unsigned int cube_quad_indices[24] = {
  0, 1, 5, 4,
  1, 2, 6, 5,
  2, 3, 7, 6,
  0, 4, 7, 3,
  4, 5, 6, 7,
  0, 3, 2, 1,
};

__aligned(16) float cube_vertex_crease_weights[8] = {
  inf, inf,inf, inf, inf, inf, inf, inf
};

__aligned(16) unsigned int cube_vertex_crease_indices[8] = {
  0,1,2,3,4,5,6,7
};

__aligned(16) float cube_edge_crease_weights[12] = {
  inf, inf, inf, inf, inf, inf, inf, inf, inf, inf, inf, inf
};

__aligned(16) unsigned int cube_edge_crease_indices[24] =
{
  0,1, 1,2, 2,3, 3,0,
  4,5, 5,6, 6,7, 7,4,
  0,4, 1,5, 2,6, 3,7,
};

#define NUM_INDICES 24
#define NUM_FACES 6
#define FACE_SIZE 4

unsigned int cube_quad_faces[6] = {
  4, 4, 4, 4, 4, 4
};

/* adds a cube to the scene */
unsigned int addTriangleCube (RTCScene scene, const Vec3fa& pos, unsigned int num_time_steps)
{
  /* create a triangulated cube with 12 triangles and 8 vertices */
  RTCGeometry geom = rtcNewTriangleMesh (g_device, RTC_GEOMETRY_STATIC, num_time_steps);
  rtcSetBuffer(geom, RTC_INDEX_BUFFER,  cube_triangle_indices , 0, 3*sizeof(unsigned int), 12);

  for (size_t t=0; t<num_time_steps; t++)
  {
    RTCBufferType bufID = RTC_VERTEX_BUFFER_(t);
    Vec3fa* vertices = (Vec3fa*) rtcNewBuffer(geom,bufID,sizeof(Vec3fa), 8);

    AffineSpace3fa rotation = AffineSpace3fa::rotate(Vec3fa(0,0,0),Vec3fa(0,1,0),2.0f*float(pi)*(float)t/(float)(num_time_steps-1));
    AffineSpace3fa scale = AffineSpace3fa::scale(Vec3fa(2.0f,1.0f,1.0f));

    for (int i=0; i<8; i++) {
      Vec3fa v = Vec3fa(cube_vertices[i][0],cube_vertices[i][1],cube_vertices[i][2]);
      vertices[i] = Vec3fa(xfmPoint(rotation*scale,v)+pos);
    }
  }

  /* create face color array */
  face_colors[0] = Vec3fa(1,0,0);
  face_colors[1] = Vec3fa(1,0,0);
  face_colors[2] = Vec3fa(0,1,0);
  face_colors[3] = Vec3fa(0,1,0);
  face_colors[4] = Vec3fa(0.5f);
  face_colors[5] = Vec3fa(0.5f);
  face_colors[6] = Vec3fa(1.0f);
  face_colors[7] = Vec3fa(1.0f);
  face_colors[8] = Vec3fa(0,0,1);
  face_colors[9] = Vec3fa(0,0,1);
  face_colors[10] = Vec3fa(1,1,0);
  face_colors[11] = Vec3fa(1,1,0);

  rtcCommitGeometry(geom);
  unsigned int geomID = rtcAttachGeometry(scene,geom);
  rtcReleaseGeometry(geom);
  return geomID;
}

/* adds a cube to the scene */
unsigned int addQuadCube (RTCScene scene, const Vec3fa& pos, unsigned int num_time_steps)
{
  /* create a quad cube with 6 quads and 8 vertices */
  RTCGeometry geom = rtcNewQuadMesh (g_device, RTC_GEOMETRY_STATIC, num_time_steps);
  rtcSetBuffer(geom, RTC_INDEX_BUFFER,  cube_quad_indices , 0, 4*sizeof(unsigned int), 6);

  for (size_t t=0; t<num_time_steps; t++)
  {
    RTCBufferType bufID = RTC_VERTEX_BUFFER_(t);
    Vec3fa* vertices = (Vec3fa*) rtcNewBuffer(geom,bufID,sizeof(Vec3fa), 8);

    AffineSpace3fa rotation = AffineSpace3fa::rotate(Vec3fa(0,0,0),Vec3fa(0,1,0),2.0f*float(pi)*(float)t/(float)(num_time_steps-1));
    AffineSpace3fa scale = AffineSpace3fa::scale(Vec3fa(2.0f,1.0f,1.0f));

    for (int i=0; i<8; i++) {
      Vec3fa v = Vec3fa(cube_vertices[i][0],cube_vertices[i][1],cube_vertices[i][2]);
      vertices[i] = Vec3fa(xfmPoint(rotation*scale,v)+pos);
    }
  }

  rtcCommitGeometry(geom);
  unsigned int geomID = rtcAttachGeometry(scene,geom);
  rtcReleaseGeometry(geom);
  return geomID;
}

/* adds a subdivision cube to the scene */
unsigned int addSubdivCube (RTCScene scene, const Vec3fa& pos, unsigned int num_time_steps)
{
  /* create a triangulated cube with 6 quads and 8 vertices */
  RTCGeometry geom = rtcNewSubdivisionMesh(g_device, RTC_GEOMETRY_STATIC, num_time_steps);

  //rtcSetBuffer(geom, RTC_VERTEX_BUFFER, cube_vertices,  0, sizeof(Vec3fa  ), 8);
  rtcSetBuffer(geom, RTC_INDEX_BUFFER,  cube_quad_indices, 0, sizeof(unsigned int), NUM_INDICES);
  rtcSetBuffer(geom, RTC_FACE_BUFFER,   cube_quad_faces,0, sizeof(unsigned int), NUM_FACES);

  rtcSetBuffer(geom, RTC_EDGE_CREASE_INDEX_BUFFER,   cube_edge_crease_indices,  0, 2*sizeof(unsigned int), 0);
  rtcSetBuffer(geom, RTC_EDGE_CREASE_WEIGHT_BUFFER,  cube_edge_crease_weights,  0, sizeof(float), 0);

  rtcSetBuffer(geom, RTC_VERTEX_CREASE_INDEX_BUFFER, cube_vertex_crease_indices,0, sizeof(unsigned int), 0);
  rtcSetBuffer(geom, RTC_VERTEX_CREASE_WEIGHT_BUFFER,cube_vertex_crease_weights,0, sizeof(float), 0);

  float* level = (float*) rtcNewBuffer(geom, RTC_LEVEL_BUFFER, sizeof(float), NUM_INDICES);
  for (size_t i=0; i<NUM_INDICES; i++) level[i] = 16.0f;

  for (size_t t=0; t<num_time_steps; t++)
  {
    RTCBufferType bufID = RTC_VERTEX_BUFFER_(t);
    Vec3fa* vertices = (Vec3fa*) rtcNewBuffer(geom,bufID,sizeof(Vec3fa),8);

    AffineSpace3fa rotation = AffineSpace3fa::rotate(Vec3fa(0,0,0),Vec3fa(0,1,0),2.0f*float(pi)*(float)t/(float)(num_time_steps-1));
    AffineSpace3fa scale = AffineSpace3fa::scale(Vec3fa(2.0f,1.0f,1.0f));

    for (int i=0; i<8; i++) {
      Vec3fa v = Vec3fa(cube_vertices[i][0],cube_vertices[i][1],cube_vertices[i][2]);
      vertices[i] = Vec3fa(xfmPoint(rotation*scale,v)+pos);
    }
  }

  rtcCommitGeometry(geom);
  unsigned int geomID = rtcAttachGeometry(scene,geom);
  rtcReleaseGeometry(geom);
  return geomID;
}

/* add hair geometry */
unsigned int addCurve (RTCScene scene, const Vec3fa& pos, RTCCurveType type, unsigned int num_time_steps)
{
  RTCGeometry geom = rtcNewCurveGeometry(g_device, RTC_GEOMETRY_STATIC, type, RTC_BASIS_BSPLINE, num_time_steps);
  rtcSetTessellationRate (geom,16.0f);

  Vec3fa* bspline = (Vec3fa*) alignedMalloc(16*sizeof(Vec3fa));
  for (int i=0; i<16; i++) {
    float f = (float)(i)/16.0f;
    bspline[i] = Vec3fa(2.0f*f-1.0f,sin(12.0f*f),cos(12.0f*f));
  }

  for (size_t t=0; t<num_time_steps; t++)
  {
    RTCBufferType bufID = RTC_VERTEX_BUFFER_(t);
    Vec3fa* vertices = (Vec3fa*) rtcNewBuffer(geom,bufID,sizeof(Vec3fa),16);

    AffineSpace3fa rotation = AffineSpace3fa::rotate(Vec3fa(0,0,0),Vec3fa(0,1,0),2.0f*float(pi)*(float)t/(float)(num_time_steps-1));
    AffineSpace3fa scale = AffineSpace3fa::scale(Vec3fa(2.0f,1.0f,1.0f));

    for (int i=0; i<16; i++)
      vertices[i] = Vec3fa(xfmPoint(rotation*scale,bspline[i])+pos,0.2f);
  }

  int* indices = (int*) rtcNewBuffer(geom,RTC_INDEX_BUFFER,sizeof(int),13);
  for (int i=0; i<13; i++) indices[i] = i;

  alignedFree(bspline);

  rtcCommitGeometry(geom);
  unsigned int geomID = rtcAttachGeometry(scene,geom);
  rtcReleaseGeometry(geom);
  return geomID;
}

/* add line geometry */
unsigned int addLines (RTCScene scene, const Vec3fa& pos, unsigned int num_time_steps)
{
  RTCGeometry geom = rtcNewCurveGeometry (g_device, RTC_GEOMETRY_STATIC, RTC_CURVE_RIBBON, RTC_BASIS_LINEAR, num_time_steps);

  Vec3fa* bspline = (Vec3fa*) alignedMalloc(16*sizeof(Vec3fa));
  for (int i=0; i<16; i++) {
    float f = (float)(i)/16.0f;
    bspline[i] = Vec3fa(2.0f*f-1.0f,sin(12.0f*f),cos(12.0f*f));
  }

  for (size_t t=0; t<num_time_steps; t++)
  {
    RTCBufferType bufID = RTC_VERTEX_BUFFER_(t);
    Vec3fa* vertices = (Vec3fa*) rtcNewBuffer(geom,bufID,sizeof(Vec3fa),16);

    AffineSpace3fa rotation = AffineSpace3fa::rotate(Vec3fa(0,0,0),Vec3fa(0,1,0),2.0f*float(pi)*(float)t/(float)(num_time_steps-1));
    AffineSpace3fa scale = AffineSpace3fa::scale(Vec3fa(2.0f,1.0f,1.0f));

    for (int i=0; i<16; i++)
      vertices[i] = Vec3fa(xfmPoint(rotation*scale,bspline[i])+pos,0.2f);
  }

  int* indices = (int*) rtcNewBuffer(geom,RTC_INDEX_BUFFER,sizeof(int),15);
  for (int i=0; i<15; i++) indices[i] = i;

  alignedFree(bspline);

  rtcCommitGeometry(geom);
  unsigned int geomID = rtcAttachGeometry(scene,geom);
  rtcReleaseGeometry(geom);
  return geomID;
}

/* adds an instanced triangle cube to the scene, rotate instance */
RTCScene addInstancedTriangleCube (RTCScene global_scene, const Vec3fa& pos, unsigned int num_time_steps)
{
  RTCScene scene = rtcDeviceNewScene(g_device);
  RTCGeometry geom = rtcNewTriangleMesh (g_device, RTC_GEOMETRY_STATIC, 1);
  rtcSetBuffer(geom, RTC_INDEX_BUFFER,  cube_triangle_indices , 0, 3*sizeof(unsigned int), 12);
  rtcSetBuffer(geom, RTC_VERTEX_BUFFER, cube_vertices, 0, 4*sizeof(float), 8);
  rtcCommitGeometry(geom);
  rtcAttachGeometry(scene,geom);
  rtcReleaseGeometry(geom);
  rtcCommit(scene);

  RTCGeometry inst = rtcNewInstance(g_device,scene,num_time_steps);
  
  for (size_t t=0; t<num_time_steps; t++)
  {
    AffineSpace3fa rotation = AffineSpace3fa::rotate(Vec3fa(0,0,0),Vec3fa(0,1,0),2.0f*float(pi)*(float)t/(float)(num_time_steps-1));
    AffineSpace3fa scale = AffineSpace3fa::scale(Vec3fa(2.0f,1.0f,1.0f));
    AffineSpace3fa translation = AffineSpace3fa::translate(pos);
    AffineSpace3fa xfm = translation*rotation*scale;
    rtcSetTransform(inst,RTC_MATRIX_COLUMN_MAJOR_ALIGNED16,(float*)&xfm,t);
  }

  rtcCommitGeometry(inst);
  rtcAttachGeometry(global_scene,inst);
  rtcReleaseGeometry(inst);
  return scene;
}

/* adds an instanced quad cube to the scene, rotate instance and geometry */
RTCScene addInstancedQuadCube (RTCScene global_scene, const Vec3fa& pos, unsigned int num_time_steps)
{
  RTCScene scene = rtcDeviceNewScene(g_device);
  RTCGeometry geom = rtcNewQuadMesh (g_device, RTC_GEOMETRY_STATIC, num_time_steps);
  rtcSetBuffer(geom, RTC_INDEX_BUFFER,  cube_quad_indices , 0, 4*sizeof(unsigned int), 6);

  for (size_t t=0; t<num_time_steps; t++)
  {
    RTCBufferType bufID = RTC_VERTEX_BUFFER_(t);
    Vec3fa* vertices = (Vec3fa*) rtcNewBuffer(geom,bufID,sizeof(Vec3fa),8);

    AffineSpace3fa rotation = AffineSpace3fa::rotate(Vec3fa(0,0,0),Vec3fa(0,1,0),0.5f*2.0f*float(pi)*(float)t/(float)(num_time_steps-1));
    AffineSpace3fa scale = AffineSpace3fa::scale(Vec3fa(2.0f,1.0f,1.0f));

    for (int i=0; i<8; i++) {
      Vec3fa v = Vec3fa(cube_vertices[i][0],cube_vertices[i][1],cube_vertices[i][2]);
      vertices[i] = Vec3fa(xfmPoint(rotation*scale,v));
    }
  }

  rtcCommitGeometry(geom);
  rtcAttachGeometry(scene,geom);
  rtcReleaseGeometry(geom);  
  rtcCommit(scene);

  RTCGeometry inst = rtcNewInstance(g_device,scene,num_time_steps);

  for (size_t t=0; t<num_time_steps; t++)
  {
    AffineSpace3fa rotation = AffineSpace3fa::rotate(Vec3fa(0,0,0),Vec3fa(0,1,0),0.5f*2.0f*float(pi)*(float)t/(float)(num_time_steps-1));
    AffineSpace3fa translation = AffineSpace3fa::translate(pos);
    AffineSpace3fa xfm = translation*rotation;
    rtcSetTransform(inst,RTC_MATRIX_COLUMN_MAJOR_ALIGNED16,(float*)&xfm,t);
  }

  rtcCommitGeometry(inst);
  rtcAttachGeometry(global_scene,inst);
  rtcReleaseGeometry(inst);
  return scene;
}

// ======================================================================== //
//                     User defined sphere geometry                         //
// ======================================================================== //

struct Sphere
{
  ALIGNED_STRUCT
  Vec3fa p;                      //!< position of the sphere
  float r;                      //!< radius of the sphere
  unsigned int geomID;
  unsigned int num_time_steps;
};

void sphereBoundsFunc(const struct RTCBoundsFunctionArguments* const args)
{
  const Sphere* spheres = (const Sphere*) args->geomUserPtr;
  RTCBounds* bounds_o = args->bounds_o;
  const unsigned int time = args->time;
  const Sphere& sphere = spheres[args->item];
  float ft = 2.0f*float(pi) * (float) time / (float) (sphere.num_time_steps-1);
  Vec3fa p = sphere.p + Vec3fa(cos(ft),0.0f,sin(ft));
  bounds_o->lower_x = p.x-sphere.r;
  bounds_o->lower_y = p.y-sphere.r;
  bounds_o->lower_z = p.z-sphere.r;
  bounds_o->upper_x = p.x+sphere.r;
  bounds_o->upper_y = p.y+sphere.r;
  bounds_o->upper_z = p.z+sphere.r;
}

void sphereIntersectFuncN(const RTCIntersectFunctionNArguments* const args)
{
  const int* valid = args->valid;
  void* ptr  = args->geomUserPtr;
  RTCRayN* rays = args->rays;
  unsigned int N = args->N;
  unsigned int item = args->item;
  assert(N == 1);
  const Sphere* spheres = (const Sphere*)ptr;
  const Sphere& sphere = spheres[item];

  if (!valid[0])
    return;
  
  Ray *ray = (Ray *)rays;
  
  const int time_segments = sphere.num_time_steps-1;
  const float time = ray->time*(float)(time_segments);
  const int itime = clamp((int)(floor(time)),(int)0,time_segments-1);
  const float ftime = time - (float)(itime);
  const float ft0 = 2.0f*float(pi) * (float) (itime+0) / (float) (sphere.num_time_steps-1);
  const float ft1 = 2.0f*float(pi) * (float) (itime+1) / (float) (sphere.num_time_steps-1);
  const Vec3fa p0 = sphere.p + Vec3fa(cos(ft0),0.0f,sin(ft0));
  const Vec3fa p1 = sphere.p + Vec3fa(cos(ft1),0.0f,sin(ft1));
  const Vec3fa sphere_p = (1.0f-ftime)*p0 + ftime*p1;
  
  const Vec3fa v = ray->org-sphere_p;
  const float A = dot(ray->dir,ray->dir);
  const float B = 2.0f*dot(v,ray->dir);
  const float C = dot(v,v) - sqr(sphere.r);
  const float D = B*B - 4.0f*A*C;
  if (D < 0.0f) return;
  const float Q = sqrt(D);
  const float rcpA = rcp(A);
  const float t0 = 0.5f*rcpA*(-B-Q);
  const float t1 = 0.5f*rcpA*(-B+Q);
  if ((ray->tnear < t0) & (t0 < ray->tfar)) {
    ray->u = 0.0f;
    ray->v = 0.0f;
    ray->tfar = t0;
    ray->geomID = sphere.geomID;
    ray->primID = (unsigned int) item;
    ray->Ng = ray->org+t0*ray->dir-sphere_p;
  }
  if ((ray->tnear < t1) & (t1 < ray->tfar)) {
    ray->u = 0.0f;
    ray->v = 0.0f;
    ray->tfar = t1;
    ray->geomID = sphere.geomID;
    ray->primID = (unsigned int) item;
    ray->Ng = ray->org+t1*ray->dir-sphere_p;
  }
}

void sphereOccludedFuncN(const RTCOccludedFunctionNArguments* const args)
{
  const int* valid = args->valid;
  void* ptr  = args->geomUserPtr;
  RTCRayN* rays = args->rays;
  unsigned int N = args->N;
  unsigned int item = args->item;
  assert(N == 1);
  const Sphere* spheres = (const Sphere*)ptr;
  const Sphere& sphere = spheres[item];

  if (!valid[0])
    return;
  
  Ray *ray = (Ray *)rays;
  const int time_segments = sphere.num_time_steps-1;
  const float time = ray->time*(float)(time_segments);
  const int itime = clamp((int)(floor(time)),(int)0,time_segments-1);
  const float ftime = time - (float)(itime);
  const float ft0 = 2.0f*float(pi) * (float) (itime+0) / (float) (sphere.num_time_steps-1);
  const float ft1 = 2.0f*float(pi) * (float) (itime+1) / (float) (sphere.num_time_steps-1);
  const Vec3fa p0 = sphere.p + Vec3fa(cos(ft0),0.0f,sin(ft0));
  const Vec3fa p1 = sphere.p + Vec3fa(cos(ft1),0.0f,sin(ft1));
  const Vec3fa sphere_p = (1.0f-ftime)*p0 + ftime*p1;
  
  const Vec3fa v = ray->org-sphere_p;
  const float A = dot(ray->dir,ray->dir);
  const float B = 2.0f*dot(v,ray->dir);
  const float C = dot(v,v) - sqr(sphere.r);
  const float D = B*B - 4.0f*A*C;
  if (D < 0.0f) return;
  const float Q = sqrt(D);
  const float rcpA = rcp(A);
  const float t0 = 0.5f*rcpA*(-B-Q);
  const float t1 = 0.5f*rcpA*(-B+Q);
  if ((ray->tnear < t0) & (t0 < ray->tfar)) {
    ray->geomID = 0;
  }
  if ((ray->tnear < t1) & (t1 < ray->tfar)) {
    ray->geomID = 0;
  }
}

Sphere* addUserGeometrySphere (RTCScene scene, const Vec3fa& p, float r, unsigned int num_time_steps)
{
  RTCGeometry geom = rtcNewUserGeometry(g_device,RTC_GEOMETRY_STATIC,1,num_time_steps);
  Sphere* sphere = (Sphere*) alignedMalloc(sizeof(Sphere));
  sphere->p = p;
  sphere->r = r;
  sphere->geomID = rtcAttachGeometry(scene,geom);
  sphere->num_time_steps = num_time_steps;
  rtcSetUserData(geom,sphere);
  rtcSetBoundsFunction(geom,sphereBoundsFunc,nullptr);
  rtcSetIntersectFunction(geom,sphereIntersectFuncN);
  rtcSetOccludedFunction (geom,sphereOccludedFuncN);
  rtcCommitGeometry(geom);
  rtcReleaseGeometry(geom);
  return sphere;
}

/* adds a ground plane to the scene */
unsigned int addGroundPlane (RTCScene scene)
{
  /* create a triangulated plane with 2 triangles and 4 vertices */
  RTCGeometry geom = rtcNewTriangleMesh (g_device, RTC_GEOMETRY_STATIC, 1);

  /* set vertices */
  Vertex* vertices = (Vertex*) rtcNewBuffer(geom,RTC_VERTEX_BUFFER,sizeof(Vertex),4);
  vertices[0].x = -10; vertices[0].y = -2; vertices[0].z = -10;
  vertices[1].x = -10; vertices[1].y = -2; vertices[1].z = +10;
  vertices[2].x = +10; vertices[2].y = -2; vertices[2].z = -10;
  vertices[3].x = +10; vertices[3].y = -2; vertices[3].z = +10;

  /* set triangles */
  Triangle* triangles = (Triangle*) rtcNewBuffer(geom,RTC_INDEX_BUFFER,sizeof(Triangle),2);
  triangles[0].v0 = 0; triangles[0].v1 = 2; triangles[0].v2 = 1;
  triangles[1].v0 = 1; triangles[1].v1 = 2; triangles[1].v2 = 3;

  rtcCommitGeometry(geom);
  unsigned int geomID = rtcAttachGeometry(scene,geom);
  rtcReleaseGeometry(geom);
  return geomID;
}

RTCScene scene0 = nullptr;
RTCScene scene1 = nullptr;
RTCScene scene2 = nullptr;
RTCScene scene3 = nullptr;
Sphere* sphere0 = nullptr;
Sphere* sphere1 = nullptr;

/* called by the C++ code for initialization */
extern "C" void device_init (char* cfg)
{
  /* initialize last seen camera */
  g_accu_vx = Vec3fa(0.0f);
  g_accu_vy = Vec3fa(0.0f);
  g_accu_vz = Vec3fa(0.0f);
  g_accu_p  = Vec3fa(0.0f);

  /* create new Embree device */
  g_device = rtcNewDevice(cfg);
  error_handler(nullptr,rtcDeviceGetError(g_device));

  /* set error handler */
  rtcDeviceSetErrorFunction(g_device,error_handler,nullptr);

  /* create scene */
  g_scene = rtcDeviceNewScene(g_device);

  /* add geometry to the scene */
  addTriangleCube(g_scene,Vec3fa(-5,1,-5),g_num_time_steps);
  addTriangleCube(g_scene,Vec3fa(-5,5,-5),g_num_time_steps2);

  addQuadCube    (g_scene,Vec3fa( 0,1,-5),g_num_time_steps);
  addQuadCube    (g_scene,Vec3fa( 0,5,-5),g_num_time_steps2);

  addSubdivCube  (g_scene,Vec3fa(+5,1,-5),g_num_time_steps);
  addSubdivCube  (g_scene,Vec3fa(+5,5,-5),g_num_time_steps2);

  addLines       (g_scene,Vec3fa(-5,1, 0),g_num_time_steps);
  addLines       (g_scene,Vec3fa(-5,5, 0),g_num_time_steps2);

  addCurve (g_scene,Vec3fa( 0,1, 0),RTC_CURVE_RIBBON,g_num_time_steps);
  addCurve (g_scene,Vec3fa( 0,5, 0),RTC_CURVE_RIBBON,g_num_time_steps2);

  addCurve (g_scene,Vec3fa(+5,1, 0),RTC_CURVE_SURFACE,g_num_time_steps);
  addCurve (g_scene,Vec3fa(+5,5, 0),RTC_CURVE_SURFACE,g_num_time_steps2);

  scene0 = addInstancedTriangleCube(g_scene,Vec3fa(-5,1,+5),g_num_time_steps);
  scene1 = addInstancedTriangleCube(g_scene,Vec3fa(-5,5,+5),g_num_time_steps2);

  scene2 = addInstancedQuadCube    (g_scene,Vec3fa( 0,1,+5),g_num_time_steps);
  scene3 = addInstancedQuadCube    (g_scene,Vec3fa( 0,5,+5),g_num_time_steps2);

  sphere0 = addUserGeometrySphere   (g_scene,Vec3fa(+5,1,+5),1.0f,g_num_time_steps);
  sphere1 = addUserGeometrySphere   (g_scene,Vec3fa(+5,5,+5),1.0f,g_num_time_steps2);

  addGroundPlane(g_scene);

  /* commit changes to scene */
  rtcCommit (g_scene);

  /* set start render mode */
  renderTile = renderTileStandard;
  key_pressed_handler = device_key_pressed_default;
}

int frameID = 50;

/* task that renders a single screen tile */
Vec3fa renderPixelStandard(float x, float y, const ISPCCamera& camera, RayStats& stats)
{
  RTCIntersectContext context;
  rtcInitIntersectionContext(&context);
  
  float time = abs((int)(0.01f*frameID) - 0.01f*frameID);
  if (g_time != -1) time = g_time;

  /* initialize ray */
  Ray ray;
  ray.org = Vec3fa(camera.xfm.p);
  ray.dir = Vec3fa(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz));
  ray.tnear = 0.0f;
  ray.tfar = inf;
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
  ray.primID = RTC_INVALID_GEOMETRY_ID;
  ray.instID = RTC_INVALID_GEOMETRY_ID;
  ray.mask = -1;
  ray.time = time;

  /* intersect ray with scene */
  rtcIntersect1(g_scene,&context,RTCRay_(ray));
  RayStats_addRay(stats);

  /* shade pixels */
  Vec3fa color = Vec3fa(0.0f);
  if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
  {
    Vec3fa diffuse = Vec3fa(0.5f,0.5f,0.5f);
    if (ray.instID == RTC_INVALID_GEOMETRY_ID)
      ray.instID = ray.geomID;
    switch (ray.instID / 2) {
    case 0: diffuse = face_colors[ray.primID]; break;
    case 1: diffuse = face_colors[2*ray.primID]; break;
    case 2: diffuse = face_colors[2*ray.primID]; break;

    case 3: diffuse = Vec3fa(0.5f,0.0f,0.0f); break;
    case 4: diffuse = Vec3fa(0.0f,0.5f,0.0f); break;
    case 5: diffuse = Vec3fa(0.0f,0.0f,0.5f); break;

    case 6: diffuse = face_colors[ray.primID]; break;
    case 7: diffuse = face_colors[2*ray.primID]; break;
    case 8: diffuse = Vec3fa(0.5f,0.5f,0.0f); break;
    default: diffuse = Vec3fa(0.5f,0.5f,0.5f); break;
    }
    color = color + diffuse*0.5f;
    Vec3fa lightDir = normalize(Vec3fa(-1,-4,-1));

    /* initialize shadow ray */
    Ray shadow;
    shadow.org = ray.org + ray.tfar*ray.dir;
    shadow.dir = neg(lightDir);
    shadow.tnear = 0.001f;
    shadow.tfar = inf;
    shadow.geomID = 1;
    shadow.primID = 0;
    shadow.instID = RTC_INVALID_GEOMETRY_ID;
    shadow.mask = -1;
    shadow.time = time;

    /* trace shadow ray */
    rtcOccluded1(g_scene,&context,RTCRay_(shadow));
    RayStats_addShadowRay(stats);

    /* add light contribution */
    if (shadow.geomID)
      color = color + diffuse*clamp(-dot(lightDir,normalize(ray.Ng)),0.0f,1.0f);
  }
  return color;
}

/* renders a single screen tile */
void renderTileStandard(int taskIndex,
                        int threadIndex,
                        int* pixels,
                        const unsigned int width,
                        const unsigned int height,
                        const float time,
                        const ISPCCamera& camera,
                        const int numTilesX,
                        const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y=y0; y<y1; y++) for (unsigned int x=x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelStandard((float)x,(float)y,camera,g_stats[threadIndex]);

    /* write color to framebuffer */
    Vec3fa accu_color = g_accu[y*width+x] + Vec3fa(color.x,color.y,color.z,1.0f); g_accu[y*width+x] = accu_color;
    float f = rcp(max(0.001f,accu_color.w));
    unsigned int r = (unsigned int) (255.0f * clamp(accu_color.x*f,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(accu_color.y*f,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(accu_color.z*f,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;
  }
}

/* task that renders a single screen tile */
void renderTileTask (int taskIndex, int threadIndex, int* pixels,
                         const unsigned int width,
                         const unsigned int height,
                         const float time,
                         const ISPCCamera& camera,
                         const int numTilesX,
                         const int numTilesY)
{
  renderTile(taskIndex,threadIndex,pixels,width,height,time,camera,numTilesX,numTilesY);
}

/* called by the C++ code to render */
extern "C" void device_render (int* pixels,
                           const unsigned int width,
                           const unsigned int height,
                           const float time,
                           const ISPCCamera& camera)
{
  /* create accumulator */
  if (g_accu_width != width || g_accu_height != height) {
    alignedFree(g_accu);
    g_accu = (Vec3fa*) alignedMalloc(width*height*sizeof(Vec3fa));
    g_accu_width = width;
    g_accu_height = height;
    for (size_t i=0; i<width*height; i++)
      g_accu[i] = Vec3fa(0.0f);
  }

  /* reset accumulator */
  bool camera_changed = g_changed; g_changed = false;
  camera_changed |= ne(g_accu_vx,camera.xfm.l.vx); g_accu_vx = camera.xfm.l.vx;
  camera_changed |= ne(g_accu_vy,camera.xfm.l.vy); g_accu_vy = camera.xfm.l.vy;
  camera_changed |= ne(g_accu_vz,camera.xfm.l.vz); g_accu_vz = camera.xfm.l.vz;
  camera_changed |= ne(g_accu_p, camera.xfm.p);    g_accu_p  = camera.xfm.p;
  //camera_changed = true;
  if (camera_changed) {
    g_accu_count=0;
    for (size_t i=0; i<width*height; i++)
      g_accu[i] = Vec3fa(0.0f);
  }

  /* render next frame */
  frameID++;
  const int numTilesX = (width +TILE_SIZE_X-1)/TILE_SIZE_X;
  const int numTilesY = (height+TILE_SIZE_Y-1)/TILE_SIZE_Y;
  parallel_for(size_t(0),size_t(numTilesX*numTilesY),[&](const range<size_t>& range) {
    const int threadIndex = (int)TaskScheduler::threadIndex();
    for (size_t i=range.begin(); i<range.end(); i++)
      renderTileTask((int)i,threadIndex,pixels,width,height,time,camera,numTilesX,numTilesY);
  }); 
}

/* called by the C++ code for cleanup */
extern "C" void device_cleanup ()
{
  alignedFree(sphere0); sphere0 = nullptr;
  alignedFree(sphere1); sphere1 = nullptr;
  rtcReleaseScene(scene0); scene0 = nullptr;
  rtcReleaseScene(scene1); scene1 = nullptr;
  rtcReleaseScene(scene2); scene2 = nullptr;
  rtcReleaseScene(scene3); scene3 = nullptr;
  rtcReleaseScene (g_scene); g_scene = nullptr;
  rtcDeleteDevice(g_device); g_device = nullptr;
  alignedFree(g_accu); g_accu = nullptr;
  g_accu_width = 0;
  g_accu_height = 0;
  g_accu_count = 0;
}

} // namespace embree
