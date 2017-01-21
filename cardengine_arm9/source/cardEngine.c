/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <nds.h> 
#include <nds/fifomessages.h>
#include "cardEngine.h"

extern vu32* volatile cardStruct;
//extern vu32* volatile cacheStruct;
extern u32 sdk_version;
vu32* volatile sharedAddr = (vu32*)0x027FFB08;
extern volatile int (*readCachedRef)(u32*); // this pointer is not at the end of the table but at the handler pointer corresponding to the current irq
u32 currentSector = 0;


u32 cardId (void) {
	nocashMessage("\narm9 cardId\n");

	return	1;
}

void cardRead (u32* cacheStruct) {
	nocashMessage("\narm9 cardRead\n");	
	
	u32 commandRead;
	u32 src = cardStruct[0];
	u32 len = cardStruct[1];
	u32* dst = (u32*) cardStruct[2];
	
	u32 page = (src/512)*512;
	
	u32 sector = (src/0x8000)*0x8000;
	
	// send a log command for debug purpose
	// -------------------------------------
	commandRead = 0x026ff800;	
	
	sharedAddr[0] = dst;
	sharedAddr[1] = len;
	sharedAddr[2] = src;
	sharedAddr[3] = commandRead;
	
	IPC_SendSync(0xEE24);
	
	while(sharedAddr[3] != (vu32)0);
	// -------------------------------------

	
	if(page == src && len > 0x8000 && dst < 0x02700000 && dst > 0x02000000 && ((u32)dst)%4==0) {
		// read directly at arm7 level
		commandRead = 0x025FFB08;
		
		cacheFlush();
		
		sharedAddr[0] = dst;
		sharedAddr[1] = len;
		sharedAddr[2] = src;
		sharedAddr[3] = commandRead;
		
		IPC_SendSync(0xEE24);
		
		while(sharedAddr[3] != (vu32)0);
		
	} else {
		// read via the WRAM cache
		while(len > 0) {
			// read max 32k via the WRAM cache
			if(!currentSector || sector != currentSector) {
				// send a command to the arm7 to fill the WRAM cache
				commandRead = 0x027ff800;
				
				// transfer the WRAM-B cache to the arm7
				REG_MBK_2=0x8185898D;
				REG_MBK_3=0x9195999D;
				
				// write the command
				sharedAddr[0] = 0x03740000;
				sharedAddr[1] = 0x8000;
				sharedAddr[2] = sector;
				sharedAddr[3] = commandRead;
				
				IPC_SendSync(0xEE24);	

				while(sharedAddr[3] != (vu32)0);	
				
				// transfer back the WRAM-B cache to the arm9
				REG_MBK_2=0x8084888C;
				REG_MBK_3=0x9094989C;
				
				currentSector = sector;
			}			
			
			if(len>512 && len % 32 == 0) {
				// copy directly
				fastCopy32(0x03740000-sector+src,dst,len);
			} else {
				bool remainToRead = true;
				u32 src2 = cardStruct[0];
				while (remainToRead && (src2-sector < 0x8000) ) {
					u32 src2 = cardStruct[0];
					u32 len2 = cardStruct[1];
					// read via the 512b ram cache
					u32* cacheBuffer = cacheStruct + 0x20;
					u32* cachePage = cacheStruct + 8;
					fastCopy32(0x03740000+src2-sector, cacheBuffer, 512);
					*cachePage = page;
					remainToRead = (*readCachedRef)(cacheStruct);
				}
			}
			if(len < 0x8000) len =0;
			else {
				// bigger than 32k unaligned command
				len = len - 0x8000 + (src-sector) ;
				src = sector + 0x8000;
				dst = dst + 0x8000 - (src-sector);
				sector = page = src;
			}			
		}
	}	
}



