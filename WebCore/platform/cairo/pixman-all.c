#include "config.h"

#if PLATFORM(CAIRO)

#include "pixman/src/fbcompose.c"
#include "pixman/src/fbedge.c"
#include "pixman/src/fbpict.c"
#include "pixman/src/fbtrap.c"
#include "pixman/src/icblt.c"
#include "pixman/src/icbltone.c"
#include "pixman/src/iccolor.c"
#include "pixman/src/icformat.c"
#include "pixman/src/icimage.c"
#include "pixman/src/icpixels.c"
#include "pixman/src/icrect.c"
#undef LaneCases1
#include "pixman/src/icstipple.c"
#include "pixman/src/ictransform.c"
#include "pixman/src/ictrap.c"
#include "pixman/src/ictri.c"
#undef Mask
#include "pixman/src/icutil.c"
#include "pixman/src/pixregion.c"
#include "pixman/src/renderedge.c"

#endif // PLATFORM(CAIRO)
