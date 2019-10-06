ax = 0;
bx = 1;
ay = 0;
by = 1;
s = 1e-2;
axis([ax-s bx+s ay-s by+s])
daspect([1 1 1]);
% axis off;

fprintf('%10s %12.4e\n','qmin',qmin);
fprintf('%10s %12.4e\n','qmax',qmax);

showpatchborders;
setpatchborderprops('linewidth',1);

cv = linspace(0,1,11);
drawcontourlines(cv);

if Frame == 0 
    if mq == 1
        % caxis([-1,1]*1e-3);
    end
elseif Frame == 1
    caxis([0,2]);
    % no color axis
elseif Frame == 2 
    if mq == 1
        caxis([0,2]);
    elseif mq == 3        
        caxis([0,2]);
    end
end

colormap(parula);
colorbar;

hold on;
% plot_stars();
hold off;

NoQuery = 0;
prt = false;
if (prt)
    hidepatchborders;
    if (ishandle(circle))
        delete(circle);
    end
    mx = 8;
    maxlevel = 7;
    dpi = 2^7;    % fix at 128
    
    eff_res = mx*2^maxlevel;
    figsize = (eff_res/dpi)*[1,1];
    prefix = 'plot';
    plot_tikz_fig(Frame,figsize,prefix,dpi)
end

if (length(amrdata) == 1)
    % We are only on a single grid
    figure(2);
    clf;    
    h = surf(xcenter,ycenter,q');
    set(h,'edgecolor','none');
    view(3);
    axis square;
%     set(gcf,'color','k');
%     set(gcf,'clipping','off');
%     axis off;
%     camlight;
end

xlabel('x','fontsize',16);
ylabel('y','fontsize',16);

figure(1);


shg

clear afterframe
