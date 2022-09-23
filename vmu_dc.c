/*
FrNES VMU Support-
Adapted from the NesterDC source
*/

#include "vmu_dc.h"
#include "macros.h"

const char FILENAME_STRING[] = "FrNES .60   ";

//Code hack so that I don't need threads to make a stable build :(
//Sux0rs
uint16 nullfunc()
{
	int i;

	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	for (i = 0; i < 255; i++);
	return 0;
}

// This rouine came from Marcus's website
// http://mc.pp.se/dc/vms/fileheader.html
uint16 calcCRC(const unsigned char *buf, int size)
{
  int i;
  int c;
  int n = 0;

  for (i = 0; i < size; i++)
  {
    n ^= (buf[i]<<8);
    for (c = 0; c < 8; c++)
      if (n & 0x8000)
	n = (n << 1) ^ 4129;
      else
	n = (n << 1);
  }
  return (n & 0xffff);
}

uint16 get_fat_block (uint8 vmu_addr, uint16 block_number)
{
	uint16 fatblock;
	
	uint8 buf[512];
	uint16 *buf16 = (uint16*)buf;

	//Make sure that we've been passed a valid vmu address
	if (!(vmu_addr))
		return 0;

	//Read the Root block
	if (vmu_block_read(vmu_addr, 255, buf))
		return 0;

	timer_spin_sleep (30);
	
	//fatblock is the location of the FAT
	fatblock = buf16[0x46/2];

	//Read the FAT in 
	if (vmu_block_read(vmu_addr, fatblock, buf) != 0)
		return 0;

	timer_spin_sleep (30);

	//returns the directory entry in the FAT
	return buf16[block_number];
}

uint16 find_file (uint8 vmu_addr, char* filename)
{
	int i;
	int j;
	int n;
	uint8 buf[512];
	uint8 buf2[512];
	uint8* ent;
	uint16* buf16 = (uint16*)buf;
	uint16 dirblock;
	uint16 dirlength;
	dirent_vmu* vmu_val;
	int number;
	file_hdr_vmu* hdr_ptr;

	//Make sure we have a VMU
	if (!(vmu_addr))
		return 0;

	// Read the root block and find out where the directory is located
	if (vmu_block_read(vmu_addr, 255, buf))
		return 0;

	//change this
	timer_spin_sleep (30);

	dirblock = buf16[0x4a/2];
	dirlength = buf16[0x4c/2];

	// Draw entries and look for the ICONDATA.VMS entry
	for (n = dirlength; n > 0; n--)
	{
		// Read one dir block
		if (vmu_block_read(vmu_addr, dirblock, buf) != 0)
			return 0;

		//
		timer_spin_sleep (30);

		// Each block has 16 entries
		for (i = 0; i < 16; i++)
		{
			ent = buf+i*32;
			vmu_val = (dirent_vmu *)ent;

			if (!(*ent))
				continue;

			number = 0;
			for (j = 0; j < 12; j++)
				if ((*vmu_val).filename[j] == filename[j])
					number++;

			if (number == 12)
			{
				hdr_ptr = (file_hdr_vmu *)buf2;

				if (vmu_block_read(vmu_addr, (*vmu_val).firstblk, buf2))
				{
					//printf("Can't read VMU root block\r\n");
					return 0;
				}
				timer_spin_sleep (30);

				return ((*vmu_val).firstblk);
			}
		}
		dirblock--;
	}

	return 0;
}

void update_fat (uint8 vmu_addr, uint8 *free_mem, uint8 blocks_left)
{
	uint16 fatblock;
	int i;
	uint8 buf[512];
	uint16 *buf16 = (uint16*)buf;
	uint16 this_block = 0;
	uint8 max_block = blocks_left;

	if (!(vmu_addr))
		return;

	if (vmu_block_read(vmu_addr, 255, buf))
		return;

	timer_spin_sleep (30);
	
	fatblock = buf16[0x46/2];

	if (vmu_block_read(vmu_addr, fatblock, buf) != 0)
	{
		return;
	}
	timer_spin_sleep (30);

	for (i = 0; i < 200; i++)
	{
		if (free_mem[199-i] == 1)
		{
			// first file block
			if (blocks_left == max_block)
				this_block = 199 - i;

			// need the next one
			if (blocks_left < max_block)
			{
				buf16[this_block] = 199 - i;
				this_block = 199 - i;
			}

			blocks_left--; 
		}
		// last file block
		if (blocks_left == 0)
		{ 
			buf16[this_block] = 0xfffa;
			break;
		}
	}
	if (vmu_block_write (vmu_addr, fatblock, (uint8 *)buf16) != 0)
	{
//		dc_print ("there was an error updating the fat");
	}
	timer_spin_sleep (30);
}

void upload_vmu_data (uint8 vmu_addr, uint8 *vmu_file, uint8 *free_mem, uint8 blocks_left)
{
	uint8 *temp_ptr = vmu_file;
	uint8 temp_p[512];
	int i;
	
	for (i = 0; i < 200; i++)
	{
		if (free_mem[199-i] == 1)
		{
			if (vmu_block_read (vmu_addr, 199 - i, temp_p) != 0)
			{
//				dc_print ("there was an error reading data");
//				print_uint32 ((uint32)blocks_left);
			}

			timer_spin_sleep (30);

			if (vmu_block_write (vmu_addr, 199 - i, temp_ptr) != 0)
			{
//				dc_print ("there was an error writing data");
//				print_uint32 ((uint32)blocks_left);
			}
			blocks_left--;
			temp_ptr += 512;
			timer_spin_sleep (30);
		}
		if (blocks_left == 0)
			break;
	}
}

void upload_data_by_block (uint8 vmu_addr, uint8 *vmu_file, uint16 block_number, uint8 blocks_left)
{
//	uint8 temp_p[512];
	uint8 *temp_ptr = vmu_file;
	int i;
	
	for (i = 0; i < blocks_left; i++)
	{
		vmu_block_write (vmu_addr, block_number, temp_ptr);
		timer_spin_sleep (30);
	
		temp_ptr += 512;

		block_number = get_fat_block (vmu_addr, block_number);
	}
}

void update_vmu_dir (uint8 vmu_addr, uint16 block, const char *filename, uint8 blocks)
{
	int i;
	int j;
	int n;
	uint8 *ent;
	uint8 buf[512];
	uint16 *buf16 = (uint16*)buf, dirblock, dirlength;
	dirent_vmu real_dir_entry;
	dirent_vmu* dir_entry = &real_dir_entry;
	uint8 *temp_ptr;

	if (!(vmu_addr))
	{
		return;
	}

	// Read the root block and find out where the directory is located
	if (vmu_block_read(vmu_addr, 255, buf))
	{
//		dc_print ("there was an error reading the root block");
		//printf("Can't read VMU root block\r\n");
	}
	timer_spin_sleep (30);

	dirblock = buf16[0x4a/2];
	dirlength = buf16[0x4c/2];

	create_dir_vmu (dir_entry, filename, block, blocks); 

	// Draw entries and look for the ICONDATA.VMS entry
	for (n = dirlength; n > 0; n--)
	{
		// Read one dir block
		if (vmu_block_read(vmu_addr, dirblock, buf) != 0)
		{
		//	dc_print ("there was an error reading the dirblock");
		//	printf("Can't read VMU block %d\r\n", dirblock);
			return;
		}
		timer_spin_sleep (30);

		// Each block has 16 entries
		for (i=0; i < 16; i++)
		{
			ent = buf+i*32;
			if (!*ent)
			{	
				temp_ptr = (uint8 *)dir_entry;

				for (j = 0; j < 32; j++)
					ent[j] = temp_ptr[j];

				if (vmu_block_write(vmu_addr, dirblock, buf) != 0)
				{
//					dc_print ("there was an error updating the directory"); 	
				}
				vmu_block_read(vmu_addr, dirblock, buf);
				timer_spin_sleep (30);
				return;
			}
		}
		dirblock--;
	}

}

void do_crc (uint8 *buffer, uint16 bytes)
{
	uint16 crc = calcCRC (buffer, bytes);		

	file_hdr_vmu* vmu_hdr = (file_hdr_vmu *)buffer;
	(*vmu_hdr).crc = crc;
}

uint8 check_free_blocks (uint8 vmu_addr, uint8 *free_mem)
{
	uint16 fatblock;
	
	uint8 buf[512];
	uint16 *buf16 = (uint16*)buf;

	uint8 free_blocks = 0;
	int i;

	if (vmu_block_read(vmu_addr, 255, buf))
	{
		return 0;
	}
	timer_spin_sleep (30);
	
	fatblock = buf16[0x46/2];

	if (vmu_block_read(vmu_addr, fatblock, buf) != 0)
	{
		return 0;
	}
	timer_spin_sleep (30);

	for (i = 0; i < 200; i++)
	{
		if (buf16[i] == 0xfffc)
		{
			free_blocks++;
			free_mem[i] = 1;
		}
		else
		{
		//	print_uint32 ((uint32)buf16[i]);
		//	timer_spin_sleep (3000);
		}
	}

	return free_blocks;
}

uint16 find_first_free_block (uint8 *fat_table)
{
	uint16 first_free = 0;
	int i;
	for (i = 0; i < 200; i++)
	{
		if (fat_table[i] == 1)
			first_free = i;	
	}
	return first_free;
}

void create_dir_vmu (dirent_vmu *dir_entry, const char *rom_name, uint16 first_free_block, uint8 blocks)
{
	uint8 *temp_ptr;
	int i;
	uint8 filename_size;

	temp_ptr = (uint8*)dir_entry;

	// zero out dir_entry
	for (i = 0; i < 32; i++)
	{
		temp_ptr[i] = 0x00;
	}

	filename_size = strlen(rom_name);

	for (i = 0; i < 12; i++)
	{
		if (i < filename_size)
			dir_entry->filename[i] = rom_name[i];
		else 
			dir_entry->filename[i] = ' ';
	}

//	dir_entry->filename[11] = '&';

	//strcpy (dir_entry->filename, rom_name);

	dir_entry->firstblk = first_free_block;
	dir_entry->filetype = 0x33;
	dir_entry->copyprotect = 0x00;
	dir_entry->filesize = blocks;
	dir_entry->hdroff = 0;
}

void create_vmu_header(uint8 *header, const char *rom_name, const char *desc_long, uint16 filesize)
{
	file_hdr_vmu *vmu_h = (file_hdr_vmu *)header;
	int i;
	uint8 *temp_ptr = header;

	// zero header mem
	for (i = 0; i < 640; i++)
	{
		*temp_ptr++ = 0x00;
	}

	strcpy (vmu_h->desc_short, rom_name);

	strcpy (vmu_h->desc_long, desc_long);
	strcpy (vmu_h->app_id, "FrNES v.50");

	vmu_h->icon_cnt = 1;
	vmu_h->file_bytes = filesize;

	/*
	for (i = 0; i < 512; i++)
	{
		if (i < 128)
			header[i+128] = 0;
		else if (i < 256)
			header[i+128] = 5;
		else if (i < 384)
			header[i+128] = 33;
		else
			header[i+128] = 51;
	}*/
}

// this routine came from the ghettoplay example that comes 
// with libdream
void vmu_icon_draw(const char *vmu_icon, uint8 vmu) {
	uint8 bitmap[48*32/8] = {0};
	int x, y, xi, xb;

	if (vmu_icon) {	
		for (y=0; y<32; y++)
			for (x=0; x<48; x++) {
				xi = x / 8;
				xb = 0x80 >> (x % 8);
				if (vmu_icon[(31-y)*48+(47-x)] == '+')
					bitmap[y*(48/8)+xi] |= xb;
			}
	}
	
	vmu_draw_lcd(vmu, bitmap);
}

// the save and load routines contain some nasty code.
// i should rewrite these, but they seem to be working.
int16 load_user_settings(uint8 vmu_addr)
{
	uint16 block_number;
	// used to store data read from the vmu (2 blocks)
	uint8 buf[1024];
	uint8 *buf_ptr = (uint8 *)buf;
	user_settings settings;
	user_settings* settings_ptr = &settings;

	// return if there is no vmu present
	if (!(vmu_addr))
		return -1;

	timer_spin_sleep (3000);

	block_number = find_file(vmu_addr, (char*) FILENAME_STRING);

	// a return value of 0 indicates that we didn't find it
	if (block_number == 0)
		return -1;

	// read the header vmu block
	if (vmu_block_read(vmu_addr, block_number, buf_ptr) != 0)
	{
		return -1;
	}

	timer_spin_sleep (100);
	// get the next block in the vmu for the settings file
	block_number = get_fat_block (vmu_addr, block_number);
	buf_ptr += 512;
	
	// read the next block
	if (vmu_block_read(vmu_addr, block_number, buf_ptr) != 0)
	{
		return -1;
	}
	timer_spin_sleep (100);

	settings_ptr = (user_settings *)(buf_ptr + 128);	

	// here are the settings
	*opt_P1AnalogEnabled = (*settings_ptr).opt_P1AnalogEnabled;
	*opt_P2AnalogEnabled = (*settings_ptr).opt_P2AnalogEnabled;
	*opt_P1SelectKey = (*settings_ptr).opt_P1SelectKey;
	*opt_P1AKey = (*settings_ptr).opt_P1AKey;
	*opt_P1BKey = (*settings_ptr).opt_P1BKey;
	*opt_P2SelectKey = (*settings_ptr).opt_P2SelectKey;
	*opt_P2AKey = (*settings_ptr).opt_P2AKey;
	*opt_P2BKey = (*settings_ptr).opt_P2BKey;

	*opt_Stretch = (*settings_ptr).opt_Stretch;
	*opt_Filter = (*settings_ptr).opt_Filter;
	opt_ClipVars[0] = (*settings_ptr).opt_ClipVars[0];
	opt_ClipVars[1] = (*settings_ptr).opt_ClipVars[1];
	opt_ClipVars[2] = (*settings_ptr).opt_ClipVars[2];
	opt_ClipVars[3] = (*settings_ptr).opt_ClipVars[3];
	
	*opt_SoundEnabled = (*settings_ptr).opt_SoundEnabled;
	*opt_FrameSkip = (*settings_ptr).opt_FrameSkip;
	*opt_AutoFrameSkip = (*settings_ptr).opt_AutoFrameSkip;
	*opt_DiscFormat = (*settings_ptr).opt_DiscFormat;
	*opt_SRAM = (*settings_ptr).opt_SRAM;

	GUI_OutsideWindowColor = (*settings_ptr).GUI_OutsideWindowColor;
	GUI_InsideWindowColor = (*settings_ptr).GUI_InsideWindowColor;
	GUI_TextColor = (*settings_ptr).GUI_TextColor;
	GUI_SelectedTextColor = (*settings_ptr).GUI_SelectedTextColor;

	return 1;
}

int16 save_user_settings(uint8 vmu_addr)
{
	uint8 free_blocks;
	uint8 free_mem[200];
	uint8* temp_ptr = free_mem;
	uint16 block_number;
	uint8 buf[1024];
	uint8 *vmu_file = (uint8 *)buf;
	user_settings settings;
	user_settings* settings_ptr = &settings;
	int i;
	uint8 first_free_block;

	// don't need to be here is there is no vmu
	if (!vmu_addr)
		return -1;

	//Zero the memory
	for (i = 0; i < 200; i++)
	{
		*temp_ptr++ = '0';
	}

	timer_spin_sleep (30);

	block_number = find_file(vmu_addr, (char*) FILENAME_STRING);

	// found the game in SaveRAM
	if (block_number != 0)
	{
		//dc_print ("Saving User Settings...");
//		free_blocks = check_free_blocks (vmu_addr, free_mem);

		create_vmu_header (vmu_file, "FrNES .60", "Options File", 384 - 40 + sizeof(user_settings));

		for (i = 640; i < 1024; i++)
		{
			vmu_file[i] = 0;
		}

		settings_ptr = (user_settings *)(vmu_file + 640);
		(*settings_ptr).opt_P1AnalogEnabled = *opt_P1AnalogEnabled;
		(*settings_ptr).opt_P2AnalogEnabled = *opt_P2AnalogEnabled;
		(*settings_ptr).opt_P1SelectKey = *opt_P1SelectKey;
		(*settings_ptr).opt_P1AKey = *opt_P1AKey;
		(*settings_ptr).opt_P1BKey = *opt_P1BKey;
		(*settings_ptr).opt_P2SelectKey = *opt_P2SelectKey;
		(*settings_ptr).opt_P2AKey = *opt_P2AKey;
		(*settings_ptr).opt_P2BKey = *opt_P2BKey;

		(*settings_ptr).opt_Stretch = *opt_Stretch;
		(*settings_ptr).opt_Filter = *opt_Filter;
		(*settings_ptr).opt_ClipVars[0] = opt_ClipVars[0];
		(*settings_ptr).opt_ClipVars[1] = opt_ClipVars[1];
		(*settings_ptr).opt_ClipVars[2] = opt_ClipVars[2];
		(*settings_ptr).opt_ClipVars[3] = opt_ClipVars[3];
		
		(*settings_ptr).opt_SoundEnabled = *opt_SoundEnabled;
		(*settings_ptr).opt_FrameSkip = *opt_FrameSkip;
		(*settings_ptr).opt_AutoFrameSkip = *opt_AutoFrameSkip;
		(*settings_ptr).opt_DiscFormat = *opt_DiscFormat;
		(*settings_ptr).opt_SRAM = *opt_SRAM;

		(*settings_ptr).GUI_OutsideWindowColor = GUI_OutsideWindowColor;
		(*settings_ptr).GUI_InsideWindowColor = GUI_InsideWindowColor;
		(*settings_ptr).GUI_TextColor = GUI_TextColor;
		(*settings_ptr).GUI_SelectedTextColor =	GUI_SelectedTextColor;

//		vmu_file.crc = calcCRC(vmu_file, 1024);
		do_crc (vmu_file, 1024);

//		vmu_block_write(vmu_addr, 
		upload_data_by_block (vmu_addr, vmu_file, block_number, 2);
		return 2;
	}
	else
	{
		free_blocks = check_free_blocks (vmu_addr, free_mem);

		if (free_blocks < 2)
		{
//			dc_print ("You need 2 free blocks to Save user settings");
			timer_spin_sleep (5000);
			return -1;
		}

		create_vmu_header (vmu_file, "FrNES .60", "Options File", 384 - 40 + sizeof(user_settings));

		// fill 2 blocks with zeros
		for (i = 640; i < 1024; i++)
		{
			vmu_file[i] = 0;
		}

		settings_ptr = (user_settings *)(vmu_file + 640);

		(*settings_ptr).opt_P1AnalogEnabled = *opt_P1AnalogEnabled;
		(*settings_ptr).opt_P2AnalogEnabled = *opt_P2AnalogEnabled;
		(*settings_ptr).opt_P1SelectKey = *opt_P1SelectKey;
		(*settings_ptr).opt_P1AKey = *opt_P1AKey;
		(*settings_ptr).opt_P1BKey = *opt_P1BKey;
		(*settings_ptr).opt_P2SelectKey = *opt_P2SelectKey;
		(*settings_ptr).opt_P2AKey = *opt_P2AKey;
		(*settings_ptr).opt_P2BKey = *opt_P2BKey;

		(*settings_ptr).opt_Stretch = *opt_Stretch;
		(*settings_ptr).opt_Filter = *opt_Filter;
		(*settings_ptr).opt_ClipVars[0] = opt_ClipVars[0];
		(*settings_ptr).opt_ClipVars[1] = opt_ClipVars[1];
		(*settings_ptr).opt_ClipVars[2] = opt_ClipVars[2];
		(*settings_ptr).opt_ClipVars[3] = opt_ClipVars[3];
		
		(*settings_ptr).opt_SoundEnabled = *opt_SoundEnabled;
		(*settings_ptr).opt_FrameSkip = *opt_FrameSkip;
		(*settings_ptr).opt_AutoFrameSkip = *opt_AutoFrameSkip;
		(*settings_ptr).opt_DiscFormat = *opt_DiscFormat;
		(*settings_ptr).opt_SRAM = *opt_SRAM;

		(*settings_ptr).GUI_OutsideWindowColor = GUI_OutsideWindowColor;
		(*settings_ptr).GUI_InsideWindowColor = GUI_InsideWindowColor;
		(*settings_ptr).GUI_TextColor = GUI_TextColor;
		(*settings_ptr).GUI_SelectedTextColor =	GUI_SelectedTextColor;
		
		do_crc (vmu_file, 1024);

		first_free_block = find_first_free_block(free_mem);

		update_fat (vmu_addr, free_mem, 2);

		upload_vmu_data (vmu_addr, vmu_file, free_mem, 2);

		update_vmu_dir (vmu_addr, first_free_block, FILENAME_STRING, 2);
		return 1;
	}
}


int16 save_SRAM(uint8 vmu_addr, char* filename, char* description, unsigned char* SRAM_ptr)
{
	uint32 i = 0;
	uint16 block_number = 0;
	uint8 temp_mem[200];
	uint8 *free_mem;
	uint8 *temp_ptr;
	uint8 temp_buf[9216];
	uint8 *vmu_file = (uint8 *)temp_buf;
	uint8 free_blocks;
	uint8 first_free_block;

	if (!(vmu_addr))
		return -1;

	// has anything been written to Save RAM?
	for(i = 0; i < 0x2000; i++)
	{
		if (SRAM_ptr[i] != 0x00)
			break;
	}

	if (i < 0x2000)
	{
		free_mem = (uint8 *)temp_mem;
		temp_ptr = free_mem;

		for (i = 0; i < 200; i++)
			*temp_ptr++ = '0';
	
		timer_spin_sleep(30);

		block_number = find_file(vmu_addr, filename);

		// found the game in SaveRAM
		if (block_number != 0)
		{
			create_vmu_header (vmu_file, filename, description, 8576);

			for (i = 640; i < 1024; i++)
			{
				vmu_file[i] = 0;
			}

			for (i = 0; i < 8192; i++)
			{
				vmu_file[i + 1024] = SRAM_ptr[i];
			}

			do_crc (vmu_file, 9216);

			upload_data_by_block (vmu_addr, vmu_file, block_number, 18);
			return 2;
		}
		else
		{
			free_blocks = check_free_blocks (vmu_addr, free_mem);

			if (free_blocks < 18)
				return -1;
			
			create_vmu_header (vmu_file, filename, description, 8576);

			// filler
			for (i = 640; i < 1024; i++)
			{
				vmu_file[i] = 0;
			}

			for (i = 0; i < 8192; i++)
			{
				vmu_file[i + 1024] = SRAM_ptr[i];
			}

			do_crc (vmu_file, 9216);

			first_free_block = find_first_free_block(free_mem);

			update_fat (vmu_addr, free_mem, 18);

			upload_vmu_data (vmu_addr, vmu_file, free_mem, 18);

			update_vmu_dir (vmu_addr, first_free_block, filename, 18);
			return 1;
		}
	}
	return -1;
}

int16 load_SRAM(uint8 vmu_addr, char* filename, unsigned char* SRAM_ptr)
{
	int i;
	uint16 block_number;
	unsigned char *temp_ptr = SRAM_ptr;
	
	if (!(vmu_addr))
		return -1;

	timer_spin_sleep (3000);

	for (i = 0; i < 0x2000; i++)
	{
		*temp_ptr++ = 0x00;
	}

	block_number = find_file(vmu_addr, filename);

	if (block_number == 0)
		return -2;

	block_number = get_fat_block (vmu_addr, block_number);
	
	for (i = 0; i < 16; i++)
	{
		block_number = get_fat_block (vmu_addr, block_number);

		if (vmu_block_read(vmu_addr, block_number, SRAM_ptr + (i * 512)) != 0)
		{
			return -3;
		}
		
		timer_spin_sleep (50);
	}

	return 1;
}
