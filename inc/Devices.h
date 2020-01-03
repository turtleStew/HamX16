//Bacon-16: Device manager
#ifndef DEVICES_H
#define DEVICES_H

#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

/*Devices to emulate:
 * buzzer
 * 	-simple audio output, generates single tone beeps n amount of times when it receives a call state
 * gpu
 * 	-rendering is provided by direct pixel manipulation or providing a texture ID to display a char or sprite at the coordinates in the registers
 * keyboard
 * 	-simply stores physical input in a char register and sets the interrupt flag high
 * harddisk
 * 	-should be as close to a real hard drive as possible, else can be an emulated ssd or other flash drive. Data is stored in a raw binary file which can use the emulator's os's file system
 * external drive (like a floppy drive or cd rom)
 * 	-similar to the harddisk, but a binary file can be dynamically loaded from a folder when the user inputs a specific key combo which mounts and unmounts the file
 * dio hub (controller)
 * 	-is responsible for "managing" the other devices. Can be considered a south bridge
 * 	-to be split up into timed sections to emulate bus timing
 */

//The DIO device bus has a total of 18 bits, the first two are used for selection and interrupt signaling while the other 16 bits are fully accessible via a normal 16 bit register. The signal bits are controlled
//	by the DIO controller or DIO "hub"
//The interrupt signals from all of the devices are combined and sent to the CPU's flag register. All device data output operations should send an interrupt
//All devices can have direct access to system RAM. The gpu stream instructions can send bytes directly between the system RAM and the VRAM

//https://stackoverflow.com/questions/10110905/simple-sound-wave-generator-with-sdl-in-c

SDL_Window *win;
SDL_Renderer *ren;
SDL_Event ev;

const unsigned int resW = 640, resH = 360;

//Device data
struct Gpu {
	unsigned char vram[8388608]; //max res of rgba 800x600 and 4 render buffers. Maxmum address space of 65535 per one page of 128 pages. 8.39mb
	unsigned char regs[16];
	SDL_Texture *buffer;

	/*Graphics card specifications
	 * -bus connects to buffer processor
	 * -buffer processor determines if stream instruction or gpu instruction
	 * -if stream instruction, data is streamed to vram and gpu is halted
	 * -if gpu instruction, the gpu executes it
	 * -a display adapter (implemented in SDL) controlled by the gpu renders data directly from the section
	 * 	of vram designated as the render buffer. The adapter has its own registers used to set resolution,
	 * 	framebuffer address, and other important "settings"
	 * -the gpu can be programmed?
	 * -resolution scaling can be achieved by recreating the texture with the set render resolution. The
	 * 	display resolution will be a fixed value, unless emulator settings are to be implemented
	 */

	/*GPU registers:
	 * -pc
	 * -acc
	 * -tmp
	 * -buf: contains the memory address for the render buffer which is to be displayed
	 *Buffer processor registers:
	 * -spt: stream memory pointer, specifies the end address space of a stream in vram
	 * -spg: stream memory page pointer, specifies the end address page of a stream in vram
	 * -typ: constant register which stores the device type ID
	 * -a pair of hidden registers are used to store the current page and address for streaming
	 */

	/*GPU instructions:
	 * -mov arg1, arg2 - moves register 1 to register 2
	 * -gpx arg1, arg2 - moves four bytes at vram address arg1, arg2 to the rg and ba registers. The first bit of arg1 is unused
	 * -spx arg1, arg2 - replaces four bytes at vram address arg1, arg2 with the contents of the rg and ba registers. The first bit of arg1 is unused
	 * -clr	arg1	   - sets the specified internal register to zero
	 * -srs	arg1, arg2 - sets the resolution to the one specified by arg1 and arg2
	 *Buffer processor instructions:
	 * -str arg1, arg2 - begins streaming. arg1 specifies the start page and arg2 specifies the start address. The stream pointers specify the page and address to stop at
	 */

	/*To initialize gpu in B16:
	 * 1. set rsw and rsh registers to resolution
	 * 2. set buf register to the address of the first byte to be used by the buffer
	 * 3. if required, store the address for another buffer somewhere and repeat until no more render buffers are needed
	 * 4. set the stream pointers to the size of the data to stream + memory offset. The offset should be after the size of the last buffer + its offset
	 * 5. set cpu registers for the start page and address of the vram for streaming and stream the data
	 */
} gpu;

//Device initialization funtions
void init_gpu() {
	gpu.buffer = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, resW, resH);
	for(int y = 0; y < resH; y++) {
		for(int x = 0; x < resW; x++) {
			int bA = (4 * ((resW * y) + x));
			gpu.vram[bA] = 255;
			gpu.vram[bA + 1] = 255;
			gpu.vram[bA + 2] = 255;
			gpu.vram[bA + 3] = 255;
		}
	}

	for(int y = 0; y < resH; y++) {
		for(int x = 0; x < resW; x++) {
			int bA = (4 * ((resW * y) + x)) + (resW * resH * 4);
			gpu.vram[bA] = 0;
			gpu.vram[bA + 1] = 0;
			gpu.vram[bA + 2] = 255;
			gpu.vram[bA + 3] = 255;
		}
	}

	for(int y = 0; y < resH; y++) {
		for(int x = 0; x < resW; x++) {
			int bA = (4 * ((resW * y) + x)) + 2 * (resW * resH * 4);
			gpu.vram[bA] = 0;
			gpu.vram[bA + 1] = 255;
			gpu.vram[bA + 2] = 0;
			gpu.vram[bA + 3] = 255;
		}
	}

	for(int y = 0; y < resH; y++) {
		for(int x = 0; x < resW; x++) {
			int bA = (4 * ((resW * y) + x)) + 3 * (resW * resH * 4);
			gpu.vram[bA] = 255;
			gpu.vram[bA + 1] = 0;
			gpu.vram[bA + 2] = 0;
			gpu.vram[bA + 3] = 255;
		}
	}
}

//Device update funtions
void update_gpu(unsigned short *ioReg) {
	if(*ioReg >> 17) {
		//Process instructions
		unsigned char lByte  = *ioReg >> 8;
		//unsigned char rByte  = *ioReg & 0xFF;
		//unsigned char mode   = lByte >> 6;
		unsigned char opcode = (lByte & 0b00111110) >> 1;
		//unsigned char arg1   = rByte >> 4;
		//unsigned char arg2   = rByte & 0xF;
		//unsigned short imm   = ((lByte & 1) << 8) | rByte;


		/*Instructions (received as a 16 bit register):
		 * -mov arg1, arg2		 - moves register 1 to register 2
		 * -add arg1, arg2
		 * -sub arg1, arg2
		 * -gb0 vram[arg1], arg2 - moves an address in the portion of vram used by buffer 0 to arg2
		 * -gb1 vram[arg1], arg2 - moves an address in the portion of vram used by buffer 1 to arg2
		 * -gt0 vram[arg1], arg2 - moves an address in the portion of vram used by texture page 0 to arg2
		 * -gt1 vram[arg1], arg2 - moves an address in the portion of vram used by texture page 1 to arg2
		 * -sb0 arg1, vram[arg2] - moves arg1 to an address in the portion of vram used by buffer 0
		 * -sb1 arg1, vram[arg2] - moves arg1 to an address in the portion of vram used by buffer 1
		 * -st0 arg1, vram[arg2] - moves arg1 to an address in the portion of vram used by texture page 0
		 * -st1 arg1, vram[arg2] - moves arg1 to an address in the portion of vram used by texture page 1
		 * -cpy arg1, arg2		 - copies mem at arg1 to mem at arg2. The size register is used to specify the number of bytes to be copied
		 * -snd arg1, arg2		 - a byte in the main memory at arg1 is copied to address arg2 in vram
		 * -rec arg1, arg2		 - a byte in vram at arg1 is copied to address arg2 in the main memory
		 * -rst					 - sets all internal registers to zero
		 */

		switch(opcode) {
			case(1): { //mov
				break;
			}
			case(2): { //add
				break;
			}
			case(3): { //sub
				break;
			}
			case(4): { //gb0
				break;
			}
			case(5): { //gb1
				break;
			}
			case(6): { //gt0
				break;
			}
			case(7): { //gt1
				break;
			}
			case(8): { //sb0
				break;
			}
			case(9): { //sb1
				break;
			}
			case(10): { //st0
				break;
			}
			case(11): { //st1
				break;
			}
			case(12): { //cpy
				break;
			}
			case(13): { //snd
				break;
			}
			case(14): { //rec
				break;
			}
			case(15): { //rst
				break;
			}
		}
	}

	//render output
	SDL_UpdateTexture(gpu.buffer, NULL, &gpu.vram[resW * resH * 4 * 1], resW * 4);
	SDL_RenderCopy(ren, gpu.buffer, NULL, NULL);

	//Reset io enable bit
	*ioReg &= 0b011111111;
}

//DIO functions
void initDevices(unsigned short *dio) {
	//Some dll loading code, or maybe lua instead. Will be hard coded for now
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	win = SDL_CreateWindow("HamX16", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, resW, resH, SDL_WINDOW_SHOWN);
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

	init_gpu();
}

void updateDevices(unsigned short *dio, unsigned short *regs, int *execute) {
	SDL_PollEvent(&ev);
	switch(ev.type) {
		case SDL_QUIT: {
			*execute = 0;
			break;
		}
	}

	SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
	SDL_RenderClear(ren);
	
	update_gpu(&dio[0]);
	
	SDL_RenderPresent(ren);

	//Clear intrrupt bits
	regs[3] &= 0b1111111101111111;
	regs[4]  = 0;
}

void end() {
	SDL_DestroyWindow(win);
	SDL_DestroyRenderer(ren);
	SDL_Quit();
}

#endif
