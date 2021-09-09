#include <glib.h>
#include <glib/gprintf.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <GL/glut.h>
#include <math.h>
#include "radio_image.h"

#define LASER_ADDR 0x52
#define SERVO_CONTROLLER 0x40
#define PAN_ADDR 0x08
#define TILT_ADDR 0x0C
#define RANGE_REGISTER 

/*	^^^  I probably don't need half of these... Added during testing haven't removed any. 	*/

radio_image myimage[2500]; 	/* defined in header file	*/

/* 	These three values used by multiple functions	*/
float scale_factor;
float rotation[3];
char *save_filename;

double servotoradians(int pulsewidth){
/* 	This will change depending on your equipment!
	Convert servo pulse width to radians... one radian is equal to 509.3, center(0,0) is assumed at center of servo range (1220) */
	
	return (double)(((pulsewidth - 1220.0) / 509.3));
}

int8_t read_reg8(int file_handle, int i2c_addr, char address){
/*	Takes an i2c bus(file), slave address, and register address and returns 8 byte read value from device */
	
  const gchar *buffer;
	if (ioctl(file_handle, I2C_SLAVE, i2c_addr) < 0) {
		printf("%s \n", "Byte read failed...failed to acquire bus access and/or talk to slave.");
		return 1;
	}
	char buf0[1] = {0};
	char reg_addr[10];
	reg_addr[0] = address;
	if (write(file_handle,reg_addr,1) != 1){
		/*error handling: i2c transaction failed*/
		printf("Failed to write to the i2c bus.\n");
		/*buffer = g_strerror(errno);
		printf(buffer);*/
		printf("\n\n");
	}
	if (read(file_handle,buf0,1) != 1){
		/*error handling: i2c transaction failed*/
		printf("Failed to write to the i2c bus.\n");
		/*buffer = g_strerror(errno);
		printf(buffer);*/
		printf("\n\n");
	}
	return buf0[0];
}

int8_t write_reg8(int file_handle, int i2c_addr, int reg_addr, int8_t value){
/*	Takes an i2c bus(file), slave address, register address, and 8 bit int
	then writes to a single register on device 				*/
	
  const gchar *buffer;
	if (ioctl(file_handle, I2C_SLAVE, i2c_addr) < 0) {
		printf("%s \n", "Byte write failed...failed to acquire bus access and/or talk to slave.");
		return 1;
	}
	char message[10] = {0};
	message[0] = reg_addr;
	message[1] = value;
	if (write(file_handle, message, 2) != 2) {
        	/* ERROR HANDLING: i2c transaction failed */
        	printf("Byte write failed...failed to write l_value.\n");
        	buffer = g_strerror(errno);
		printf(buffer);
        	printf("\n\n");
		return 1;
	}
	return 0;
}

int16_t write_reg16(int file_handle, int i2c_addr, int reg_addr, int16_t value){
/*	Takes an i2c bus(file), slave address, register address, and 16 bit integer
	and writes DWORD (2 sequential 8 bit registers) value to I2C device		 */
  const gchar *buffer;
	if (ioctl(file_handle, I2C_SLAVE, i2c_addr) < 0) {
		printf("%s \n", "DWORD write failed...failed to acquire bus access and/or talk to slave.");
		return 1;
	}
	char message[10] = {0};
	message[0] = reg_addr;
	message[1] = ((value & 0xff));
	if (write(file_handle, message, 2) != 2) {
        	/* ERROR HANDLING: i2c transaction failed */
        	printf("DWORD write failed...failed to write l_value.\n");
        	buffer = g_strerror(errno);
		printf(buffer);
        	printf("\n\n");
		return 1;
	}
	message[0] = ((reg_addr + 1));
	message[1] = ((((value >> 8)) & 0x00ff));
	if (write(file_handle, message, 2) != 2) {
        	 /*ERROR HANDLING: i2c transaction failed */
        	printf("DWORD write failed...failed to write h_value.\n");
        	buffer = g_strerror(errno);
		printf(buffer);
        	printf("\n\n");
		return 1;
	}
	return 0;
}

int16_t read_reg16(int file_handle, int i2c_addr, char address){
/*	Takes an i2c bus(file), slave address, and register address
	then reads from two sequential 8 bit registersby incrementing
	and returns the corresponding 16 bit value			*/
	
  const gchar *buffer;
	if (ioctl(file_handle, I2C_SLAVE, i2c_addr) < 0) {
		printf("%s \n", "DWORD read failed...failed to acquire bus access and/or talk to slave.");
		return 1;
	}
  char buf0[2] = {0};
  char buf1[1] = {0};
  char reg_addr[10];
  reg_addr[0] = address;
  int16_t retval;
  if (write(file_handle,reg_addr,1) != 1){
      /* ERROR HANDLING: i2c transaction failed */
      printf("Failed to write to the i2c bus.\n");
      buffer = g_strerror(errno);
      printf(buffer);
      printf("\n\n");
  }
  if (read(file_handle,buf0,2) != 2){
      /* ERROR HANDLING: i2c transaction failed */
      printf("Failed to read from the i2c bus.\n");
      buffer = g_strerror(errno);
      printf(buffer);
      printf("\n\n");
  }

	/* Bitwise operation combines l_value and h_value*/
  retval = ((int16_t)buf0[0] << 8 ) | buf0[1];
  return retval;
}

void displayMe(void)
{
/*	This function draws the myimage array and displays it on screen */
	
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f( 0.0f, 1.0f, 0.0f);
    glPushMatrix ();
/*	Rotate objects if specified	*/
	glRotatef(rotation[0], 1.0, 0.0, 0.0);
	glRotatef(rotation[1], 0.0, 1.0, 0.0);
	

/*	Each point is drawn as a cube.  The color of the cube changes depending on the distance.
	You could also code this to draw a surface from the points, I just don't know how to do 
	lighting in opengl (it was too hard to see what was going on without it).  Cube is 
	extrapolated to keep the array and save file a pure record of the actual measured points. 			*/
	
      	for ( int position = 0; position < 2500; position++){
		if (myimage[position].z){ 
			glBegin(GL_QUADS);
			float xpos;
			float ypos;
			float zpos;
			xpos = (0-1)*(myimage[position].x / scale_factor);
			ypos = (myimage[position].y / scale_factor);
			zpos = (myimage[position].z / 2000.0);
			glColor3f ( (1-zpos), (1-zpos), (zpos));
				//front
			glVertex3f( xpos, ypos, (zpos));
			glVertex3f( (xpos - 0.05), ypos, (zpos));
			glVertex3f( (xpos - 0.05), (ypos - 0.05), (zpos));
			glVertex3f( xpos, (ypos - 0.05), (zpos));
				//back
			glVertex3f( xpos, ypos, (zpos-0.05));
			glVertex3f( (xpos - 0.05), ypos, (zpos-0.05));
			glVertex3f( (xpos - 0.05), (ypos - 0.05), (zpos-0.05));
			glVertex3f( xpos, (ypos - 0.05), (zpos-0.05));
				//left
			glVertex3f( xpos, ypos, (zpos));
			glVertex3f( (xpos), ypos, (zpos-0.05));
			glVertex3f( (xpos), (ypos - 0.05), (zpos-0.05));
			glVertex3f( xpos, (ypos - 0.05), (zpos));
				//right
			glVertex3f( (xpos - 0.05), ypos, (zpos));
			glVertex3f( (xpos - 0.05), ypos, (zpos-0.05));
			glVertex3f( (xpos - 0.05), (ypos - 0.05), (zpos-0.05));
			glVertex3f( (xpos - 0.05), (ypos - 0.05), (zpos));
				//bottom
			glVertex3f( xpos, (ypos - 0.05), (zpos));
			glVertex3f( xpos, (ypos - 0.05), (zpos-0.05));
			glVertex3f( (xpos - 0.05), (ypos - 0.05), (zpos-0.05));
			glVertex3f( (xpos - 0.05), (ypos - 0.05), (zpos));
				//top
			glVertex3f( xpos, ypos, (zpos));
			glVertex3f( xpos, ypos, (zpos-0.05));
			glVertex3f( (xpos - 0.05), ypos, (zpos-0.05));
			glVertex3f( (xpos - 0.05), ypos, zpos);


			glEnd();
		}
	}
    glPopMatrix ();
    glutSwapBuffers();
}

int distance(int file){
/*	This function reads from the TOF10120 "laser" distance sensor and returns the value.  
	Sensor returns an integer between 0 and 2000(corresponding to millimeters). 
	Requires the user to pass the correct i2c bus. 						*/
	
	int measurement = 0;
	measurement = read_reg16(file, LASER_ADDR, 0x0);
	return measurement;
}

void conversion_3d(int iteration, int localdistance, double pan, double tilt){
	
	/* Takes a vector and a distance, returns an X, Y and Z coordinate for the point.
	   Assumes the origin is 0,0,0. Return order is X,Y,Z.   			*/
	if (pan){
		double w = cos(pan) * localdistance;
		myimage[iteration].x = sin(pan) * localdistance;
		myimage[iteration].y = sin(tilt) * w;
		/* Ugly bithack for absolute value (Distance)*/
		double bithack = ((cos(tilt) * w));
		int64_t holder = *(int64_t*)&bithack;
		holder = holder & 0b0111111111111111111111111111111111111111111111111111111111111111;
		myimage[iteration].z = *(double*)&holder;
		/* end hack*/
	}
	else {
		myimage[iteration].x = 0;
		myimage[iteration].y = sin(tilt) * localdistance;
		/* Ugly bithack for absolute value (Distance)*/
		double bithack = ((cos(tilt) * localdistance));
		int64_t holder = *(int64_t*)&bithack;
		holder = holder & 0b0111111111111111111111111111111111111111111111111111111111111111;
		myimage[iteration].z = *(double*)&holder;
		/* end hack*/
	}
	return;
}

void Keypress(unsigned char key, int x, int y){
/*	These key events allow manipulation of the displayed image 
	(this event loop starts after image is scanned) 		*/

	if (key == 'w'){
		if (rotation[0] < 360){
			rotation[0] += 1;
		}
		else rotation[0] = 1;
	}
	else if (key == 'a'){
		if (rotation[1] < 360){
			rotation[1] += 1;
		}
		else rotation[1] = 0;
	}
	else if (key == 's'){
		if (rotation[0] > 0){
			rotation[0] -= 1;
		}
		else rotation[0] = 360;
	}
	else if (key == 'd'){
		if (rotation[1] > 0){
			rotation[1] -= 1;
		}
		else rotation[1] = 360;
	}
	else if (key == 'r'){
		while(1){
			if (rotation[0] < 360){
				rotation[0] += 0.3;
			}
			else rotation[0] = 0.3;
	
			if (rotation[1] < 360){
				rotation[1] += 1;
			}
			else rotation[1] = 1;
			displayMe();
			usleep(4000);
		}
	}
	glutPostRedisplay();
	return;
}

int main(int argc, char** argv)
{
	
/*
	This program maps a fixed resolution of 50x50 "pixels" at
	a fixed servo rotation between readings.  If you wish more
	flexibility add a command line argument for each value and 
	plug them in to this function.
*/
printf("%s \n", "Depth Mapper V0.2");

if (argv[0] != "-h"){

/*	Zero array used for storing coordinates. */
	for ( int count = 0; count < 2500; count++){
	myimage[count].x = 0;
	myimage[count].y = 0;
	myimage[count].z = 0;
	}
	
/*	Initialize variables */
	rotation[0] = 0;
	rotation[1] = 0;
	rotation[2] = 0;
	int readflag = 0;
	scale_factor = 1000.0;
	save_filename = "/home/otacon/opengl/last.3d";
	int servo_speed = 20000;
	
	
/*	Read from command line
	check for:
		scale factor
		motor speed
		save file
		is this a read or write operation
							*/
	for (int i=0; i<argc;i++){
		if (argv[i][1] == 's'){
			scale_factor = (float)atoi(argv[((i+1))]);
		}
		if (argv[i][1] == 'q'){
			servo_speed = atoi(argv[((i+1))]);
		}
		if (argv[i][1] == 'r'){
			if (readflag) {
				printf("%s", "Cannot read and write at the same time! (select either -r or -w)");
				return 1;
			}
			readflag = 1;
			if (argv[((i+1))]){
				save_filename = argv[((i+1))];
			}
			else {
				printf ("%s", "Please specify filename");
				return 1;
			}
		}
		if (argv[i][1] == 'w'){
			if (readflag) {
				printf("%s", "Cannot read and write at the same time! (select either -r or -w)");
				return 1;
			}
			readflag = 2;
			if (argv[((i+1))]){
				save_filename = argv[((i+1))];
			}
			else {
				printf ("%s", "Please specify filename");
				return 1;
			}
		}

		
	}

	/* initialize opengl */
    	glutInit(&argc, argv);
    	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE);
    	glutInitWindowSize(1024, 1024);
    	glutInitWindowPosition(100, 100);
    	glutCreateWindow("Depth Mapper v0.2");
    	glutDisplayFunc(displayMe);
    	glEnable(GL_DEPTH_TEST);
    	glDepthFunc(GL_LESS);
    	glDepthRange(0.5f,1.0f);
    	glClearDepth(1.0f);

	if (readflag == 0) {
/*	Quit if the user did not tell us to do anything!	*/
		printf("%s \n", "Too few arguments...");
		printf("%s \n %s \n %s \n %s \n %s \n %s \n", "Usage as Follows:", "depthmap -options arguments", "Options:", "-w filename		write to file", "-r filename 		Read from file", "-s scale_factor	Division factor for downscaling image(smaller = bigger / Default 1000)"); 
		return 1;
	}
	
	else if (readflag == 2){
	/* Write Condition */
		/* open I2C bus */
		int file;
		char *filename = "/dev/i2c-0";

		printf("%s \n", "Connecting to I2C device");
		if ((file = open(filename, O_RDWR)) < 0) {
			/*error handling*/
			perror("Failed to open the i2c bus (0)");
		exit(1);
		}
		if (ioctl(file, I2C_SLAVE, LASER_ADDR) < 0) {
			/*error handling*/
			printf("%s \n", "Failed to acquire bus access and/or talk to slave.");
		exit(1);
		}
		printf("%s \n", "done");

		/*wake up servo controller*/
		printf("%s", "Waking up servo controller...");

		write_reg8(file, SERVO_CONTROLLER, 0x0, 0x0);
		sleep(1);

		printf("%s \n", "done");

		int flag = 0; /* used for allowing the sensor to sweep both left->right and right->left (saves some time) */

		/* run servo mapping loop */
		for (int tilt = 49; tilt > (0-1); tilt--){
		printf("%d%s \n",((tilt*2)), "%");
			write_reg16(file, SERVO_CONTROLLER, TILT_ADDR, (820 - (tilt*8)));
			usleep(servo_speed);
			write_reg16(file, SERVO_CONTROLLER, TILT_ADDR, (820 - (tilt*8)));
			usleep(servo_speed);
			if (flag){
				for (int pan = 49; pan > (0-1); pan--){
					write_reg16(file, SERVO_CONTROLLER, PAN_ADDR, (820 + (pan*16)));
					usleep(servo_speed);
					write_reg16(file, SERVO_CONTROLLER, PAN_ADDR, (820 + (pan*16)));
					usleep(servo_speed);
					int error_catch[3] = {0};
					error_catch[0] = distance(file);
					usleep(30);
					error_catch[1] = distance(file);
					usleep(30);
					error_catch[2] = distance(file);
					if (error_catch[0] < 2000 && error_catch[1] < 2000 && error_catch[2] < 2000){
						int localdistance = (error_catch[0] + error_catch[1] + error_catch[2]) / 3;
						double x_radians = servotoradians( ((820 + (pan*16) )) );
						double y_radians = servotoradians( ((1620 - (tilt*8) )) ) ;
						int iteration = (tilt * 50) + pan;
						conversion_3d(iteration, localdistance, x_radians, y_radians);
						printf("%f, %f, %f, %d \n",x_radians,myimage[iteration].x,myimage[iteration].z,localdistance);
					}
					else printf("%s \n", "Distance out of limits.");
				displayMe();
				}
			flag = 0;
			}
			else{
				for (int pan = 0; pan < 50; pan++){
					write_reg16(file, SERVO_CONTROLLER, PAN_ADDR, (820 + (pan*16)));
					usleep(servo_speed);
					write_reg16(file, SERVO_CONTROLLER, PAN_ADDR, (820 + (pan*16)));
					usleep(servo_speed);
					int error_catch[3] = {0};
					error_catch[0] = distance(file);
					usleep(30);
					error_catch[1] = distance(file);
					usleep(30);
					error_catch[2] = distance(file);
					if (error_catch[0] < 2000 && error_catch[1] < 2000 && error_catch[2] < 2000){
						int localdistance = (error_catch[0]  + error_catch[1] + error_catch[2]) / 3;
						double x_radians = servotoradians( ((820 + (pan*16) )) );
						double y_radians = servotoradians( ((1620 - (tilt*8) )) ) ;
						int iteration = (tilt * 50) + pan;
						conversion_3d(iteration, localdistance, x_radians, y_radians);
						printf("%f, %f, %f, %d \n",x_radians,myimage[iteration].x,myimage[iteration].z,localdistance);
					}
					else printf("%s \n", "Distance out of limits.");
					displayMe();
				}
			flag = 1;
			}
		}
		int write_file;
		printf("%s", "Attempting to save file...");
		if ((write_file = open(save_filename, O_RDWR|O_CREAT, 0777)) < 0) {
			/*error handling*/
			perror("Failed to create file!");
			exit(1);
		}
		if (write(write_file,myimage,sizeof(myimage)) != sizeof(myimage)){
			perror("Failed to read file");
			exit(1);
		}
		printf("%s \n", "success!");
		close(write_file);
		close(file);
		
	}
	else if (readflag == 1){
	
	/* Read array from file specified*/
		int read_file;
		printf("%s", "Attempting to read file...");
		if ((read_file = open(save_filename, O_RDWR)) < 0) {
			/*error handling*/
			perror("Failed to open file");
			exit(1);
		}
		if (read(read_file,myimage,sizeof(myimage)) != sizeof(myimage)){
			perror("Failed to read file");
			exit(1);
		}
		printf("%s \n", "success!");
		close(read_file);
	}
	displayMe();
	glutKeyboardFunc(Keypress);
	glutMainLoop();
}

else printf("%s \n %s \n %s \n %s \n %s \n %s \n %s \n", "Usage as Follows:", "depthmap -options arguments", "Options:", "-w filename		write to file", "-r filename 		Read from file", "-s scale_factor		Division factor for downscaling image(smaller -> bigger / Default 1000)", "-q Servo Speed	Default 10000 (lower is faster)"); 

return 0;

}

