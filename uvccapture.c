#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
//#include <jpeglib.h>
#include <time.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>

#include "v4l2uvc.h"

static const char version[] = "test";
int run = 1;
static struct v4l2_queryctrl queryctrl;
static struct v4l2_querymenu querymenu;

void
sigcatch (int sig)
{
	fprintf (stderr, "Exiting...\n");
	run = 0;
}

void
usage (void)
{
	fprintf (stderr, "uvccapture version %s\n", version);
	fprintf (stderr, "Usage is: uvccapture [options]\n");
	fprintf (stderr, "Options:\n");
	fprintf (stderr, "-v\t\tVerbose\n");
	fprintf (stderr, "-o<filename>\tOutput filename(default: snap.jpg)\n");
	fprintf (stderr, "-d<device>\tV4L2 Device(default: /dev/video0)\n");
	fprintf (stderr,
			 "-x<width>\tImage Width(must be supported by device)(>960 activates YUYV capture)\n");
	fprintf (stderr,
			 "-y<height>\tImage Height(must be supported by device)(>720 activates YUYV capture)\n");
	fprintf (stderr,
			 "-c<command>\tCommand to run after each image capture(executed as <command> <output_filename>)\n");
	fprintf (stderr,
			 "-t<integer>\tTake continuous shots with <integer> seconds between them (0 for single shot)\n");
	fprintf (stderr,
			 "-q<percentage>\tJPEG Quality Compression Level (activates YUYV capture)\n");
	fprintf (stderr, "-r\t\tUse read instead of mmap for image capture\n");
	fprintf (stderr,
			 "-w\t\tWait for capture command to finish before starting next capture\n");
	fprintf (stderr, "-m\t\tToggles capture mode to YUYV capture\n");
	fprintf (stderr, "Camera Settings:\n");
	fprintf (stderr, "-B<integer>\tBrightness\n");
	fprintf (stderr, "-C<integer>\tContrast\n");
	fprintf (stderr, "-S<integer>\tSaturation\n");
	fprintf (stderr, "-G<integer>\tGain\n");
	exit (8);
}

int
spawn (char *argv[], int wait, int verbose)
{
	pid_t pid;
	int rv;

	switch (pid = fork ()) {
	case -1:
		return -1;
	case 0:
		// CHILD
		execvp (argv[0], argv);
		fprintf (stderr, "Error executing command '%s'\n", argv[0]);
		exit (1);
	default:
		// PARENT
		if (wait == 1) {
			if (verbose >= 1)
				fprintf (stderr, "Waiting for command to finish...");
			waitpid (pid, &rv, 0);
			if (verbose >= 1)
				fprintf (stderr, "\n");
		} else {
			// Clean zombies
			waitpid (-1, &rv, WNOHANG);
			rv = 0;
		}
		break;
	}

	return rv;
}

int
compress_yuyv_to_jpeg (struct vdIn *vd, FILE * file, int quality)
{
	return (0);
}

int
uvc_test_main (int argc, char *argv[])
{
	char *videodevice = "/dev/video0";
	char *outputfile = "snap";
	char filename[20];
	char *post_capture_command[3];
	int format = V4L2_PIX_FMT_MJPEG;
	int grabmethod = 1;
	int width = 640;
	int height = 480;
	int brightness = 0, contrast = 0, saturation = 0, gain = 0;
	int verbose = 0;
	int delay = 0;
	int quality = 95;
	int post_capture_command_wait = 0;
	time_t ref_time;
	struct vdIn *videoIn;
	FILE *file;

	(void) signal (SIGINT, sigcatch);
	(void) signal (SIGQUIT, sigcatch);
	(void) signal (SIGKILL, sigcatch);
	(void) signal (SIGTERM, sigcatch);
	(void) signal (SIGABRT, sigcatch);
	(void) signal (SIGTRAP, sigcatch);

	// set post_capture_command to default values
	post_capture_command[0] = NULL;
	post_capture_command[1] = NULL;
	post_capture_command[2] = NULL;

	//Options Parsing (FIXME)
	while ((argc > 1) && (argv[1][0] == '-')) {
		switch (argv[1][1]) {
		case 'v':
			verbose++;
			break;

		case 'o':
			outputfile = &argv[1][2];
			break;

		case 'd':
			videodevice = &argv[1][2];
			break;

		case 'x':
			width = atoi (&argv[1][2]);
			break;

		case 'y':
			height = atoi (&argv[1][2]);
			break;

		case 'r':
			grabmethod = 0;
			break;

		case 'm':
			format = V4L2_PIX_FMT_YUYV;
			break;

		case 't':
			delay = atoi (&argv[1][2]);
			break;

		case 'c':
			post_capture_command[0] = &argv[1][2];
			break;

		case 'w':
			post_capture_command_wait = 1;
			break;

		case 'B':
			brightness = atoi (&argv[1][2]);
			break;

		case 'C':
			contrast = atoi (&argv[1][2]);
			break;

		case 'S':
			saturation = atoi (&argv[1][2]);
			break;

		case 'G':
			gain = atoi (&argv[1][2]);
			break;

		case 'q':
			quality = atoi (&argv[1][2]);
			break;

		case 'h':
			usage ();
			break;

		default:
			fprintf (stderr, "Unknown option %s \n", argv[1]);
			usage ();
		}
		++argv;
		--argc;
	}

	if ((width > 960) || (height > 720) || (quality != 95))
		format = V4L2_PIX_FMT_YUYV;

	if (post_capture_command[0])
		post_capture_command[1] = filename;

	if (verbose >= 1) {
		fprintf (stderr, "Using videodevice: %s\n", videodevice);
		fprintf (stderr, "Saving images to: %s\n", outputfile);
		fprintf (stderr, "Image size: %dx%d\n", width, height);
		fprintf (stderr, "Taking snapshot every %d seconds\n", delay);
		if (grabmethod == 1)
			fprintf (stderr, "Taking images using mmap\n");
		else
			fprintf (stderr, "Taking images using read\n");
		if (post_capture_command[0])
			fprintf (stderr, "Executing '%s' after each image capture\n",
					 post_capture_command[0]);
	}
	videoIn = (struct vdIn *) calloc (1, sizeof (struct vdIn));
	if (init_videoIn
		(videoIn, (char *) videodevice, width, height, format, grabmethod) < 0)
		exit (1);
#if 1
	//Reset all camera controls
	if (verbose >= 1)
		fprintf (stderr, "Resetting camera settings\n");
	v4l2ResetControl (videoIn, V4L2_CID_BRIGHTNESS);
	v4l2ResetControl (videoIn, V4L2_CID_CONTRAST);
	v4l2ResetControl (videoIn, V4L2_CID_SATURATION);
	v4l2ResetControl (videoIn, V4L2_CID_GAIN);

	//Setup Camera Parameters
	if (brightness != 0) {
		if (verbose >= 1)
			fprintf (stderr, "Setting camera brightness to %d\n", brightness);
		v4l2SetControl (videoIn, V4L2_CID_BRIGHTNESS, brightness);
	} else if (verbose >= 1) {
		fprintf (stderr, "Camera brightness level is %d\n",
				 v4l2GetControl (videoIn, V4L2_CID_BRIGHTNESS));
	}
	if (contrast != 0) {
		if (verbose >= 1)
			fprintf (stderr, "Setting camera contrast to %d\n", contrast);
		v4l2SetControl (videoIn, V4L2_CID_CONTRAST, contrast);
	} else if (verbose >= 1) {
		fprintf (stderr, "Camera contrast level is %d\n",
				 v4l2GetControl (videoIn, V4L2_CID_CONTRAST));
	}
	if (saturation != 0) {
		if (verbose >= 1)
			fprintf (stderr, "Setting camera saturation to %d\n", saturation);
		v4l2SetControl (videoIn, V4L2_CID_SATURATION, saturation);
	} else if (verbose >= 1) {
		fprintf (stderr, "Camera saturation level is %d\n",
				 v4l2GetControl (videoIn, V4L2_CID_SATURATION));
	}
	if (gain != 0) {
		if (verbose >= 1)
			fprintf (stderr, "Setting camera gain to %d\n", gain);
		v4l2SetControl (videoIn, V4L2_CID_GAIN, gain);
	} else if (verbose >= 1) {
		fprintf (stderr, "Camera gain level is %d\n",
				 v4l2GetControl (videoIn, V4L2_CID_GAIN));
	}
#endif

	ref_time = time (NULL);

	while (run) {
		if (verbose >= 2)
			fprintf (stderr, "Grabbing frame\n");
		if (uvcGrab (videoIn) < 0) {
			fprintf (stderr, "Error grabbing\n");
			close_v4l2 (videoIn);
			free (videoIn);
			exit (1);
		}

		if (1) {
			sprintf(filename,"%s_%d.jpg",outputfile,(int)ref_time);
			if (verbose >= 1)
				fprintf (stderr, "Saving image to: %s\n", filename);
			file = fopen (filename, "wb");
			/*save to file*/
			if (file != NULL) {
				switch (videoIn->formatIn) {
				case V4L2_PIX_FMT_YUYV:
					compress_yuyv_to_jpeg (videoIn, file, quality);
					break;
				default:
					fwrite (videoIn->tmpbuffer, videoIn->buf.bytesused + DHT_SIZE, 1,file);
					break;
				}
				fclose (file);
				videoIn->getPict = 0;
			}
			/*post command*/
			if (post_capture_command[0]) {
				if (verbose >= 1)
					fprintf (stderr, "Executing '%s %s'\n", post_capture_command[0],post_capture_command[1]);
				if (spawn (post_capture_command, post_capture_command_wait, verbose)) {
					fprintf (stderr, "Command exited with error\n");
					close_v4l2 (videoIn);
					free (videoIn);
					exit (1);
				}
			}
			ref_time = time (NULL);
		}
	}
	close_v4l2 (videoIn);
	free (videoIn);
	return 0;
}

static void enumerate_menu (int fd)
{
	printf ("  Menu items:\n");

	memset (&querymenu, 0, sizeof (querymenu));
	querymenu.id = queryctrl.id;

	for (querymenu.index = queryctrl.minimum; querymenu.index <= queryctrl.maximum; querymenu.index++) {
		if (ioctl (fd, VIDIOC_QUERYMENU, &querymenu)==0) {
			printf ("  %s\n", querymenu.name);
		}
	}
}

void query_all_ctrl(int fd)
{
	
	memset (&queryctrl, 0, sizeof (queryctrl));
	printf("###################begin query all ctrl############################\n");
	for (queryctrl.id = V4L2_CID_BASE; queryctrl.id < V4L2_CID_LASTP1; queryctrl.id++) {
		if (ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)==0) {
			if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;

			printf ("Control %s\n", queryctrl.name);

			if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
				enumerate_menu (fd);
		} else {
			if (errno == EINVAL)
				continue;

			perror ("VIDIOC_QUERYCTRL");
			exit (EXIT_FAILURE);
		}
	}

	for (queryctrl.id = V4L2_CID_PRIVATE_BASE;;
	     queryctrl.id++) {
		if ( ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)==0) {
			if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;

			printf ("Control %s\n", queryctrl.name);

			if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
				enumerate_menu (fd);
		} else {
			if (errno == EINVAL)
				break;

			perror ("VIDIOC_QUERYCTRL");
			exit (EXIT_FAILURE);
		}
	}
printf("###################end query all ctrl############################\n");

}
struct vdIn * init_camera(void) {
	char *videodevice = "/dev/video0";
	int format = V4L2_PIX_FMT_MJPEG;
	int grabmethod = 1;
	int width = 640;
	int height = 480;
	int brightness = 0, contrast = 0, saturation = 0, gain = 0;
	struct vdIn *videoIn;
	
	videoIn = (struct vdIn *) calloc (1, sizeof (struct vdIn));
	
	pthread_mutex_init(&videoIn->tmpbufflock,NULL);
	pthread_mutex_init(&videoIn->vd_data_lock,NULL);
	memset(videoIn->hrb_tid,0,sizeof(videoIn->hrb_tid));
	//threadcfg.qvga_flag = 0;
	if(strncmp(threadcfg.record_resolution,"vga",3)==0){
		printf("vga \n");
		width=640;
		height=480;
	}else if(strncmp(threadcfg.record_resolution,"qvga",4)==0){
		//threadcfg.qvga_flag = 1;
		width=320;
		height=240;
		printf("qvga \n");
	}else{
		width = 1280;
		height = 720;
		printf("720p\n");
	}
	brightness = threadcfg.brightness;
	contrast    = threadcfg.contrast;
	saturation = threadcfg.saturation;
	gain		 = threadcfg.gain;
	
	if (init_videoIn(videoIn, (char *) videodevice, width, height, format, grabmethod) < 0) {
		printf("init camera device error\n");
		return (struct vdIn *) 0;
	}
	//query_all_ctrl(videoIn->fd);
	v4l2ResetControl (videoIn, V4L2_CID_BRIGHTNESS);
	v4l2ResetControl (videoIn, V4L2_CID_CONTRAST);
	v4l2ResetControl (videoIn, V4L2_CID_SATURATION);
	v4l2ResetControl (videoIn, V4L2_CID_GAIN);

	//Setup Camera Parameters
	/*
#define V4L2_CID_ROTATE  (V4L2_CID_BASE + 34)
	if(v4l2SetControl(videoIn, V4L2_CID_ROTATE, 180)<0)
		printf("########################v4l2 not support rotate#####################\n");
		*/
	if (brightness != 0) {
		//v4l2SetControl (videoIn, V4L2_CID_BRIGHTNESS, brightness);
		v4l2_contrl_brightness(videoIn, brightness);
	}
	if (contrast != 0) {
		//v4l2SetControl (videoIn, V4L2_CID_CONTRAST, contrast);
		v4l2_contrl_contrast(videoIn, contrast);
	}
	if (saturation != 0) {
		v4l2SetControl (videoIn, V4L2_CID_SATURATION, saturation);
	}
	if (gain != 0) {
		v4l2SetControl (videoIn, V4L2_CID_GAIN, gain);
	}
	return videoIn;
}

int close_camera(struct vdIn * videoIn)
{
	close_v4l2 (videoIn);
	free(videoIn);
	return 0;
}


