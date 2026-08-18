/**
 * keep_data.c
 *
 * This file provides explicit references to the embedded binary data symbols
 * to prevent --gc-sections from discarding the .rodata.video sections that
 * contain bottom_bar and clean_text overlay data.
 *
 * Without this, the linker would remove the unreferenced sections and the
 * binary data would be all zeros at runtime.
 */

#include <stdint.h>

/* External symbols defined by objcopy-embedded binary files */
extern const uint8_t _binary_bottom_bar_raw_start[];
extern const uint8_t _binary_bottom_bar_raw_end[];
extern const uint8_t _binary_clean_text_rgb_raw_start[];
extern const uint8_t _binary_clean_text_rgb_raw_end[];
extern const uint8_t _binary_clean_text_alpha_raw_start[];
extern const uint8_t _binary_clean_text_alpha_raw_end[];

/* Volatile pointers to prevent compiler optimization from removing the reference */
static const uint8_t * const volatile __attribute__((used)) _keep_bottom_bar = _binary_bottom_bar_raw_start;
static const uint8_t * const volatile __attribute__((used)) _keep_clean_text_rgb = _binary_clean_text_rgb_raw_start;
static const uint8_t * const volatile __attribute__((used)) _keep_clean_text_alpha = _binary_clean_text_alpha_raw_start;