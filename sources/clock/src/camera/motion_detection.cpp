/*! \file motion_detection.cpp */
#include "motion_detection.h"
#include "../utils/defines.h"
#include <cstdlib>
#include <unistd.h>
#include <vector>

using namespace std;

/*! \brief Finds the centre of the largest moving object in the frame pair
    \param _threshold Threshold for motion detection
    \return Centre X of that object, or -1 if nothing is moving

    \note The largest contour, not the mean of all of them. Averaging the centroids of every
    \note moving blob returns the centre of nothing in particular: a hand crossing in front of
    \note a person who is also shifting about produced a figure pulled between the two, which
    \note moved less than the hand did and sometimes in the other direction.

    \note Only the current frame is blurred here: the previous one was blurred on the pass that
    \note captured it and has been kept. The old code blurred both halves of every pair and ran
    \note two pairs per call, four Gaussians where one will do.
*/
int cMotionDetection::ComputeMotionCenter(int _threshold)
{
    if (m_prevBlur.empty() || m_curBlur.empty()) return -1;

    absdiff(m_prevBlur, m_curBlur, m_diff);
    threshold(m_diff, m_thresh, _threshold, 255, THRESH_BINARY);

    //This guard is meant to throw out a light being switched on, or a cloud crossing the sun,
    //which light the whole band at once and are not motion. Measured against FRAME_WIDTH *
    //FRAME_HEIGHT it never could: the band is half the frame, so the ratio topped out at
    //exactly 0.5 and the test was for more than 0.5. Against the band's own area it works.
    const double activeRatio = static_cast<double>(countNonZero(m_thresh)) /
                               static_cast<double>(m_thresh.total());
    if (activeRatio > MOTION_GLOBAL_CHANGE) return -1;

    std::vector<std::vector<Point>> contours;
    findContours(m_thresh, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    double largest = 0.0;
    int center = -1;
    for (const auto &contour : contours)
    {
        const double area = contourArea(contour);
        if (area < MOTION_MIN_CONTOUR || area <= largest) continue;  // noise, or not the biggest
        Moments m = moments(contour);
        if (m.m00 > 0)
        {
            largest = area;
            center = static_cast<int>(m.m10 / m.m00);
        }
    }
    return center;
}

/*! \brief Constructor
    \param _cameraconfig Camera configuration
*/
cMotionDetection::cMotionDetection(sCameraConfig *_cameraconfig)
{
    m_cameraconfig = _cameraconfig;
}

cMotionDetection::~cMotionDetection()
= default;

/*! \brief Sizes the travel a gesture must cover against the band it will be given.
    \param _band Size of the grayscale band.
*/
void cMotionDetection::Init(const Size &_band)
{
    m_minTravel = _band.width * MOTION_MIN_TRAVEL_PERCENT / 100;
    if(m_minTravel < 1)
        m_minTravel = 1;
    m_path.reserve(MOTION_PATH_MAX);
    printf("Motion detection: band %dx%d, a gesture must travel %d px\n",
           _band.width, _band.height, m_minTravel);
}

/*! \brief Judges the path collected so far and clears it.
    \return MOTION_LEFT, MOTION_RIGHT, or MOTION_NONE when the path says nothing certain.

    \note Two tests, both of which must pass. The path has to have gone somewhere -- a net
    \note displacement of a quarter of the band -- and it has to have gone there without
    \note changing its mind, which is what rules out a wave: a wave ends up near where it
    \note started and spends half its steps disagreeing with its own net direction.
    \note Returning NONE rather than guessing is the point of the whole exercise. Undirected
    \note movement has already been reported as MOTION_ANY while the gesture was running, so
    \note nothing is lost by declining to invent a direction for it.
*/
int cMotionDetection::CloseEpisode()
{
    const size_t samples = m_path.size();
    if (samples < MOTION_MIN_SAMPLES)
    {
        m_path.clear();
        return MOTION_NONE;
    }

    const int net = m_path.back() - m_path.front();
    int steps = 0, agree = 0;
    for (size_t i = 1; i < samples; i++)
    {
        const int step = m_path[i] - m_path[i - 1];
        if (abs(step) < MOTION_STEP_MIN) continue;      //too small to have an opinion
        steps++;
        if ((step > 0) == (net > 0)) agree++;
    }
    m_path.clear();

    if (abs(net) < m_minTravel)
        return MOTION_NONE;
    if (steps == 0 || (agree * 100 / steps) < MOTION_MONOTONIC_PERCENT)
        return MOTION_NONE;

    //Convention inherited from the original code: an object travelling to the right in the
    //image reports MOTION_LEFT. Kept deliberately -- the camera faces the room, so the image
    //is mirrored with respect to whoever is standing in front of it.
    if (net > 0)
    {
        printf("Gesture to the left (%d px over %zu frames)\n", net, samples);
        return MOTION_LEFT;
    }
    printf("Gesture to the right (%d px over %zu frames)\n", -net, samples);
    return MOTION_RIGHT;
}

/*! \brief Checks one frame for motion.
    \param _gray Grayscale detection band, as captured by the caller.
    \return MOTION_LEFT, MOTION_RIGHT, MOTION_ANY or MOTION_NONE

    \note One frame in, one verdict out, never blocking. The old version looped internally and
    \note the only way out of that loop was a frame pair with no movement in it, so while
    \note somebody was actually moving in front of the clock it never returned at all: the
    \note movement was reported after it had stopped, and face detection, which runs from the
    \note same loop, did not get to run for as long as the movement lasted.
*/
int cMotionDetection::Check(const Mat &_gray)
{
    m_sawMovement = false;
    GaussianBlur(_gray, m_curBlur, Size(5, 5), 1.5);

    if (!m_haveprev)
    {
        std::swap(m_prevBlur, m_curBlur);
        m_haveprev = true;
        return MOTION_NONE;
    }

    const int center = ComputeMotionCenter(m_cameraconfig->threshold);
    std::swap(m_prevBlur, m_curBlur);   //the current frame becomes the previous one

    if (center < 0)
    {
        //Nothing moving. A gesture is only over once the band has been quiet for a few frames
        //in a row, so that a hand pausing mid sweep does not split one gesture into two.
        if (++m_quiet >= MOTION_EPISODE_END_FRAMES && !m_path.empty())
            return CloseEpisode();
        return MOTION_NONE;
    }

    m_sawMovement = true;
    m_quiet = 0;
    m_path.push_back(center);

    //Past this length it is not a gesture, it is somebody moving about in front of the clock.
    //Judge what there is and start over, so the path cannot grow without bound.
    if (m_path.size() >= MOTION_PATH_MAX)
        return CloseEpisode();

    //Somebody walking up to the clock produces plenty of movement with no direction to it yet,
    //and the old code had no way to say so: MOTION_ANY existed in the protocol and in the state
    //machine, which treats it as "stay awake", but nothing ever sent it. Rate limited because
    //the state machine adds to the awake time on each one, and at frame rate that would grow
    //without bound for as long as anyone stood there.
    if (m_path.size() >= MOTION_PRESENCE_FRAMES && m_anyCooldown.IsTimeOut())
    {
        m_anyCooldown.SetTimeOut(MOTION_ANY_COOLDOWN_MS);
        printf("Motion detected!\n");
        return MOTION_ANY;
    }

    return MOTION_NONE;
}
//eof motion_detection.cpp
