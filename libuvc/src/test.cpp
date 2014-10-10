#include <stdio.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <unistd.h>
#include "libuvc/libuvc.h"
#include "fps_calc.h"
#include <iostream>
#include <fstream>
#include <zmq.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#define ZMQ_PUB_PORT 5929
#define ZMQ_SUB_PORT 5930

#define MAX_FRAMES 20000

// vendor and product id of the first camera
#define CAMERA_0_VID 0x05a9
#define CAMERA_0_PID 0xa603

//Image format settings
#define IMAGE_FRAME_WIDTH 1280
#define IMAGE_FRAME_HEIGHT 800
#define IMAGE_FRAME_RATE 10
#define USE_MJPEG

// number of cameras; don't change this
#define NUM_CAMS 1

static int frame_count = 0 ;
using namespace cv;

typedef boost::posix_time::ptime Time;

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


using namespace std;
zmq::context_t context(1);
zmq::socket_t zmq_pub(context, ZMQ_PUB);
zmq::socket_t zmq_sub(context, ZMQ_SUB); 

inline void sendMessage(char *message, uint32_t length) {
    cout << "message length: " << length << endl;
    zmq::message_t m(message, length, NULL);
    zmq_pub.send(m, ZMQ_NOBLOCK);
}


int main(int argc, char **argv) {
  uvc_context_t *ctx;
  uvc_error_t res;
  CameraController camera[NUM_CAMS];

  string pub_addr = "tcp://localhost:" + boost::lexical_cast<string>(ZMQ_PUB_PORT);
  cout << pub_addr << endl; 
  zmq_pub.connect(pub_addr.c_str());

  string sub_addr = "tcp://*:" + boost::lexical_cast<string>(ZMQ_SUB_PORT);
  cout << sub_addr << endl; 
  zmq_sub.bind(sub_addr.c_str());
  zmq_sub.setsockopt(ZMQ_SUBSCRIBE, NULL, 0);

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

      res = uvc_get_stream_ctrl_format_size(
              camera[cam_idx].devh, &camera[cam_idx].ctrl, 
              UVC_FRAME_FORMAT_MJPEG, IMAGE_FRAME_WIDTH,
              IMAGE_FRAME_HEIGHT, IMAGE_FRAME_RATE 
              );
      if (res < 0) {
          uvc_perror(res, "get_mode");
          return -1;
      }

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
  while(true) {
    frame_count++;
    for (int cam_idx = 0; cam_idx < NUM_CAMS; cam_idx++) { 
      uvc_frame_t *frame;
      //grab a frame
      res = uvc_stream_get_frame(camera[cam_idx].strmh, &frame, 0);
      if (res < 0)  {
        uvc_perror(res, "get_frame");
      }
      Time currentTime(boost::posix_time::microsec_clock::local_time());
      cout << currentTime << endl;

      if (frame != NULL && frame_count % 10 == 0) {
        sendMessage((char *)frame->data, frame->data_bytes);
      }
    }

    try { 
      zmq::message_t command_msg;
      if (zmq_sub.recv(&command_msg, ZMQ_NOBLOCK) == true) {
        string command((const char*)command_msg.data(), command_msg.size());
        cout << command << endl;
      }

    } catch (const zmq::error_t &e) {}

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

