/* 
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) 2007-2009  The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "lvllib.h"

#include "file.h"
#include "opentyr.h"

JE_LvlPosType lvlPos;

char levelFile[13]; /* string [12] */
JE_word lvlNum;

void JE_analyzeLevel(void)
{
	FILE *f = dir_fopen_die(data_dir(), levelFile, "rb");

	fread_u16_die(&lvlNum, 1, f);

	fread_s32_die(lvlPos, lvlNum, f);
	
	lvlPos[lvlNum] = (JE_longint)ftell_eof(f);

	efclose(f);
}
