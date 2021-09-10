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
		int cmp = strncasecmp(str, config[mid]->name, CONFIGLEN);
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
char* configToString(config_* var){
	int retmaxlen = 1000;
	char ret[retmaxlen+1];
	enum configType t = var->type;
	if(t == STRG){
		strncpy(ret, var->strg, retmaxlen);
	}else if(t == FLOT){
		snprintf(ret, retmaxlen, "%lf", var->flot);
	}else if(t == INTE){
		snprintf(ret, retmaxlen, "%d", var->inte);
	}else if(t == BOOL){
		if(var->inte){
			strcpy(ret, "true");
		}else{
			strcpy(ret, "false");
		}
	}
	char* mret = malloc(strlen(ret)+1);
	strcpy(mret, ret);
	return mret;
}
void configFromString(config_* var, char* str){
	enum configType t = var->type;
	if(t == STRG){
		// threadsafe handoff
		char* oldstr = var->strg;
		char* newstr = malloc(strlen(str)+1);
		strcpy(newstr, str);
		var->strg = newstr;
		free(oldstr);
	}else{
		if(strlen(str) < 1) return;
		if(t == FLOT){
			var->flot = strtod(str, NULL);
		}else if(t == INTE){
			var->inte = atoi(str);
		}else if(t == BOOL){
			if(str[0] == 't' || str[0] == 'T'){
				var->inte = 1;
			}else if(str[0] == 'f' || str[0] == 'F'){
				var->inte = 0;
			}
		}
	}
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
			if(strcasecmp(istr, jstr) < 0){
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
		assert(0 == strcasecmp((*ptr)->name, str));
	}
}
