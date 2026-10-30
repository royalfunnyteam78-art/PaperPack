set -e
run(){ ./harness "$@"; }
run TornPaperNote test.ppm o_torn.ppm 2.0 6
run InkEdgePhoto test.ppm o_ink.ppm 2.0 6
run CircleBubble test.ppm o_bub.ppm 2.0 6 pos=0.7,0.4
run PolaroidFrame test.ppm o_pol.ppm 2.0 6
run PaperSway test.ppm o_sway.ppm 1.3 6 amount=30
run AutoZoom test.ppm o_zoom.ppm 3.0 6 pandir=2
run SlideInOut test.ppm o_slide.ppm 0.25 6
ALPHAKEY=1 run StickerOutline cut.ppm o_stk.ppm 0 6 glowsize=4
run MarkerDraw page.ppm o_m0.ppm 1.5 6 style=0 pointa=0.12,0.36 pointb=0.6,0.36
run MarkerDraw page.ppm o_m1.ppm 1.5 6 style=1 color=1,0.92,0.1 pointa=0.12,0.30 pointb=0.7,0.30
run MarkerDraw page.ppm o_m2.ppm 1.5 6 style=2 pointa=0.1,0.2 pointb=0.6,0.5
run MarkerDraw page.ppm o_m3.ppm 1.5 6 style=3 pointa=0.55,0.5 pointb=0.9,0.8
run ArrowRoute test.ppm o_a0.ppm 2.0 6
run ArrowRoute test.ppm o_a2.ppm 2.0 6 style=2
