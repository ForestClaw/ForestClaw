      subroutine clawpack46_tag4refinement(mx,my,mbc,
     &      meqn, xlower,ylower,dx,dy,blockno,
     &      q, tag_threshold, init_flag,tag_patch)
      implicit none

      integer mx,my, mbc, meqn, tag_patch, init_flag
      integer blockno
      double precision xlower, ylower, dx, dy
      double precision tag_threshold
      double precision q(1-mbc:mx+mbc,1-mbc:my+mbc,meqn)

      double precision pi
      common /compi/ pi

      integer i,j, mq
      double precision qmin, qmax
      double precision quad(-1:1,-1:1), xc, yc
      integer ii,jj

      logical exceeds_th, gradient_exceeds_th

      tag_patch = 0

c     # We check only internal cells, because there is a problem with the
c     # corner values on the cubed sphere. Possible bad values in (unused)
c     # corners at block corners are triggering refinement where three blocks meet.
      mq = 1
      qmin = q(1,1,mq)
      qmax = q(1,1,mq)
      do j = 1,my
         do i = 1,mx
             xc = xlower + (i-0.5)*dx
             yc = ylower + (j-0.5)*dy
             qmin = min(qmin,q(i,j,mq))
             qmax = max(qmax,q(i,j,mq))
              do ii = -1,1
                  do jj = -1,1
                        quad(ii,jj) = q(i+ii,j+jj,mq)
                  end do
              end do
              exceeds_th = gradient_exceeds_th(blockno,
     &                     q(i,j,mq),qmin,qmax,quad, dx,dy,xc,yc, 
     &                     tag_threshold)

            if (exceeds_th) then
               tag_patch = 1
               return
            endif
         enddo
      enddo

      end
