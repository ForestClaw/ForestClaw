## Makefile.am in applications/clawpack/transport/slotted_disk

bin_PROGRAMS += applications/clawpack/transport/slotted_disk/slotted_disk

include_HEADERS += applications/clawpack/transport/slotted_disk/slotted_disk_user.h

applications_clawpack_transport_slotted_disk_slotted_disk_SOURCES = \
	applications/clawpack/transport/slotted_disk/slotted_disk.cpp \
	applications/clawpack/transport/slotted_disk/slotted_disk_user.cpp \
	applications/clawpack/transport/slotted_disk/setprob.f \
	applications/clawpack/transport/slotted_disk/fdisc.f \
	applications/clawpack/transport/slotted_disk/slotteddisk.f \
	applications/clawpack/transport/slotted_disk/user_4.6/qinit.f \
	applications/clawpack/transport/slotted_disk/user_5.0/qinit.f90 \
	applications/clawpack/transport/slotted_disk/user_4.6/tag4refinement.f \
	applications/clawpack/transport/slotted_disk/user_5.0/tag4refinement.f \
	applications/clawpack/transport/slotted_disk/user_4.6/tag4coarsening.f \
	applications/clawpack/transport/slotted_disk/user_5.0/tag4coarsening.f \
	applications/clawpack/transport/all/transport_user.h \
	applications/clawpack/transport/all/velocity_wind.f \
	applications/clawpack/transport/all/gaussian.f \
	applications/clawpack/transport/all/transport_user.h \
	applications/clawpack/transport/all/transport_user.cpp \
	applications/clawpack/transport/all/transport_options.h \
	applications/clawpack/transport/all/transport_domain.c \
	applications/clawpack/transport/all/transport_options.c \
	applications/clawpack/transport/all/user46_b4step2_manifold.f \
	applications/clawpack/transport/all/user5_b4step2_manifold.f90 \
	applications/clawpack/transport/all/fclaw2d_map_cubedsphere.c \
	applications/clawpack/transport/all/fclaw2d_map_pillowsphere.c \
	applications/clawpack/advection/2d/all/user46_setaux_manifold.f \
	applications/clawpack/advection/2d/all/user5_setaux_manifold.f90 \
	applications/clawpack/advection/2d/all/clawpack46_rpn2adv_manifold.f \
	applications/clawpack/advection/2d/all/clawpack46_rpt2adv_manifold.f \
	applications/clawpack/advection/2d/all/clawpack5_rpn2adv_manifold.f90 \
	applications/clawpack/advection/2d/all/clawpack5_rpt2adv_manifold.f90

applications_clawpack_transport_slotted_disk_slotted_disk_CPPFLAGS = \
        $(AM_CPPFLAGS) $(FCLAW_CLAWPACK46_CPPFLAGS) $(FCLAW_CLAWPACK5_CPPFLAGS)

applications_clawpack_transport_slotted_disk_slotted_disk_LDADD = \
        $(FCLAW_CLAWPACK46_LDADD) $(FCLAW_CLAWPACK5_LDADD) $(LDADD)