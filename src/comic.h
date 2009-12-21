#ifndef __TBO_COMIC__
#define __TBO_COMIC__

#include <gtk/gtk.h>
#include "tbo-types.h"

Comic *tbo_comic_new (const char *title, int width, int height);
void tbo_comic_free (Comic *comic);
Page *tbo_comic_new_page (Comic *comic);
void tbo_comic_del_page (Comic *comic, int nth);
int tbo_comic_len (Comic *comic);

#endif

