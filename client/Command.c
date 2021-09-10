#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "Command.h"
#include "Config.h"
#include "Gamestate.h"

void lcomm_connect(char* str, char* tokptr){
	char* host = strtok_r(NULL, " ", &tokptr);
	if(host == NULL){
		appendHistory("Missing host");
		return;
	}
	appendHistory("Unimplemented");
}
void lcomm_env(char* str, char* tokptr){
	//FIXME this needs a mutex if configurations can be removed
	for(int idx = 0; idx < config_count; idx++){
		char p[1001];
		char* strrep = configToString(config[idx]);
		snprintf(p, 1000, "%s: %s", config[idx]->name, strrep);
		free(strrep);
		appendHistory(p);
	}
}
void lcomm_get(char* str, char* tokptr){
	char* cfg = strtok_r(NULL, " ", &tokptr);
	if(cfg == NULL){
		appendHistory("Missing variable");
		return;
	}
	config_* myconf = getConfig(cfg);
	if(myconf == NULL){
		appendHistory("No such variable");
		return;
	}
	char* ret = configToString(myconf);
	appendHistory(ret);
	free(ret);
}
void lcomm_set(char* str, char* tokptr){
	char* cfg = strtok_r(NULL, " ", &tokptr);
	char* val = strtok_r(NULL, " ", &tokptr);
	if(cfg == NULL){
		appendHistory("Missing variable");
		return;
	}
	if(val == NULL){
		appendHistory("Missing value");
		return;
	}
	config_* myconf = getConfig(cfg);
	if(myconf == NULL){
		appendHistory("No such variable");
		return;
	}
	configFromString(myconf, val);
}
void lcomm_typeof(char* str, char* tokptr){
	char* cfg = strtok_r(NULL, " ", &tokptr);
	if(cfg == NULL){
		appendHistory("Missing variable");
		return;
	}
	config_* myconf = getConfig(cfg);
	if(myconf == NULL){
		appendHistory("No such variable");
		return;
	}
	enum configType type = myconf->type;
	if(type == STRG){
		appendHistory("String");
	}else if(type == FLOT){
		appendHistory("Float");
	}else if(type == INTE){
		appendHistory("Integer");
	}else if(type == BOOL){
		appendHistory("Boolean");
	}else{
		appendHistory("Unknown Type");
	}
}

void localCommand(char* str){
	char* savetokptr = NULL;
	char* tok = strtok_r(str, " ", &savetokptr);
	if(0 == strcasecmp(tok, "$help")){
		appendHistory("$help - local command help");
		appendHistory("$connect <host> - connect to a game server");
		appendHistory("$env - print the current configuration");
		appendHistory("$get <config> - print the specified config variable");
		appendHistory("$set <config> <value> - set the specified config variable");
		appendHistory("$typeof <config> - print the data type of the config variable");
	}else if(0 == strcasecmp(tok, "$connect")){
		lcomm_connect(str, savetokptr);
	}else if(0 == strcasecmp(tok, "$env")){
		lcomm_env(str, savetokptr);
	}else if(0 == strcasecmp(tok, "$get")){
		lcomm_get(str, savetokptr);
	}else if(0 == strcasecmp(tok, "$set")){
		lcomm_set(str, savetokptr);
	}else if(0 == strcasecmp(tok, "$typeof")){
		lcomm_typeof(str, savetokptr);
	}else{
		appendHistory("Unknown command");
	}
}
