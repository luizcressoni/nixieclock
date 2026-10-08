/*! \file motion_detection.h */
#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include "../utils/structs.h"
#include "../utils/ctimer.h"
#include "../utils/defines.h"

using namespace cv;

#define MOTION_NONE  0
#define MOTION_LEFT  1
#define MOTION_RIGHT 2
#define MOTION_ANY   3  //when motion is detected, but direction is not important

/*! \brief Class for motion detection in video streams.
 *
 * This class uses OpenCV to detect motion in a video stream captured from a camera.
 * It compares frames to determine if there is any significant change, indicating motion.
 * The class can be configured with camera settings and can return the direction of motion detected.
 *
 * \note It is handed the frame the main loop captured rather than capturing its own, and it
 * \note keeps everything that spans frames as members. All of it used to be locals rebuilt on
 * \note every call, which is why the old code had to block inside its own loop to get anywhere,
 * \note and why it could not return while the movement was still going on.
 *
 * \note Direction is decided per gesture, not per frame. The detector collects the path the
 * \note moving object traces while it is visible, and judges that path once, when the movement
 * \note stops. Accumulating a score frame by frame cannot distinguish a sweep from a wave --
 * \note the wave holds both directions in equal measure, so the old code fired whichever half
 * \note happened to reach the threshold first, which is a coin toss with "go to sleep" on one
 * \note of its faces. A path is either mostly in one direction or it is not.
*/
class cMotionDetection
{
    protected:
    sCameraConfig *m_cameraconfig; // Reference to the camera configuration
    Mat m_prevBlur, m_curBlur, m_diff, m_thresh;   //scratch buffers, reused across frames
    Mat m_signed;                  //signed difference, before the global change is taken out
    Mat m_labels, m_stats, m_centroids;            //connected components, reused likewise
    Mat m_kernelOpen, m_kernelJoin;                //morphology: drop specks, then join fragments
    int m_lastThreshold{};         //threshold in force on the last frame, for the log
    bool m_haveprev{false};
    bool m_sawMovement{};          //did the last Check() find a moving area at all
    int m_quiet{};                 //consecutive frames with nothing moving
    int m_minTravel{};             //net displacement a gesture needs, scaled in Init()
    std::vector<int> m_path;       //centre of the moving object, one entry per frame
    cTimer m_anyCooldown;          //rate limit for the undirected presence report

    int ComputeMotionCenter(int _threshold);
    int CloseEpisode();

    public:
    explicit cMotionDetection(sCameraConfig *_cameraconfig);
    ~cMotionDetection();

    void Init(const Size &_band);
    int Check(const Mat &_gray);
    void Reset();

    /*! \brief Threshold the last frame pair was cut at: the configured floor, or more if the
        \note sensor noise asked for it. Logged with the frame rate, for calibration. */
    [[nodiscard]] int GetThreshold() const { return m_lastThreshold; }

    /*! \brief Whether the last Check() found a moving area, verdict or not.
        \note This is the cheap trigger the face cascade is gated on: there is no point paying
        \note for a whole frame of Haar/LBP windows while the room has been still for seconds. */
    [[nodiscard]] bool SawMovement() const { return m_sawMovement; }

    /*! \brief Whether a gesture is being traced right now.
        \note The face cascade stands down while this is true. A sweep is three to six frames
        \note and the path needs at least three points to be judged, so those frames buy more
        \note as gesture samples than as a face that will still be there a moment later. It
        \note stops being true once the path is too long to be a gesture, so somebody standing
        \note in front of the clock and fidgeting cannot suppress face detection indefinitely. */
    [[nodiscard]] bool GestureInProgress() const
    { return !m_path.empty() && m_path.size() < MOTION_GESTURE_FRAMES; }
};

//eof motion_detection.h
