#include <stdio.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <unistd.h>
#include "libuvc/libuvc.h"
#include "fps_calc.h"
#include <iostream>
#include <fstream>

// number of frames to capture before exiting (default 300 frames at 18 fps => 16 seconds of capture time)
#define MAX_FRAMES 200

// vendor and product id of the first camera
#define CAMERA_0_VID 0x05a9
#define CAMERA_0_PID 0xa601

//Image format settings
#define IMAGE_FRAME_WIDTH 1280
#define IMAGE_FRAME_HEIGHT 800
#define IMAGE_FRAME_RATE 10
#define USE_MJPEG

// uncomment this if you want to show a display
#define DISPLAY

// number of cameras; don't change this
#define NUM_CAMS 1

static int frame_count = 0 ;
using namespace cv;

void toOpenCV(uvc_frame_t* rgb, IplImage *img) { 
  img->height = rgb->height;
  img->width = rgb->width;
  img->widthStep = rgb->step;
  img->nChannels = 3;
  img->imageData = (char*)rgb->data;
}

struct CameraController {
    uvc_device_t *dev;
    uvc_device_handle_t *devh;
    uvc_stream_ctrl_t ctrl;
    uvc_stream_handle_t *strmh;
};

int main(int argc, char **argv) {
  uvc_context_t *ctx;
  uvc_error_t res;
  CameraController camera[NUM_CAMS];

  res = uvc_init(&ctx, NULL);
  if (res < 0) {
    uvc_perror(res, "uvc_init");
    return res;
  }
  puts("UVC initialized");

  // connect to camera 1. The vendor and product id is needed
  res = uvc_find_device(
          ctx, &camera[0].dev,
          CAMERA_0_VID, CAMERA_0_PID, NULL);

  if (res < 0) {
      uvc_perror(res, "uvc_find_device");
      puts("Device found");
  }
 
  //configure and start each stream
  for (int cam_idx = 0; cam_idx < NUM_CAMS; cam_idx++) { 
      res = uvc_open(camera[cam_idx].dev, &camera[cam_idx].devh);

      if (res < 0) {
          uvc_perror(res, "uvc_open");
          return -1;
      }

      printf("Device opened\n");
      uvc_print_diag(camera[cam_idx].devh, stderr);

      res = uvc_get_stream_ctrl_format_size(
              camera[cam_idx].devh, &camera[cam_idx].ctrl, 
              UVC_FRAME_FORMAT_MJPEG, IMAGE_FRAME_WIDTH,
              IMAGE_FRAME_HEIGHT, IMAGE_FRAME_RATE 
              );
      if (res < 0) {
          uvc_perror(res, "get_mode");
          return -1;
      }

      uvc_print_stream_ctrl(&camera[cam_idx].ctrl, stderr);

      res = uvc_stream_open_ctrl(camera[cam_idx].devh, &camera[cam_idx].strmh,
              &camera[cam_idx].ctrl);
      
      if (res < 0) {
          uvc_perror(res, "open_ctrl");
          return -1;
      }
  }

  // approximately sync the first frame by starting the streams at the same
  // time

  for (int cam_idx = 0; cam_idx < NUM_CAMS; cam_idx++) { 
      res = uvc_stream_start_iso(camera[cam_idx].strmh, NULL, NULL);
      if (res < 0) {
          uvc_perror(res, "start_iso");
          return -1;
      }
  }

  //Poll, Poll, Poll!
  for (frame_count = 0; frame_count < MAX_FRAMES; frame_count++) {
      for (int cam_idx = 0; cam_idx < NUM_CAMS; cam_idx++) { 
          uvc_frame_t *frame;
          //grab a frame
          res = uvc_stream_get_frame(camera[cam_idx].strmh, &frame, 0);
          if (res < 0)  {
              uvc_perror(res, "get_frame");
          }

          /* define DISPLAY if you want to see the picture in real time;
           * not recommended in general since it could cause you to drop frames */
#ifdef DISPLAY
          if (frame != NULL && frame_count % 1 == 0) {
              std::vector<char> frame_data((char *)frame->data, (char *)frame->data + frame->data_bytes);
              Mat mImg = imdecode(Mat(frame_data), CV_LOAD_IMAGE_COLOR);
              string window_name = "img" + boost::lexical_cast<std::string>(cam_idx);
              Mat disp_img; 
              resize(mImg, disp_img, Size(320, 180));
              imshow(window_name.c_str(), disp_img);
              waitKey(5);
          }
#endif
      }

      FPS_CALC("rate");
  }

  for (int cam_idx = 0; cam_idx < NUM_CAMS; cam_idx++) { 
      uvc_stream_close(camera[cam_idx].strmh); 
      uvc_close(camera[cam_idx].devh);
      uvc_unref_device(camera[cam_idx].dev);
  }
  uvc_exit(ctx);
  printf("UVC exited\n");

  return 0;
}

