set(all ${PROJECT_SOURCE_DIR}/clawpack/advection/2d/all)

add_library(hemisphere_f OBJECT 
  setprob.f 
  fdisc.f 
  psi.f 
  user_4.6/qinit.f 
  user_5.0/qinit.f90 
  ${all}/user46_setaux_manifold.f 
  ${all}/user5_setaux_manifold.f90 
  ${all}/clawpack46_rpn2adv_manifold.f 
  ${all}/clawpack46_rpt2adv_manifold.f 
  ${all}/clawpack46_tag4refinement.f 
  ${all}/clawpack46_tag4coarsening.f 
  ${all}/clawpack5_rpn2adv_manifold.f90 
  ${all}/clawpack5_rpt2adv_manifold.f90 
  ${all}/clawpack5_tag4refinement.f 
  ${all}/clawpack5_tag4coarsening.f
)

add_executable(hemisphere 
  hemisphere.cpp 
  hemisphere_options.c 
  hemisphere_user.cpp 
  fclaw2d_map_pillowsphere.c 
  fclaw2d_map_pillowsphere5.c 
  $<TARGET_OBJECTS:hemisphere_f>
)

target_include_directories(hemisphere PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${all}
)

target_link_libraries(hemisphere PRIVATE 
 FORESTCLAW::CLAWPACK4.6
 FORESTCLAW::CLAWPACK5
)