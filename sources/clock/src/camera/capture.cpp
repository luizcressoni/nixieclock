/*! \file capture.cpp */
#include "capture.h"
#include "../utils/defines.h"

/*! \brief Captures a frame from the device and converts it to grayscale
    \return true if the frame is captured and converted successfully, false otherwise
    \param cap The VideoCapture object to capture the frame from
    \param gray Receives the grayscale frame
    \param _brightness If not null, receives the raw mean level of the frame

    \note There is deliberately no contrast stretch and no CLAHE here any more. Both were
    \note costing real time on a single core ARM1176 -- CLAHE alone was six times the price of
    \note equalizeHist, and the Ptr was being reallocated on every single frame -- while buying
    \note nothing: the LBP cascade is invariant to illumination by construction, and measured
    \note side by side on the same faces the plain grayscale detected everything the enhanced
    \note image did. Removing them also puts the brightness reading back on honest ground. It
    \note used to be taken after both, which is to say after two steps whose entire purpose is
    \note to erase the absolute light level the reading was trying to report.
*/
bool capture_frame(VideoCapture &cap, Mat &gray, uint8_t *_brightness)
{
    Mat frame;

    cap >> frame;
    if (frame.empty()) 
    {
        printf("Empty frame\n");
        return false;
    }

    cvtColor(frame, gray, COLOR_BGR2GRAY);

    if (_brightness != nullptr)
    {
        const double raw = cv::mean(gray)[0];
        *_brightness = static_cast<uint8_t>(raw < 0.0 ? 0.0 : (raw > 255.0 ? 255.0 : raw));
    }

    return true;
}

/*! \brief Maps a raw mean level onto the 0..100 scale the clock dims its tubes by
    \param _raw Raw mean of the grayscale band, 0 to 255
    \note The old 21..75 band was calibrated by hand on top of the CLAHE output, so it does not
    \note carry over now that the reading is taken on the raw frame. These two ends are a
    \note starting point for an indoor room, not a measurement: the camera prints the raw value
    \note next to the scaled one every time it sends a brightness update, so the real ends can
    \note be read off the log on the clock itself and set here.
*/
uint8_t scale_brightness(double _raw)
{
    double scaled = (_raw - BRIGHTNESS_RAW_DARK) * 100.0 /
                    (BRIGHTNESS_RAW_BRIGHT - BRIGHTNESS_RAW_DARK);

    if (scaled > 100.0)
        scaled = 100.0;
    else if (scaled < 0.0)
        scaled = 0.0;

    return static_cast<uint8_t>(scaled);
}

//eof capture.cpp
