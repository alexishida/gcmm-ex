/****************************************************************************
 * Task-oriented user interface
 *
 * Keeps navigation and controls consistent across every workflow. Drawing is
 * intentionally simple and overscan-safe for 480i/576i displays.
 ****************************************************************************/
/**
 * @file ui.c
 * @brief Task-oriented UI rendering and controller input state machine.
 *
 * UI owns controller loops and visual consistency. Workflow code receives only
 * user intent and keeps responsibility for card and storage side effects.
 */
#include <gccore.h>
#include <stdio.h>
#include <string.h>

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include "bitmap.h"
#include "freetype.h"
#include "ui.h"
#include "ui_logo_bmp.h"

/* Single GCMM-EX palette. Background artwork supplies UI_BACKGROUND. */
#define UI_BACKGROUND  getcolour(3, 12, 30)
#define UI_PANEL       getcolour(10, 20, 42)
#define UI_PANEL_ALT   getcolour(18, 34, 62)
#define UI_BORDER      getcolour(47, 92, 132)
#define UI_ACCENT      getcolour(45, 196, 196)
#define UI_SELECTED    getcolour(42, 76, 122)
#define UI_TEXT        234, 242, 255
#define UI_MUTED       153, 174, 204
#define UI_SUCCESS     getcolour(52, 181, 112)
#define UI_WARNING     getcolour(220, 165, 53)
#define UI_DESTRUCTIVE getcolour(186, 68, 78)
#define UI_DISABLED    122, 139, 166
#define UI_PAGE_SIZE   11

typedef enum {
	UI_KEY_NONE = 0,
	UI_KEY_UP,
	UI_KEY_DOWN,
	UI_KEY_LEFT,
	UI_KEY_RIGHT,
	UI_KEY_CONFIRM,
	UI_KEY_BACK,
	UI_KEY_CONTEXT,
	UI_KEY_MARK,
	UI_KEY_PREVIOUS,
	UI_KEY_NEXT,
	UI_KEY_HELP
} ui_key;

static bool ui_destructive_confirmation;
static const char *ui_transfer_source = "Not selected";
static const char *ui_transfer_destination = "Not selected";

#ifdef HW_RVL
extern bool power;
void PowerOff(void);
#endif

extern u32 retraceCount;
extern const char appversion[];

static void ui_wait_release(void)
{
	while (PAD_ButtonsHeld(0)
#ifdef HW_RVL
	       || WPAD_ButtonsHeld(0)
#endif
	) {
#ifdef HW_RVL
		if (power)
			PowerOff();
#endif
		VIDEO_WaitVSync();
	}
}

static ui_key ui_direction_from_masks(u32 pad
#ifdef HW_RVL
	, u32 remote
#endif
)
{
	if (pad & PAD_BUTTON_UP
#ifdef HW_RVL
	    || remote & (WPAD_BUTTON_UP | WPAD_CLASSIC_BUTTON_UP)
#endif
	)
		return UI_KEY_UP;
	if (pad & PAD_BUTTON_DOWN
#ifdef HW_RVL
	    || remote & (WPAD_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_DOWN)
#endif
	)
		return UI_KEY_DOWN;
	if (pad & PAD_BUTTON_LEFT
#ifdef HW_RVL
	    || remote & (WPAD_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_LEFT)
#endif
	)
		return UI_KEY_LEFT;
	if (pad & PAD_BUTTON_RIGHT
#ifdef HW_RVL
	    || remote & (WPAD_BUTTON_RIGHT | WPAD_CLASSIC_BUTTON_RIGHT)
#endif
	)
		return UI_KEY_RIGHT;
	return UI_KEY_NONE;
}

static ui_key ui_read_key(void)
{
	static bool stick_latched = false;
	static ui_key repeated_direction = UI_KEY_NONE;
	static u32 next_repeat;
	u32 pad = PAD_ButtonsDown(0);
	u32 held_pad = PAD_ButtonsHeld(0);
	s8 stick_x = PAD_StickX(0);
	s8 stick_y = PAD_StickY(0);
#ifdef HW_RVL
	u32 remote = WPAD_ButtonsDown(0);
	u32 held_remote = WPAD_ButtonsHeld(0);

	if (power)
		PowerOff();
#endif

	/* ButtonsDown lasts one retrace. Use held state for actions so short
	 * presses between UI redraws are not lost; callers release before reuse. */
	if ((pad | held_pad) & PAD_BUTTON_A
#ifdef HW_RVL
	    || (remote | held_remote) & (WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A)
#endif
	)
		return UI_KEY_CONFIRM;
	if ((pad | held_pad) & PAD_BUTTON_B
#ifdef HW_RVL
	    || (remote | held_remote) & (WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B)
#endif
	)
		return UI_KEY_BACK;
	if ((pad | held_pad) & PAD_BUTTON_X
#ifdef HW_RVL
	    || (remote | held_remote) & (WPAD_BUTTON_PLUS | WPAD_CLASSIC_BUTTON_X)
#endif
	)
		return UI_KEY_CONTEXT;
	if ((pad | held_pad) & PAD_BUTTON_Y
#ifdef HW_RVL
	    || (remote | held_remote) & (WPAD_BUTTON_MINUS | WPAD_CLASSIC_BUTTON_Y)
#endif
	)
		return UI_KEY_MARK;
	if ((pad | held_pad) & PAD_TRIGGER_L
#ifdef HW_RVL
	    || (remote | held_remote) & (WPAD_BUTTON_1 | WPAD_CLASSIC_BUTTON_FULL_L)
#endif
	)
		return UI_KEY_PREVIOUS;
	if ((pad | held_pad) & PAD_TRIGGER_R
#ifdef HW_RVL
	    || (remote | held_remote) & (WPAD_BUTTON_2 | WPAD_CLASSIC_BUTTON_FULL_R)
#endif
	)
		return UI_KEY_NEXT;
	if ((pad | held_pad) & PAD_BUTTON_START
#ifdef HW_RVL
	    || (remote | held_remote) & (WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_PLUS)
#endif
	)
		return UI_KEY_HELP;

	{
		ui_key direction = ui_direction_from_masks(pad
#ifdef HW_RVL
			, remote
#endif
		);
		if (direction != UI_KEY_NONE) {
			repeated_direction = direction;
			next_repeat = retraceCount + 18;
			return direction;
		}
	}

	{
		ui_key direction = ui_direction_from_masks(held_pad
#ifdef HW_RVL
			, held_remote
#endif
		);
		if (direction == UI_KEY_NONE) {
			repeated_direction = UI_KEY_NONE;
		} else if (direction != repeated_direction) {
			repeated_direction = direction;
			next_repeat = retraceCount + 18;
			return direction;
		} else if ((s32)(retraceCount - next_repeat) >= 0) {
			next_repeat = retraceCount + 4;
			return direction;
		}
	}

	if (stick_x > -35 && stick_x < 35 && stick_y > -35 && stick_y < 35)
		stick_latched = false;
	if (!stick_latched) {
		if (stick_y > 55) {
			stick_latched = true;
			return UI_KEY_UP;
		}
		if (stick_y < -55) {
			stick_latched = true;
			return UI_KEY_DOWN;
		}
		if (stick_x < -55) {
			stick_latched = true;
			return UI_KEY_LEFT;
		}
		if (stick_x > 55) {
			stick_latched = true;
			return UI_KEY_RIGHT;
		}
	}

	return UI_KEY_NONE;
}

static void ui_begin(const char *title, const char *subtitle)
{
	ClearScreen();
	DrawBoxFilled(24, 24, 615, 398, UI_PANEL);
	DrawBox(24, 24, 615, 398, UI_BORDER);
	DrawBoxFilled(24, 24, 615, 69, UI_PANEL_ALT);
	DrawBoxFilled(24, 68, 615, 70, UI_ACCENT);
	DrawBMPAt((u8 *)ui_logo_bmp, 482, 36);
	setfontsize(12);
	setfontcolour(UI_MUTED);
	DrawText(432, 55, (char *)appversion);

	setfontsize(20);
	setfontcolour(UI_TEXT);
	DrawText(42, 55, (char *)title);
	if (subtitle && subtitle[0]) {
		setfontsize(12);
		setfontcolour(UI_MUTED);
		DrawText(42, 91, (char *)subtitle);
	}
	setfontsize(14);
}

static void ui_footer(const char *help)
{
	writeStatusBar((char *)help, "START Help");
}

static int ui_marked_count(const bool *marked, int entry_count)
{
	int i;
	int count = 0;

	if (!marked || entry_count < 0 || entry_count > CARD_MAXFILES)
		return 0;
	for (i = 0; i < entry_count; i++)
		if (marked[i])
			count++;
	return count;
}

int UI_MenuDisabled(const char *title, const char *subtitle,
	const char *const items[], const bool enabled[], int item_count,
	int initial_selection, const char *help, bool allow_back)
{
	int selected = initial_selection;
	int first;
	int i;
	int enabled_count = 0;
	char page[32];
	ui_key key;

	if (item_count <= 0)
		return -1;
	for (i = 0; i < item_count; i++)
		if (!enabled || enabled[i])
			enabled_count++;
	if (!enabled_count)
		return -1;
	if (selected < 0 || selected >= item_count)
		selected = 0;
	while (enabled && !enabled[selected])
		selected = selected + 1 < item_count ? selected + 1 : 0;

	ui_wait_release();
	for (;;) {
		first = selected >= 9 ? selected - 8 : 0;
		ui_begin(title, subtitle);
		for (i = first; i < item_count && i < first + 9; i++) {
			int y = 125 + (i - first) * 29;
			if (i == selected) {
				DrawBoxFilled(42, y - 20, 595, y + 7,
					ui_destructive_confirmation ? UI_DESTRUCTIVE : UI_SELECTED);
				DrawBoxFilled(42, y - 20, 47, y + 7,
					ui_destructive_confirmation ? UI_WARNING : UI_ACCENT);
				setfontcolour(UI_TEXT);
				DrawText(60, y, ">");
			} else if (enabled && !enabled[i]) {
				setfontcolour(UI_DISABLED);
			} else {
				setfontcolour(UI_MUTED);
			}
			DrawText(82, y, (char *)items[i]);
		}
		if (item_count > 9) {
			snprintf(page, sizeof(page), "Page %d/%d", selected / 9 + 1,
				(item_count + 8) / 9);
			setfontsize(12);
			setfontcolour(UI_MUTED);
			DrawText(530, 91, page);
			setfontsize(14);
		}
		ui_footer(help ? help : (allow_back ? "A Select   B Back" : "A Select"));
		ShowScreen();

		key = ui_read_key();
		if (key == UI_KEY_UP || key == UI_KEY_LEFT) {
			do {
				selected = selected > 0 ? selected - 1 : item_count - 1;
			} while (enabled && !enabled[selected]);
		} else if (key == UI_KEY_DOWN || key == UI_KEY_RIGHT) {
			do {
				selected = selected + 1 < item_count ? selected + 1 : 0;
			} while (enabled && !enabled[selected]);
		} else if (key == UI_KEY_CONFIRM) {
			ui_wait_release();
			return selected;
		} else if (key == UI_KEY_BACK && allow_back) {
			ui_wait_release();
			return -1;
		} else if (key == UI_KEY_HELP) {
			UI_Help();
		}
		VIDEO_WaitVSync();
	}
}

int UI_Menu(const char *title, const char *subtitle,
	const char *const items[], int item_count, int initial_selection,
	const char *help, bool allow_back)
{
	return UI_MenuDisabled(title, subtitle, items, NULL, item_count,
		initial_selection, help, allow_back);
}

int UI_HomeMenu(const char *card_a, const char *card_b,
	const char *storage, const char *transfer, int initial_selection)
{
	static const char *const items[] = {
		"Manage saves",
		"Back up memory card",
		"Restore backup",
		"Settings"
	};
	int selected = initial_selection;
	int i;
	ui_key key;

	if (selected < 0 || selected >= 4)
		selected = 0;
	ui_wait_release();
	for (;;) {
		ui_begin("GCMM-EX", "Connected devices");
		DrawBoxFilled(42, 104, 595, 189, UI_PANEL_ALT);
		setfontsize(12);
		setfontcolour(UI_TEXT);
		DrawText(58, 124, (char *)card_a);
		DrawText(330, 124, (char *)card_b);
		setfontcolour(UI_MUTED);
		DrawText(58, 150, (char *)storage);
		DrawText(58, 176, (char *)transfer);

		setfontsize(14);
		setfontcolour(UI_TEXT);
		DrawText(42, 216, "What would you like to do?");
		for (i = 0; i < 4; i++) {
			int y = 251 + i * 35;
			if (i == selected) {
				DrawBoxFilled(42, y - 22, 595, y + 9, UI_SELECTED);
				DrawBoxFilled(42, y - 22, 47, y + 9, UI_ACCENT);
				setfontcolour(UI_TEXT);
				DrawText(60, y, ">");
			} else {
				setfontcolour(UI_MUTED);
			}
			DrawText(82, y, (char *)items[i]);
		}
		ui_footer("A Select   B Exit");
		ShowScreen();

		key = ui_read_key();
		if (key == UI_KEY_UP || key == UI_KEY_LEFT) {
			selected = selected > 0 ? selected - 1 : 3;
		} else if (key == UI_KEY_DOWN || key == UI_KEY_RIGHT) {
			selected = selected < 3 ? selected + 1 : 0;
		} else if (key == UI_KEY_CONFIRM) {
			ui_wait_release();
			return selected;
		} else if (key == UI_KEY_BACK) {
			ui_wait_release();
			return -1;
		} else if (key == UI_KEY_HELP) {
			UI_Help();
		}
		VIDEO_WaitVSync();
	}
}

ui_list_action UI_SaveList(const char *title, const char *subtitle,
	u8 entries[][1024], int entry_count, int *selection, bool *marked,
	int card_slot)
{
	int first;
	int i;
	int marked_count;
	char footer[96];
	char label[58];
	char page[32];
	ui_key key;

	if (!selection || !entries || entry_count <= 0 || entry_count > 1024 ||
		(marked && entry_count > CARD_MAXFILES))
		return UI_LIST_BACK;
	if (*selection < 0 || *selection >= entry_count)
		*selection = 0;
	ui_wait_release();

	for (;;) {
		if (card_slot >= CARD_SLOTA && card_slot <= CARD_SLOTB &&
			CARD_Probe(card_slot) <= 0)
			return UI_LIST_DEVICE_REMOVED;
		first = (*selection / UI_PAGE_SIZE) * UI_PAGE_SIZE;
		marked_count = ui_marked_count(marked, entry_count);
		ui_begin(title, subtitle);
		setfontsize(13);
		for (i = first; i < entry_count && i < first + UI_PAGE_SIZE; i++) {
			int y = 118 + (i - first) * 24;
			if (i == *selection) {
				DrawBoxFilled(42, y - 17, 595, y + 6, UI_SELECTED);
				DrawBoxFilled(42, y - 17, 47, y + 6, UI_ACCENT);
				setfontcolour(UI_TEXT);
			} else {
				setfontcolour(UI_MUTED);
			}
			if (strnlen((char *)entries[i], sizeof(entries[i])) >= sizeof(entries[i]))
				return UI_LIST_BACK;
			snprintf(label, sizeof(label), "%s %.48s",
				marked && marked[i] ? "[x]" : "[ ]", (char *)entries[i]);
			DrawText(58, y, label);
		}
		snprintf(page, sizeof(page), "Page %d/%d", *selection / UI_PAGE_SIZE + 1,
			(entry_count + UI_PAGE_SIZE - 1) / UI_PAGE_SIZE);
		setfontsize(12);
		setfontcolour(UI_MUTED);
		DrawText(530, 91, page);
		setfontsize(14);
		if (marked)
			snprintf(footer, sizeof(footer),
				"A Open   X Options   Y Mark (%d)   B Back   L/R Device", marked_count);
		else
			snprintf(footer, sizeof(footer), "A Select   B Back");
		ui_footer(footer);
		ShowScreen();

		key = ui_read_key();
		if (key == UI_KEY_UP || key == UI_KEY_LEFT) {
			*selection = *selection > 0 ? *selection - 1 : entry_count - 1;
		} else if (key == UI_KEY_DOWN || key == UI_KEY_RIGHT) {
			*selection = *selection + 1 < entry_count ? *selection + 1 : 0;
		} else if (key == UI_KEY_CONFIRM) {
			ui_wait_release();
			return UI_LIST_OPEN;
		} else if (key == UI_KEY_CONTEXT) {
			ui_wait_release();
			return UI_LIST_CONTEXT;
		} else if (key == UI_KEY_MARK && marked) {
			marked[*selection] = !marked[*selection];
		} else if (key == UI_KEY_PREVIOUS) {
			ui_wait_release();
			return UI_LIST_PREVIOUS_DEVICE;
		} else if (key == UI_KEY_NEXT) {
			ui_wait_release();
			return UI_LIST_NEXT_DEVICE;
		} else if (key == UI_KEY_BACK) {
			ui_wait_release();
			return UI_LIST_BACK;
		} else if (key == UI_KEY_HELP) {
			UI_Help();
		}
		VIDEO_WaitVSync();
	}
}

static bool ui_confirm(const char *title, const char *message,
	const char *detail, const char *confirm_label, bool destructive)
{
	const char *items[2] = { "Cancel", confirm_label };
	bool previous_style = ui_destructive_confirmation;
	int choice;

	ui_destructive_confirmation = destructive;
	choice = UI_Menu(title, message, items, 2, 0,
		"A Confirm selection   B Cancel", true);

	if (detail && detail[0] && choice == 1) {
		const char *final_items[2] = { "Cancel", confirm_label };
		choice = UI_Menu("Final confirmation", detail, final_items, 2, 0,
			"Review carefully. A Confirm   B Cancel", true);
	}
	ui_destructive_confirmation = previous_style;
	return choice == 1;
}

bool UI_Confirm(const char *title, const char *message,
	const char *detail, const char *confirm_label)
{
	return ui_confirm(title, message, detail, confirm_label, false);
}

bool UI_ConfirmDestructive(const char *title, const char *message,
	const char *detail, const char *confirm_label)
{
	return ui_confirm(title, message, detail, confirm_label, true);
}

void UI_Message(const char *title, const char *message, const char *detail)
{
	ui_key key;

	ui_wait_release();
	for (;;) {
		ui_begin(title, message);
		DrawBoxFilled(42, 120, 595, 230, UI_PANEL_ALT);
		setfontsize(14);
		setfontcolour(UI_TEXT);
		DrawText(58, 158, (char *)(detail && detail[0] ? detail : "Operation finished."));
		ui_footer("A OK   B Back");
		ShowScreen();
		key = ui_read_key();
		if (key == UI_KEY_CONFIRM || key == UI_KEY_BACK) {
			ui_wait_release();
			return;
		}
		VIDEO_WaitVSync();
	}
}

void UI_Details(const char *title, const char *const lines[], int line_count)
{
	ui_key key;
	int i;

	ui_wait_release();
	for (;;) {
		ui_begin(title, "Save details");
		DrawBoxFilled(42, 112, 595, 350, UI_PANEL_ALT);
		setfontsize(14);
		for (i = 0; i < line_count && i < 8; i++) {
			if (i == 0)
				setfontcolour(UI_TEXT);
			else
				setfontcolour(UI_MUTED);
			DrawText(58, 143 + i * 27, (char *)lines[i]);
		}
		ui_footer("A Close   B Back");
		ShowScreen();
		key = ui_read_key();
		if (key == UI_KEY_CONFIRM || key == UI_KEY_BACK) {
			ui_wait_release();
			return;
		}
		VIDEO_WaitVSync();
	}
}

void UI_SetTransferState(const char *source, const char *destination)
{
	ui_transfer_source = source ? source : "Not selected";
	ui_transfer_destination = destination ? destination : "Not selected";
}

void UI_Progress(const char *title, const char *item, int current, int total)
{
	int filled;
	char progress[48];
	bool determinate = total > 0;

	if (!determinate)
		total = 1;
	if (current < 0)
		current = 0;
	if (current > total)
		current = total;
	filled = 480 * current / total;

	ui_begin(title, item);
	setfontsize(12);
	setfontcolour(UI_MUTED);
	DrawText(58, 124, (char *)ui_transfer_source);
	DrawText(58, 150, (char *)ui_transfer_destination);
	DrawBoxFilled(78, 188, 560, 224, UI_PANEL_ALT);
	DrawBox(78, 188, 560, 224, UI_BORDER);
	if (determinate && filled > 0)
		DrawBoxFilled(80, 190, 80 + filled, 222, UI_ACCENT);
	if (determinate)
		snprintf(progress, sizeof(progress), "%d of %d   %d%%", current, total,
			100 * current / total);
	else
		snprintf(progress, sizeof(progress), "Working...");
	setfontsize(14);
	setfontcolour(UI_TEXT);
	DrawText(-1, 260, progress);
	ui_footer("Do not remove the memory card or storage device.");
	ShowScreen();
}

bool UI_CancelRequested(void)
{
	if (ui_read_key() != UI_KEY_BACK)
		return false;
	ui_wait_release();
	return true;
}

void UI_Help(void)
{
	ui_wait_release();
	ui_begin("Controls", "Same controls throughout GCMM-EX");
	setfontsize(14);
	setfontcolour(UI_TEXT);
	DrawText(58, 130, "D-pad / Analog    Navigate");
	DrawText(58, 160, "A                 Select / confirm");
	DrawText(58, 190, "B                 Back / cancel");
#ifdef HW_RVL
	DrawText(58, 220, "+ / -             Options / mark");
	DrawText(58, 250, "1 / 2             Previous / next device");
	DrawText(58, 280, "HOME              Help");
#else
	DrawText(58, 220, "X / Y             Options / mark");
	DrawText(58, 250, "L / R             Previous / next device");
	DrawText(58, 280, "START             Help");
#endif
	ui_footer("A Close   B Close");
	ShowScreen();
	for (;;) {
		ui_key key = ui_read_key();
		if (key == UI_KEY_CONFIRM || key == UI_KEY_BACK || key == UI_KEY_HELP) {
			ui_wait_release();
			return;
		}
		VIDEO_WaitVSync();
	}
}
