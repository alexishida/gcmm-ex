/**
 * @file ui.h
 * @brief Task-oriented, controller-only UI shared by GameCube and Wii builds.
 *
 * UI owns menu rendering and input loops. Return values identify user intent;
 * workflow code remains responsible for storage and card operations.
 */
#ifndef GCMM_UI_H
#define GCMM_UI_H

#include <gccore.h>
#include <stdbool.h>

/** Action returned by UI_SaveList after its input loop finishes. */
typedef enum {
	UI_LIST_BACK = 0,
	UI_LIST_OPEN,
	UI_LIST_CONTEXT,
	UI_LIST_PREVIOUS_DEVICE,
	UI_LIST_NEXT_DEVICE,
	UI_LIST_DEVICE_REMOVED
} ui_list_action;

/** Show enabled menu. Returns item index, or -1 when user goes back. */
int UI_Menu(const char *title, const char *subtitle,
	const char *const items[], int item_count, int initial_selection,
	const char *help, bool allow_back);
/** Show menu with disabled choices that cannot be selected. */
int UI_MenuDisabled(const char *title, const char *subtitle,
	const char *const items[], const bool enabled[], int item_count,
	int initial_selection, const char *help, bool allow_back);
/** Show main task menu with current card, storage, and transfer state. */
int UI_HomeMenu(const char *card_a, const char *card_b,
	const char *storage, const char *transfer, int initial_selection);
/** Show paged saves, update selection/marks, and return requested action. */
ui_list_action UI_SaveList(const char *title, const char *subtitle,
	u8 entries[][1024], int entry_count, int *selection, bool *marked,
	int card_slot);
/** Show standard confirmation prompt. */
bool UI_Confirm(const char *title, const char *message,
	const char *detail, const char *confirm_label);
/** Show destructive confirmation prompt with deliberate confirmation flow. */
bool UI_ConfirmDestructive(const char *title, const char *message,
	const char *detail, const char *confirm_label);
/** Show message until user dismisses it. */
void UI_Message(const char *title, const char *message, const char *detail);
/** Display details until user dismisses them. */
void UI_Details(const char *title, const char *const lines[], int line_count);
/** Set labels used by transfer-progress screen; strings must outlive display. */
void UI_SetTransferState(const char *source, const char *destination);
/** Render transfer progress. current is clamped to zero through total. */
void UI_Progress(const char *title, const char *item, int current, int total);
/** Poll supported controllers for cancellation without blocking. */
bool UI_CancelRequested(void);
/** Show platform-specific controller help. */
void UI_Help(void);

#endif /* GCMM_UI_H */
