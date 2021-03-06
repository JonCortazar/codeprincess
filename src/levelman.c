//-----------------------------LICENSE NOTICE------------------------------------
//  This file is part of CodePrincess: A CPCtelera game made for #bitbitjam3
//  Copyright (C) 2016 ronaldo / Fremos / Cheesetea / ByteRealms (@FranGallegoBR)
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include <cpctelera.h>
#include "entityman.h"
#include "levels/level0.h"
#include "tiles/tileset0.h"
#include "tiles/tileset1.h"

#define SWITCH_PTR(P1, P2) { void* auxp; auxp=(void*)(P1); (P1)=(P2); (P2)=auxp; }

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
/// INTERNAL STRUCTURES
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

typedef enum {
     LS_RedrawBg    = 0x80   // Flag signaling background redraw
   , LS_RedrawFloor = 0x40   // Flag signaling floor redraw
} TLevelStatus;

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
/// DATA
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

u8   m_backgroundMaps[2][20*17]; // 2 Tilemaps for present background
u8*  m_bgMapPtr[2];              // Pointers to the 2 background maps for switching
u8** m_bgTilPtr[2];              // Pointers to the 2 background tilesets 
u8   m_levelStatus;              // Flags controlling level status (76543210 > 7-6: redraw Floor, Bg, 1-0: Level number)
u8*  m_levelMap;                 // Map of the present level
u16  m_levelOffset;              // Offset of the hero into the level (for scrolling background)

// Tilesets
u8* const g_tileset0[16] = {g_tileset0_00, g_tileset0_01, g_tileset0_02, g_tileset0_03, g_tileset0_04, g_tileset0_05, g_tileset0_06, g_tileset0_07, g_tileset0_08, g_tileset0_09, g_tileset0_10, g_tileset0_11, g_tileset0_12, g_tileset0_13, g_tileset0_14, g_tileset0_15};
u8* const g_tileset1[16] = {g_tileset1_00, g_tileset1_01, g_tileset1_02, g_tileset1_03, g_tileset1_04, g_tileset1_05, g_tileset1_06, g_tileset1_07, g_tileset1_08, g_tileset1_09, g_tileset1_10, g_tileset1_11, g_tileset1_12, g_tileset1_13, g_tileset1_14, g_tileset1_15};

// Level Tileset Layouts
u8** const m_levelTilesets[2][16] = {
     { g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1 }
   , { g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1, g_tileset0, g_tileset1 }
};

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
/// FUNCTIONS
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
/// fillBG
///   Fills in the background with some level tiles
///////////////////////////////////////////////////////////////
void fillBg(u8* bg, u16 idx) {
   u8 i, j;
   for(i=0; i < 17;i++) {
      for(j=0; j < 20;j++) {
         *bg = cpct_get4Bits(m_levelMap, idx);
         bg++;
         idx++;
      }
      idx += 320 - 20;
   }
}

///////////////////////////////////////////////////////////////
/// drawBg
///   Draws the background completely
///////////////////////////////////////////////////////////////
void drawBg() {
   u8    *scr = cpctm_screenPtr(CPCT_VMEM_START, 40, 40);
   u8       i = 2;

   while (i--) {
      cpct_etm_setTileset2x4  (m_bgTilPtr[i]);
      cpct_etm_drawTilemap2x4 (20, 17, scr, m_bgMapPtr[i]);
      scr -= 40;
//      cpct_etm_setTileset2x4(g_tileset0);
//      cpct_etm_drawTilemap2x4 (20, 17, SCR_P1, m_bgMapPtr[0]);
//      cpct_etm_setTileset2x4(g_tileset1);
//      cpct_etm_drawTilemap2x4 (20, 17, SCR_P2, m_bgMapPtr[1]);
   }
}

///////////////////////////////////////////////////////////////
/// LM_redrawBackgroundBox
///   Redraws part of the background as a defined box
///////////////////////////////////////////////////////////////
void LM_redrawBackgroundBox(u8 x, u8 y, u8 w, u8 h) {
   u8 h_up, h_down, h_start;
   // Check if I'm ocluding the wall
   if (y < 108) {
      // General calculations
      u8 tx1, tx2, tw1 = 0, tw2 = 0;
      u8 ty   = (y - 40) >> 2;
      u8 tw   = (w >> 1) + (x & 1);
      h_up    = 108 - y;
      h_down  = (h_up > h) ? 0 : h - h_up;
      h_start = 108;
      h_up    = (h_up >> 2) + (h_up & 3 ? 1 : 0);

      // Check which walls I am ocluding
      if (x < 40 - w) {
         tx1 = x >> 1;
         tw1 = tw;
      } else if (x >= 40) {
         tx2 = (x - 40) >> 1;
         tw2 = tw;
      } else {
         tx1 = x >> 1;
         tx2 = 0;
         tw1 = 20 - tx1;
         tw2 = tw - tw1;
      }

      // Redraw walls
      if (tw1) {
         cpct_etm_setTileset2x4 (m_bgTilPtr[0]);
         cpct_etm_drawTileBox2x4(tx1, ty, tw1, h_up, 20
                                 , cpctm_screenPtr(CPCT_VMEM_START,  0, 40)
                                 , m_bgMapPtr[0]);
      }
      if (tw2) {
         cpct_etm_setTileset2x4 (m_bgTilPtr[1]);
         cpct_etm_drawTileBox2x4(tx2, ty, tw2, h_up, 20
                                 , cpctm_screenPtr(CPCT_VMEM_START, 40, 40)
                                 , m_bgMapPtr[1]);

      }
   } else {
      // Not ocluding the wall, only terrain
      h_down  = h;
      h_start = y;
   }

   // Terrain part
   if (h_down) {
      u8* scr = cpct_getScreenPtr(CPCT_VMEM_START, x, h_start);
      cpct_drawSolidBox(scr, 0x33, w, h_down);
   }
}

///////////////////////////////////////////////////////////////
/// LM_initialize
///   Initializes the level manager object
///////////////////////////////////////////////////////////////
void LM_initialize() {
   // At first, redraw floor and Background is required
   // And select level 0 (2 LSbits = 0)
   m_levelStatus  = LS_RedrawFloor | LS_RedrawBg;
   
   // Set level, offset and background pointers
   m_levelMap     = g_level0;
   m_levelOffset  = 0;
   m_bgMapPtr[0]  = m_backgroundMaps[0];
   m_bgMapPtr[1]  = m_backgroundMaps[1];
   m_bgTilPtr[0]  = m_levelTilesets[0][0];
   m_bgTilPtr[1]  = m_levelTilesets[0][1];

   // Fill in present background part of the level map
   fillBg(m_bgMapPtr[0], 0);
   fillBg(m_bgMapPtr[1], 20);
}

///////////////////////////////////////////////////////////////
/// LM_scrollRight
///   Scrolls map 1 place to the right
///////////////////////////////////////////////////////////////
void LM_scrollRight() {
   // Move right part of the map to the left switching pointers
   // Then offset the map and fill the right part with new background
   m_levelOffset++;
   SWITCH_PTR(m_bgMapPtr[0], m_bgMapPtr[1]);
   m_bgTilPtr[0] = m_bgTilPtr[1];
   m_bgTilPtr[1] = m_levelTilesets[m_levelStatus & 3][m_levelOffset+1];
   fillBg(m_bgMapPtr[1], 20*(m_levelOffset+1));

   // Mark for redrawing
   m_levelStatus |= LS_RedrawBg;

   // Scroll all entities 40 points left (80 pixels) left
   EM_scroll(-40);

   // TEST
   EM_createEntity(60, 80, E_Agent);
}

///////////////////////////////////////////////////////////////
/// LM_update
///   Updates the state of the level
///////////////////////////////////////////////////////////////
void LM_update(u8 hero_x) {
   // Check for scroll
   if (hero_x > 68 && m_levelOffset < 14) {
      LM_scrollRight();
   }
}

///////////////////////////////////////////////////////////////
/// LM_draw
///   Draws the level and redraws background when required
///////////////////////////////////////////////////////////////
void LM_draw() {
   // Redraw floor when required
   if (m_levelStatus & LS_RedrawFloor) {
      cpct_drawSolidBox(cpctm_screenPtr(CPCT_VMEM_START,  0, 108)
                      , 0x33, 40, 92);
      cpct_drawSolidBox(cpctm_screenPtr(CPCT_VMEM_START, 40, 108)
                      , 0x33, 40, 92);
      m_levelStatus ^= LS_RedrawFloor;
   }
   
   // Redraw Background when required
   if (m_levelStatus & LS_RedrawBg) {
      drawBg();
      m_levelStatus ^= LS_RedrawBg;
   }
}
