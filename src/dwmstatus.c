/** ********************************************************************
 * DWM STATUS by <clement@6pi.fr>
 * Forked and maintained by s.digirolamo218@gmail.com
 *
 * Compile with:
 * gcc -Wall -pedantic -std=c99 -lX11 -lasound dwmstatus.c
 *
 **/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>

/* Alsa */
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
/* Oss (not working, using popen + ossmix)
#include <linux/soundcard.h>
*/

#define CPU_NBR 4
#define BAR_HEIGHT 15
#define BAT_NOW_FILE "/sys/class/power_supply/BAT0/energy_now"
#define BAT_FULL_FILE "/sys/class/power_supply/BAT0/energy_full"
#define BAT_STATUS_FILE "/sys/class/power_supply/BAT0/status"

#define TEMP_SENSOR_FILE "/sys/class/hwmon/hwmon2/temp1_input"
#define MEMINFO_FILE "/proc/meminfo"

#define LENGTH(X) (sizeof X / sizeof X[0])

/* Xresources preferences */
enum resource_type {
	STRING = 0,
	INTEGER = 1,
	FLOAT = 2
};

typedef struct {
	char *name;
	enum resource_type type;
	void *dst;
} ResourcePref;

int   getBattery();
int   getBatteryStatus();
int   getMemPercent();
void  getCpuUsage(int *cpu_percent);
char  *getDate();
char  *getTime();
float getFreq(char *file);
int   getTemperature();
int   getVolume();
void  setStatus(Display *dpy, char *str);
int   getWifiPercent();
static void load_xresources(void);
static void resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst);
static void getVolblock(char *buff, int size);
static void getCpublock(char *buff, int size);
static void getMemblock(char *buff, int size);
static void getTempblock(char *buff, int size);
static void getDateblock(char *buff, int size);
static void getTimeblock(char *buff, int size);
static void getWifiblock(char *buff, int size);

char* vBar(int percent, int w, int h, char* fg_color, char* bg_color);
char* hBar(int percent, int w, int h, char* fg_color, char* bg_color);
int h2Bar(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color);
int hBarBordered(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color, char *border_color);
int getBatteryBar(char *string, size_t size, int w, int h);
void percentColorGeneric(char* string, int percent, int invert);


static char divcolor[]  = "#ffffff";
static char volcolor[]  = "#ffffff";
static char cpucolor[]  = "#ffffff";
static char memcolor[]  = "#ffffff";
static char wificolor[] = "#ffffff";
static char tempcolor[] = "#ffffff";
static char datecolor[] = "#ffffff";
static char timecolor[] = "#ffffff";

/*
 * Xresources preferences to load at startup
 */
ResourcePref resources[] = {
	{ "divcolor",  STRING, &divcolor },
	{ "volcolor",  STRING, &volcolor },
	{ "cpucolor",  STRING, &cpucolor },
	{ "memcolor",  STRING, &memcolor },
	{ "wificolor", STRING, &wificolor },
	{ "tempcolor", STRING, &tempcolor },
	{ "datecolor", STRING, &datecolor },
	{ "timecolor", STRING, &timecolor },
};


/* *******************************************************************
 * MAIN
 ******************************************************************* */

int
main(void)
{
	const int MSIZE = 1024;
	Display *dpy;
	char *status;

	char volblock[128];
	char cpublock[128];
	char memblock[128];
	char wifiblock[128];
	char tempblock[128];
	char batblock[128];
	char dateblock[128];
	char timeblock[128];




	char bat0[256];


	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Cannot open display.\n");
		return EXIT_FAILURE;
	}

	status = (char*) malloc(sizeof(char)*MSIZE);
	if(!status)
		return EXIT_FAILURE;

	load_xresources();
	char div[32];
	snprintf(div, sizeof(div), "^c%s^|", divcolor);

	 while(1) {
		getBatteryBar(bat0, 256, 30, 11);


		getVolblock(volblock, sizeof(volblock));
		//getCpublock(cpublock, sizeof(cpublock));
		getMemblock(memblock, sizeof(memblock));
		getWifiblock(wifiblock, sizeof(wifiblock));
		getTempblock(tempblock, sizeof(tempblock));
		getDateblock(dateblock, sizeof(dateblock));
		getTimeblock(timeblock, sizeof(timeblock));

		int ret = snprintf(
			 status,
			 MSIZE,
			 "  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  %s  ",
			 volblock,
			 div,
			 memblock,
			 div,
			 wifiblock,
			 div,
			 tempblock,
			 div,
			 bat0,
			 div,
			 dateblock,
			 div,
			 timeblock
		 );
		if(ret >= MSIZE)
			fprintf(stderr, "error: buffer too small %d/%d\n", MSIZE, ret);

		setStatus(dpy, status);
		sleep(1);
	}

	/* USELESS
	free(status);
	XCloseDisplay(dpy);

	return EXIT_SUCESS;
	*/
}

/* *******************************************************************
 * FUNCTIONS
 ******************************************************************* */

static void
getVolblock(char *buff, int size)
{
	int volume = getVolume();
	snprintf(buff, size, "^c%s^ %d%c", volcolor, volume, '%');
}

static void
getCpublock(char *buff, int size)
{
	int cpu_percent[CPU_NBR];
	char *cpu_bar[CPU_NBR];
	char cpu_color[8];

	getCpuUsage(cpu_percent);

	for(int i = 0; i < CPU_NBR; ++i) {
		percentColorGeneric(cpu_color, cpu_percent[i], 1);
		cpu_bar[i] = vBar(cpu_percent[i], 2, 13, cpu_color, "#444444");
	}
	snprintf(
		buff,
		size,
		"^c%s^[CPU^f1^%s^f4^%s^f4^%s^f4^%s^f3^^c%s^]",
		cpucolor,
		cpu_bar[0],
		cpu_bar[1],
		cpu_bar[2],
		cpu_bar[3],
		cpucolor
	);

	for(int i = 0; i < CPU_NBR; ++i)
		free(cpu_bar[i]);
}

static void
getMemblock(char *buff, int size)
{
	int mem_percent;
	char *mem_bar;

	mem_percent = getMemPercent();
	mem_bar = hBar(mem_percent, 20, 9,	"#FF0000", "#444444");

	snprintf(
		buff,
		size,
		"^c%s^ ^f1^%s^f20^",
		memcolor,
		mem_bar,
		memcolor
	);
	free(mem_bar);
}

static void
getWifiblock(char *buff, int size)
{
	int wifi = getWifiPercent();

	snprintf(buff, size, "^c%s^ %d", wificolor, wifi);
}

static void
getTempblock(char *buff, int size)
{
	int temp = getTemperature();
	snprintf(buff, size, "^c%s^ %d°C", tempcolor, temp);
}

static void
getDateblock(char *buff, int size)
{
	char *date = getDate();
	snprintf(buff, size, "^c%s^%s", datecolor, date);
	free(date);
}

static void
getTimeblock(char *buff, int size)
{
	char *time = getTime();
	snprintf(buff, size, "^c%s^%s", timecolor, time);
	free(time);
}

static void
resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst)
{
	char *sdst = NULL;
	int *idst = NULL;
	float *fdst = NULL;

	sdst = dst;
	idst = dst;
	fdst = dst;

	char fullname[256];
	char *type;
	XrmValue ret;

	snprintf(fullname, sizeof(fullname), "%s.%s", "dwmstatus", name);
	fullname[sizeof(fullname) - 1] = '\0';

	XrmGetResource(db, fullname, "*", &type, &ret);

	if (!(ret.addr == NULL))
	{
		switch (rtype) {
		case STRING:
			strcpy(sdst, ret.addr);
			break;
		case INTEGER:
			*idst = strtoul(ret.addr, NULL, 10);
			break;
		case FLOAT:
			*fdst = strtof(ret.addr, NULL);
			break;
		}
	}
}

void
load_xresources(void)
{
	Display *display;
	char *resm;
	XrmDatabase db;
	ResourcePref *p;

	display = XOpenDisplay(NULL);
	resm = XResourceManagerString(display);
	if (!resm)
		return;

	db = XrmGetStringDatabase(resm);
	for (p = resources; p < resources + LENGTH(resources); p++) {
		resource_load(db, p->name, p->type, p->dst);
	}
	XCloseDisplay(display);
}

char* vBar(int percent, int w, int h, char* fg_color, char* bg_color)
{
	char *value;
	if((value = (char*) malloc(sizeof(char)*128)) == NULL)
		{
			fprintf(stderr, "Cannot allocate memory for buf.\n");
			exit(1);
		}
	char* format = "^c%s^^r0,%d,%d,%d^^c%s^^r0,%d,%d,%d^";

	int bar_height = (percent*h)/100;
	int y = (BAR_HEIGHT - h)/2;
	snprintf(value, 128, format, bg_color, y, w, h, fg_color, y + h-bar_height, w, bar_height);
	return value;
}

char* hBar(int percent, int w, int h, char* fg_color, char* bg_color)
{
	char *value;
	if((value = (char*) malloc(sizeof(char)*128)) == NULL)
		{
			fprintf(stderr, "Cannot allocate memory for buf.\n");
			exit(1);
		}
	char* format = "^c%s^^r0,%d,%d,%d^^c%s^^r0,%d,%d,%d^";

	int bar_width = (percent*w)/100;
	int y = (BAR_HEIGHT - h)/2;
	snprintf(value, 128, format, bg_color, y, w, h, fg_color, y, bar_width, h);
	return value;
}

int hBar2(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color)
{
	char *format = "^c%s^^r0,%d,%d,%d^^c%s^^r%d,%d,%d,%d^";
	int bar_width = (percent*w)/100;

	int y = (BAR_HEIGHT - h)/2;
	return snprintf(string, size, format, fg_color, y, bar_width, h, bg_color, bar_width, y, w - bar_width, h);
}

int hBarBordered(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color, char *border_color)
{
	char tmp[128];
	hBar2(tmp, 128, percent, w - 2, h -2, fg_color, bg_color);
	int y = (BAR_HEIGHT - h)/2;
	char *format = "^c%s^^r0,%d,%d,%d^^f1^%s";
	return snprintf(string, size, format, border_color, y, w, h, tmp);
}

void
setStatus(Display *dpy, char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

void percentColorGeneric(char* string, int percent, int invert)
{
	char *format = "#%X0%X000";
	int a = (percent*15)/100;
	int b = 15 - a;
	if(!invert) {
		snprintf(string, 8, format, b, a);
	}
	else {
		snprintf(string, 8, format, a, b);
	}
}

void percentColor(char* string, int percent)
{
	percentColorGeneric(string, percent, 0);
}

int getBatteryBar(char *string, size_t size, int w, int h)
{
	int percent = getBattery();

	char *bg_color = "#444444";
	char *border_color = "#EEEEEE";
	char fg_color[8];
	if(getBatteryStatus())
		memcpy(fg_color, border_color, 8);
	else
		percentColor(fg_color, percent);

	char tmp[128];
	hBarBordered(tmp, 128, percent, w -2, h, fg_color, bg_color, border_color);

	char *format = "%s^c%s^^f%d^^r0,%d,%d,%d^^f%d^";
	int y = (BAR_HEIGHT - 5)/2;
	return snprintf(string, size, format, tmp, border_color, w - 2, y, 2, 5, 2);
}

int
getBattery()
{
	FILE *fd;
	int energy_now;

	static int energy_full = -1;
	if(energy_full == -1)
		{
			fd = fopen(BAT_FULL_FILE, "r");
			if(fd == NULL) {
				fprintf(stderr, "Error opening energy_full.\n");
				return -1;
			}
			fscanf(fd, "%d", &energy_full);
			fclose(fd);
		}

	fd = fopen(BAT_NOW_FILE, "r");
	if(fd == NULL) {
		fprintf(stderr, "Error opening energy_now.\n");
		return -1;
	}
	fscanf(fd, "%d", &energy_now);
	fclose(fd);

	return ((float)energy_now	/ (float)energy_full) * 100;
}

/**
 * Return 0 if the battery is discharing, 1 if it's full or is
 * charging, -1 if an error occured.
 **/
int
getBatteryStatus()
{
	FILE *fd;
	char first_letter;

	if( NULL == (fd = fopen(BAT_STATUS_FILE, "r")))
		return -1;

	fread(&first_letter, sizeof(char), 1, fd);
	fclose(fd);

	if(first_letter == 'D')
		return 0;

	return 1;
}

int
getMemPercent()
{
	FILE *fd;
	int mem_total;
	int mem_free;
	int mem_available;
	fd = fopen(MEMINFO_FILE, "r");
	if(fd == NULL) {
		fprintf(stderr, "Error opening energy_full.\n");
				return -1;
	}
	fscanf(fd, "MemTotal:%*[ ]%d kB\nMemFree:%*[ ]%d kB\nMemAvailable:%*[ ]%d", &mem_total, &mem_free, &mem_available);
	fclose (fd);
	return ((float)(mem_total-mem_available)/(float)mem_total) * 100;
}


void
getCpuUsage(int* cpu_percent)
{
	size_t len = 0;
	char *line = NULL;
	int i;
	long int idle_time, other_time;
	char cpu_name[8];

	static int new_cpu_usage[CPU_NBR][4];
	static int old_cpu_usage[CPU_NBR][4];

	FILE *f;
	if(NULL == (f = fopen("/proc/stat", "r")))
		return;

	for(i = 0; i < CPU_NBR; ++i)
		{
			getline(&line,&len,f);
			sscanf(
						 line,
						 "%s %d %d %d %d",
						 cpu_name,
						 &new_cpu_usage[i][0],
						 &new_cpu_usage[i][1],
						 &new_cpu_usage[i][2],
						 &new_cpu_usage[i][3]
						 );

			if(line != NULL)
				{
					free(line);
					line = NULL;
				}

			idle_time = new_cpu_usage[i][3] - old_cpu_usage[i][3];
			other_time = new_cpu_usage[i][0] - old_cpu_usage[i][0]
				+ new_cpu_usage[i][1] - old_cpu_usage[i][1]
				+ new_cpu_usage[i][2] - old_cpu_usage[i][2];

			if(idle_time + other_time != 0)
				cpu_percent[i] = (other_time*100)/(idle_time + other_time);
			else
				cpu_percent[i] = 0;

			old_cpu_usage[i][0] = new_cpu_usage[i][0];
			old_cpu_usage[i][1] = new_cpu_usage[i][1];
			old_cpu_usage[i][2] = new_cpu_usage[i][2];
			old_cpu_usage[i][3] = new_cpu_usage[i][3];
		}

		fclose(f);
}

float
getFreq(char *file)
{
	FILE *fd;
	char *freq;
	float ret;

	freq = (char*) malloc(10);
	fd = fopen(file, "r");
	if(fd == NULL)
		{
			fprintf(stderr, "Cannot open '%s' for reading.\n", file);
			exit(1);
		}

	fgets(freq, 10, fd);
	fclose(fd);

	ret = atof(freq)/1000000;
	free(freq);
	return ret;
}

char *
getDate()
{
	char *buf;
	time_t result;
	struct tm *resulttm;

	if((buf = (char*) malloc(sizeof(char)*65)) == NULL)
		{
			fprintf(stderr, "Cannot allocate memory for buf.\n");
			exit(1);
		}

	result = time(NULL);
	resulttm = localtime(&result);
	if(resulttm == NULL)
		{
			fprintf(stderr, "Error getting localtime.\n");
			exit(1);
		}

	if(!strftime(buf, sizeof(char)*65-1, "%a %d %B %Y", resulttm))
		{
			fprintf(stderr, "strftime is 0.\n");
			exit(1);
		}

	return buf;
}

char *
getTime()
{
	char *buf;
	time_t result;
	struct tm *resulttm;

	if((buf = (char*) malloc(sizeof(char)*65)) == NULL)
		{
			fprintf(stderr, "Cannot allocate memory for buf.\n");
			exit(1);
		}

	result = time(NULL);
	resulttm = localtime(&result);
	if(resulttm == NULL)
		{
			fprintf(stderr, "Error getting localtime.\n");
			exit(1);
		}

	if(!strftime(buf, sizeof(char)*65-1, "%H:%M", resulttm))
		{
			fprintf(stderr, "strftime is 0.\n");
			exit(1);
		}

	return buf;
}

int
getTemperature()
{
	int temp;
	FILE *fd = fopen(TEMP_SENSOR_FILE, "r");
	if(fd == NULL)
		{
			fprintf(stderr, "Error opening temp1_input.\n");
			return -1;
		}
	fscanf(fd, "%d", &temp);
	fclose(fd);

	return temp / 1000;
}

int
getWifiPercent()
{
	//size_t len = 0;
	int percent = 0;
	//char line[512] = {'\n'};
	FILE *fd = fopen("/proc/net/wireless", "r");
	if(fd == NULL)
		{
			fprintf(stderr, "Error opening wireless info");
			return -1;
		}
	fscanf(fd, "%*[^\n]\n%*[^\n]\n%*s %*[0-9] %d", &percent);
	fclose(fd);
	return percent;
}


int
getVolume()
{
	const char* MIXER = "Master";
	/* OSS
	const char* OSSMIXCMD = "ossmix vmix0-outvol";
	const char* OSSMIXFORMAT = "Value of mixer control vmix0-outvol is currently set to %f (dB)";
	*/

	float vol = 0;
	long pmin, pmax, pvol;

	/* Alsa {{{ */
	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_alloca(&sid);

	if(snd_mixer_open(&handle, 0) < 0)
		return 0;

	if(snd_mixer_attach(handle, "default") < 0
		 || snd_mixer_selem_register(handle, NULL, NULL) < 0
		 || snd_mixer_load(handle) > 0)
		{
			snd_mixer_close(handle);
			return 0;
		}

	for(elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem))
		{
			snd_mixer_selem_get_id(elem, sid);
			if(!strcmp(snd_mixer_selem_id_get_name(sid), MIXER))
				{
					snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
					snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &pvol);
					vol = ((float)pvol / (float)(pmax - pmin)) * 100;
				}
		}

	snd_mixer_close(handle);
	/* }}} */
	/* Oss (soundcard.h not working) {{{
		 if(!(f = popen(OSSMIXCMD, "r")))
			 return;

		 fscanf(f, OSSMIXFORMAT, &vol);
		 pclose(f);
		 }}} */

	return vol;
}

