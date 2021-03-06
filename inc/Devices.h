//Bacon-16: Device manager
#ifndef DEVICES_H
#define DEVICES_H

#ifdef _WIN32
	#include <windows.h>
	HINSTANCE *devices;
	#define MODTYPE HINSTANCE
	#define loadLib(lib) (HINSTANCE)LoadLibrary(lib)
	#define getInit(device) (init_t)GetProcAddress(device, "init")
	#define getUpdate(device) (update_t)GetProcAddress(device, "update")
	#define getUnload(device) (unload_t)GetProcAddress(device, "unload")
	#define closeLib(lib) FreeLibrary(lib)
#elif __linux__
	#include <dlfcn.h>
	void **devices;
	#define MODTYPE void*
	#define loadLib(lib) dlopen(lib, RTLD_LAZY)
	#define getInit(device) dlsym(device, "init")
	#define getUpdate(device) dlsym(device, "update")
	#define getUnload(device) dlsym(device, "unload")
	#define closeLib(lib) dlclose(lib)
#else
	#error This operating system is not supported
#endif

#include "SharedData.h"

#include <math.h>
#include <stdlib.h>

typedef void (*init_t)(ini_t*, unsigned int, struct SData*);
typedef void (*update_t)(unsigned int*, struct SData*);
typedef void (*unload_t)();

struct SData sharedData;

init_t *inits;
update_t *updates;
unload_t *unloads;

unsigned int deviceCount = 0;
unsigned int initCount = 0;
unsigned int updateCount = 0;
unsigned int unloadCount = 0;
unsigned int nextDIO = 0;

//DIO functions
void initDevices(unsigned int *dio, unsigned int dioCount, ini_t *cfg) {
	SDL_Init(SDL_INIT_VIDEO);
	sharedData.win = SDL_CreateWindow("HamX16", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, 0);

	devices = calloc(0, sizeof(MODTYPE));
	inits   = calloc(1, sizeof(init_t));
	updates = calloc(1, sizeof(update_t));
	unloads = calloc(1, sizeof(unload_t));

	for(unsigned int i = 0; i < dioCount; i++) {
		char *module;
		char device[6];
		sprintf(device, "%i", i);
		module = (char*)ini_get(cfg, "devices", device);
		if(module) {
			deviceCount++;
			devices = realloc(devices, deviceCount);
			devices[deviceCount - 1] = loadLib(module);

			if(devices[deviceCount - 1]) { //this could be made more efficient in case a function load fails
				initCount++;
				inits = realloc(inits, initCount);
				inits[initCount - 1] = getInit(devices[deviceCount - 1]);
				updateCount++;
				updates = realloc(updates, updateCount);
				updates[updateCount - 1] = getUpdate(devices[deviceCount - 1]);
				unloadCount++;
				unloads = realloc(unloads, unloadCount);
				unloads[unloadCount - 1] = getUnload(devices[deviceCount - 1]);
			}
		}
	}

	for(unsigned int i = 0; i < initCount; i++) {
		if(nextDIO < dioCount) {
			if(inits[i]) {
				inits[i](cfg, nextDIO, &sharedData);
				nextDIO++;
			}
		} else break;
	}
}

void updateDevices(unsigned int *dio, unsigned short *flgReg, unsigned short *intReg, int *execute) {
	SDL_PollEvent(&sharedData.ev);
	if(sharedData.ev.type == SDL_QUIT) *execute = 0;

	for(unsigned int i = 0; i < updateCount; i++) if(updates[i]) updates[i](dio, &sharedData);

	//Clear interrupt bits
	*flgReg &= 0b1111111101111111;
	*intReg  = 0;
}

void unloadDevices() {
	for(unsigned int i = 0; i < unloadCount; i++) if(unloads[i]) unloads[i]();
	for(unsigned int i = 0; i < deviceCount; i++) closeLib(devices[i]);
	free(devices);
	free(inits);
	free(updates);
	SDL_DestroyWindow(sharedData.win);
	SDL_Quit();
}

#endif