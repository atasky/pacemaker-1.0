/* $Id: crmdmain.c,v 1.23 2004/07/30 15:31:05 andrew Exp $ */
/* 
 * Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <sys/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <apphb.h>

#include <clplumbing/ipc.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/uids.h>
#include <clplumbing/realtime.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_poll.h>

#include <crm/crm.h>
#include <crm/common/ctrl.h>
#include <crm/common/ipc.h>
#include <crm/common/xml.h>

#include <crmd.h>
#include <crmd_fsa.h>

#include <crm/dmalloc_wrapper.h>

const char* crm_system_name = SYS_NAME;
#define OPTARGS	"skrhV"

void usage(const char* cmd, int exit_status);
int init_start(void);
void crmd_hamsg_callback(const struct ha_msg* msg, void* private_data);
gboolean crmd_tickle_apphb(gpointer data);

GMainLoop*  crmd_mainloop = NULL;

int
main(int argc, char ** argv)
{
    int	req_restart = FALSE;
    int	req_status  = FALSE;
    int	req_stop    = FALSE;
    int	argerr      = 0;
    int flag;
    
    cl_log_set_entity(crm_system_name);
    cl_log_set_logfile(DAEMON_LOG);
    cl_log_set_debugfile(DAEMON_DEBUG);
    cl_log_set_facility(LOG_USER);
    CL_SIGNAL(DEBUG_INC, alter_debug);
    CL_SIGNAL(DEBUG_DEC, alter_debug);
    
    while ((flag = getopt(argc, argv, OPTARGS)) != EOF) {
		switch(flag) {
			case 'V':
				alter_debug(DEBUG_INC);
				break;
			case 's':		/* Status */
				req_status = TRUE;
				break;
			case 'k':		/* Stop (kill) */
				req_stop = TRUE;
				break;
			case 'r':		/* Restart */
				req_restart = TRUE;
				break;
			case 'h':		/* Help message */
				usage(crm_system_name, LSB_EXIT_OK);
				break;
			default:
				++argerr;
				break;
		}
    }
    
    if (optind > argc) {
		++argerr;
    }
    
    if (argerr) {
		usage(crm_system_name,LSB_EXIT_GENERIC);
    }
    
    // read local config file
    
    if (req_status){
		return init_status(PID_FILE, crm_system_name);
    }
  
    if (req_stop){
		return init_stop(PID_FILE);
    }
	
    if (req_restart) { 
		init_stop(PID_FILE);
    }
	
    return init_start();
}


int
init_start(void)
{
    long pid;
    enum crmd_fsa_state state;

    if ((pid = get_running_pid(PID_FILE, NULL)) > 0) {
		crm_crit("already running: [pid %ld].", pid);
		exit(LSB_EXIT_OK);
    }
	
    fsa_state = S_PENDING;
    state = s_crmd_fsa(C_STARTUP, I_STARTUP, NULL);

    if (state == S_PENDING) {
	    /* Create the mainloop and run it... */
	    crmd_mainloop = g_main_new(FALSE);
	    crm_info("Starting %s", crm_system_name);
	    
#ifdef REALTIME_SUPPORT
	    static int  crm_realtime = 1;
	    if (crm_realtime == 1){
		    cl_enable_realtime();
	    }else if (crm_realtime == 0){
		    cl_disable_realtime();
	    }
	    cl_make_realtime(SCHED_RR, 5, 64, 64);
#endif

	    g_main_run(crmd_mainloop);
	    return_to_orig_privs();
    } else {

	    crm_err("Startup of CRMd failed.  Current state: %s",
		   fsa_state2string(state));
	    
    }
    
    
    if (unlink(PID_FILE) == 0) {
	    crm_info("[%s] stopped", crm_system_name);
    }
    
    return state != S_PENDING;
}



void
usage(const char* cmd, int exit_status)
{
    FILE* stream;

    stream = exit_status ? stderr : stdout;

    fprintf(stream, "usage: %s [-srkh]"
			"[-c configure file]\n", cmd);
/* 	fprintf(stream, "\t-d\tsets debug level\n"); */
/* 	fprintf(stream, "\t-s\tgets daemon status\n"); */
/* 	fprintf(stream, "\t-r\trestarts daemon\n"); */
/* 	fprintf(stream, "\t-k\tstops daemon\n"); */
/* 	fprintf(stream, "\t-h\thelp message\n"); */
    fflush(stream);

    exit(exit_status);
}


gboolean
crmd_tickle_apphb(gpointer data)
{
    char	app_instance[APPNAME_LEN];
    int     rc = 0;
    sprintf(app_instance, "%s_%ld", crm_system_name, (long)getpid());

    rc = apphb_hb();
    if (rc < 0) {
		cl_perror("%s apphb_hb failure", app_instance);
		exit(3);
    }
    return TRUE;
}
