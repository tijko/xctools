#ifndef __BATTERY_H__
#define __BATTERY_H__

#include "project.h"
#include "xcpmd.h"


//Battery info for consumption by dbus and others
extern struct battery_info   last_info[MAX_BATTERY_SUPPORTED];
extern struct battery_status last_status[MAX_BATTERY_SUPPORTED];


int get_battery_percentage(int battery_index);
int get_num_batteries_present(void);
int get_num_batteries(void);
void update_batteries(void);
int update_battery_status(int battery_index);
int update_battery_info(int battery_index);
void write_battery_status_to_xenstore(int battery_index);
void write_battery_info_to_xenstore(int battery_index);


#endif
