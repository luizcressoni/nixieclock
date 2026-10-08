/*! \file capture.cpp */
#include "capture.h"
#include "../utils/defines.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <cstring>
#include <cerrno>

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

/*! \brief Sets one V4L2 control on the camera
    \note Through a second descriptor on the node rather than through VideoCapture: OpenCV maps
    \note only a handful of controls, and not the ones that matter here. V4L2 allows any number of
    \note descriptors to set controls while another one streams.
*/
bool set_camera_control(uint32_t _id, int32_t _value, const char *_name)
{
    const int fd = open(CAMERA_DEVICE_PATH, O_RDWR);
    if(fd < 0)
    {
        printf("Camera control %s: cannot open %s (%s)\n", _name, CAMERA_DEVICE_PATH, strerror(errno));
        return false;
    }
    v4l2_control ctrl{};
    ctrl.id = _id;
    ctrl.value = _value;
    const bool ok = ioctl(fd, VIDIOC_S_CTRL, &ctrl) == 0;
    if(ok)
        printf("Camera control %s set to %d\n", _name, _value);
    else
        printf("Camera control %s not set (%s)\n", _name, strerror(errno));
    close(fd);
    return ok;
}

/*! \brief Takes one frame's raw mean and updates the verdict
    \return true when the verdict changed on this call
    \note The very first reading decides on its own, from the middle of the two thresholds: there
    \note is no previous verdict for the hysteresis to hold on to. It is never a switch on.
    \note The hold is shorter on the way up (LIGHT_HOLD_UP_MS): the long one is there for the
    \note tubes ramping down as the clock goes to sleep, and when somebody switches the light on
    \note in a dark room the clock is already asleep.
*/
bool cLightMeter::Update(uint8_t _raw)
{
    m_level = m_filter.Update(_raw);
    m_switchedOn = false;
    if(m_level < LIGHT_DARK_BELOW)
    {
        m_darkSeen.SetTimeOut(LIGHT_SWITCH_ON_MS);
        m_darkLevel = m_level;
    }

    if(!m_primed)
    {
        m_primed = true;
        m_bright = m_level >= (LIGHT_DARK_BELOW + LIGHT_BRIGHT_ABOVE) / 2;
        return true;
    }

    const bool want = m_bright ? (m_level >= LIGHT_DARK_BELOW) : (m_level >= LIGHT_BRIGHT_ABOVE);
    if(want == m_bright)
    {
        m_crossing = false;
        return false;
    }
    if(!m_crossing)
    {
        m_crossing = true;
        //want is true here only on the way up, the crossing being over LIGHT_BRIGHT_ABOVE
        m_fastRise = want && !m_darkSeen.IsTimeOut();
        m_hold.SetTimeOut(want ? LIGHT_HOLD_UP_MS : LIGHT_HOLD_MS);
        return false;
    }
    if(!m_hold.IsTimeOut())
        return false;

    m_crossing = false;
    m_bright = want;
    m_switchedOn = want && m_fastRise && m_level >= m_darkLevel + LIGHT_SWITCH_ON_RISE;
    return true;
}

//eof capture.cpp
