c
c
c
c     =================================================
      function fdisc(blockno,xc,yc)
c     =================================================
      implicit double precision (a-h,o-z)
      common/cdisc/ x0,y0,alf,beta,r0,idisc


      integer blockno
      integer*8 cont, get_context

c
c     # for computing cell averages for initial data that has a
c     # discontinuity along some curve.  fdisc should be negative to the
c     # left of the curve and positive to the right
c     # idisc specifies the nature of the discontinuity for two
c     # particular cases (a straight line and circle) but this routine
c     # can be modified for any other curve.
c

      cont = get_context()
      call fclaw2d_map_c2m(cont,blockno,xc,yc,x,y,z)

      go to (10,20) idisc
c
   10 continue
c     # straight line through (x0,y0) with normal (alf,beta) pointing
c     # into right state
c
      fdisc = (x-x0)*alf + (y-y0)*beta
      return
c
   20 continue
c     # circle of radius r0:
      fdisc = (x-x0)**2 + (y-y0)**2 - r0**2
c
      return
      end