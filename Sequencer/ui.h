#ifndef UI_H
#define UI_H

#include "../drivers/ps2.h"

void ui_init(void);
void ui_handle_key(KEY_EVENT evt);
void ui_handle_mouse(MOUSE_PACKET pkt);

#endif /* UI_H */
