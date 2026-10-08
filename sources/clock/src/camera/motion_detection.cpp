/*! \file motion_detection.cpp */
#include "motion_detection.h"
#include "../utils/defines.h"
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <algorithm>

using namespace std;

/*! \brief Median of an 8 bit image, by histogram */
static int median_u8(const Mat &_img)
{
    int hist[256] = {};
    for(int y = 0; y < _img.rows; y++)
    {
        const uint8_t *p = _img.ptr<uint8_t>(y);
        for(int x = 0; x < _img.cols; x++)
            hist[p[x]]++;
    }
    const size_t half = _img.total() / 2;
    size_t acc = 0;
    for(int v = 0; v < 256; v++)
        if((acc += hist[v]) > half)
            return v;
    return 255;
}

#if MOTION_COMPENSATE_GLOBAL
/*! \brief Median of a signed difference of two 8 bit images (CV_16S, -255..255), by histogram */
static int median_s16(const Mat &_img)
{
    int hist[511] = {};
    for(int y = 0; y < _img.rows; y++)
    {
        const int16_t *p = _img.ptr<int16_t>(y);
        for(int x = 0; x < _img.cols; x++)
            hist[p[x] + 255]++;
    }
    const size_t half = _img.total() / 2;
    size_t acc = 0;
    for(int v = 0; v < 511; v++)
        if((acc += hist[v]) > half)
            return v - 255;
    return 255;
}
#endif

/*! \brief Finds the centre of the largest moving object in the frame pair
    \param _threshold Configured threshold for motion detection, used as a floor
    \return Centre X of that object, or -1 if nothing is moving

    \note The largest blob, not the mean of all of them. Averaging the centroids of every
    \note moving blob returns the centre of nothing in particular: a hand crossing in front of
    \note a person who is also shifting about produced a figure pulled between the two, which
    \note moved less than the hand did and sometimes in the other direction.

    \note Only the current frame is blurred here: the previous one was blurred on the pass that
    \note captured it and has been kept. The old code blurred both halves of every pair and ran
    \note two pairs per call, four Gaussians where one will do.

    \note Three things changed against a dim room (see MOTION_NOISE_K, MOTION_COMPENSATE_GLOBAL):
    \note the global part of the change is taken out before thresholding, the threshold rises
    \note with the measured noise, and the mask is cleaned up before it is measured -- specks of
    \note noise opened away, the pieces of one low contrast object dilated back into one. Blobs
    \note are measured in pixels: contourArea is the area of the outline's polygon, which for a
    \note thin or ragged blob is close to nothing however many pixels it has.
*/
int cMotionDetection::ComputeMotionCenter(int _threshold)
{
    if (m_prevBlur.empty() || m_curBlur.empty()) return -1;

    subtract(m_curBlur, m_prevBlur, m_signed, noArray(), CV_16S);
#if MOTION_COMPENSATE_GLOBAL
    const int shift = median_s16(m_signed);
    if (shift != 0)
        subtract(m_signed, Scalar(shift), m_signed);
#endif
    convertScaleAbs(m_signed, m_diff);

    //Most of any frame is still background, so the median difference is the sensor noise.
    const int noise = median_u8(m_diff);
    m_lastThreshold = std::max(_threshold, MOTION_NOISE_K * noise);
    threshold(m_diff, m_thresh, m_lastThreshold, 255, THRESH_BINARY);

    //This guard is meant to throw out a light being switched on, or a cloud crossing the sun,
    //which light the whole band at once and are not motion. Measured against FRAME_WIDTH *
    //FRAME_HEIGHT it never could: the band is half the frame, so the ratio topped out at
    //exactly 0.5 and the test was for more than 0.5. Against the band's own area it works.
    //With the global change compensated above, what is left for it is the uneven change: a lamp
    //lighting one side of the room more than the other.
    const double activeRatio = static_cast<double>(countNonZero(m_thresh)) /
                               static_cast<double>(m_thresh.total());
    if (activeRatio > MOTION_GLOBAL_CHANGE) return -1;

    morphologyEx(m_thresh, m_thresh, MORPH_OPEN, m_kernelOpen);
    dilate(m_thresh, m_thresh, m_kernelJoin);

    const int count = connectedComponentsWithStats(m_thresh, m_labels, m_stats, m_centroids, 8, CV_32S);

    int largest = 0;
    int center = -1;
    for (int i = 1; i < count; i++)     //label 0 is the background
    {
        const int area = m_stats.at<int>(i, CC_STAT_AREA);
        if (area < MOTION_MIN_CONTOUR || area <= largest) continue;  // noise, or not the biggest
        largest = area;
        center = cvRound(m_centroids.at<double>(i, 0));
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
    m_kernelOpen = getStructuringElement(MORPH_RECT, Size(3, 3));
    m_kernelJoin = getStructuringElement(MORPH_RECT, Size(5, 5));
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

/*! \brief Forgets the previous frame and any gesture in progress.
    \note For any gap in the frames -- a failed capture, the light gate closing and opening
    \note again. The next frame is compared with nothing instead of with one taken seconds ago,
    \note which would have been all difference, and a gesture is not stitched across the gap.
*/
void cMotionDetection::Reset()
{
    m_haveprev = false;
    m_sawMovement = false;
    m_quiet = 0;
    m_path.clear();
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
