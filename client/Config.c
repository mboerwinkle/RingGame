#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "loadJsonConfig.h"
#include "Config.h"

config_ factoryconfig[] = {
{.type=BOOL, .inte=0, .name="cgdrawfps"},
{.type=BOOL, .inte=0, .name="cgdrawsrvfps"},
{.type=BOOL, .inte=0, .name="cgdrawping"},
{.type=BOOL, .inte=0, .name="cgdrawstats"},
{.type=INTE, .inte=800, .name="cgwidth"},
{.type=INTE, .inte=800, .name="cgheight"},
{.type=INTE, .inte=60, .name="cgmaxfps"},
{.type=BOOL, .inte=1, .name="cgvsync"},
{.type=BOOL, .inte=1, .name="cgcull"},
{.type=FLOT, .flot=1.0, .name="cimousesensi"},
{.type=BOOL, .inte=0, .name=""}
};

config_
*V_CGDRAWFPS,
*V_CGDRAWSRVFPS,
*V_CGDRAWPING,
*V_CGDRAWSTATS,
*V_CGWIDTH,
*V_CGHEIGHT,
*V_CGMAXFPS,
*V_CGVSYNC,
*V_CGCULL,
*V_CIMOUSESENSI;

void* initconfigpairings[] = {
&V_CGDRAWFPS,			"cgdrawfps",
&V_CGDRAWSRVFPS,		"cgdrawsrvfps",
&V_CGDRAWPING,			"cgdrawping",
&V_CGDRAWSTATS,		"cgdrawstats",
&V_CGWIDTH,				"cgwidth",
&V_CGHEIGHT,			"cgheight",
&V_CGMAXFPS,			"cgmaxfps",
&V_CGVSYNC,				"cgvsync",
&V_CGCULL,				"cgcull",
&V_CIMOUSESENSI,		"cimousesensi",
NULL
};


config_** config = NULL;
int config_count = 0;

config_* getConfig(char* str){
	int start = 0;
	int end = config_count;
	while(start < end){
		int mid = (start+end)/2;
		int cmp = strncmp(str, config[mid]->name, CONFIGLEN);
		if(cmp < 0){
			end = mid;
		}else if(cmp > 0){
			start = mid+1;
		}else{
			return config[mid];
		}
	}
	return NULL;
}

void mergeSortConfig(int start, int end){
	if(start+1 >= end) return;
	int mididx = (start+end)/2;
	mergeSortConfig(start, mididx);
	mergeSortConfig(mididx, end);
	config_* (res[end-start]);
	int i = start;
	int j = mididx;
	for(int residx = 0; residx < end-start; residx++){
		char* istr = NULL;
		char* jstr = NULL;
		if(i < mididx){
			istr = config[i]->name;
		}
		if(j < end){
			jstr = config[j]->name;
		}
		if(istr && jstr){
			if(strcmp(istr, jstr) < 0){
				res[residx] = config[i];
				i++;
			}else{
				res[residx] = config[j];
				j++;
			}
		}else{//FIXME this can be turned into an early return with memcpy from the remaining tail
			if(istr){
				res[residx] = config[i];
				i++;
			}else{
				res[residx] = config[j];
				j++;
			}
		}
	}
	memcpy(&(config[start]), res, (end-start)*sizeof(config_*));
}
void initConfig(){
	// initialize config_count;
	while(strncmp("", factoryconfig[config_count].name, CONFIGLEN)){
		config_count++;
	}
	// copy factory config into config
	config = calloc(config_count, sizeof(config_*));
	for(int idx = 0; idx < config_count; idx++){
		config[idx] = malloc(sizeof(config_));
		memcpy(config[idx], &(factoryconfig[idx]), sizeof(config_));
		// copy strings
		if(factoryconfig[idx].type == STRG && factoryconfig[idx].strg != NULL){
			int stlen = strlen(factoryconfig[idx].strg);
			config[idx]->strg = malloc(stlen+1);
			strcpy(config[idx]->strg, factoryconfig[idx].strg);
		}
	}
	// merge sort config
	mergeSortConfig(0, config_count);
	printf("CONFIG STRINGS:\n");
	for(int idx = 0; idx < config_count; idx++){
		printf("%s\n", config[idx]->name);
	}
	// populate internal config pointers
	for(int idx = 0; initconfigpairings[idx] != NULL; idx+=2){
		config_** ptr = initconfigpairings[idx];
		char* str = initconfigpairings[idx+1];
		*ptr = getConfig(str);
		assert(*ptr != NULL && "Internal config doesn't exist");
		assert(0 == strcmp((*ptr)->name, str));
	}
}
