/*! \file face_detection.cpp */
#include "face_detection.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <linux/types.h>

//#define OPENCV_DRAW
//#define OPENCV_DETECT_EYES

#ifdef USE_OPENCV
//opencv
#include "/usr/local/include/opencv4/opencv2/opencv.hpp"
//#include "/usr/local/include/opencv4/opencv2/objdetect.hpp"
//#include "/usr/local/include/opencv4/opencv2/highgui.hpp"
//#include "/usr/local/include/opencv4/opencv2/imgproc.hpp"
using namespace cv;

VideoCapture gVcapture;
Rect roi;
CascadeClassifier cascade;
Mat frame, frame1;
int minsize=60, maxsize=160;
int u32face_timeoutms = 60*1000;
bool gKeepCamRunning = false;
pthread_t mThreadCam = 0;
bool savepic=false;

pthread_mutex_t mutexcap = PTHREAD_MUTEX_INITIALIZER;

bool detectAndDraw( cv::Mat& img)//, double scale)
{
    bool retorno = false;
    std::vector<Rect> faces;
    cv::Mat gray;   //, smallImg;

    cvtColor( img, gray, COLOR_BGR2GRAY ); // Convert to Gray Scale
//    double fx = 1 / scale;

    // Resize the Grayscale Image
//    resize( gray, smallImg, Size(), fx, fx, INTER_LINEAR );
    equalizeHist( gray, gray );

    cascade.detectMultiScale( gray, faces, 1.1, 2, 0|CASCADE_SCALE_IMAGE, Size(minsize, minsize), Size(maxsize, maxsize));

    if(faces.size() > 0)
    {
        printf("Found %d faces\n", faces.size());
        if(savepic)
        {
            // Draw circles around the faces
            for ( size_t i = 0; i < faces.size(); i++ )
            {
                Rect r = faces[i];
//                Mat smallImgROI;
//                std::vector<Rect> nestedObjects;
//                Point center;
                Scalar color = Scalar(255, 0, 0); // Color for Drawing tool
//                int radius;

//                double aspect_ratio = (double)r.width/r.height;
//                if( 0.75 < aspect_ratio && aspect_ratio < 1.3 )
//                {
//                    center.x = cvRound((r.x + r.width*0.5)*scale);
//                    center.y = cvRound((r.y + r.height*0.5)*scale);
//                    radius = cvRound((r.width + r.height)*0.25*scale);
//                    circle( img, center, radius, color, 3, 1, 0 );
//                }
//                else
                    rectangle( img, Point(cvRound(r.x), cvRound(r.y)),
                               Point(cvRound(r.x + r.width-1),cvRound(r.y + r.height-1)),
                              color, 1, 8, 0);
            }
            imwrite("/tmp/last_face.jpg", img);
        }
        retorno = true;
    }

    return retorno;
}

//
void *nixiecamthread(void *data)
{
    do
    {
        pthread_mutex_lock(&mutexcap);
        gVcapture.grab();
        pthread_mutex_unlock(&mutexcap);
        usleep(30000);
    }while(gKeepCamRunning);

    return NULL;
}


bool start_face_detection(tinyxml2::XMLElement *_facedetectconfig)
{
    if(_facedetectconfig == NULL)
        return false;

    _facedetectconfig->QueryBoolAttribute("save_jpg", &savepic);

    tinyxml2::XMLElement *aux = _facedetectconfig->FirstChildElement("crop");

    roi.x = 110;
    roi.y = 0;
    roi.width = 320;
    roi.height = 240;

    if(aux)
    {
        aux->QueryIntAttribute("x", &roi.x);
        aux->QueryIntAttribute("y", &roi.y);
        aux->QueryIntAttribute("width", &roi.width);
        aux->QueryIntAttribute("height", &roi.height);
    }

    aux = _facedetectconfig->FirstChildElement("face_size");
    if(aux != NULL)
    {
        aux->QueryIntAttribute("min", &minsize);
        aux->QueryIntAttribute("max", &maxsize);
    }

    aux = _facedetectconfig->FirstChildElement("brightness");
    if(aux != NULL)
    {
        if(aux->QueryIntAttribute("timeoutseconds", &u32face_timeoutms) == tinyxml2::XML_SUCCESS)
        {
            u32face_timeoutms *= 1000;
        }
    }


    aux = _facedetectconfig->FirstChildElement("cascade");
    bool casloaded;
    if(aux != NULL)
        casloaded = cascade.load(aux->Attribute("file"));
    else
        casloaded = cascade.load( "/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_default.xml");

    if(!casloaded)
    {
        printf("Cascade classifier file not loaded!\n");
    }
//#ifdef OPENCV_DETECT_EYES
//    aux = _facedetectconfig->FirstChildElement("nestedcascade");
//    if(aux != NULL)
//        nestedCascade.load(aux->Attribute("file"));
//    else
//        nestedCascade.load( "/usr/local/share/opencv4/haarcascades/haarcascade_eye_tree_eyeglasses.xml");
//#endif

    gVcapture.set(CAP_PROP_FPS, 1);
    gVcapture.set(CAP_PROP_BUFFERSIZE, 1);

    gVcapture.open(0);

    if(gVcapture.isOpened())
    {
        gKeepCamRunning = true;
        if(pthread_create(&mThreadCam, NULL, nixiecamthread, NULL) == 0)
        {
            return true;
        }
        mThreadCam = 0;
    }
    return false;
}


__u32 face_detected()
{
    //double scale=1.0;

    if(gVcapture.isOpened())
    {
        pthread_mutex_lock(&mutexcap);
        gVcapture >> frame;
        pthread_mutex_unlock(&mutexcap);

        if( frame.empty() )
            return false;

        if(savepic) imwrite("/tmp/last_frame.jpg", frame);

        frame1 = frame(roi);
//        time_t ta;
//        time(&ta);
//        tm *now = localtime(&ta);
        //printf("Detecting at %02d:%02d:%02d\n", now->tm_hour, now->tm_min, now->tm_sec);
        //printf("Detecting...\n");
        if(detectAndDraw( frame1))
        {
            time_t ta;
            time(&ta);
            tm *now = localtime(&ta);
            printf("Found face at %02d:%02d:%02d\n", now->tm_hour, now->tm_min, now->tm_sec);
            return u32face_timeoutms;
        }
    }
    return 0;
}

void end_face_detection()
{
    gKeepCamRunning = false;
    if(mThreadCam != 0)
        pthread_join(mThreadCam, NULL);
}
#else
bool start_face_detection()
{
    return false;
}

bool face_detected()
{
    return false;
}
#endif

//eof face_detection.cpp
