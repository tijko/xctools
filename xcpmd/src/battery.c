#include <stdlib.h>
#include "project.h"
#include "xcpmd.h"
#include "battery.h"


//Battery info for consumption by dbus and others
struct battery_info   last_info[MAX_BATTERY_SUPPORTED];
struct battery_status last_status[MAX_BATTERY_SUPPORTED];

//Event struct for libevent
struct event refresh_battery_event;

static bool battery_slot_is_present(int battery_index);
static void cleanup_removed_battery(int battery_index);
static DIR * get_battery_dir(int battery_index);
static void set_battery_status_attribute(char * attrib_name, char * attrib_value, struct battery_status * status);
static void set_battery_info_attribute(char *attrib_name, char *attrib_value, struct battery_info *info);


//Get the overall warning level of all batteries in the system.
int get_current_battery_level(void) {

    int percentage;

    percentage = get_overall_battery_percentage();

    if (percentage <= BATTERY_CRITICAL_PERCENT)
        return CRITICAL;
    else if (percentage <= BATTERY_LOW_PERCENT)
        return LOW;
    else if (percentage <= BATTERY_WARNING_PERCENT)
        return WARNING;
    else
        return NORMAL;
}

//Get the overall battery percentage of the system.
//May return weird values if one battery is mA and the other is mW.
int get_overall_battery_percentage(void) {

    int i;
    unsigned long capacity_left, capacity_total;

    capacity_left = capacity_total = 0;

    for (i=0; i < MAX_BATTERY_SUPPORTED; ++i) {
        if (last_status[i].present == YES) {
            capacity_left += last_status[i].remaining_capacity;
            capacity_total += last_info[i].last_full_capacity;
        }
    }
    
    return (int) ((100 * capacity_left) / capacity_total);
}


//Get the specified battery's current charge in percent.
int get_battery_percentage(int battery_index) {

    int percentage;
    struct battery_status * status;
    struct battery_info * info;

    if (battery_index > MAX_BATTERY_SUPPORTED)
        return 0;

    if (last_status[battery_index].present == NO)
        return 0;

    status = &last_status[battery_index];
    info = &last_info[battery_index];

    //Avoid dividing by zero
    if (info->last_full_capacity == 0)
        percentage = 0;
    else
        percentage = status->remaining_capacity * 100 / info->last_full_capacity;

    return percentage;
}


//Creates a xenstore battery dir with the specified index if it doesn't already exist.
static void make_xenstore_battery_dir(int battery_index) {

    char xenstore_path[256];
    char ** dir_entries;
    unsigned int num_entries, i;
    bool flag;

    dir_entries = xenstore_ls(&num_entries, "/pm");
    sprintf(xenstore_path, "%s%i", XS_BATTERY_PATH, battery_index);

    flag = false;
    for (i = 0; i < num_entries; ++i) {
        if (!strcmp(dir_entries[i], xenstore_path)) {
            flag = true;
            break;
        }
    }
    if (!flag)
        xenstore_mkdir(xenstore_path);

    //xenstore_ls() calls malloc(), so be sure to free().
    free(dir_entries);

}


//Returns the directory of a battery at a particular index.
static DIR * get_battery_dir(int battery_index) {

    DIR *dir = NULL;
    char path[256];

    sprintf(path, "%s/BAT%i", BATTERY_DIR_PATH, battery_index);
    dir = opendir(path);

    return dir;
}


//Given an attribute name and value, sets the appropriate member of a battery_status struct.
static void set_battery_status_attribute(char * attrib_name, char * attrib_value, struct battery_status * status) {

    if (!strcmp(attrib_name, "status")) {
        //The spec says bit 0 and bit 1 are mutually exclusive
        if ( strstr(attrib_value, "Discharging") )
            status->state |= 0x1;
        else if ( strstr(attrib_value, "Charging") )
            status->state |= 0x2;
    }
    else if (!strcmp(attrib_name, "capacity_level")) {
        if (strstr(attrib_value, "critical"))
            status->state |= 4;
    }
    else if (!strcmp(attrib_name, "current_now")) {
        status->current_now = strtoull(attrib_value, NULL, 10) / 1000;
    }
    else if (!strcmp(attrib_name, "charge_now")) {
        status->charge_now = strtoull(attrib_value, NULL, 10) / 1000;
    }
    else if (!strcmp(attrib_name, "power_now")) {
        status->power_now = strtoull(attrib_value, NULL, 10) / 1000;
    }
    else if (!strcmp(attrib_name, "energy_now")) {
        status->energy_now = strtoull(attrib_value, NULL, 10) / 1000;
    }
    else if (!strcmp(attrib_name, "voltage_now")) {
        status->present_voltage = strtoull(attrib_value, NULL, 10) / 1000;
    }
    else if (!strcmp(attrib_name, "present")) {
        if (strstr(attrib_value, "1"))
            status->present = YES;
    }
}


//Given an attribute name and value, sets the appropriate member of a battery_info struct.
static void set_battery_info_attribute(char *attrib_name, char *attrib_value, struct battery_info *info) {

    if (!strcmp(attrib_name, "present")) {
        if (strstr(attrib_value, "1"))
	        info->present = YES;
    }
    else if (!strcmp(attrib_name, "charge_full_design")) {
        info->charge_full_design = strtoull(attrib_value, NULL, 10) / 1000;
    }
    else if (!strcmp(attrib_name, "charge_full")) {
        info->charge_full = strtoull(attrib_value, NULL, 10) / 1000;
    }
    else if (!strcmp(attrib_name, "energy_full_design")) {
        info->energy_full_design = strtoull(attrib_value, NULL, 10) / 1000;
    }
    else if (!strcmp(attrib_name, "energy_full")) {
        info->energy_full = strtoull(attrib_value, NULL, 10) / 1000;
    }
    else if (!strcmp(attrib_name, "voltage_min_design")) {
        info->design_voltage = strtoull(attrib_value, NULL, 10) / 1000;
    }
    else if (!strcmp(attrib_name, "model_name")) {
        strncpy(info->model_number, attrib_value, 32);
    }
    else if (!strcmp(attrib_name, "serial_number")) {
        strncpy(info->serial_number, attrib_value, 32);
    }
    else if (!strcmp(attrib_name, "technology")) {
        if (strstr(attrib_value, "Li-ion"))
	        strncpy(info->battery_type, "LION\n\0", 6);
	    else if (strstr(attrib_value, "Li-poly"))
	        strncpy(info->battery_type, "LiP\n\0", 6);
	    else
	        strncpy(info->battery_type, attrib_value, 32);
        info->battery_technology = RECHARGEABLE;
    }
    else if (!strcmp(attrib_name, "manufacturer")) {
      strncpy(info->oem_info, attrib_value, 32);
    }
}


//Gets a battery's status from the sysfs and stores it in last_status.
int update_battery_status(int battery_index) {

    int i, rc;
    DIR *battery_dir;
    struct dirent * dp;
    FILE *file;
    char filename[256];
    char data[128];
    char *ptr;

    struct battery_status status;
    memset(&status, 0, sizeof(struct battery_status));

    if (battery_index >= MAX_BATTERY_SUPPORTED)
        return -1;

    battery_dir = get_battery_dir(battery_index);
    if (!battery_dir) {
        //Battery directory does not exist--this normally occurs when a battery slot is removed
        if (errno == ENOENT) {
            status.present = NO;
            memcpy(&last_status[battery_index], &status, sizeof(struct battery_status));
            return 1;
        }
        else {
            xcpmd_log(LOG_ERR, "opendir in update_battery_status for directory %s/BAT%d failed with error %d\n", BATTERY_DIR_PATH, battery_index, errno);
            return 0;
        }
    }

    //Loop over the files in the directory.
    while ((dp = readdir(battery_dir)) != NULL) {

        //Convert from dirent to file and read out the data.
        if (dp->d_type == DT_REG) {

            memset(filename, 0, sizeof(filename));
            sprintf(filename, "%s/BAT%i/%s", BATTERY_DIR_PATH, battery_index, dp->d_name);

            file = fopen(filename, "r");
            if (file == NULL)
                continue;

            memset(data, 0, sizeof(data));
            fgets(data, sizeof(data), file);

            fclose(file);

            //Trim off leading spaces.
            ptr = data;
            while(*ptr == ' ')
                ptr += sizeof(char);

            //Set the attribute represented by this file.
            set_battery_status_attribute(dp->d_name, ptr, &status);

        }
    }

    // This check handles both cases for mA batteries: if are not charging
    // (current_now == 0) but have capacity or the battery is totally dead
    // (charge_now == 0) but it is charging. If both are zero, they will
    // both be zero in the end.
    if (status.charge_now != 0 || status.current_now != 0) {
        // Rate in mA, remaining in mAh
        status.present_rate = status.current_now;
        status.remaining_capacity = status.charge_now;
    }
    else {
        // Rate in mW, remaining in mWh
        status.present_rate = status.power_now;
        status.remaining_capacity = status.energy_now;
    }

    closedir(battery_dir);
    memcpy(&last_status[battery_index], &status, sizeof(struct battery_status));
    //print_battery_status();
    return 1;
}


//Gets a battery's info from the sysfs and stores it in last_info.
int update_battery_info(int battery_index) {

    int i, rc;
    DIR *battery_dir;
    struct dirent * dp;
    FILE *file;
    char filename[256];
    char data[128];
    char *ptr;

    struct battery_info info;
    memset(&info, 0, sizeof(struct battery_info));

    if (battery_index >= MAX_BATTERY_SUPPORTED)
        return 0;

    if (!battery_slot_is_present(battery_index)) {
        return 0;
    }


    battery_dir = get_battery_dir(battery_index);
    if (!battery_dir) {
        xcpmd_log(LOG_ERR, "opendir in update_battery_info() for directory %s/BAT%d failed with error %d\n", BATTERY_DIR_PATH, battery_index, errno);
        return 0;
    }

    //Loop over the files in the directory.
    while ((dp = readdir(battery_dir)) != NULL) {

        //Convert from dirent to file and read out the data.
        if (dp->d_type == DT_REG) {

            memset(filename, 0, sizeof(filename));
            sprintf(filename, "%s/BAT%i/%s", BATTERY_DIR_PATH, battery_index, dp->d_name);

            file = fopen(filename, "r");
            if (file == NULL)
                continue;

            memset(data, 0, sizeof(data));
            fgets(data, sizeof(data), file);

            fclose(file);

            //Trim off leading spaces.
            ptr = data;
            while(*ptr == ' ')
                ptr += sizeof(char);

            //Set the attribute represented by this file.
            set_battery_info_attribute(dp->d_name, ptr, &info);

        }
    }

    //In sysfs, the charge nodes are for batteries reporting in mA and
    //the energy nodes are for mW.
    if (info.charge_full_design != 0) {
        info.power_unit = mA;
        info.design_capacity = info.charge_full_design;
        info.last_full_capacity = info.charge_full;
    }
    else {
        info.power_unit = mW;
        info.design_capacity = info.energy_full_design;
        info.last_full_capacity = info.energy_full;
    }

    //Unlike the old procfs files, sysfs does not report some values like the
    //warn and low levels. These values are generally ignored anyway. The
    //various OS's decide what to do at different depletion levels through
    //their own policies. These are just some approximate values to pass.
    //TODO govern these by policy
    info.design_capacity_warning = info.last_full_capacity * (BATTERY_WARNING_PERCENT / 100);
    info.design_capacity_low = info.last_full_capacity * (BATTERY_LOW_PERCENT / 100);

    info.capacity_granularity_1 = 1;
    info.capacity_granularity_2 = 1;

    closedir(battery_dir);
    memcpy(&last_info[battery_index], &info, sizeof(struct battery_info));

    return 1;
}


//Exactly what it says on the tin.
void write_battery_info_to_xenstore(int battery_index) {

    if (!battery_slot_is_present(battery_index)) {
        xcpmd_log(LOG_INFO, "Detected removal of battery slot %d in info.\n", battery_index);
        cleanup_removed_battery(battery_index);
        return;
    }

    struct battery_info * info;
    char bif[1024], string_info[256], xenstore_path[128];

    info = &last_info[battery_index];

    memset(bif, 0, 1024);
    memset(string_info, 0, 256);
    // write 9 dwords (so 9*4) + length of 4 strings + 4 null terminators
    snprintf(bif, 3, "%02x",
             (unsigned int)(9*4 +
                            strlen(info->model_number) +
                            strlen(info->serial_number) +
                            strlen(info->battery_type) +
                            strlen(info->oem_info) + 4));
    write_ulong_lsb_first(bif+2, info->power_unit);
    write_ulong_lsb_first(bif+10, info->design_capacity);
    write_ulong_lsb_first(bif+18, info->last_full_capacity);
    write_ulong_lsb_first(bif+26, info->battery_technology);
    write_ulong_lsb_first(bif+34, info->design_voltage);
    write_ulong_lsb_first(bif+42, info->design_capacity_warning);
    write_ulong_lsb_first(bif+50, info->design_capacity_low);
    write_ulong_lsb_first(bif+58, info->capacity_granularity_1);
    write_ulong_lsb_first(bif+66, info->capacity_granularity_2);

    snprintf(string_info, 256, "%02x%s%02x%s%02x%s%02x%s",
             (unsigned int)strlen(info->model_number), info->model_number,
             (unsigned int)strlen(info->serial_number), info->serial_number,
             (unsigned int)strlen(info->battery_type), info->battery_type,
             (unsigned int)strlen(info->oem_info), info->oem_info);
    strncat(bif+73, string_info, 1024-73-1);

    //Ensure the directory exists before trying to write the leaves
    make_xenstore_battery_dir(battery_index);

    //Now write the leaves.
    sprintf(xenstore_path, "%s%i/%s", XS_BATTERY_PATH, battery_index, XS_BIF_LEAF);
    xenstore_write(bif, xenstore_path);


    //Here for compatibility--will be removed eventually
    if (battery_index == 0)
        xenstore_write(bif, XS_BIF);
    else
        xenstore_write(bif, XS_BIF1);
}


//Exactly what it says on the tin.
void write_battery_status_to_xenstore(int battery_index) {

    struct battery_status * status;
    char bst[35], xenstore_path[128];
    int num_batteries, current_battery_level;

    if (battery_index > MAX_BATTERY_SUPPORTED)
        return;

    num_batteries = get_num_batteries();
    if (num_batteries == 0)
        xenstore_write("0", XS_BATTERY_PRESENT);
    else
        xenstore_write("1", XS_BATTERY_PRESENT);

    if (!battery_slot_is_present(battery_index)) {
        cleanup_removed_battery(battery_index);
        return;
    }

    status = &last_status[battery_index];

    //Delete the BST and reset the "present" flag if the battery is not currently present.
    if (status->present != YES) {

        sprintf(xenstore_path, "%s%i/%s", XS_BATTERY_PATH, battery_index, XS_BST_LEAF);
        xenstore_rm(xenstore_path);

        sprintf(xenstore_path, "%s%i/%s", XS_BATTERY_PATH, battery_index, XS_BATTERY_PRESENT_LEAF);
        xenstore_write("0", xenstore_path);
        return;
    }

    //Build the BST structure.
    memset(bst, 0, 35);
    snprintf(bst, 3, "%02x", 16);
    write_ulong_lsb_first(bst+2, status->state);
    write_ulong_lsb_first(bst+10, status->present_rate);
    write_ulong_lsb_first(bst+18, status->remaining_capacity);
    write_ulong_lsb_first(bst+26, status->present_voltage);

    //Ensure the directory exists before trying to write the leaves
    make_xenstore_battery_dir(battery_index);

    //Now write the leaves.
    sprintf(xenstore_path, XS_BATTERY_PATH "%i/" XS_BST_LEAF, battery_index);
    xenstore_write(bst, xenstore_path);

    sprintf(xenstore_path, "%s%i/%s", XS_BATTERY_PATH, battery_index, XS_BATTERY_PRESENT_LEAF);
    xenstore_write("1", xenstore_path);

    //Here for compatibility--will be removed eventually
    if (battery_index == 0)
        xenstore_write(bst, XS_BST);
    else
        xenstore_write(bst, XS_BST1);

    current_battery_level = get_current_battery_level();
    if (current_battery_level == NORMAL || get_ac_adapter_status() == ON_AC)
        xenstore_rm(XS_CURRENT_BATTERY_LEVEL);
    else {
        xenstore_write_int(current_battery_level, XS_CURRENT_BATTERY_LEVEL);
        notify_com_citrix_xenclient_xcpmd_battery_level_notification(xcdbus_conn, XCPMD_SERVICE, XCPMD_PATH);
        xcpmd_log(LOG_ALERT, "Battery level below normal - %d!\n", current_battery_level);
    }

#ifdef XCPMD_DEBUG
    //xcpmd_log(LOG_DEBUG, "~Updated battery information in xenstore\n");
#endif
}


//Updates status and info of all batteries locally and in the xenstore.
void update_batteries(void) {

    struct battery_status old_status[MAX_BATTERY_SUPPORTED];
    struct battery_info old_info[MAX_BATTERY_SUPPORTED];
    char path[256];
    unsigned int i;

    if ( pm_specs & PM_SPEC_NO_BATTERIES )
        return;

    if (get_num_batteries() == 0)
        return;

    //Keep a copy of what the battery status/info used to be.
    memcpy(old_status, last_status, sizeof(last_status));
    memcpy(old_info, last_info, sizeof(last_info));

    //Two loops prevents the system from shutting down immediately after startup
    //if BAT0 is depleted and BAT1 isn't.
    for (i=0; i < MAX_BATTERY_SUPPORTED; ++i) {
        update_battery_status(i);
        update_battery_info(i);
    }

    //Write back to the xenstore and only send notifications if things have changed.
    for (i=0; i < MAX_BATTERY_SUPPORTED; ++i) {

        write_battery_status_to_xenstore(i);
        write_battery_info_to_xenstore(i);

        if (memcmp(&old_info[i], &last_info[i], sizeof(struct battery_info))) {
            sprintf(path, "%s%i/%s", XS_BATTERY_EVENT_PATH, i, XS_BATTERY_INFO_EVENT_LEAF);
            xenstore_write("1", path);
        }

        if (memcmp(&old_status[i], &last_status[i], sizeof(struct battery_status))) {
            sprintf(path, "%s%i/%s", XS_BATTERY_EVENT_PATH, i, XS_BATTERY_STATUS_EVENT_LEAF);
            xenstore_write("1", path);
        }
    }

    if (memcmp(old_info, last_info, sizeof(last_info)))
        notify_com_citrix_xenclient_xcpmd_battery_info_changed(xcdbus_conn, XCPMD_SERVICE, XCPMD_PATH);

    if (memcmp(old_status, last_status, sizeof(last_status))) {
        //Here for compatibility--should eventually be removed
        xenstore_write("1", XS_BATTERY_STATUS_CHANGE_EVENT_PATH);

        notify_com_citrix_xenclient_xcpmd_battery_status_changed(xcdbus_conn, XCPMD_SERVICE, XCPMD_PATH);
    }
}


//Counts the number of battery slots in the sysfs.
int get_num_batteries(void) {

    int count = 0;
    DIR * dir;
    struct dirent * dp;

    dir = opendir(BATTERY_DIR_PATH);
    if (!dir) {
        xcpmd_log(LOG_ERR, "opendir in get_num_batteries failed for directory %s with error %d\n", BATTERY_DIR_PATH, errno);
        return 0;
    }

    //Count all entries whose names start with BAT.
    while ((dp = readdir(dir)) != NULL) {
        if (!strncmp(dp->d_name, "BAT", 3))
            ++count;
    }

    closedir(dir);

    return count;
}


//Counts the number of batteries present in the sysfs.
int get_num_batteries_present(void) {

    int count;
    DIR * dir;
    struct dirent * dp;
    FILE * file;
    char data[128];
    char filename[256];

    dir = opendir(BATTERY_DIR_PATH);
    if (!dir) {
        xcpmd_log(LOG_ERR, "opendir in get_num_batteries_present() failed for directory %s with error %d\n", BATTERY_DIR_PATH, errno);
        return 0;
    }

    //Check all /sys/class/power_supply/BAT*/present.
    count = 0;
    while ((dp = readdir(dir)) != NULL) {

        if (!strncmp(dp->d_name, "BAT", 3)) {
            sprintf(filename, "%s/%s/present", BATTERY_DIR_PATH, dp->d_name);
            file = fopen(filename, "r");
            if (file == NULL)
                continue;

            fgets(data, sizeof(data), file);
            fclose(file);

            if (strstr(data, "1")) {
                ++count;
            }
        }
    }

    closedir(dir);

    return count;
}


//Remove a battery's entries from the xenstore.
static void cleanup_removed_battery(int battery_index) {

    char path[256];

    sprintf(path, "%s%d", XS_BATTERY_PATH, battery_index);
    xenstore_rm(path);

    if (battery_index > 0) {
        xenstore_rm(XS_BST1);
        xenstore_rm(XS_BIF1);
    }
    else {
        xenstore_rm(XS_BST);
        xenstore_rm(XS_BIF);
    }

    if (get_num_batteries_present() == 0)
        xenstore_write("0", XS_BATTERY_PRESENT);
    else
        xenstore_write("1", XS_BATTERY_PRESENT);

}


//Checks the sysfs to see if a battery slot is present at the specified index.
static bool battery_slot_is_present(int battery_index) {

    DIR * dir;
    char path[256];

    sprintf(path, "%s/BAT%d", BATTERY_DIR_PATH, battery_index);
    dir = opendir(path);
    if (!dir) {
        //if (errno == ENOENT)
        //    xcpmd_log(LOG_DEBUG, "Opendir in battery_slot_is_present() failed for directory %s with error ENOENT\n", path);
        //else
        //    xcpmd_log(LOG_ERR, "opendir in battery_slot_is_present() failed for directory %s with error %d\n", path, errno);
        return false;
    }
    closedir(dir);

    return true;
}


//Updates battery info/status and schedules itself to run again in 4 seconds.
void wrapper_refresh_battery_event(int fd, short event, void *opaque) {

    struct timeval tv;
    memset(&tv, 0, sizeof(tv));

    update_batteries();

    tv.tv_sec = 4;
    evtimer_add(&refresh_battery_event, &tv);
}
