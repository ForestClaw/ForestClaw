# provides imported targets P4EST::P4EST, ...
include(ExternalProject)
include(${CMAKE_CURRENT_LIST_DIR}/GitSubmodule.cmake)

set(p4est_external true CACHE BOOL "build p4est library" FORCE)

git_submodule("${PROJECT_SOURCE_DIR}/p4est")

# --- p4est externalProject
# this keeps project scopes totally separate, which avoids
# tricky to diagnose behaviors

if(NOT P4EST_ROOT)
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(P4EST_ROOT ${PROJECT_BINARY_DIR}/local CACHE PATH "default library root")
  else()
    set(P4EST_ROOT ${CMAKE_INSTALL_PREFIX})
  endif()
endif()

if(BUILD_SHARED_LIBS)
  set(P4EST_LIBRARIES ${P4EST_ROOT}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}p4est${CMAKE_SHARED_LIBRARY_SUFFIX})
else()
  set(P4EST_LIBRARIES ${P4EST_ROOT}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}p4est${CMAKE_STATIC_LIBRARY_SUFFIX})
endif()

set(P4EST_INCLUDE_DIRS ${P4EST_ROOT}/include)

ExternalProject_Add(P4EST
SOURCE_DIR ${PROJECT_SOURCE_DIR}/p4est
CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${P4EST_ROOT} -DCMAKE_BUILD_TYPE=Release -Dmpi:BOOL=${mpi} -Dopenmp:BOOL=${openmp} -DSC_ROOT:PATH=${SC_ROOT}
BUILD_BYPRODUCTS ${P4EST_LIBRARIES}
DEPENDS SC
)


ExternalProject_Get_property(P4EST SOURCE_DIR)

# FIXME: patch p4est with FindMPI.cmake
# (remove when p4est is updated with new FindMPI.cmaake)
ExternalProject_Add_Step(P4EST patch_find_mpi DEPENDEES patch
COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/Modules/FindMPI.cmake ${SOURCE_DIR}/cmake/Modules/)


# --- imported target

file(MAKE_DIRECTORY ${P4EST_INCLUDE_DIRS})
# avoid race condition

# this GLOBAL is required to be visible via other
# project's FetchContent of this project
add_library(P4EST::P4EST STATIC IMPORTED GLOBAL)
set_target_properties(P4EST::P4EST PROPERTIES
  IMPORTED_LOCATION ${P4EST_LIBRARIES}
  INTERFACE_INCLUDE_DIRECTORIES ${P4EST_INCLUDE_DIRS}
  INTERFACE_LINK_LIBRARIES $<LINK_ONLY:SC::SC>
)

add_dependencies(P4EST::P4EST P4EST)
