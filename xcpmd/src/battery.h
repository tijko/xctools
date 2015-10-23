#ifndef __BATTERY_H__
#define __BATTERY_H__

#include "project.h"
#include "xcpmd.h"


//Battery info for consumption by dbus and others
extern struct battery_info   *last_info;
extern struct battery_status *last_status;
extern unsigned int num_battery_structs_allocd;

extern struct event refresh_battery_event;

int get_battery_percentage(unsigned int battery_index);
int get_num_batteries_present(void);
int get_num_batteries(void);
void update_batteries(void);
int update_battery_status(unsigned int battery_index);
int update_battery_info(unsigned int battery_index);
void write_battery_status_to_xenstore(unsigned int battery_index);
void write_battery_info_to_xenstore(unsigned int battery_index);
int get_overall_battery_percentage(void);
int get_current_battery_level(void);
void wrapper_refresh_battery_event(int fd, short event, void *opaque);
int battery_slot_exists(unsigned int battery_index);
int battery_is_present(unsigned int battery_index);


#endif
