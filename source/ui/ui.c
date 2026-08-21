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
#include "bannerload.h"
#include "freetype.h"
#include "gci.h"
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
#define UI_ACCENT_TEXT 45, 196, 196
#define UI_WARNING_TEXT 220, 165, 53
/* Panel-safe variants of the status colours, which are too dark as text. */
#define UI_SUCCESS_TEXT 92, 214, 146
#define UI_ERROR_TEXT   232, 110, 120
#define UI_PAGE_SIZE   7

/*
 * Footer button glyphs. Both builds name the GameCube controller, because the
 * cards being managed are GameCube cards and the footer stays identical across
 * platforms. The Wii Remote and Classic Controller remain fully supported;
 * UI_Help lists their equivalents.
 */
#define UI_BTN_OPTIONS "X"
#define UI_BTN_MARK    "Y"
#define UI_BTN_DEVICE  "L R"
#define UI_BTN_HELP    "START"

/* Footer layout. Bounds stay inside the overscan-safe area of an SD display. */
#define UI_FOOTER_LEFT    42
#define UI_FOOTER_RIGHT   598
#define UI_FOOTER_BASE    461
#define UI_CHIP_TOP       443
#define UI_CHIP_BOTTOM    465
#define UI_CHIP_PADDING   7
#define UI_CHIP_GAP       6
#define UI_HINT_GAP       18
#define UI_HINT_GAP_MIN   8

/* Save-details grid: two columns by three rows inside the content panel. */
#define UI_DETAIL_FIELDS  6

/* Action-menu rows. Twelve rows at these steps end at 398, inside the panel. */
#define UI_ACTION_ROW_STEP     26
#define UI_ACTION_HEADING_STEP 24

/** A footer control: the button glyph plus the action it performs. */
typedef struct {
	const char *button;
	const char *label;
} ui_hint;

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
static u16 ui_preview_format;
static const u16 *ui_preview_rgb;
static const u8 *ui_preview_ci;
static const u16 *ui_preview_palette;
static const char *ui_preview_title;
static const char *ui_preview_source;

#ifdef HW_RVL
extern bool power;
void PowerOff(void);
#endif

extern u32 retraceCount;
extern const char appversion[];
extern GXRModeObj *vmode;

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
	DrawBoxFilled(24, 24, 615, 418, UI_PANEL);
	DrawBox(24, 24, 615, 418, UI_BORDER);
	DrawBoxFilled(24, 24, 615, 69, UI_PANEL_ALT);
	DrawBoxFilled(24, 68, 615, 70, UI_ACCENT);
	DrawBMPAt((u8 *)ui_logo_bmp, 482, 36);
	setfontsize(20);
	setfontcolour(UI_TEXT);
	DrawText(42, 55, (char *)title);
	setfontsize(12);
	setfontcolour(UI_MUTED);
	DrawText(42 + TextWidth((char *)title) + 12, 55, (char *)appversion);
	if (subtitle && subtitle[0]) {
		setfontsize(12);
		setfontcolour(UI_MUTED);
		DrawText(42, 91, (char *)subtitle);
	}
	setfontsize(14);
}

static void ui_footer_bar(void)
{
	DrawBoxFilled(0, 424, vmode->fbWidth - 1, vmode->xfbHeight - 1, UI_PANEL_ALT);
	DrawBoxFilled(0, 424, vmode->fbWidth - 1, 427, UI_ACCENT);
	setfontsize(12);
}

static int ui_chip_width(const char *button)
{
	return TextWidth(button) + 2 * UI_CHIP_PADDING;
}

static int ui_draw_chip(int x, const char *button)
{
	int width = ui_chip_width(button);

	DrawBoxFilled(x, UI_CHIP_TOP, x + width, UI_CHIP_BOTTOM, UI_SELECTED);
	DrawBox(x, UI_CHIP_TOP, x + width, UI_CHIP_BOTTOM, UI_BORDER);
	setfontcolour(UI_ACCENT_TEXT);
	DrawText(x + UI_CHIP_PADDING, UI_FOOTER_BASE, (char *)button);
	return width;
}

static int ui_hint_width(const ui_hint *hint)
{
	int width = ui_chip_width(hint->button);

	if (hint->label && hint->label[0])
		width += UI_CHIP_GAP + TextWidth(hint->label);
	return width;
}

/*
 * Draws the button hints left to right, an optional note after them, and the
 * persistent help control right aligned. Spacing shrinks before anything is
 * dropped, so a crowded footer stays inside the safe area instead of running
 * under the help control.
 */
static void ui_footer_hints(const ui_hint *hints, int count, const char *note)
{
	int note_width = (note && note[0]) ? TextWidth(note) : 0;
	int items = count + (note_width ? 1 : 0);
	int help_x;
	int content;
	int gap;
	int x;
	int i;

	ui_footer_bar();
	if (count <= 0) {
		if (note_width) {
			setfontcolour(UI_MUTED);
			DrawText(UI_FOOTER_LEFT, UI_FOOTER_BASE, (char *)note);
		}
		setfontsize(14);
		return;
	}

	help_x = UI_FOOTER_RIGHT - ui_chip_width(UI_BTN_HELP) - UI_CHIP_GAP -
		TextWidth("Help");
	content = note_width;
	for (i = 0; i < count; i++)
		content += ui_hint_width(&hints[i]);

	gap = UI_HINT_GAP;
	if (items > 1) {
		int slack = (help_x - UI_HINT_GAP - UI_FOOTER_LEFT - content) /
			(items - 1);

		if (slack < gap)
			gap = slack > UI_HINT_GAP_MIN ? slack : UI_HINT_GAP_MIN;
	}

	x = UI_FOOTER_LEFT;
	for (i = 0; i < count; i++) {
		if (x + ui_hint_width(&hints[i]) > help_x)
			break;
		x += ui_draw_chip(x, hints[i].button);
		if (hints[i].label && hints[i].label[0]) {
			setfontcolour(UI_MUTED);
			DrawText(x + UI_CHIP_GAP, UI_FOOTER_BASE, (char *)hints[i].label);
			x += UI_CHIP_GAP + TextWidth(hints[i].label);
		}
		x += gap;
	}
	if (note_width && x + note_width <= help_x) {
		setfontcolour(UI_MUTED);
		DrawText(x, UI_FOOTER_BASE, (char *)note);
	}

	x = help_x + ui_draw_chip(help_x, UI_BTN_HELP);
	setfontcolour(UI_TEXT);
	DrawText(x + UI_CHIP_GAP, UI_FOOTER_BASE, "Help");
	setfontsize(14);
}

/*
 * Draws text on up to two lines, breaking on the last space that still fits
 * the given pixel width. Measuring beats counting characters here, because the
 * bundled font is proportional.
 */
static void ui_draw_wrapped(int x, int y, int width, int line_height,
	const char *text)
{
	char line[128];
	size_t length;
	size_t split;

	if (!text || !text[0])
		return;
	length = strnlen(text, sizeof(line) - 1);
	memcpy(line, text, length);
	line[length] = '\0';
	if (TextWidth(line) <= width) {
		DrawText(x, y, line);
		return;
	}
	for (split = length; split > 0; split--) {
		if (text[split] != ' ')
			continue;
		line[split] = '\0';
		if (TextWidth(line) <= width)
			break;
		line[split] = ' ';
	}
	if (!split) {
		DrawText(x, y, line);	/* No break point, so let it run long. */
		return;
	}
	DrawText(x, y, line);
	DrawText(x, y + line_height, (char *)text + split + 1);
}

/* Footer for screens that take no input, so no help control is offered. */
static void ui_footer_notice(const char *notice)
{
	ui_footer_bar();
	setfontcolour(UI_WARNING_TEXT);
	DrawText(UI_FOOTER_LEFT, UI_FOOTER_BASE, (char *)notice);
	setfontsize(14);
}

void UI_ClearSavePreview(void)
{
	ui_preview_format = 0;
	ui_preview_rgb = NULL;
	ui_preview_ci = NULL;
	ui_preview_palette = NULL;
	ui_preview_title = NULL;
	ui_preview_source = NULL;
}

void UI_SetSavePreview(u16 banner_format, const u16 *rgb_banner,
	const u8 *ci_banner, const u16 *palette, const char *title, const char *source)
{
	ui_preview_format = banner_format & CARD_BANNER_MASK;
	ui_preview_rgb = rgb_banner;
	ui_preview_ci = ci_banner;
	ui_preview_palette = palette;
	ui_preview_title = title;
	ui_preview_source = source;
}

/* Framed banner at twice its native 96x32, centred in the given box. */
static void ui_draw_banner_frame(int x, int y, int width, int height)
{
	/* The framebuffer packs two pixels per word, so DrawBannerAt rejects an
	 * odd x outright. Round the centred position down to keep it drawable. */
	int banner_x = (x + (width - 2 * CARD_BANNER_W) / 2) & ~1;
	int banner_y = y + (height - 2 * CARD_BANNER_H) / 2;

	DrawBoxFilled(x, y, x + width, y + height, UI_PANEL);
	DrawBox(x, y, x + width, y + height, UI_BORDER);
	if (ui_preview_format == CARD_BANNER_RGB && ui_preview_rgb) {
		DrawBannerRGBAt(ui_preview_rgb, banner_x, banner_y, 2);
	} else if (ui_preview_format == CARD_BANNER_CI && ui_preview_ci &&
		ui_preview_palette) {
		DrawBannerCIAt(ui_preview_ci, ui_preview_palette, banner_x, banner_y, 2);
	} else {
		setfontsize(11);
		setfontcolour(UI_DISABLED);
		DrawText(x + (width - TextWidth("No banner available")) / 2,
			y + height / 2 + 4, "No banner available");
	}
}

static void ui_draw_save_preview(void)
{
	char title[31];

	DrawBoxFilled(356, 112, 595, 370, UI_PANEL_ALT);
	DrawBox(356, 112, 595, 370, UI_BORDER);
	setfontsize(12);
	setfontcolour(UI_MUTED);
	DrawText(372, 132, "Selected save");
	ui_draw_banner_frame(374, 145, 203, 80);
	snprintf(title, sizeof(title), "%.30s", ui_preview_title && ui_preview_title[0] ?
		ui_preview_title : "Save title unavailable");
	setfontsize(12);
	setfontcolour(UI_TEXT);
	DrawText(372, 257, title);
	setfontsize(11);
	setfontcolour(UI_MUTED);
	DrawText(372, 283, (char *)(ui_preview_source ? ui_preview_source : "Banner preview"));
	setfontsize(14);
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
	static const ui_hint hints[] = {
		{ "A", "Select" },
		{ "B", "Back" }
	};
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
		ui_footer_hints(hints, allow_back ? 2 : 1, help);
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

/* Advances past headings and unavailable rows in the given direction. */
static int ui_action_step(const ui_menu_item items[], int item_count, int from,
	int step)
{
	int index = from;
	int guard;

	for (guard = 0; guard < item_count; guard++) {
		index += step;
		if (index < 0)
			index = item_count - 1;
		else if (index >= item_count)
			index = 0;
		if (items[index].enabled && !items[index].heading)
			return index;
	}
	return from;
}

int UI_ActionMenu(const char *title, const char *subtitle,
	const ui_menu_item items[], int item_count, int initial_selection,
	const char *note)
{
	static const ui_hint hints[] = {
		{ "A", "Select" },
		{ "B", "Back" }
	};
	int selected = initial_selection;
	int focusable = 0;
	int i;
	ui_key key;

	if (!items || item_count <= 0 || item_count > UI_ACTION_MAX_ITEMS)
		return -1;
	for (i = 0; i < item_count; i++)
		if (items[i].enabled && !items[i].heading)
			focusable++;
	if (!focusable)
		return -1;
	if (selected < 0 || selected >= item_count)
		selected = 0;
	if (!items[selected].enabled || items[selected].heading)
		selected = ui_action_step(items, item_count, selected, 1);

	ui_wait_release();
	for (;;) {
		int y = 120;

		ui_begin(title, subtitle);
		for (i = 0; i < item_count; i++) {
			if (items[i].heading) {
				setfontsize(11);
				setfontcolour(UI_ACCENT_TEXT);
				DrawText(58, y, (char *)items[i].label);
				y += UI_ACTION_HEADING_STEP;
				continue;
			}
			setfontsize(14);
			if (i == selected) {
				DrawBoxFilled(42, y - 19, 595, y + 6,
					items[i].destructive ? UI_DESTRUCTIVE : UI_SELECTED);
				DrawBoxFilled(42, y - 19, 47, y + 6,
					items[i].destructive ? UI_WARNING : UI_ACCENT);
				setfontcolour(UI_TEXT);
				DrawText(60, y, ">");
			} else if (!items[i].enabled) {
				setfontcolour(UI_DISABLED);
			} else if (items[i].destructive) {
				setfontcolour(UI_ERROR_TEXT);
			} else {
				setfontcolour(UI_MUTED);
			}
			DrawText(82, y, (char *)items[i].label);
			y += UI_ACTION_ROW_STEP;
		}
		ui_footer_hints(hints, 2, note);
		ShowScreen();

		key = ui_read_key();
		if (key == UI_KEY_UP || key == UI_KEY_LEFT) {
			selected = ui_action_step(items, item_count, selected, -1);
		} else if (key == UI_KEY_DOWN || key == UI_KEY_RIGHT) {
			selected = ui_action_step(items, item_count, selected, 1);
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

int UI_Menu(const char *title, const char *subtitle,
	const char *const items[], int item_count, int initial_selection,
	const char *help, bool allow_back)
{
	return UI_MenuDisabled(title, subtitle, items, NULL, item_count,
		initial_selection, help, allow_back);
}

int UI_HomeMenu(int initial_selection)
{
	static const ui_hint hints[] = {
		{ "A", "Select" },
		{ "B", "Exit" }
	};
	static const char *const items[] = {
		"Manage saves",
		"Backup saves (GCI)",
		"Restore from backup",
		"Backup full card (RAW)",
		"Restore full card (RAW)",
		"Format memory card",
		"Device & storage details",
		"Settings & Info",
		"Exit"
	};
	int item_count = sizeof(items) / sizeof(items[0]);
	int selected = initial_selection;
	int i;
	ui_key key;

	if (selected < 0 || selected >= item_count)
		selected = 0;
	ui_wait_release();
	for (;;) {
		ui_begin("GCMM-EX", NULL);
		setfontsize(16);
		setfontcolour(UI_TEXT);
		DrawText(42, 116, "What would you like to do?");
		for (i = 0; i < item_count; i++) {
			int y = 140 + i * 30;
			if (i == selected) {
				DrawBoxFilled(42, y - 22, 595, y + 8, UI_SELECTED);
				DrawBoxFilled(42, y - 22, 47, y + 8, UI_ACCENT);
				setfontcolour(UI_TEXT);
				DrawText(60, y, ">");
			} else {
				setfontcolour(UI_MUTED);
			}
			DrawText(82, y, (char *)items[i]);
		}
		ui_footer_bar();
		{
			int x = UI_FOOTER_LEFT;
			int gap = UI_HINT_GAP;
			int fi;
			/* Start (help) on the left */
			x += ui_draw_chip(x, UI_BTN_HELP);
			setfontcolour(UI_MUTED);
			DrawText(x + UI_CHIP_GAP, UI_FOOTER_BASE, "Start");
			x += UI_CHIP_GAP + TextWidth("Start") + gap;
			/* Navigation hints */
			for (fi = 0; fi < 2; fi++) {
				x += ui_draw_chip(x, (char *)hints[fi].button);
				setfontcolour(UI_MUTED);
				DrawText(x + UI_CHIP_GAP, UI_FOOTER_BASE, (char *)hints[fi].label);
				x += UI_CHIP_GAP + TextWidth(hints[fi].label) + gap;
			}
			/* Creator credit, right-aligned */
			setfontcolour(UI_MUTED);
			DrawText(UI_FOOTER_RIGHT - TextWidth("Created by Alex Ishida"),
				UI_FOOTER_BASE, "Created by Alex Ishida");
		}
		setfontsize(14);
		ShowScreen();

		key = ui_read_key();
		if (key == UI_KEY_UP || key == UI_KEY_LEFT) {
			selected = selected > 0 ? selected - 1 : item_count - 1;
		} else if (key == UI_KEY_DOWN || key == UI_KEY_RIGHT) {
			selected = selected < item_count - 1 ? selected + 1 : 0;
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
	int card_slot, bool show_preview)
{
	static const ui_hint browse_hints[] = {
		{ "A", "Select" },
		{ "B", "Back" }
	};
	/* L/R swaps between both card slots, so the hint names where it leads. */
	const char *other_card = card_slot == CARD_SLOTA ? "Card B" :
		card_slot == CARD_SLOTB ? "Card A" : "Device";
	/* Without the banner panel the rows own the full content width. */
	int row_right = show_preview ? 340 : 595;
	int name_chars = show_preview ? 48 : 72;
	int first;
	int i;
	int marked_count;
	char mark_label[16];
	char label[80];
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
				DrawBoxFilled(42, y - 17, row_right, y + 6, UI_SELECTED);
				DrawBoxFilled(42, y - 17, 47, y + 6, UI_ACCENT);
				setfontcolour(UI_TEXT);
			} else {
				setfontcolour(UI_MUTED);
			}
			if (strnlen((char *)entries[i], sizeof(entries[i])) >= sizeof(entries[i]))
				return UI_LIST_BACK;
			/* Selection boxes only where marking is actually available. */
			if (marked)
				snprintf(label, sizeof(label), "%s %.*s",
					marked[i] ? "[x]" : "[ ]", name_chars, (char *)entries[i]);
			else
				snprintf(label, sizeof(label), "%.*s", name_chars,
					(char *)entries[i]);
			DrawText(58, y, label);
		}
		if (show_preview)
			ui_draw_save_preview();
		snprintf(page, sizeof(page), "Page %d/%d", *selection / UI_PAGE_SIZE + 1,
			(entry_count + UI_PAGE_SIZE - 1) / UI_PAGE_SIZE);
		setfontsize(12);
		setfontcolour(UI_MUTED);
		DrawText(530, 91, page);
		setfontsize(14);
		if (marked) {
			const ui_hint manage_hints[] = {
				{ "A", "Open" },
				{ UI_BTN_OPTIONS, "Options" },
				{ UI_BTN_MARK, mark_label },
				{ "B", "Back" },
				{ UI_BTN_DEVICE, other_card }
			};

			if (marked_count > 0)
				snprintf(mark_label, sizeof(mark_label), "Mark %d", marked_count);
			else
				snprintf(mark_label, sizeof(mark_label), "Mark");
			ui_footer_hints(manage_hints, 5, NULL);
		} else {
			ui_footer_hints(browse_hints, 2, NULL);
		}
		ShowScreen();

		key = ui_read_key();
		if (key == UI_KEY_UP || key == UI_KEY_LEFT) {
			*selection = *selection > 0 ? *selection - 1 : entry_count - 1;
			return UI_LIST_PREVIEW;
		} else if (key == UI_KEY_DOWN || key == UI_KEY_RIGHT) {
			*selection = *selection + 1 < entry_count ? *selection + 1 : 0;
			return UI_LIST_PREVIEW;
		} else if (key == UI_KEY_CONFIRM) {
			ui_wait_release();
			return UI_LIST_OPEN;
		} else if (key == UI_KEY_CONTEXT) {
			ui_wait_release();
			return UI_LIST_CONTEXT;
		} else if (key == UI_KEY_MARK && marked) {
			/* Marking stays in this loop, so the button must be released
			 * before it counts again. Held state alone would toggle the
			 * entry on every retrace of a single press. */
			marked[*selection] = !marked[*selection];
			ui_wait_release();
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

static void ui_draw_confirmation_detail(const char *detail)
{
	char line[72];
	size_t length;
	size_t split;

	if (!detail || !detail[0])
		return;
	length = strnlen(detail, 140);
	if (length < sizeof(line)) {
		DrawText(58, 151, (char *)detail);
		return;
	}
	split = sizeof(line) - 1;
	while (split && detail[split] != ' ')
		split--;
	if (!split)
		split = sizeof(line) - 1;
	memcpy(line, detail, split);
	line[split] = '\0';
	DrawText(58, 145, line);
	while (detail[split] == ' ')
		split++;
	DrawText(58, 166, (char *)detail + split);
}

static bool ui_confirm(const char *title, const char *message,
	const char *detail, const char *confirm_label, bool destructive)
{
	static const ui_hint hints[] = {
		{ "A", "Confirm" },
		{ "B", "Cancel" }
	};
	const char *items[2] = { "Cancel", confirm_label };
	bool previous_style = ui_destructive_confirmation;
	int selected = 0;
	int i;
	ui_key key;

	ui_destructive_confirmation = destructive;
	ui_wait_release();
	for (;;) {
		ui_begin(title, message);
		DrawBoxFilled(42, 112, 595, 184, UI_PANEL_ALT);
		setfontsize(12);
		setfontcolour(UI_MUTED);
		ui_draw_confirmation_detail(detail);
		setfontsize(14);
		for (i = 0; i < 2; i++) {
			int y = 229 + i * 34;

			if (i == selected) {
				DrawBoxFilled(42, y - 20, 595, y + 7,
					destructive ? UI_DESTRUCTIVE : UI_SELECTED);
				DrawBoxFilled(42, y - 20, 47, y + 7,
					destructive ? UI_WARNING : UI_ACCENT);
				setfontcolour(UI_TEXT);
				DrawText(60, y, ">");
			} else {
				setfontcolour(UI_MUTED);
			}
			DrawText(82, y, (char *)items[i]);
		}
		ui_footer_hints(hints, 2, NULL);
		ShowScreen();

		key = ui_read_key();
		if (key == UI_KEY_UP || key == UI_KEY_LEFT || key == UI_KEY_DOWN ||
			key == UI_KEY_RIGHT) {
			selected = selected ? 0 : 1;
		} else if (key == UI_KEY_CONFIRM) {
			ui_wait_release();
			break;
		} else if (key == UI_KEY_BACK) {
			selected = 0;
			ui_wait_release();
			break;
		} else if (key == UI_KEY_HELP) {
			UI_Help();
		}
		VIDEO_WaitVSync();
	}
	ui_destructive_confirmation = previous_style;
	return selected == 1;
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

typedef enum {
	UI_MESSAGE_INFO = 0,
	UI_MESSAGE_SUCCESS,
	UI_MESSAGE_ERROR
} ui_message_status;

/*
 * The outcome sentence is the reason this screen exists, so it leads: largest
 * type, inside the panel, under a coloured stripe and status word that say how
 * the operation ended before the sentence is even read. detail supports it.
 */
static void ui_message(const char *title, const char *message,
	const char *detail, ui_message_status status)
{
	/* A message is acknowledged rather than left, so OK is the control that
	 * gets named. B still dismisses it. */
	static const ui_hint hints[] = {
		{ "A", "OK" }
	};
	ui_key key;

	ui_wait_release();
	for (;;) {
		ui_begin(title, NULL);
		DrawBoxFilled(42, 132, 595, 288, UI_PANEL_ALT);
		DrawBox(42, 132, 595, 288, UI_BORDER);
		DrawBoxFilled(42, 132, 48, 288,
			status == UI_MESSAGE_SUCCESS ? UI_SUCCESS :
			status == UI_MESSAGE_ERROR ? UI_DESTRUCTIVE : UI_ACCENT);

		setfontsize(11);
		if (status == UI_MESSAGE_SUCCESS)
			setfontcolour(UI_SUCCESS_TEXT);
		else if (status == UI_MESSAGE_ERROR)
			setfontcolour(UI_ERROR_TEXT);
		else
			setfontcolour(UI_ACCENT_TEXT);
		DrawText(66, 160, status == UI_MESSAGE_SUCCESS ? "SUCCESS" :
			status == UI_MESSAGE_ERROR ? "FAILED" : "NOTICE");

		setfontsize(18);
		setfontcolour(UI_TEXT);
		ui_draw_wrapped(66, 194, 513, 26,
			message && message[0] ? message : "Operation finished.");

		if (detail && detail[0]) {
			setfontsize(12);
			setfontcolour(UI_MUTED);
			ui_draw_wrapped(66, 250, 513, 18, detail);
		}
		ui_footer_hints(hints, 1, NULL);
		ShowScreen();
		key = ui_read_key();
		if (key == UI_KEY_CONFIRM || key == UI_KEY_BACK) {
			ui_wait_release();
			return;
		}
		VIDEO_WaitVSync();
	}
}

void UI_Message(const char *title, const char *message, const char *detail)
{
	ui_message(title, message, detail, UI_MESSAGE_INFO);
}

void UI_MessageSuccess(const char *title, const char *message, const char *detail)
{
	ui_message(title, message, detail, UI_MESSAGE_SUCCESS);
}

void UI_MessageError(const char *title, const char *message, const char *detail)
{
	ui_message(title, message, detail, UI_MESSAGE_ERROR);
}

/* Read-only summary of connected devices and the current workflow. Rendered as
 * a plain screen instead of a home-card so the home menu keeps room for more
 * actions. */
void UI_DeviceDetails(const char *card_a, const char *card_b,
	const char *storage, const char *transfer)
{
	static const ui_hint hints[] = {
		{ "B", "Back" }
	};
	ui_key key;

	ui_wait_release();
	for (;;) {
		ui_begin("Device & storage details", NULL);
		DrawBoxFilled(42, 132, 595, 320, UI_PANEL_ALT);
		DrawBox(42, 132, 595, 320, UI_BORDER);
		setfontsize(14);
		setfontcolour(UI_TEXT);
		DrawText(58, 158, (char *)(card_a ? card_a : ""));
		DrawText(58, 194, (char *)(card_b ? card_b : ""));
		DrawText(58, 230, (char *)(storage ? storage : ""));
		DrawText(58, 266, (char *)(transfer ? transfer : ""));
		setfontsize(11);
		setfontcolour(UI_MUTED);
		DrawText(58, 306,
			"Status shown is sampled when this screen is opened.");
		ui_footer_hints(hints, 1, NULL);
		ShowScreen();
		key = ui_read_key();
		if (key == UI_KEY_CONFIRM || key == UI_KEY_BACK) {
			ui_wait_release();
			return;
		}
		VIDEO_WaitVSync();
	}
}

/*
 * Storage modules raise failures from inside long operations. These used to
 * print two lines into a grey strip over whatever screen was up, which read as
 * nothing having happened at all. They get the same screen as every other
 * failure now.
 */
void WaitPrompt(char *msg)
{
	ui_message("Operation stopped", msg && msg[0] ? msg : "The operation failed.",
		"No further step was attempted.", UI_MESSAGE_ERROR);
}

void UI_SaveDetails(const char *subtitle, const char *title,
	const char *description, const char *filename,
	const ui_field fields[], int field_count)
{
	/* One way out is enough on a read-only screen. A still dismisses it, so a
	 * reflexive press after opening with A does not leave the user stuck. */
	static const ui_hint hints[] = {
		{ "B", "Back" }
	};
	char text[64];
	ui_key key;
	int i;

	if (field_count < 0)
		field_count = 0;
	if (field_count > UI_DETAIL_FIELDS)
		field_count = UI_DETAIL_FIELDS;

	ui_wait_release();
	for (;;) {
		ui_begin("Save details", subtitle);
		DrawBoxFilled(42, 112, 595, 404, UI_PANEL_ALT);
		DrawBox(42, 112, 595, 404, UI_BORDER);

		/* Identity block: banner on the left, names beside it. */
		ui_draw_banner_frame(58, 128, 204, 80);
		setfontsize(16);
		setfontcolour(UI_TEXT);
		snprintf(text, sizeof(text), "%.32s", title && title[0] ? title : "Untitled save");
		DrawText(282, 154, text);
		setfontsize(12);
		setfontcolour(UI_MUTED);
		if (description && description[0]) {
			snprintf(text, sizeof(text), "%.40s", description);
			DrawText(282, 178, text);
		}
		setfontsize(11);
		setfontcolour(UI_DISABLED);
		snprintf(text, sizeof(text), "%.48s",
			filename && filename[0] ? filename : "Internal filename unavailable");
		DrawText(282, 200, text);
		DrawHLine(58, 579, 228, UI_BORDER);

		/* Metadata grid, filled left to right then top to bottom. */
		for (i = 0; i < field_count; i++) {
			int x = (i % 2) ? 325 : 58;
			int y = 254 + (i / 2) * 52;

			setfontsize(11);
			setfontcolour(UI_ACCENT_TEXT);
			DrawText(x, y, (char *)fields[i].label);
			setfontsize(14);
			setfontcolour(UI_TEXT);
			snprintf(text, sizeof(text), "%.40s",
				fields[i].value && fields[i].value[0] ? fields[i].value : "Unavailable");
			DrawText(x, y + 22, text);
		}
		ui_footer_hints(hints, 1,
			"Backup and delete actions are under X in the save list.");
		ShowScreen();
		key = ui_read_key();
		if (key == UI_KEY_CONFIRM || key == UI_KEY_BACK) {
			ui_wait_release();
			return;
		}
		VIDEO_WaitVSync();
	}
}

void UI_About(const char *author, const char *foundation)
{
	static const char *const ui_about_left[3] = {
		"Backup / restore / copy / move",
		"GCI backup, GCI GCS SAV restore",
		"RAW, GCP and MCI card images"
	};
	static const char *const ui_about_right[3] = {
		"Both memory-card slots",
		"Banners and animated icons",
		"SD, SD Gecko, SD2SP2, USB, GC Loader"
	};
	static const ui_hint hints[] = {
		{ "B", "Back" }
	};
	ui_key key;
	int i;

	ui_wait_release();
	for (;;) {
		ui_begin("About GCMM-EX", "GameCube memory-card manager");
		DrawBoxFilled(42, 112, 595, 350, UI_PANEL_ALT);
		DrawBox(42, 112, 595, 350, UI_BORDER);
		DrawBoxFilled(58, 126, 579, 176, UI_PANEL);
		DrawBoxFilled(58, 126, 63, 176, UI_ACCENT);
		setfontsize(18);
		setfontcolour(UI_TEXT);
		DrawText(78, 152, "Complete memory-card management");
		setfontsize(12);
		setfontcolour(UI_MUTED);
		DrawText(78, 170, "A modern workflow for GameCube and Wii.");

		setfontsize(12);
		setfontcolour(UI_MUTED);
		for (i = 0; i < 3; i++) {
			DrawText(58, 202 + i * 22, (char *)ui_about_left[i]);
			DrawText(325, 202 + i * 22, (char *)ui_about_right[i]);
		}
		DrawHLine(58, 579, 262, UI_BORDER);

		setfontsize(11);
		setfontcolour(45, 196, 196);
		DrawText(58, 280, "CREATED BY");
		DrawText(325, 280, "BASED ON");
		setfontsize(16);
		setfontcolour(UI_TEXT);
		DrawText(58, 302, (char *)(author && author[0] ? author : "Unknown"));
		setfontsize(14);
		DrawText(325, 302, (char *)(foundation && foundation[0] ? foundation : "GCMM"));

		setfontsize(11);
		setfontcolour(UI_MUTED);
		DrawText(58, 330, "MIT License");
		setfontsize(14);
		ui_footer_hints(hints, 1, NULL);
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
	/* Inner span only, so a full bar stops short of its own border. */
	filled = 478 * current / total;

	ui_begin(title, item);

	/* Where the data is going, on the same grid the details screen uses. */
	DrawBoxFilled(42, 112, 595, 190, UI_PANEL_ALT);
	DrawBox(42, 112, 595, 190, UI_BORDER);
	setfontsize(11);
	setfontcolour(UI_ACCENT_TEXT);
	DrawText(58, 136, "SOURCE");
	DrawText(325, 136, "DESTINATION");
	setfontsize(14);
	setfontcolour(UI_TEXT);
	snprintf(progress, sizeof(progress), "%.30s", ui_transfer_source);
	DrawText(58, 166, progress);
	snprintf(progress, sizeof(progress), "%.30s", ui_transfer_destination);
	DrawText(325, 166, progress);

	/* The share completed leads, because it is what the user waits on. */
	if (determinate)
		snprintf(progress, sizeof(progress), "%d%%", 100 * current / total);
	else
		snprintf(progress, sizeof(progress), "Working...");
	setfontsize(30);
	setfontcolour(UI_TEXT);
	DrawText(-1, 258, progress);

	DrawBoxFilled(78, 282, 560, 312, UI_PANEL_ALT);
	DrawBox(78, 282, 560, 312, UI_BORDER);
	if (determinate && filled > 0)
		DrawBoxFilled(80, 284, 80 + filled, 310, UI_ACCENT);

	setfontsize(12);
	setfontcolour(UI_MUTED);
	if (determinate) {
		snprintf(progress, sizeof(progress), "%d of %d", current, total);
		DrawText(-1, 336, progress);
	}
	ui_footer_notice("Do not remove the memory card or storage device.");
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
	static const ui_hint hints[] = {
		{ "B", "Close" }
	};

	ui_wait_release();
	ui_begin("Controls", "Same controls throughout GCMM-EX");
	setfontsize(14);
	setfontcolour(UI_TEXT);
	DrawText(58, 130, "D-pad / Analog    Navigate");
	DrawText(58, 160, "A                 Select / confirm");
	DrawText(58, 190, "B                 Back / cancel");
	DrawText(58, 220, "X / Y             Options / mark");
	DrawText(58, 250, "L / R             Previous / next device");
	DrawText(58, 280, "START             Help");
#ifdef HW_RVL
	DrawHLine(58, 579, 306, UI_BORDER);
	setfontsize(12);
	setfontcolour(UI_ACCENT_TEXT);
	DrawText(58, 330, "WII REMOTE AND CLASSIC CONTROLLER");
	setfontcolour(UI_MUTED);
	DrawText(58, 354, "+ / -             Options / mark");
	DrawText(58, 376, "1 / 2             Previous / next device");
	DrawText(58, 398, "HOME              Help");
	setfontsize(14);
#endif
	ui_footer_hints(hints, 1, NULL);
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
