/****************************************************************************
 * Task-oriented user interface
 ****************************************************************************/
#ifndef _GCMM_UI_H_
#define _GCMM_UI_H_

#include <gccore.h>
#include <stdbool.h>

typedef enum {
	UI_LIST_BACK = 0,
	UI_LIST_OPEN,
	UI_LIST_CONTEXT,
	UI_LIST_PREVIOUS_DEVICE,
	UI_LIST_NEXT_DEVICE
} ui_list_action;

int UI_Menu(const char *title, const char *subtitle,
	const char *const items[], int item_count, int initial_selection,
	const char *help, bool allow_back);
int UI_HomeMenu(const char *card_a, const char *card_b,
	const char *storage, int initial_selection);
ui_list_action UI_SaveList(const char *title, const char *subtitle,
	u8 entries[][1024], int entry_count, int *selection, bool *marked);
bool UI_Confirm(const char *title, const char *message,
	const char *detail, const char *confirm_label);
void UI_Message(const char *title, const char *message, const char *detail);
void UI_Details(const char *title, const char *const lines[], int line_count);
void UI_Progress(const char *title, const char *item, int current, int total);
void UI_Help(void);

#endif
