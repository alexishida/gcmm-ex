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
	UI_LIST_PREVIEW,
	UI_LIST_DEVICE_REMOVED
} ui_list_action;

/**
 * Show enabled menu. Returns item index, or -1 when user goes back.
 *
 * help is an optional note shown in the footer after the button hints. Pass
 * NULL for menus that need no extra explanation; the hints are always drawn.
 */
int UI_Menu(const char *title, const char *subtitle,
	const char *const items[], int item_count, int initial_selection,
	const char *help, bool allow_back);
/** Show menu with disabled choices that cannot be selected. */
int UI_MenuDisabled(const char *title, const char *subtitle,
	const char *const items[], const bool enabled[], int item_count,
	int initial_selection, const char *help, bool allow_back);
/** One row of an action menu. */
typedef struct {
	const char *label;
	bool enabled;		/**< Unavailable rows draw dimmed and cannot be focused. */
	bool destructive;	/**< Irreversible rows draw in the warning palette. */
	bool heading;		/**< Group label. Never focused, never returned. */
} ui_menu_item;

/**
 * Show grouped actions and return the chosen index, or -1 when the user goes
 * back. Rows are drawn in the order given and never paged, so keep the list
 * short enough to fit; UI_ACTION_MAX_ITEMS is the ceiling.
 */
int UI_ActionMenu(const char *title, const char *subtitle,
	const ui_menu_item items[], int item_count, int initial_selection,
	const char *note);
#define UI_ACTION_MAX_ITEMS 12

/** Show main task menu with current card, storage, and transfer state. */
int UI_HomeMenu(int initial_selection);
/**
 * Show paged saves, update selection/marks, and return requested action.
 *
 * marked enables the selection boxes; pass NULL for a plain list. show_preview
 * reserves the right-hand banner panel, so lists of files that carry no banner
 * pass false and use the full width for names.
 */
ui_list_action UI_SaveList(const char *title, const char *subtitle,
	u8 entries[][1024], int entry_count, int *selection, bool *marked,
	int card_slot, bool show_preview);
/** Supply banner data for the currently selected memory-card save. */
void UI_SetSavePreview(u16 banner_format, const u16 *rgb_banner,
	const u8 *ci_banner, const u16 *palette, const char *title, const char *source);
/** Clear the selected-save preview when no valid banner is available. */
void UI_ClearSavePreview(void);
/** Show standard confirmation prompt. */
bool UI_Confirm(const char *title, const char *message,
	const char *detail, const char *confirm_label);
/** Show destructive confirmation prompt with warning details. */
bool UI_ConfirmDestructive(const char *title, const char *message,
	const char *detail, const char *confirm_label);
/** Show a neutral notice until the user dismisses it. */
void UI_Message(const char *title, const char *message, const char *detail);
/** Show a completed operation. message states the outcome, detail refines it. */
void UI_MessageSuccess(const char *title, const char *message, const char *detail);
/** Show a failed operation. message states what went wrong, detail how to fix. */
void UI_MessageError(const char *title, const char *message, const char *detail);

/** Read-only summary of connected memory cards, storage, and the workflow. */
void UI_DeviceDetails(const char *card_a, const char *card_b,
	const char *storage, const char *transfer);

/** Label and value pair shown in the save-details grid. */
typedef struct {
	const char *label;
	const char *value;
} ui_field;

/**
 * Show details for one save until the user dismisses them.
 *
 * The banner comes from the current preview state, so callers set it with
 * UI_SetSavePreview first. Fields fill a two-column grid in the order given.
 * Every string must outlive the call.
 */
void UI_SaveDetails(const char *subtitle, const char *title,
	const char *description, const char *filename,
	const ui_field fields[], int field_count);
/** Display application identity, credits, and license information. */
void UI_About(const char *author, const char *foundation);
/** Set labels used by transfer-progress screen; strings must outlive display. */
void UI_SetTransferState(const char *source, const char *destination);
/** Render transfer progress. current is clamped to zero through total. */
void UI_Progress(const char *title, const char *item, int current, int total);
/** Poll supported controllers for cancellation without blocking. */
bool UI_CancelRequested(void);
/** Show platform-specific controller help. */
void UI_Help(void);
/**
 * Report a storage or card failure raised deep inside a workflow and wait for
 * the user. Kept under its legacy name so storage modules call it unchanged.
 */
void WaitPrompt(char *msg);

#endif /* GCMM_UI_H */
