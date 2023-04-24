// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "rthwif_embree.h"
#include "rthwif_embree_builder.h"
#include "../common/scene.h"
#include "../builders/primrefgen.h"
#include "../rthwif/rtbuild/rthwif_builder.h"

#if defined(EMBREE_LEVEL_ZERO)
#include <level_zero/ze_api.h>
#endif

namespace embree
{
  using namespace embree::isa;

  enum Flags : uint32_t {
    NONE,
    DEPTH_TEST_LESS_EQUAL = 1 << 0  // when set we use <= for depth test, otherwise <
  };

  static void align(size_t& ofs, size_t alignment) {
    ofs = (ofs+(alignment-1))&(-alignment);
  }

  struct DispatchGlobals
  {
    uint64_t rtMemBasePtr;               // base address of the allocated stack memory
    uint64_t callStackHandlerKSP;             // this is the KSP of the continuation handler that is invoked by BTD when the read KSP is 0
    uint32_t asyncStackSize;             // async-RT stack size in 64 byte blocks
    uint32_t numDSSRTStacks : 16;        // number of stacks per DSS
    uint32_t syncRayQueryCount : 4;      // number of ray queries in the sync-RT stack: 0-15 mapped to: 1-16
    unsigned _reserved_mbz : 12;
    uint32_t maxBVHLevels;               // the maximal number of supported instancing levels (0->8, 1->1, 2->2, ...)
    Flags flags;                         // per context control flags
  };

  void* zeRaytracingInitExt(sycl::device device, sycl::context context)
  {
    ::zeRaytracingInitExt();
    
#if defined(EMBREE_SYCL_ALLOC_DISPATCH_GLOBALS)

#if !defined(EMBREE_LEVEL_ZERO)
#  error "Level Zero required to properly allocate RTDispatchGlobals"
#endif
    
    size_t maxBVHLevels = RTC_MAX_INSTANCE_LEVEL_COUNT+1;

    size_t rtstack_bytes = (64+maxBVHLevels*(64+32)+63)&-64;
    size_t num_rtstacks = 1<<17; // this is sufficiently large also for PVC
    size_t dispatchGlobalSize = 128+num_rtstacks*rtstack_bytes;
    
    void* dispatchGlobalsPtr = rthwifAllocAccelBuffer(dispatchGlobalSize,device,context);
    memset(dispatchGlobalsPtr, 0, dispatchGlobalSize);

    DispatchGlobals* dg = (DispatchGlobals*) dispatchGlobalsPtr;
    dg->rtMemBasePtr = (uint64_t) dispatchGlobalsPtr + dispatchGlobalSize;
    dg->callStackHandlerKSP = 0;
    dg->asyncStackSize = 0;
    dg->numDSSRTStacks = 0;
    dg->syncRayQueryCount = 0;
    dg->_reserved_mbz = 0;
    dg->maxBVHLevels = maxBVHLevels;
    dg->flags = DEPTH_TEST_LESS_EQUAL;

    return dispatchGlobalsPtr;

#else

    return nullptr;

#endif
  }

  void rthwifCleanup(void* dispatchGlobalsPtr, sycl::context context)
  {
#if defined(EMBREE_SYCL_ALLOC_DISPATCH_GLOBALS)
    rthwifFreeAccelBuffer(dispatchGlobalsPtr, context);
#endif

    zeRaytracingExitExt();
  }

#if defined(EMBREE_LEVEL_ZERO)

  int rthwifIsSYCLDeviceSupported(const sycl::device& sycl_device)
  {
    /* disabling of device check through env variable */
    const char* disable_device_check = getenv("EMBREE_DISABLE_DEVICEID_CHECK");
    if (disable_device_check && strcmp(disable_device_check,"1") == 0)
      return 1;

    sycl::platform platform = sycl_device.get_platform();
    ze_driver_handle_t hDriver = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(platform);

    uint32_t count = 0;
    std::vector<ze_driver_extension_properties_t> extensions;
    ze_result_t result = zeDriverGetExtensionProperties(hDriver,&count,extensions.data());
    if (result != ZE_RESULT_SUCCESS) return -1;

    extensions.resize(count);
    result = zeDriverGetExtensionProperties(hDriver,&count,extensions.data());
    if (result != ZE_RESULT_SUCCESS) return -1;

    bool ze_extension_ray_tracing = false;
    for (uint32_t i=0; i<extensions.size(); i++)
    {
      //std::cout << extensions[i].name << " version " << extensions[i].version << std::endl;
      
      if (strncmp("ZE_extension_raytracing",extensions[i].name,sizeof(extensions[i].name)))
        continue;
      
      ze_extension_ray_tracing = true; //extensions[i].version >= ZE_RAYTRACING_EXT_VERSION_1_0;
      break;
    }
    if (!ze_extension_ray_tracing)
      return -1;
  
#if 0

    /* check if GPU device is supported */
    ze_device_handle_t hDevice = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(sycl_device);

    /* check if ray tracing hardware is supported */
    ze_device_raytracing_ext_properties_t raytracing_properties;
    memset(&raytracing_properties,0,sizeof(raytracing_properties));
    raytracing_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_RAYTRACING_EXT_PROPERTIES;
    raytracing_properties.pNext = nullptr;
    
    ze_device_module_properties_t module_properties;
    memset(&module_properties,0,sizeof(module_properties));
    module_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES;
    module_properties.pNext = &raytracing_properties;
      
    ze_result_t result = zeDeviceGetModuleProperties(hDevice, &module_properties);
    if (result != ZE_RESULT_SUCCESS) return -1;

    const bool rayQuerySupported = raytracing_properties.flags & ZE_DEVICE_RAYTRACING_EXT_FLAG_RAYQUERY;
    if (!rayQuerySupported)
      return -1;
#endif

    return sycl_device.get_info<sycl::info::device::max_compute_units>();
  }

#else

  int rthwifIsSYCLDeviceSupported(const sycl::device& device)
  {
    // TODO: SYCL currently has no functionality to check if a GPU has RTHW
    // capabilities. Therefore, we return true when the device is a GPU,
    // the backend is level_zero, and the GPU has 8 threads per EU because
    // that indicates raytracing hardware.
    uint32_t threadsPerEU = 0;
    if (device.has(sycl::aspect::ext_intel_gpu_hw_threads_per_eu)) {
      threadsPerEU = device.get_info<sycl::ext::intel::info::device::gpu_hw_threads_per_eu>();
    }
    sycl::platform platform = device.get_platform();
    if(!device.is_gpu() || (threadsPerEU < 8) || (platform.get_info<sycl::info::platform::name>() != "Intel(R) Level-Zero"))
      return -1;
    else
      return device.get_info<sycl::info::device::max_compute_units>();
  }

#endif

#if defined(EMBREE_LEVEL_ZERO)

  void* rthwifAllocAccelBuffer(size_t bytes, sycl::device device, sycl::context context)
  {
    ze_context_handle_t hContext = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(context);
    ze_device_handle_t  hDevice  = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(device);

    ze_raytracing_mem_alloc_ext_desc_t rt_desc;
    rt_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_RAYTRACING_EXT_PROPERTIES;
    rt_desc.pNext = nullptr;
    rt_desc.flags = 0;

    ze_relaxed_allocation_limits_exp_desc_t relaxed;
    relaxed.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxed.pNext = &rt_desc;
    relaxed.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    
    ze_device_mem_alloc_desc_t device_desc;
    device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    device_desc.pNext = &relaxed;
    device_desc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_CACHED;
    device_desc.ordinal = 0;
  
    ze_host_mem_alloc_desc_t host_desc;
    host_desc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    host_desc.pNext = nullptr;
    host_desc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_CACHED;
    
    void* ptr = nullptr;
    ze_result_t result = zeMemAllocShared(hContext,&device_desc,&host_desc,bytes,ZE_RAYTRACING_ACCELERATION_STRUCTURE_ALIGNMENT_EXT,hDevice,&ptr);
    if (result != ZE_RESULT_SUCCESS)
      throw_RTCError(RTC_ERROR_OUT_OF_MEMORY,"BVH memory allocation failed");
    _unused(result);
    return ptr;
  }
  
  void rthwifFreeAccelBuffer(void* ptr, sycl::context context)
  {
    if (ptr == nullptr) return;
    ze_context_handle_t hContext = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(context);
    ze_result_t result = zeMemFree(hContext,ptr);
    assert(result == ZE_RESULT_SUCCESS);
    _unused(result);
  }

#else

  void* rthwifAllocAccelBuffer(size_t bytes, sycl::device device, sycl::context context)
  {
    void* ptr = sycl::aligned_alloc_shared(ZE_RAYTRACING_ACCELERATION_STRUCTURE_ALIGNMENT_EXT, bytes, device, context);
    if (ptr == nullptr)
      throw_RTCError(RTC_ERROR_OUT_OF_MEMORY,"BVH memory allocation failed");
    return ptr;
  }

  void rthwifFreeAccelBuffer(void* ptr, sycl::context context)
  {
    if (ptr == nullptr) return;
    sycl::free(ptr, context);
  }

#endif

  struct GEOMETRY_INSTANCE_DESC : ze_raytracing_geometry_instance_ext_desc_t
  {
    ze_raytracing_transform_float3x4_aligned_column_major_ext_t xfmdata;
  };

  struct GEOMETRY_TYPE
  {
    GEOMETRY_TYPE(ze_raytracing_geometry_type_ext_t type, size_t extraBytes = 0)
      : type(type), extraBytes(extraBytes) {}
    
    ze_raytracing_geometry_type_ext_t type;
    size_t extraBytes;
  };

  size_t sizeof_RTHWIF_GEOMETRY(GEOMETRY_TYPE type)
  {
    switch (type.type) {
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_TRIANGLES  : return sizeof(ze_raytracing_geometry_triangles_ext_desc_t)+type.extraBytes;
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_QUADS      : return sizeof(ze_raytracing_geometry_quads_ext_desc_t)+type.extraBytes;
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR: return sizeof(ze_raytracing_geometry_aabbs_fptr_ext_desc_t)+type.extraBytes;
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_INSTANCE  : return sizeof(ze_raytracing_geometry_instance_ext_desc_t)+type.extraBytes;
    default: assert(false); return 0;
    }
  }

  size_t alignof_RTHWIF_GEOMETRY(GEOMETRY_TYPE type)
  {
    switch (type.type) {
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_TRIANGLES  : return alignof(ze_raytracing_geometry_triangles_ext_desc_t);
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_QUADS      : return alignof(ze_raytracing_geometry_quads_ext_desc_t);
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR: return alignof(ze_raytracing_geometry_aabbs_fptr_ext_desc_t);
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_INSTANCE  : return alignof(ze_raytracing_geometry_instance_ext_desc_t);
    default: assert(false); return 0;
    }
  }

  ze_raytracing_geometry_ext_flag_t getGeometryFlags(Scene* scene, Geometry* geom)
  {
    /* invoke any hit callback when Embree filter functions are present */
    ze_raytracing_geometry_ext_flag_t gflags = ZE_RAYTRACING_GEOMETRY_EXT_FLAG_OPAQUE;
    if (geom->hasArgumentFilterFunctions() || geom->hasGeometryFilterFunctions())
      gflags = ZE_RAYTRACING_GEOMETRY_EXT_FLAG_NONE;
    
#if defined(EMBREE_RAY_MASK)
    /* invoke any hit callback when high mask bits are enabled */
    if (geom->mask & 0xFFFFFF80)
      gflags = ZE_RAYTRACING_GEOMETRY_EXT_FLAG_NONE;
#endif
    
    return gflags;
  }

  void createGeometryDesc(ze_raytracing_geometry_triangles_ext_desc_t* out, Scene* scene, TriangleMesh* geom)
  {
    memset(out,0,sizeof(ze_raytracing_geometry_triangles_ext_desc_t));
    out->geometryType = ZE_RAYTRACING_GEOMETRY_TYPE_EXT_TRIANGLES;
    out->geometryFlags = getGeometryFlags(scene,geom);
    out->geometryMask = mask32_to_mask8(geom->mask);
    out->triangleFormat = ZE_RAYTRACING_FORMAT_EXT_TRIANGLE_INDICES_UINT32;
    out->vertexFormat = ZE_RAYTRACING_FORMAT_EXT_FLOAT3;
    out->triangleBuffer = (ze_raytracing_triangle_indices_uint32_ext_t*) geom->triangles.getPtr();
    out->triangleCount = geom->triangles.size();
    out->triangleStride = geom->triangles.getStride();
    out->vertexBuffer = (ze_raytracing_float3_ext_t*) geom->vertices0.getPtr();
    out->vertexCount = geom->vertices0.size();
    out->vertexStride = geom->vertices0.getStride();
  }

  void createGeometryDesc(ze_raytracing_geometry_quads_ext_desc_t* out, Scene* scene, QuadMesh* geom)
  {
    memset(out,0,sizeof(ze_raytracing_geometry_quads_ext_desc_t));
    out->geometryType = ZE_RAYTRACING_GEOMETRY_TYPE_EXT_QUADS;
    out->geometryFlags = getGeometryFlags(scene,geom);
    out->geometryMask = mask32_to_mask8(geom->mask);
    out->quadFormat = ZE_RAYTRACING_FORMAT_EXT_QUAD_INDICES_UINT32;
    out->vertexFormat = ZE_RAYTRACING_FORMAT_EXT_FLOAT3;
    out->quadBuffer = (ze_raytracing_quad_indices_uint32_ext_t*) geom->quads.getPtr();
    out->quadCount = geom->quads.size();
    out->quadStride = geom->quads.getStride();
    out->vertexBuffer = (ze_raytracing_float3_ext_t*) geom->vertices0.getPtr();
    out->vertexCount = geom->vertices0.size();
    out->vertexStride = geom->vertices0.getStride();
  }

  void getProceduralAABB(const uint32_t primIDStart, const uint32_t primIDCount, void* geomUserPtr, void* buildUserPtr, ze_raytracing_aabb_ext_t* boundsOut)
  {
    BBox1f time_range = * (BBox1f*) buildUserPtr;
    Geometry* geom = (Geometry*) geomUserPtr;
      
    for (uint32_t i=0; i<primIDCount; i++)
    {
      const uint32_t primID = primIDStart+i;
      PrimRef prim;
      range<size_t> r(primID);
      size_t k = 0;
      uint32_t geomID = 0;

      PrimInfo pinfo = empty;
      if (geom->numTimeSegments() > 0)
        pinfo = geom->createPrimRefArrayMB(&prim,time_range,r,k,geomID);
      else
        pinfo = geom->createPrimRefArray(&prim,r,k,geomID);

      /* invalid primitive */
      if (pinfo.size() == 0) {
        boundsOut[i].lower.x = pos_inf;
        boundsOut[i].lower.y = pos_inf;
        boundsOut[i].lower.z = pos_inf;
        boundsOut[i].upper.x = neg_inf;
        boundsOut[i].upper.y = neg_inf;
        boundsOut[i].upper.z = neg_inf;
      }
      else
      {
        BBox3fa bounds = prim.bounds();
        boundsOut[i].lower.x = bounds.lower.x;
        boundsOut[i].lower.y = bounds.lower.y;
        boundsOut[i].lower.z = bounds.lower.z;
        boundsOut[i].upper.x = bounds.upper.x;
        boundsOut[i].upper.y = bounds.upper.y;
        boundsOut[i].upper.z = bounds.upper.z;
      }
    }
  };

  void createGeometryDescProcedural(ze_raytracing_geometry_aabbs_fptr_ext_desc_t* out, Scene* scene, Geometry* geom)
  {
    uint32_t numPrimitives = geom->size();
    if (GridMesh* mesh = dynamic_cast<GridMesh*>(geom))
      numPrimitives = mesh->getNumTotalQuads(); // FIXME: slow
    
    memset(out,0,sizeof(ze_raytracing_geometry_aabbs_fptr_ext_desc_t));
    out->geometryType = ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR;
    out->geometryFlags = ZE_RAYTRACING_GEOMETRY_EXT_FLAG_NONE;
    out->geometryMask = mask32_to_mask8(geom->mask);
    out->primCount = numPrimitives;
    out->getBounds = getProceduralAABB;
    out->geomUserPtr = geom;
  }
  
  void createGeometryDesc(GEOMETRY_INSTANCE_DESC* out, Scene* scene, Instance* geom)
  {
    assert(geom->gsubtype == AccelSet::GTY_SUBTYPE_INSTANCE_QUATERNION);
    memset(out,0,sizeof(GEOMETRY_INSTANCE_DESC));
    out->geometryType = ZE_RAYTRACING_GEOMETRY_TYPE_EXT_INSTANCE;
    out->instanceFlags = ZE_RAYTRACING_INSTANCE_EXT_FLAG_NONE;
    out->geometryMask = mask32_to_mask8(geom->mask);
    out->instanceUserID = 0;
    const AffineSpace3fa local2world = geom->getLocal2World();
    out->transformFormat = ZE_RAYTRACING_FORMAT_EXT_FLOAT3X4_ALIGNED_COLUMN_MAJOR;
    out->transform = (float*) &out->xfmdata;
    out->bounds = (ze_raytracing_aabb_ext_t*) &dynamic_cast<Scene*>(geom->object)->hwaccel_bounds;
    out->xfmdata = *(ze_raytracing_transform_float3x4_aligned_column_major_ext_t*) &local2world;
    EmbreeHWAccel* hwaccel = (EmbreeHWAccel*) dynamic_cast<Scene*>(geom->object)->hwaccel.data();
    out->accel = hwaccel->AccelTable[0];
  }

  void createGeometryDesc(ze_raytracing_geometry_instance_ext_desc_t* out, Scene* scene, Instance* geom)
  {
    assert(geom->gsubtype == AccelSet::GTY_SUBTYPE_DEFAULT);
    memset(out,0,sizeof(ze_raytracing_geometry_instance_ext_desc_t));
    out->geometryType = ZE_RAYTRACING_GEOMETRY_TYPE_EXT_INSTANCE;
    out->instanceFlags = ZE_RAYTRACING_INSTANCE_EXT_FLAG_NONE;
    out->geometryMask = mask32_to_mask8(geom->mask);
    out->instanceUserID = 0;
    out->transformFormat = ZE_RAYTRACING_FORMAT_EXT_FLOAT3X4_ALIGNED_COLUMN_MAJOR;
    out->transform = (float*) &geom->local2world[0];
    out->bounds = (ze_raytracing_aabb_ext_t*) &dynamic_cast<Scene*>(geom->object)->hwaccel_bounds;
    EmbreeHWAccel* hwaccel = (EmbreeHWAccel*) dynamic_cast<Scene*>(geom->object)->hwaccel.data();
    out->accel = hwaccel->AccelTable[0];
  }

  void createGeometryDesc(char* out, Scene* scene, Geometry* geom, GEOMETRY_TYPE type)
  {
    switch (type.type) {
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_TRIANGLES  : return createGeometryDesc((ze_raytracing_geometry_triangles_ext_desc_t*)out,scene,dynamic_cast<TriangleMesh*>(geom));
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_QUADS      : return createGeometryDesc((ze_raytracing_geometry_quads_ext_desc_t*)out,scene,dynamic_cast<QuadMesh*>(geom));
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR: return createGeometryDescProcedural((ze_raytracing_geometry_aabbs_fptr_ext_desc_t*)out,scene,geom);
    case ZE_RAYTRACING_GEOMETRY_TYPE_EXT_INSTANCE:
      if (type.extraBytes) return createGeometryDesc((GEOMETRY_INSTANCE_DESC*)out,scene,dynamic_cast<Instance*>(geom));
      else                 return createGeometryDesc((ze_raytracing_geometry_instance_ext_desc_t*)out,scene,dynamic_cast<Instance*>(geom));
    default: assert(false);
    }
  }

  ze_raytracing_build_quality_ext_t convertBuildQuality(RTCBuildQuality quality_flags)
  {
    switch (quality_flags) {
    case RTC_BUILD_QUALITY_LOW    : return ZE_RAYTRACING_BUILD_QUALITY_EXT_LOW;
    case RTC_BUILD_QUALITY_MEDIUM : return ZE_RAYTRACING_BUILD_QUALITY_EXT_MEDIUM;
    case RTC_BUILD_QUALITY_HIGH   : return ZE_RAYTRACING_BUILD_QUALITY_EXT_HIGH;
    case RTC_BUILD_QUALITY_REFIT  : return ZE_RAYTRACING_BUILD_QUALITY_EXT_LOW;
    default                       : return ZE_RAYTRACING_BUILD_QUALITY_EXT_MEDIUM;
    }
  }

  ze_raytracing_build_ext_flag_t convertBuildFlags(RTCSceneFlags scene_flags, RTCBuildQuality quality_flags)
  {
    uint32_t result = ZE_RAYTRACING_BUILD_EXT_FLAG_NONE;
    if (scene_flags & RTC_SCENE_FLAG_COMPACT) result |= ZE_RAYTRACING_BUILD_EXT_FLAG_COMPACT;

    /* only in high quality build mode spatial splits are allowed in Embree */
    if (quality_flags != RTC_BUILD_QUALITY_HIGH)
      result |= ZE_RAYTRACING_BUILD_EXT_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;

    return (ze_raytracing_build_ext_flag_t) result;
  }  

  BBox3f rthwifBuild(Scene* scene, AccelBuffer& accel)
  {
    DeviceGPU* gpuDevice = dynamic_cast<DeviceGPU*>(scene->device);
    if (gpuDevice == nullptr) throw std::runtime_error("internal error");
    
    sycl::device device = gpuDevice->getGPUDevice();
    ze_device_handle_t hDevice = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(device);
    sycl::platform platform = device.get_platform();
    ze_driver_handle_t hDriver = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(platform);
    
    /* create L0 builder object */
    ze_rtas_builder_exp_desc_t builderDesc = {};
    ze_rtas_builder_exp_handle_t hBuilder = nullptr;
    ze_result_t_ err = zeRTASBuilderCreateExp(hDriver, &builderDesc, &hBuilder);
    if (err != ZE_RESULT_SUCCESS_)
      throw std::runtime_error("ze_rtas_builder creation failed");
    
    auto getType = [&](unsigned int geomID) -> GEOMETRY_TYPE
    {
      /* no HW support for MB yet */
      if (scene->get(geomID)->numTimeSegments() > 0)
        return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR;

      switch (scene->get(geomID)->getType()) {
      case Geometry::GTY_FLAT_LINEAR_CURVE    : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ROUND_LINEAR_CURVE   : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ORIENTED_LINEAR_CURVE: return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_CONE_LINEAR_CURVE    : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      
      case Geometry::GTY_FLAT_BEZIER_CURVE    : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ROUND_BEZIER_CURVE   : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ORIENTED_BEZIER_CURVE: return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      
      case Geometry::GTY_FLAT_BSPLINE_CURVE    : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ROUND_BSPLINE_CURVE   : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ORIENTED_BSPLINE_CURVE: return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      
      case Geometry::GTY_FLAT_HERMITE_CURVE    : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ROUND_HERMITE_CURVE   : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ORIENTED_HERMITE_CURVE: return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      
      case Geometry::GTY_FLAT_CATMULL_ROM_CURVE    : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ROUND_CATMULL_ROM_CURVE   : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ORIENTED_CATMULL_ROM_CURVE: return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      
      case Geometry::GTY_TRIANGLE_MESH: return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_TRIANGLES; break;
      case Geometry::GTY_QUAD_MESH    : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_QUADS; break;
      case Geometry::GTY_GRID_MESH    : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_SUBDIV_MESH  : assert(false); return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      
      case Geometry::GTY_SPHERE_POINT       : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_DISC_POINT         : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_ORIENTED_DISC_POINT: return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      
      case Geometry::GTY_USER_GEOMETRY     : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;

#if RTC_MAX_INSTANCE_LEVEL_COUNT < 2
      case Geometry::GTY_INSTANCE_CHEAP    :
      case Geometry::GTY_INSTANCE_EXPENSIVE: {
        Instance* instance = scene->get<Instance>(geomID);
        EmbreeHWAccel* object = (EmbreeHWAccel*)((Scene*)instance->object)->hwaccel.data();
        if (object->numTimeSegments > 1) return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; // we need to handle instances in procedural mode if instanced scene has motion blur
        if (instance->mask & 0xFFFFFF80) return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; // we need to handle instances in procedural mode if high mask bits are set
        else if (instance->gsubtype == AccelSet::GTY_SUBTYPE_INSTANCE_QUATERNION)
          return GEOMETRY_TYPE(ZE_RAYTRACING_GEOMETRY_TYPE_EXT_INSTANCE,sizeof(GEOMETRY_INSTANCE_DESC)-sizeof(ze_raytracing_geometry_instance_ext_desc_t));
        else return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_INSTANCE;
      }
#else
      case Geometry::GTY_INSTANCE_CHEAP    : return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
      case Geometry::GTY_INSTANCE_EXPENSIVE: return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR; break;
#endif

      default: assert(false); return ZE_RAYTRACING_GEOMETRY_TYPE_EXT_AABBS_FPTR;
      }
    };

    /* calculate maximal number of motion blur time segments in scene */
    uint32_t maxTimeSegments = 1;
    for (size_t geomID=0; geomID<scene->size(); geomID++)
    {
      Geometry* geom = scene->get(geomID);
      if (geom == nullptr) continue;
      maxTimeSegments = std::max(maxTimeSegments, geom->numTimeSegments());
    }

    /* calculate size of geometry descriptor buffer */
    size_t totalBytes = 0;
    for (size_t geomID=0; geomID<scene->size(); geomID++)
    {
      Geometry* geom = scene->get(geomID);
      if (geom == nullptr) continue;
      
      const GEOMETRY_TYPE type = getType(geomID);
      align(totalBytes,alignof_RTHWIF_GEOMETRY(type));
      totalBytes += sizeof_RTHWIF_GEOMETRY(type);
    }

    /* fill geomdesc buffers */
    std::vector<ze_raytracing_geometry_ext_desc_t*> geomDescr(scene->size());
    std::vector<char> geomDescrData(totalBytes);

    size_t offset = 0;
    for (size_t geomID=0; geomID<scene->size(); geomID++)
    {
      geomDescr[geomID] = nullptr;     
      Geometry* geom = scene->get(geomID);
      if (geom == nullptr) continue;
      
      const GEOMETRY_TYPE type = getType(geomID);
      align(offset,alignof_RTHWIF_GEOMETRY(type));
      createGeometryDesc(&geomDescrData[offset],scene,scene->get(geomID),type);
      geomDescr[geomID] = (ze_raytracing_geometry_ext_desc_t*) &geomDescrData[offset];
      offset += sizeof_RTHWIF_GEOMETRY(type);
      assert(offset <= geomDescrData.size());
    }

    ze_raytracing_parallel_operation_ext_handle_t parallelOperation = nullptr;
    err = zeRaytracingParallelOperationCreateExt(hBuilder, &parallelOperation);
    if (err != ZE_RESULT_SUCCESS_)
      throw std::runtime_error("parallel operation creation failed");

    ze_raytracing_accel_format_ext_t accelFormat;
    err = zeRaytracingDeviceGetAccelFormatExt(hDevice, &accelFormat );
    if (err != ZE_RESULT_SUCCESS_)
      throw std::runtime_error("get accel format failed");

    /* estimate static accel size */
    BBox1f time_range(0,1);
    ze_raytracing_aabb_ext_t bounds;
    ze_raytracing_build_accel_ext_desc_t args;
    memset(&args,0,sizeof(args));
    args.stype = ZE_STRUCTURE_TYPE_RAYTRACING_BUILD_ACCEL_EXT_DESC;
    args.pNext = nullptr;
    args.accelFormat = accelFormat;
    args.geometries = (const ze_raytracing_geometry_ext_desc_t**) geomDescr.data();
    args.numGeometries = geomDescr.size();
    args.accelBuffer = nullptr;
    args.accelBufferBytes = 0;
    args.scratchBuffer = nullptr;
    args.scratchBufferBytes = 0;
    args.quality = convertBuildQuality(scene->quality_flags);
    args.flags = convertBuildFlags(scene->scene_flags,scene->quality_flags);
    args.parallelOperation = parallelOperation;
    args.boundsOut = &bounds;
    args.buildUserPtr = &time_range;
#if defined(EMBREE_SYCL_ALLOC_DISPATCH_GLOBALS)
    args.dispatchGlobalsPtr = dynamic_cast<DeviceGPU*>(scene->device)->dispatchGlobalsPtr;
#endif
    
    ze_raytracing_accel_size_ext_properties_t sizeTotal;
    memset(&sizeTotal,0,sizeof(ze_raytracing_accel_size_ext_properties_t));
    sizeTotal.stype = ZE_STRUCTURE_TYPE_RAYTRACING_ACCEL_SIZE_EXT_PROPERTIES;
    sizeTotal.pNext = nullptr;
    err = zeRaytracingGetAccelSizeExt(hBuilder,&args,&sizeTotal);
    if (err != ZE_RESULT_SUCCESS_)
      throw_RTCError(RTC_ERROR_UNKNOWN,"BVH size estimate failed");

    /* allocate scratch buffer */
    std::vector<char> scratchBuffer(sizeTotal.scratchBufferBytes);
    args.scratchBuffer = scratchBuffer.data();
    args.scratchBufferBytes = scratchBuffer.size();

    size_t headerBytes = sizeof(EmbreeHWAccel) + std::max(1u,maxTimeSegments)*8;
    align(headerBytes,128);

    /* build BVH */
    BBox3f fullBounds = empty;
    while (true)
    {
      /* estimate size of all mblur BVHs */
      ze_raytracing_accel_size_ext_properties_t size;
      size.accelBufferExpectedBytes  = maxTimeSegments*sizeTotal.accelBufferExpectedBytes;
      size.accelBufferWorstCaseBytes = maxTimeSegments*sizeTotal.accelBufferWorstCaseBytes;
      size_t bytes = headerBytes+size.accelBufferExpectedBytes;

      /* allocate BVH data */
      if (accel.size() < bytes) accel.resize(bytes);
      memset(accel.data(),0,accel.size()); // FIXME: not required

      /* build BVH for each time segment */
      for (uint32_t i=0; i<maxTimeSegments; i++)
      {
        const float t0 = float(i+0)/float(maxTimeSegments);
        const float t1 = float(i+1)/float(maxTimeSegments);
        time_range = BBox1f(t0,t1);
        
        args.geometries = (const ze_raytracing_geometry_ext_desc_t**) geomDescr.data();
        args.numGeometries = geomDescr.size();
        args.accelBuffer = accel.data() + headerBytes + i*sizeTotal.accelBufferExpectedBytes;
        args.accelBufferBytes = sizeTotal.accelBufferExpectedBytes;
        bounds = { { INFINITY, INFINITY, INFINITY }, { -INFINITY, -INFINITY, -INFINITY } };
        
        err = zeRaytracingBuildAccelExt(hBuilder,&args);
        if (args.parallelOperation)
        {
          assert(err == ZE_RESULT_RAYTRACING_EXT_OPERATION_DEFERRED);
          
          uint32_t maxThreads = 0;
          err = zeRaytracingParallelOperationGetMaxConcurrencyExt(parallelOperation,&maxThreads);
          if (err != ZE_RESULT_SUCCESS_)
            throw std::runtime_error("get max concurrency failed");
          
          parallel_for(maxThreads, [&](uint32_t) { err = zeRaytracingParallelOperationJoinExt(parallelOperation); });
        }
        
        fullBounds.extend(*(BBox3f*) args.boundsOut);

        if (err == ZE_RESULT_RAYTRACING_EXT_RETRY_BUILD_ACCEL)
        {
          if (sizeTotal.accelBufferExpectedBytes == sizeTotal.accelBufferWorstCaseBytes)
            throw_RTCError(RTC_ERROR_UNKNOWN,"build error");
          
          sizeTotal.accelBufferExpectedBytes = std::min(sizeTotal.accelBufferWorstCaseBytes,(size_t(1.2*sizeTotal.accelBufferExpectedBytes)+127)&-128);
          break;
        }
        
        if (err != ZE_RESULT_SUCCESS_) break;
      }
      if (err != ZE_RESULT_RAYTRACING_EXT_RETRY_BUILD_ACCEL) break;
    }

    if (err != ZE_RESULT_SUCCESS_)
      throw_RTCError(RTC_ERROR_UNKNOWN,"build error");

    /* destroy parallel operation */
    err = zeRaytracingParallelOperationDestroyExt(parallelOperation);
    if (err != ZE_RESULT_SUCCESS_)
      throw std::runtime_error("parallel operation destruction failed");

    /* destroy rtas builder again */
    err = zeRTASBuilderDestroyExp(hBuilder);
    if (err != ZE_RESULT_SUCCESS_)
      throw std::runtime_error("ze_rtas_builder destruction failed");
    
    EmbreeHWAccel* hwaccel = (EmbreeHWAccel*) accel.data();
    hwaccel->numTimeSegments = maxTimeSegments;

    for (size_t i=0; i<maxTimeSegments; i++)
      hwaccel->AccelTable[i] = (char*)hwaccel + headerBytes + i*sizeTotal.accelBufferExpectedBytes;

    return fullBounds;
  }
}
