svg2ovl - export SVG layers from inkscape drawing to overlays (PDF)

Dependencies: libxml2, inkscape

Preparing presentations using inkscape

1) In inkscape prepare a drawing with layers. The labels of some layers should start
   with overlay specification (layers without overlay specification are ignored, and
   can be used e.g. to store reusable elements).

   Overlay specification is a comma-separated list of ranges, where a range is either a 
   number or two numbers connected with a single dash. 

   E.g. a layer with label (name)
   [3,5-6,8] A sample layer
   will be visible on overlays 3,5,6 and 8 (note there is no whitespace before '[')

2) Run svg2ovl drawing.svg template

   template is the output file name with a single block of '#' that is replaced with
   (zero padded) overlay number.

   svg2ovl finds minimum and maximum overaly number from the layers' specifications,
   and creates an overaly for each of them. The overlay is produced by setting the
   visibility of respective layers, and running inkscape, so the stacking order of 
   layers is preserved.

   E.g. calling 
   svg2ovl drawing.svg image-#.pdf
   on the image from above produces files image-3.pdf, image-4.pdf, ... , image-8.pdf


inkscape is called with parameter --export-area-drawing, so in each overlay, the bouning
box is the bounding box of the visible layers. It is advised to use a lowest layer named
e.g. "[1-5] background" containing a single rectangle to ensure the same bounding box to
all overlays.


