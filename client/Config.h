#ifndef CONFIG_H
#define CONFIG_H

enum configType{STRG, FLOT, INTE, BOOL};
#define CONFIGLEN 16
typedef struct config_{
	char name[CONFIGLEN+1];
	union{
		char* strg;
		double flot;
		int inte; // also used for BOOL
	};
	enum configType type;
}config_;

extern config_
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

extern config_ factoryconfig[];
extern config_** config;
extern int config_count;

extern config_* getConfig(char* str);
extern void initConfig();
#endif
