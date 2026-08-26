/* fonts.h - central font declarations for the project */
#ifndef __FONTS_H
#define __FONTS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Declare custom fonts here so sources can include a single header.
 * Add more LV_FONT_DECLARE(...) lines when adding fonts. */
LV_FONT_DECLARE(inter_regular_17);
LV_FONT_DECLARE(inter_regular_18);
LV_FONT_DECLARE(inter_bold_40);
LV_FONT_DECLARE(inter_bold_42);
LV_FONT_DECLARE(inter_bold_50);

#ifdef __cplusplus
}
#endif

#endif /* __FONTS_H */