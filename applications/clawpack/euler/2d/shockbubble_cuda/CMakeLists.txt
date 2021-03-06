set(rp ${PROJECT_SOURCE_DIR}/clawpack/euler/2d/rp)

add_library(shockbubble_cuda_f OBJECT 
  fdisc.f 
  setprob.f 
  user_4.6/qinit.f 
  user_4.6/bc2.f 
  user_4.6/src2.f 
  user_4.6/tag4coarsening.f 
  user_4.6/tag4refinement.f 
  user_5.0/qinit.f90 
  user_5.0/setaux.f90 
  user_5.0/bc2.f90 
  user_5.0/src2.f90 
  user_5.0/tag4coarsening.f90 
  user_5.0/tag4refinement.f90 
	${rp}/clawpack46_rpn2_euler3.f
	${rp}/clawpack46_rpt2_euler3.f
  ${rp}/clawpack46_rpn2_euler4.f 
  ${rp}/clawpack46_rpt2_euler4.f 
  ${rp}/clawpack46_rpn2_euler5.f 
  ${rp}/clawpack46_rpt2_euler5.f 
  ${rp}/clawpack5_rpn2_euler4.f90 
  ${rp}/clawpack5_rpt2_euler4.f90 
  ${rp}/clawpack5_rpn2_euler5.f90 
  ${rp}/clawpack5_rpt2_euler5.f90
)

add_executable(shockbubble_cuda 
  shockbubble.cpp 
  shockbubble_options.c 
  shockbubble_user.cpp 
	user_cuda/rp2euler.cu
  $<TARGET_OBJECTS:shockbubble_cuda_f>
)

target_include_directories(shockbubble_cuda PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${rp}
)

target_link_libraries(shockbubble_cuda PRIVATE 
 FORESTCLAW::CLAWPACK4.6
 FORESTCLAW::CLAWPACK5
 FORESTCLAW::CUDACLAW
)