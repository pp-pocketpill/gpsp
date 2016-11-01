#ifndef CHIP_H
#define CHIP_H

void gpsp_plat_init(void);
void gpsp_plat_quit(void);

#define PLAT_BUTTON_COUNT 17
#define PLAT_MENU_BUTTON -1 // have one hardcoded
extern u32 button_plat_mask_to_config[PLAT_BUTTON_COUNT];

// TEST
extern void upscale_aspect(u16 *dst, u16 *src);

#endif	// CHIP_H
