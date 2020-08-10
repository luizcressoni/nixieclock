/*! \file face_detection.h */
#pragma once
#include <tinyxml2.h>
#include <linux/types.h>

#define USE_OPENCV

bool start_face_detection(tinyxml2::XMLElement *_facedetectconfig);
void end_face_detection();
__u32 face_detected();

//eof face_detection.h
