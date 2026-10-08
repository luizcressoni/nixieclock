/*! \file capture.h */
#pragma once

#include <opencv2/opencv.hpp>
#include <cstdint>
#include "../utils/median.h"
#include "../utils/ctimer.h"

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

/**
 * @brief Sets one V4L2 control on the camera, through the device node.
 * @param _id The control, a V4L2_CID_* value.
 * @param _value What to set it to.
 * @param _name For the log.
 * @return true if the driver took it. Plenty of cameras lack plenty of controls; that is not an error.
 */
bool set_camera_control(uint32_t _id, int32_t _value, const char *_name);

/**
 * @brief The total light in the room, and whether there is enough of it to detect anything by.
 *
 * Fed the raw mean of every frame. GetBrightness() is that mean, median filtered over a second:
 * 0 to 255, the absolute level the camera sees, before any scaling. IsBright() is the verdict
 * the detectors are gated on, with hysteresis (LIGHT_DARK_BELOW / LIGHT_BRIGHT_ABOVE) and a
 * hold time, so a level sitting on the line or the tubes ramping does not toggle it.
 *
 * @note With the camera's auto exposure on, the mean stays up for as long as the camera can
 * @note compensate and falls once it has run out of exposure and gain. That is exactly the
 * @note point past which the detectors stop being trustworthy, which is what the gate is for.
 */
class cLightMeter
{
    cMedianFilter m_filter;
    uint8_t m_level{};
    bool m_bright{};
    bool m_primed{};
    bool m_crossing{};      //the reading is on the other side of the line, waiting out the hold
    cTimer m_hold;
    cTimer m_darkSeen;      //runs for LIGHT_SWITCH_ON_MS after every reading under LIGHT_DARK_BELOW
    bool m_fastRise{};      //the crossing in progress started that soon after the dark
    uint8_t m_darkLevel{};  //the last reading under LIGHT_DARK_BELOW
    bool m_switchedOn{};

    public:
    explicit cLightMeter(size_t _window) : m_filter(_window) {}

    /** @brief Takes one frame's raw mean. @return true when the verdict changed. */
    bool Update(uint8_t _raw);
    /** @brief Total light, 0 (black) to 255, median filtered. */
    [[nodiscard]] uint8_t GetBrightness() const { return m_level; }
    /** @brief Whether the room is lit well enough for motion and face detection. */
    [[nodiscard]] bool IsBright() const { return m_bright; }
    /** @brief Whether the change Update() just reported was a light being switched on: dark to
     *  bright within LIGHT_SWITCH_ON_MS, rather than a slow rise. Only meaningful right then. */
    [[nodiscard]] bool SwitchedOn() const { return m_switchedOn; }
};

//eof capture.h
