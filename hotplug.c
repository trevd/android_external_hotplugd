#define LOG_TAG "hotplugd"
#include <cutils/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/types.h>
#include <linux/netlink.h>

#include <cutils/properties.h>

struct uevent {
	const char *action;
	const char *path;
	const char *subsystem;
	const char *firmware;
	const char *partition_name;
	const char *name;
	const char *type;
	const char *product_id;
	const char *vendor_id;
	int partition_num;
	int major;
	int minor;
	unsigned int seqnum;
};
struct hotplug_info {
	const char *modeswitch_d;
	const char *product_id;
	const char *vendor_id;
	int modeswitch_length;
	int debug;

} ;
void die(char *s)
{
	LOGI("dying with %s",s);
	write(2,s,strlen(s));
	exit(1);
}
char *bprintf(const char *fmt, ...)
{
	char *strp = NULL;

	va_list args;
	va_start(args, fmt);
	vasprintf(&strp, fmt, args);
	va_end(args);

	return strp;
}

static int file_exists(char *filename)
{
	struct stat sb;
	if(stat(filename, &sb) == -1) {
		LOGD("%s Not Found",filename);
		return 0 ; 
	}
	LOGD("%s Found",filename);
	return 1;
}
static int test_directory(struct hotplug_info *hotplug_info,const char * path)
{
	int isDir = 0;
	struct stat statbuf;
	if (stat(path, &statbuf) != -1) {
	   if (S_ISDIR(statbuf.st_mode)) {
	   	hotplug_info->modeswitch_d = path;
   		LOGD("%s found",path);
		isDir = 1;
	   }
	else
		LOGD("%s not found",path);
	}
	return isDir;
}
static void start_service(const char * service_name)
{
	property_set("ctl.start",service_name);
	LOGD("starting %s",service_name);
}
static void stop_service(const char * service_name)
{
	property_set("ctl.stop",service_name);
	LOGD("stop %s",service_name);
}
static void find_modeswitch_directory(struct hotplug_info *hotplug_info)
{
	LOGD("hotplug.modeswitch.d not set...checking known locations");
	if(test_directory(hotplug_info,"/etc/usb_modeswitch")) return;
	if(test_directory(hotplug_info,"/system/etc/usb_modeswitch")) return;
	if(test_directory(hotplug_info,"/system/etc/usb_modeswitch.d")) return;
	die("hotplug.modeswitch.d");

	
}
static int wait_for_property(const char *name, const char *desired_value, int maxwait)
{
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    int maxnaps = maxwait / 1;
    if (maxnaps < 1) 
        maxnaps = 1;
   
    while (maxnaps-- > 0) {
        usleep(1000000);
        if (property_get(name, value, NULL)) {
            if (desired_value == NULL ||
                    strcmp(value, desired_value) == 0) {
                return 0;
            }
        }
    }
    return -1; /* failure */
}

static void write_uevent_logcat(struct uevent *uevent,const char* label)
{
	LOGD(" %s { action:'%s' sub:'%s', type:'%s', name:'%s', vendor:%s product:%s\n\t\t\tpath:'%s' }\n",label,uevent->action, uevent->subsystem,uevent->type,uevent->name,uevent->vendor_id ,uevent->product_id,uevent->path);
}
static int property_test(const char* name,char* testvalue )
{
	char value[PROPERTY_VALUE_MAX];
	// check to see if this is a device we are interested
	LOGD("Checking value of %s",name);
	if(!property_get(name,value,NULL))
	{
		LOGD("Property Not Found");
		return 2;
	}
	LOGD("Comparing String %s %s",value,testvalue);
	int res = strncmp(value,testvalue,strlen(testvalue));
	LOGD("Result of Comparison %u",res);
	return res;
}

static void handle_event(struct uevent *uevent,struct hotplug_info *hotplug_info)
{
	write_uevent_logcat(uevent,uevent->type);
	//return ;
	int c =uevent->action[0];
	switch(c)
	{
		case 'a':{
			if(!strncmp(uevent->type,"usb_interface",12)){		
				// see if this interface is ripe for a switching
				write_uevent_logcat(uevent,uevent->type);
				char * config_filename;
				if(!strncmp(uevent->vendor_id,"1bbb",4))
				{
					// ignore a the archos key
					LOGI("Ignore Archos Stick");
					break ;
				}
				if (!(config_filename = malloc((hotplug_info->modeswitch_length + 10) * sizeof(char))))
				{
					LOGE("cannot alloc memory");
					break;
				} else {
					char * config_filename = bprintf("%s/%s:%s",hotplug_info->modeswitch_d,uevent->vendor_id ,uevent->product_id);
					LOGD("Looking For usb_modeswitch config in location %s",config_filename);
					if(!file_exists(config_filename)){ // usb_modeswitch file not found for this device
						LOGD("Config Not Found");
						break; 
						}
						
					char * usb_modeswitch_command = bprintf("switch_ms_to_3g:-v0x%s -p0x%s -c%s",uevent->vendor_id ,uevent->product_id,config_filename);
					start_service(usb_modeswitch_command);							
					free(usb_modeswitch_command);
					free(config_filename);
				}
				
			}else if(!strncmp(uevent->type,"usb-serial",10)){
				//Serial Killals
				write_uevent_logcat(uevent,uevent->type);
			
			} else if(!strncmp(uevent->subsystem,"tty",3)){
				write_uevent_logcat(uevent,uevent->type);
				if(!strncmp(uevent->name,"ttyUSB0",7))
					property_set("ril.pppd_tty", "/dev/ttyUSB0");
					
				if(!strncmp(uevent->name,"ttyUSB2",7))
				{
					property_set("rild.libargs", "-d /dev/ttyUSB2");
					property_set("rild.libpath", "/system/lib/libhuaweigeneric-ril.so");
					start_service("ril-daemon");
				}	
				
			}else {
				//write_uevent_logcat(uevent,"ignored");
			}
			
			break;
		}
		case 'r':{
			if(!strncmp(uevent->type,"usb_device",10)){
				//Serial Killa
				write_uevent_logcat(uevent,uevent->type);
				//handle_connection_connection();	
			} else if(!strncmp(uevent->subsystem,"tty",3)){
				write_uevent_logcat(uevent,uevent->type);
				if(!strncmp(uevent->name,"ttyUSB2",7))
				{
					LOGD("Stopping Rild");	
					stop_service("ril-daemon");
				}else if(!strncmp(uevent->name,"ttyUSB0",7)){
					system("pkill -9 ppp");
					LOGD("Killing PPP");	
				}
			}
			break;
		}
		case 'c':{
			//write_uevent_logcat(uevent,"ignored");
		}
			break;
	}
}

 
static void parse_hotplug_info(struct hotplug_info *hotplug_info)
{
	char value[PROPERTY_VALUE_MAX];
	if(property_get("hotplug.modeswitch.d",value,""))
		hotplug_info->modeswitch_d = value;
	else
	  find_modeswitch_directory(hotplug_info);		
	  
	hotplug_info->modeswitch_length = strlen(hotplug_info->modeswitch_d);
	
	char debug[PROPERTY_VALUE_MAX];
	if(!property_get("hotplug.debug",debug,"0"))
	hotplug_info->debug =atoi(debug);
}
static void parse_event(const char *msg, struct uevent *uevent,int debug)
{
	uevent->action = "";
	uevent->path = "";
	uevent->name = "";
	uevent->type = "";
	uevent->subsystem = "";
	uevent->firmware = "";
	uevent->major = -1;
	uevent->minor = -1;
	uevent->partition_name = NULL;
	uevent->partition_num = -1;
	uevent->vendor_id = "";
	uevent->product_id = "";

	while(*msg) {
		if(!strncmp(msg, "ACTION=", 7)) {
		    msg += 7;
		    uevent->action = msg;
		} else if(!strncmp(msg, "DEVPATH=", 8)) {
		    msg += 8;
		    uevent->path = msg;
		} else if(!strncmp(msg, "DEVNAME=", 8)) {
		    msg += 8;
		    uevent->name = msg;
		} else if(!strncmp(msg, "DEVTYPE=", 8)) {
		    msg += 8;
		    uevent->type = msg;
		} else if(!strncmp(msg, "SUBSYSTEM=", 10)) {
		    msg += 10;
		    uevent->subsystem = msg;
		} else if(!strncmp(msg, "FIRMWARE=", 9)) {
		    msg += 9;
		    uevent->firmware = msg;
    		} else if(!strncmp(msg, "MAJOR=", 6)) {
		    msg += 6;
		    uevent->major = atoi(msg);
		} else if(!strncmp(msg, "MINOR=", 6)) {
		    msg += 6;
		    uevent->minor = atoi(msg);
		} else if(!strncmp(msg, "PARTN=", 6)) {
		    msg += 6;
		    uevent->partition_num = atoi(msg);
		} else if(!strncmp(msg, "PARTNAME=", 9)) {
		    msg += 9;
		    uevent->partition_name = msg;
		} else if (!strncmp(msg, "PRODUCT=", 8)) {
		   	msg += 8 ;
		   	char * pch;
		   	pch = strtok ((char *)msg,"/");
			if(pch != NULL)
	  			uevent->vendor_id = pch;
	  		pch = strtok (NULL,"/");
	  		if(pch != NULL)
	  			uevent->product_id = pch;
		}		
		    /* advance to after the next \0 */
		while(*msg++);
	}
	return ;
}
int main(int argc, char *argv[])
{
	
	struct sockaddr_nl nls;
	struct pollfd pfd;
	char uevent_msg[1024];
	struct hotplug_info hotplug_info;
	parse_hotplug_info(&hotplug_info);
	// Open hotplug event netlink socket
	LOGI("Starting hotplugd For UsbModeSwitch Management - Settings:modeswitch.d:%s length:%u debug:%u",hotplug_info.modeswitch_d,hotplug_info.modeswitch_length,hotplug_info.debug);
	memset(&nls,0,sizeof(struct sockaddr_nl));
	nls.nl_family = AF_NETLINK;
	nls.nl_pid = getpid();
	nls.nl_groups = -1;

	pfd.events = POLLIN;
	pfd.fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (pfd.fd==-1)
		LOGE("hotplugd cannot create socket, are you root?\n");

	// Listen to netlink socket

	if (bind(pfd.fd, (void *)&nls, sizeof(struct sockaddr_nl)))
		die("bind failed\n");
	LOGD("Give us a go on your UEVENT");
	while (-1!=poll(&pfd, 1, -1)) {

		int uevent_buffer_length = recv(pfd.fd, uevent_msg, sizeof(uevent_msg), MSG_DONTWAIT);
		if (uevent_buffer_length == -1) 
			die("receive error\n");
		struct uevent uevent;
	        parse_event(uevent_msg, &uevent,hotplug_info.debug);
	        handle_event(&uevent,&hotplug_info);
	}
	die("poll error\n");

	// Dear gcc: shut up.
	return 0;
}
	        
