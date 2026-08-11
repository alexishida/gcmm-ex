/****************************************************************************
* FreeType font
****************************************************************************/
#ifndef _FTFONTS_
#define _FTFONTS_


//Uncomment this definiton to show some debug values on screen
//#define DEBUG_VALUES

//Uncomment this definiton to use time lapse for icon duration instead of retrace count
//Time code is mantained as an alternative example of animating the icons
//#define USE_TIME

#ifdef HW_RVL
void initialise_power() ;
#endif

#define COL_BG2 getcolour(64,64,64)
#define COL_FONT 255,255,255
#define COL_FONT_STATUS COL_FONT

#define COL_BLACK getcolour(0,0,0)


int FT_Init ();
void setfontsize (int pixelsize);
void setfontcolour (u8 r, u8 g, u8 b);
void DrawText (int x, int y, char *text);
unsigned int getcolour(u8 r1, u8 g1, u8 b1);
//extern void ClearScreen ();
void ShowScreen ();
void ShowAction (char *msg);
void WaitPrompt (char *msg);
void writeStatusBar(char *line1, char *line2);
/****************************************************************************
 *  Draw functions - lines, boxes
 ****************************************************************************/
void DrawHLine (int x1, int x2, int y, int color);
void DrawVLine (int x, int y1, int y2, int color);
void DrawBox (int x1, int y1, int x2, int y2, int color);
void DrawBoxFilled (int x1, int y1, int x2, int y2, int color);
void DrawBoxFilledGradient (int x1, int y1, int x2, int y2, u32 color1, u32 color2, float location);


#endif
