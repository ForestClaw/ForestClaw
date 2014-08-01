      subroutine mapc2m(xc,yc,xp,yp,zp)
      implicit none

      double precision xc,yc,xc1,yc1,xp,yp,zp
      logical isdisk, iscart, issphere

      xc1 = 2*xc - 1.d0
      yc1 = 2*yc - 1.d0

      if (iscart()) then
         call mapc2m_cart(xc1,yc1,xp,yp,zp)
      elseif (isdisk()) then
         call mapc2m_disk(xc1,yc1,xp,yp,zp)
      elseif (issphere()) then
c         write(6,*) 'mapc2m (mapc2m.f) : this should be replaced with',
c     &         ' a cubed sphere mapping'
c         stop
         call mapc2m_sphere(xc1,yc1,xp,yp,zp)
      endif

      call scale_map(xp,yp,zp)
      call rotate_map(xp,yp,zp)

      end