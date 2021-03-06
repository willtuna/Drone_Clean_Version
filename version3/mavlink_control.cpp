/****************************************************************************
 *
 *   Copyright (c) 2014 MAVlink Development Team. All rights reserved.
 *   Author: Trent Lukaczyk, <aerialhedgehog@gmail.com>
 *           Jaycee Lock,    <jaycee.lock@gmail.com>
 *           Lorenz Meier,   <lm@inf.ethz.ch>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file mavlink_control.cpp
 *
 * @brief An example offboard control process via mavlink
 *
 * This process connects an external MAVLink UART device to send an receive data
 *
 * @author Trent Lukaczyk, <aerialhedgehog@gmail.com>
 * @author Jaycee Lock,    <jaycee.lock@gmail.com>
 * @author Lorenz Meier,   <lm@inf.ethz.ch>
 *
 * 2017.07.05
 * @function si2_message_broadcast, si2_mission, get_current_time
 * @author Will Liu,<willvegapunk.ee03@g2.nctu.edu.tw>
 */



// ------------------------------------------------------------------------------
//   Includes
// ------------------------------------------------------------------------------

#include "mavlink_control.h"


// ------------------------------------------------------------------------------
//   TOP
// ------------------------------------------------------------------------------
int
top (int argc, char **argv)
{

	// --------------------------------------------------------------------------
	//   PARSE THE COMMANDS
	// --------------------------------------------------------------------------

	// Default input arguments
#ifdef __APPLE__
	char *uart_name = (char*)"/dev/tty.usbmodem1";
#else
	char *uart_name = (char*)"/dev/ttyAMA0";
#endif
	int baudrate = 57600;

	// do the parse, will throw an int if it fails
// Vega's Note
// this doesn't matter, since we all use the default value we set in Raspberry Pi
// baudrate 57600, uartname = ttyAMA0
	parse_commandline(argc, argv, uart_name, baudrate);


	// --------------------------------------------------------------------------
	//   PORT and THREAD STARTUP
	// --------------------------------------------------------------------------

	/*
	 * Instantiate a serial port object
	 *
	 * This object handles the opening and closing of the offboard computer's
	 * serial port over which it will communicate to an autopilot.  It has
	 * methods to read and write a mavlink_message_t object.  To help with read
	 * and write in the context of pthreading, it gaurds port operations with a
	 * pthread mutex lock.
	 *
	 */
	Serial_Port serial_port(uart_name, baudrate);


	/*
	 * Instantiate an autopilot interface object
	 *
	 * This starts two threads for read and write over MAVlink. The read thread
	 * listens for any MAVlink message and pushes it to the current_messages
	 * attribute. 
     * 
     * The write thread at the moment only streams a position target
	 * in the local NED frame (mavlink_set_position_target_local_ned_t), which
	 * is changed by using the method update_setpoint(). 
     * Sending these messages are only half the requirement to get response from the autopilot,
     * a signal to enter "offboard_control" mode is sent by using the enable_offboard_control()
	 * method.  Signal the exit of this mode with disable_offboard_control().  It's
	 * important that one way or another this program signals offboard mode exit,
	 * otherwise the vehicle will go into failsafe.
	 */


	Autopilot_Interface autopilot_interface(&serial_port);

	/*
	 * Setup interrupt signal handler
	 *
	 * Responds to early exits signaled with Ctrl-C.  The handler will command
	 * to exit offboard mode if required, and close threads and the port.
	 * The handler in this example needs references to the above objects.
	 *
	 */
	serial_port_quit         = &serial_port;
	autopilot_interface_quit = &autopilot_interface;
    // customize the signal Ctrl +C to exit the offboard mod
	signal(SIGINT,quit_handler); 

	/*
	 * Start the port and autopilot_interface
	 * This is where the port is opened, and read and write threads are started.
	 */
	serial_port.start();

                
	autopilot_interface.start();
    // This part is added by SIDRONE
    mavlink_message_t msg_send;

// Vega Customized Part1:  ARM the drone here to prepare the takeoff, in previous experiment we fail here
// due to the barometer is highly affected by the Wind. This is still a point need to overcome
    printf("Sleeping Prepare to arm ");

    sleep(5);

	mavlink_msg_command_long_pack( 0 , 0, &msg_send, autopilot_interface.current_messages.sysid , autopilot_interface.current_messages.compid , MAV_CMD_DO_SET_MODE , 0 , MAV_MODE_GUIDED_ARMED	, 0 , 0, 0, 0, 0, 0);

    printf("Write %b bytes\n", serial_port.write_message(msg_send) );
    printf("ARMED !!\n");



	// --------------------------------------------------------------------------
	//   RUN COMMANDS
	// --------------------------------------------------------------------------

	/*
	 * Now we can implement the algorithm we want on top of the autopilot interface
	 */

    //Comment Out this line to stop command the drone
    // This command leads us to the customize command mode. -1 means fly upward 1m for our 1st command after get 
    // into the command function
	commands(autopilot_interface,0,0,-1,0,0,0);
    
    //Read Message Function
    //  si2_message_broadcast(autopilot_interface);   
        
	// --------------------------------------------------------------------------
	//   THREAD and PORT SHUTDOWN
	// --------------------------------------------------------------------------

	/*
	 * Now that we are done we can stop the threads and close the port
	 */
	autopilot_interface.stop();
	serial_port.stop();


	// --------------------------------------------------------------------------
	//   DONE
	// --------------------------------------------------------------------------

	// woot!
	return 0;

}


// ------------------------------------------------------------------------------
//   COMMANDS
// ------------------------------------------------------------------------------

// si2_mission
void si2_mission(float dx, float dy, float dz, float vx, float vy , float vz,mavlink_set_position_target_local_ned_t &sp){
	//set_velocity(vx,vy,vz,sp);
	set_position(dx,dy,dz,sp);
	sp.type_mask = //MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_VELOCITY ;
	    	   MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_POSITION;
	return ; 
}


void
commands(Autopilot_Interface &api,float dx,float dy,float dz, float vx, float vy, float vz)
{

	// --------------------------------------------------------------------------
	//   START OFFBOARD MODE
	// --------------------------------------------------------------------------

	api.enable_offboard_control();
	usleep(200);
    // give some time to let it sink in
    // write_thread include home request so i changed to 200
	// now the autopilot is accepting setpoint commands


	// --------------------------------------------------------------------------
	//   SEND OFFBOARD COMMANDS
	// --------------------------------------------------------------------------
	printf("SEND OFFBOARD COMMANDS\n");

	// initialize command data strtuctures
	mavlink_set_position_target_local_ned_t sp;

    // Vega Note
    // the initial position is set as we instantiate the autopilot_interface type : api
	mavlink_set_position_target_local_ned_t ip = api.initial_position;

	// autopilot_interface.h provides some helper functions to build the command
    
    // Vega's Note
    // This for loop simply wait 15 sec, it doesn't send any command, it simply keep printing out current postion    
    // and target postion, if the drone is stable the current_pos should remain the same. 
        for(int i=0 ;i<15;++i){
            printf("write_thread_initialization_Waiting for %d sec\n",(15-i));
	        mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
            mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
            printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
            printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
	    
            sleep(1);
        }
	// Example 1 - Set Velocity
	/*set_velocity( vx       , // [m/s]
				  vy       , // [m/s]
				   vz       , // [m/s]
				   sp        );
        */
	// Example 2 - Set Position
	// mavlink_local_position_ned_t initial_pos = api.current_messages.local_position_ned;
    

    // Vega's Note 
    // This is the place take the function parameters to set_position, in the top function, we make our drone
    // take off by 1m
	 set_position(  dx + api.initial_position.x, 
		        dy + api.initial_position.y,
			dz + api.initial_position.z,
			sp         );
        
    	
           
        //Example 1.2 - Append Yaw Command
	//set_yaw( api.current_messages.attitude.yaw , // [rad]	
        //        sp     );
        
	// SEND THE COMMAND
	
	sp.type_mask =	  MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_POSITION;
    // Vega's Note Just let it run its write thread, send out the command to the drone
    api.update_setpoint(sp);
	
        
    // NOW pixhawk will try to move
        
	// Wait for 10 seconds, check position
    printf("\n\n\n----------------------Command Upward 1m by set_position for 10sec----------------------\n");
	for (int i=0; i <10; i++){
	    mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
        mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
        printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
		printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
	}
	
    // Vega's Note This current_pos could be used for dynamic references origin for future test
    /*
     *mavlink_local_position_ned_t current_pos = api.current_messages.local_position_ned;
     */



    // Vega's Note, This is do the first customized command , si2_mission, it use the initial_pos as the references
    // coordinate_frame's origin. So api.initial_pos.z-1 means keep in 1m above initial_pos
    printf("\n\n\n----------------------si2_mission Hold this altitude for 5sec------------------\n");
	si2_mission(api.initial_position.x , api.initial_position.y ,  api.initial_position.z-1, 0, 0 , 0 ,sp);
        
    api.update_setpoint(sp);

    // Print out current postion message
    for(int i=0; i<5;++i){
	    mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
        mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
        printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
		printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
        }



    /* This current_pos could be used for dynamic references origin for future test
     * current_pos = api.current_messages.local_position_ned;
     */



	si2_mission( api.initial_position.x+1  , api.initial_position.y , api.initial_position.z -1, 0, 0 , 0 ,sp);
    printf("\n\n\n-----si2_mission move Forward 1m for 10 seconds------------\n");
	api.update_setpoint(sp);
	for (int i=0; i <10; i++){
	    mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
        mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
        printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
		printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
	}



    printf("\n\n\n----------------------si2_mission Hold this altitude for 5sec------------------\n");
	si2_mission(api.initial_position.x+1 , api.initial_position.y ,  api.initial_position.z-1, 0, 0 , 0 ,sp);
    api.update_setpoint(sp);
    for(int i=0; i<5;++i){
	    mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
        mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
        printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
		printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
        }
	printf("\n");





    printf("\n\n\n----------------------si2_mission Turn Right for 10sec------------------\n");
	si2_mission(api.initial_position.x+1 , api.initial_position.y+1 ,  api.initial_position.z-1, 0, 0 , 0 ,sp);
    api.update_setpoint(sp);
    for(int i=0; i<10;++i){
	    mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
        mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
        printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
		printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
        }
	printf("\n");




    printf("\n\n\n----------------------si2_mission Hold this altitude for 5sec------------------\n");
	si2_mission(api.initial_position.x+1 , api.initial_position.y+1,  api.initial_position.z-1, 0, 0 , 0 ,sp);
    api.update_setpoint(sp);
    for(int i=0; i<5;++i){
	    mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
        mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
        printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
		printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
        }
	printf("\n");



    printf("\n\n\n----------------------si2_mission Move Backward for 10sec------------------\n");
	si2_mission(api.initial_position.x , api.initial_position.y+1 ,  api.initial_position.z-1, 0, 0 , 0 ,sp);
        
    api.update_setpoint(sp);
    for(int i=0; i<10;++i){
	    mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
        mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
        printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
		printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
        }
	printf("\n");






    printf("\n\n\n----------------------si2_mission Hold this altitude for 5sec------------------\n");
	si2_mission(api.initial_position.x , api.initial_position.y+1,  api.initial_position.z-1, 0, 0 , 0 ,sp);
    api.update_setpoint(sp);
    for(int i=0; i<5;++i){
	    mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
        mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
        printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
		printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
        }
	printf("\n");



    printf("\n\n\n----------------------si2_mission Turn Left for 10sec------------------\n");
	si2_mission(api.initial_position.x , api.initial_position.y ,  api.initial_position.z-1, 0, 0 , 0 ,sp);
    api.update_setpoint(sp);
    for(int i=0; i<10;++i){
	    mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
        mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
        printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
		printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
        }
	printf("\n");

    printf("\n\n\n----------------------si2_mission Move Downward for 10sec------------------\n");
	si2_mission(api.initial_position.x , api.initial_position.y ,  api.initial_position.z, 0, 0 , 0 ,sp);
    api.update_setpoint(sp);
    for(int i=0; i<10;++i){
	    mavlink_local_position_ned_t pos = api.current_messages.local_position_ned;
        mavlink_position_target_local_ned_t  target_pos = api.current_messages.position_target_local_ned;
        printf("%i TARGET  POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, target_pos.x,target_pos.y, target_pos.z);
		printf("%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
        }
	printf("\n");




	// --------------------------------------------------------------------------
	//   STOP OFFBOARD MODE
	// --------------------------------------------------------------------------

	api.disable_offboard_control();

	// now pixhawk isn't listening to setpoint commands


	// --------------------------------------------------------------------------
	//   GET A MESSAGE
	// --------------------------------------------------------------------------
	printf("READ SOME MESSAGES \n");

	// copy current messages
	Mavlink_Messages messages = api.current_messages;

	// local position in ned frame
	mavlink_local_position_ned_t pos = messages.local_position_ned;
	printf("Got message LOCAL_POSITION_NED (spec: https://pixhawk.ethz.ch/mavlink/#LOCAL_POSITION_NED)\n");
	printf("    pos  (NED):  %f %f %f (m)\n", pos.x, pos.y, pos.z );

	// hires imu
	mavlink_highres_imu_t imu = messages.highres_imu;
	printf("Got message HIGHRES_IMU (spec: https://pixhawk.ethz.ch/mavlink/#HIGHRES_IMU)\n");
	printf("    ap time:     %llu \n", imu.time_usec);
	printf("    acc  (NED):  % f % f % f (m/s^2)\n", imu.xacc , imu.yacc , imu.zacc );
	printf("    gyro (NED):  % f % f % f (rad/s)\n", imu.xgyro, imu.ygyro, imu.zgyro);
	printf("    mag  (NED):  % f % f % f (Ga)\n"   , imu.xmag , imu.ymag , imu.zmag );
	printf("    baro:        %f (mBar) \n"  , imu.abs_pressure);
	printf("    altitude:    %f (m) \n"     , imu.pressure_alt);
	printf("    temperature: %f C \n"       , imu.temperature );

	printf("\n");


	// --------------------------------------------------------------------------
	//   END OF COMMANDS
	// --------------------------------------------------------------------------

	return;

}

void si2_message_broadcast(Autopilot_Interface &api){
    while(1){
	    // --------------------------------------------------------------------------
	    //   GET A MESSAGE
	    // --------------------------------------------------------------------------
	    printf("READ SOME MESSAGES \n");

	    // copy current messages
	    Mavlink_Messages messages = api.current_messages;

	    // local position in ned frame
	    mavlink_local_position_ned_t pos = messages.local_position_ned;
	    printf("Got message LOCAL_POSITION_NED \n");
	    printf("    pos  (NED):  %f %f %f (m)\n", pos.x, pos.y, pos.z );
        printf("velocity (NED):  %f %f %f (m/s)\n ",pos.vx, pos.vy, pos.vz);

	    // highres_imu
	    mavlink_highres_imu_t imu = messages.highres_imu;
	    printf("Got message HIGHRES_IMU \n");
	    printf("    altitude:    %f (m) \n"     , imu.pressure_alt);
	    printf("\n");

        // attribute
        mavlink_attitude_t attitude = messages.attitude;
        printf("Got message ATTITUDE #30 \n");
        printf("In Degree:   row:  %f    pitch:  %f    yaw:  %f \n",attitude.roll, attitude.pitch, attitude.yaw);
        // VFR_HUD
        mavlink_vfr_hud_t vfr_hud = messages.vfr_hud;
        printf("Got message VFR_HUD  #74 \n");
        printf("Headig in 360 degree(0 = North) : %d  \n", vfr_hud.heading);
        get_current_time();
        sleep(1);
    }

}











// ------------------------------------------------------------------------------
//   Parse Command Line
// ------------------------------------------------------------------------------
// throws EXIT_FAILURE if could not open the port
void
parse_commandline(int argc, char **argv, char *&uart_name, int &baudrate)
{

	// string for command line usage
	const char *commandline_usage = "usage: mavlink_serial -d <devicename> -b <baudrate>";

	// Read input arguments
	for (int i = 1; i < argc; i++) { // argv[0] is "mavlink"

		// Help
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("%s\n",commandline_usage);
			throw EXIT_FAILURE;
		}

		// UART device ID
		if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
			if (argc > i + 1) {
				uart_name = argv[i + 1];

			} else {
				printf("%s\n",commandline_usage);
				throw EXIT_FAILURE;
			}
		}

		// Baud rate
		if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--baud") == 0) {
			if (argc > i + 1) {
				baudrate = atoi(argv[i + 1]);

			} else {
				printf("%s\n",commandline_usage);
				throw EXIT_FAILURE;
			}
		}

	}
	// end: for each input argument

	// Done!
	return;
}


// ------------------------------------------------------------------------------
//   Quit Signal Handler
// ------------------------------------------------------------------------------
// this function is called when you press Ctrl-C
void
quit_handler( int sig )
{
	printf("\n");
	printf("TERMINATING AT USER REQUEST\n");
	printf("\n");

	// autopilot interface
	try {
		autopilot_interface_quit->handle_quit(sig);
	}
	catch (int error){}

	// serial port
	try {
		serial_port_quit->handle_quit(sig);
	}
	catch (int error){}

	// end program here
	exit(0);

}


// ------------------------------------------------------------------------------
//   Main
// ------------------------------------------------------------------------------
int
main(int argc, char **argv)
{
	// This program uses throw, wrap one big try/catch here
	try
	{
		int result = top(argc,argv);
		return result;
	}

	catch ( int error )
	{
		fprintf(stderr,"mavlink_control threw exception %i \n" , error);
		return error;
	}

}


void get_current_time(void){
    const int GMT8 = 8;
    // create a struct of time which contains the year date hour minute second format
    struct tm *tm_ptr;

    // time_t is for fetch the current time from GMT Jan 1 , 1970
    time_t current_time;
    // this function would return the time calculated from 1970
    time(&current_time); 

    // Convert the time_t to struct tm format
    tm_ptr = gmtime(&current_time);

    printf("date: %04d / %02d / %02d \n", tm_ptr->tm_year+1900, tm_ptr->tm_mon+1, tm_ptr->tm_mday);
    printf("time: %02d Hr %02d Min %02d Sec \n", tm_ptr->tm_hour + GMT8, tm_ptr->tm_min , tm_ptr -> tm_sec);
}
