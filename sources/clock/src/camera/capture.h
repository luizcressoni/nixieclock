/*! \file capture.h */
#pragma once

#include <opencv2/opencv.hpp>

using namespace cv;

/**
 * @brief Captures one frame and converts it to grayscale.
 *
 * One frame per call, shared by both detectors. Face detection and motion detection used to
 * grab a frame each -- three per pass of the main loop -- which at the configured frame rate
 * cost two hundred milliseconds of wall clock before a single pixel had been looked at.
 * The whole frame is returned; a caller that wants a band takes a Mat view of it, which costs
 * nothing because a view shares the pixels rather than copying them.
 *
 * @param cap The VideoCapture object to capture the frame from.
 * @param gray Receives the grayscale image. Reused across calls, so pass the same Mat.
 * @param _brightness If not null, receives the raw mean level of the frame, 0 to 255.
 * @return true if the frame was captured and converted successfully, false otherwise.
 */
bool capture_frame(VideoCapture &cap, Mat &gray, uint8_t *_brightness);

/**
 * @brief Maps a raw mean level onto the 0..100 scale the clock dims its tubes by.
 * @param _raw Raw mean of the grayscale band, 0 to 255.
 */
uint8_t scale_brightness(double _raw);

//eof capture.h
