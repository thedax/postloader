/*******************************************************************************
 * config.c 
 *
 * Copyright (c) 2009 The Lemon Man
 * Copyright (c) 2009 Nicksasa
 * Copyright (c) 2009 WiiPower
 *
 * Distributed under the terms of the GNU General Public License (v2)
 * See http://www.gnu.org/licenses/gpl-2.0.txt for more info.
 *
 * Description:
 * -----------
 * Config file
 *
 ******************************************************************************/

#include <stdio.h>
#include <string.h>

#include "config.h"

void Set_Config_to_Defaults()
{
	videooption = 0; 
	languageoption = -1; // -1 consolde default
	videopatchoption = 0;
	
	hooktypeoption = 0;
	debuggeroption = 0;
	ocarinaoption = 0;
	
	bootmethodoption = 0;
}

