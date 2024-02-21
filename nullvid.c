/*  Sarien - A Sierra AGI resource interpreter engine
 *  Copyright (C) 1999-2001 Stuart George and Claudio Matsuoka
 *  
 *  $Id: pcvga.c,v 1.5 2002/11/16 01:20:21 cmatsuoka Exp $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as pufblished by
 *  the Free Software Foundation; see docs/COPYING for further details.
 */

#define INCL_DOS
#define INCL_WIN
#define INCL_GPI

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
//////////////////////////////////////////////////////////////
#define NUM_MASSES_X	320u
#define NUM_MASSES_Y	200u

HAB hab;
HAB habt;
HWND hwndFrame, hwndClient;
HDC hdc, hdcMemory;
HPS hps, hpsMemory;
HMTX hmtxLock;
TID tidModel;
BOOL ModelSuspended = FALSE;
HBITMAP hbm;
BITMAPINFOHEADER2 bmih;
BITMAPINFOHEADER2 bmp2data;
PBITMAPINFO2 pbmi;
BYTE RGBmap[32];
BYTE Bitmap[NUM_MASSES_X*NUM_MASSES_Y];
POINTL aptl[3] =
//  { {0, NUM_MASSES_Y}, {NUM_MASSES_X, 0u}, {0u, NUM_MASSES_Y} };	null
//  {  {NUM_MASSES_X, NUM_MASSES_Y}, {0u, 0u}, {NUM_MASSES_X, NUM_MASSES_Y} }; null
    { {0u, 0u}, {NUM_MASSES_X, NUM_MASSES_Y}, {0u, 0u} };

MRESULT EXPENTRY window_func(HWND, ULONG, MPARAM, MPARAM);
void Model(ULONG);
void PrepareGraphics(BYTE *);
void DisplayPlane(float **current);


//////////////////////////////////////////////////////////////

#include "sarien.h"
#include "graphics.h"

extern struct gfx_driver *gfx;
extern struct sarien_options opt;

UINT8	*exec_name;
static UINT8	*screen_buffer;

static int	pc_init_vidmode	(void);
static int	pc_deinit_vidmode	(void);
static void	pc_put_block		(int, int, int, int);
static void	pc_put_pixels		(int, int, int, UINT8 *);
static void	pc_timer		(void);
static int	pc_get_key		(void);
static int	pc_keypress		(void);


#define TICK_SECONDS 18

static struct gfx_driver gfx_pcvga = {
	pc_init_vidmode,
	pc_deinit_vidmode,
	pc_put_block,
	pc_put_pixels,
	pc_timer,
	pc_keypress,
	pc_get_key
};

static void pc_timer ()
{
#if 0
	static UINT32 cticks = 0;

	while (cticks == clock_ticks);
	cticks = clock_ticks;
#else
clock_ticks++;
#endif
}


int init_machine ()	//(int argc, char **argv)
{
	gfx = &gfx_pcvga;
opt.cgaemu = TRUE;
	return err_OK;
}


int deinit_machine ()
{
	return err_OK;
}


static int pc_init_vidmode ()
{
	int i;

	clock_count = 0;
	clock_ticks = 0;

	screen_buffer = calloc (GFX_WIDTH, GFX_HEIGHT);

	return err_OK;
}


static int pc_deinit_vidmode ()
{
	free (screen_buffer);

	return err_OK;
}


/* blit a block onto the screen */
static void pc_put_block (int x1, int y1, int x2, int y2)
{
  int x, y;
  int disp_val;

  for (y = 0; y < NUM_MASSES_Y; y++)
  {
    for (x = 0; x < NUM_MASSES_X; x++)
    {
      disp_val = ((int) screen_buffer[y*NUM_MASSES_X+x]);	//+ 16);
      if (disp_val > 32) disp_val = 32;
      else if (disp_val < 0) disp_val = 0;
      Bitmap[((NUM_MASSES_Y-y)*(NUM_MASSES_X))-(NUM_MASSES_X-x)] = RGBmap[disp_val];
    }
  }

  DosRequestMutexSem(hmtxLock, SEM_INDEFINITE_WAIT);

  /* This is the key to the speed. Instead of doing a GPI call to set the
     color and a GPI call to set the pixel for EACH pixel, we get by
     with only two GPI calls. */
  GpiSetBitmapBits(hpsMemory, 0L, (LONG) (NUM_MASSES_Y-2), &Bitmap[0], pbmi);
  GpiBitBlt(hps, hpsMemory, 3L, aptl, ROP_SRCCOPY, BBO_AND);

  DosReleaseMutexSem(hmtxLock);
}


static void pc_put_pixels(int x, int y, int w, UINT8 *p)
{
  UINT8 *s;
  for (s = &screen_buffer[y * 320 + x]; w--; *s++ = *p++);
}


static int pc_keypress ()
{
}


static int pc_get_key ()
{
	UINT16 key;

	key=0;
	return key;
}

int OLDmain (int argc, char *argv[]);
//////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
  HMQ hmq;
  QMSG qmsg;
  ULONG flFlags;
  unsigned char class[]="MyClass";

  flFlags = FCF_TITLEBAR |
            FCF_MINBUTTON |
            FCF_TASKLIST |
            FCF_SYSMENU;

  if ((hab = WinInitialize(0)) == 0)
  {
    printf("Error doing WinInitialize()\n");
    exit(1);
  }
  if ((hmq = WinCreateMsgQueue(hab, 0)) == (HMQ) NULL)
  {
    printf("Error doing WinCreateMsgQueue()\n");
    exit(1);
  }
  if (!WinRegisterClass(hab, (PSZ) class, (PFNWP) window_func,
                        CS_SIZEREDRAW, 0))
  {
    printf("Error doing WinRegisterClass()\n");
    exit(1);
  }
  if ((hwndFrame = WinCreateStdWindow(HWND_DESKTOP, WS_VISIBLE, &flFlags,
                                  (PSZ) class, (PSZ) "5-Point Wave Equation",
                                  WS_VISIBLE, 0, 0, &hwndClient)) == 0)
  {
    printf("Error doing WinCreateStdWindow()\n");
    exit(1);
  }
  WinSetWindowPos(hwndFrame, 0L,
                  (SHORT) (60),
                  (SHORT) (WinQuerySysValue(HWND_DESKTOP,
				            SV_CYSCREEN) - (NUM_MASSES_Y+60)),
                  (SHORT) NUM_MASSES_X,
		  (SHORT) NUM_MASSES_Y + WinQuerySysValue(HWND_DESKTOP,
			SV_CYTITLEBAR) - 2, SWP_SIZE | SWP_MOVE);

  /* Work out mapping from bitmap color table to our logical palette. */
  PrepareGraphics(RGBmap);

  while (WinGetMsg(hab, &qmsg, (HWND) NULL, 0, 0))
  {
    WinDispatchMsg(hab, &qmsg);
  }
  WinDestroyWindow(hwndFrame);
  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);
}
//////////////////////////////////////////////////////////////


MRESULT EXPENTRY
window_func(HWND handle, ULONG mess, MPARAM parm1, MPARAM parm2)
{
  LONG palette[33];
  int i;
  LONG j;
  SIZEL sizl;

  switch(mess)
  {
    case WM_CREATE:
      /* Create presentation space for screen. */
      hdc = WinOpenWindowDC(handle);
      sizl.cx = 0;
      sizl.cy = 0;
      hps = GpiCreatePS(hab, hdc, &sizl,
		PU_PELS | GPIT_MICRO | GPIA_ASSOC | GPIF_DEFAULT);

      /* Create presentation space for memory image of screen. */
      hdcMemory = DevOpenDC(hab, OD_MEMORY, (PSZ) "*", 0L, 0L, 0L);
      sizl.cx = 0;
      sizl.cy = 0;
      hpsMemory = GpiCreatePS(hab, hdcMemory, &sizl,
		PU_PELS | GPIT_MICRO | GPIA_ASSOC | GPIF_DEFAULT);

      /* Create bitmap for memory image of screen. */
      memset(&bmih, 0, sizeof(bmih));
      bmih.cbFix = sizeof(bmih);
      bmih.cx = NUM_MASSES_X;
      bmih.cy = NUM_MASSES_Y;
      bmih.cPlanes = 1;
      bmih.cBitCount = 8;
      hbm = GpiCreateBitmap(hpsMemory, &bmih, 0L, NULL, NULL);
      GpiSetBitmap(hpsMemory, hbm);

      /* Set up gray-scale palette for screen image. */
      for (i = 0; i < 32; i++)
      {
        j = i << 3;
        palette[i] = (j << 16) | (j << 8) | j;
      }
      palette[32] = 0x00ffffffL;
      GpiCreateLogColorTable(hpsMemory, (ULONG) LCOL_PURECOLOR,
		(LONG) LCOLF_CONSECRGB, (LONG) 0L, (LONG) 33L, (PLONG) palette);
      GpiSetBackMix(hpsMemory, BM_OVERPAINT);

      /* Create a semaphore to control access to the memory image
         presentation space. Only one thread can perform Gpi operations
         on it at a time. */
      DosCreateMutexSem("\\sem32\\Lock", &hmtxLock, 0, FALSE);

      /* Create a thread to run the system model. */
//      DosCreateThread(&tidModel, Model, 0UL, 0UL, 4096);
DosCreateThread(&tidModel,OLDmain, 0UL, 0UL, 4096);

      /* Take the input focus. */
      WinFocusChange(HWND_DESKTOP, handle, 0L);
      break;

    case WM_ERASEBACKGROUND:
    case WM_PAINT:
      /* Copy the memory image of the screen out to the real screen. */
      DosRequestMutexSem(hmtxLock, SEM_INDEFINITE_WAIT);
      WinBeginPaint(handle, hps, NULL);
      GpiBitBlt(hps, hpsMemory, 3L, aptl, ROP_SRCCOPY, BBO_AND);
      WinEndPaint(hps);
      DosReleaseMutexSem(hmtxLock);
      break;

    case WM_CHAR:
      if (SHORT1FROMMP(parm1) & KC_KEYUP)
        break;
      if (SHORT2FROMMP(parm2) == VK_PAUSE)
      {
        if (ModelSuspended == TRUE)
        {
          ModelSuspended = FALSE;
        }
        else
        {
          ModelSuspended = TRUE;
        }
      }
      break;
    default:
      return WinDefWindowProc(handle, mess, parm1, parm2);
  }
  return (MRESULT) FALSE;
}


void
PrepareGraphics(BYTE *RGBmap)
{
  POINTL coords;
  int x, y;

  /* Give thread access to Gpi. */
  habt = WinInitialize(0);

  /* Determine mapping from logical color value to bitmap color table
     index.  Anybody know a more direct way??? */
  DosRequestMutexSem(hmtxLock, SEM_INDEFINITE_WAIT);
  for (x = 0, y = 0; x < 33; x++)
  {
      GpiSetColor(hpsMemory, (LONG) x);
      coords.x = x;
      coords.y = y;
      GpiSetPel(hpsMemory, &coords);
  }
  bmp2data.cbFix = 16L;
  GpiQueryBitmapInfoHeader(hbm, &bmp2data);
  DosAllocMem((PPVOID)&pbmi, sizeof(BITMAPINFO2) +
		(sizeof(RGB2) * (1 << bmp2data.cPlanes) *
                (1 << bmp2data.cBitCount)),
                PAG_COMMIT | PAG_READ | PAG_WRITE);
  pbmi->cbFix = bmp2data.cbFix;
  pbmi->cx = bmp2data.cx;
  pbmi->cy = bmp2data.cy;
  pbmi->cPlanes = bmp2data.cPlanes;
  pbmi->cBitCount = bmp2data.cBitCount;
  pbmi->ulCompression = bmp2data.ulCompression;
  pbmi->cbImage = bmp2data.cbImage;
  pbmi->cxResolution = bmp2data.cxResolution;
  pbmi->cyResolution = bmp2data.cyResolution;
  pbmi->cclrUsed = bmp2data.cclrUsed;
  pbmi->cclrImportant = bmp2data.cclrImportant;
  pbmi->usUnits = bmp2data.usUnits;
  pbmi->usReserved = bmp2data.usReserved;
  pbmi->usRecording = bmp2data.usRecording;
  pbmi->usRendering = bmp2data.usRendering;
  pbmi->cSize1 = bmp2data.cSize1;
  pbmi->cSize2 = bmp2data.cSize2;
  pbmi->ulColorEncoding = bmp2data.ulColorEncoding;
  pbmi->ulIdentifier = bmp2data.ulIdentifier;
  GpiQueryBitmapBits(hpsMemory, 0L, NUM_MASSES_Y-2, &Bitmap[0], pbmi);
  DosReleaseMutexSem(hmtxLock);
  for (x = 0; x < 33; x++)
  {
    RGBmap[x] = Bitmap[x];
  }
}




	