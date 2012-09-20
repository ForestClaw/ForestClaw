      subroutine mapc2m(xc,yc,xp,yp,zp)
      implicit none

      double precision xc,yc,xc1,yc1,xp,yp,zp
      logical isdisk, iscart, issphere

      xc1 = 2*xc - 1.d0
      yc1 = 2*yc - 1.d0

      if (iscart()) then
         call mapc2m_cart(xc1,yc1,xp,yp,zp)
         xp = 0.5*xp
         yp = 0.5*yp
         zp = 0.5*zp
      elseif (isdisk()) then
         call mapc2m_disk(xc1,yc1,xp,yp,zp)
      elseif (issphere()) then
         call mapc2m_sphere(xc1,yc1,xp,yp,zp)
      endif

      end
