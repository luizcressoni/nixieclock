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

    \note The largest blob and those near its size, not the mean of all of them. Averaging the
    \note centroids of every moving blob returns the centre of nothing in particular: a hand
    \note crossing in front of a person who is also shifting about produced a figure pulled
    \note between the two, which moved less than the hand did and sometimes in the other direction.

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
    const int shift = GlobalShift();
    if (shift != 0)
        subtract(m_signed, Scalar(shift), m_signed);
#endif
    convertScaleAbs(m_signed, m_diff);

    //Most of any strip is still background, so its median difference is the sensor noise. Per
    //strip, the quietest one, for the same reason as GlobalShift(): with a hand close to the lens
    //over half the band changes, the whole band median was the hand, and the threshold rose to six
    //times the hand's own contrast and cut it out.
    int noise = 255;
    const int strip = m_diff.cols / MOTION_GLOBAL_STRIPS;
    for (int i = 0; i < MOTION_GLOBAL_STRIPS; i++)
        noise = std::min(noise, median_u8(m_diff(Rect(i * strip, 0, strip, m_diff.rows))));
    m_lastThreshold = std::max(_threshold, MOTION_NOISE_K * noise);
    threshold(m_diff, m_thresh, m_lastThreshold, 255, THRESH_BINARY);

    if (IsLightChange())
    {
        m_lightRejects++;
        return -1;
    }

    morphologyEx(m_thresh, m_thresh, MORPH_OPEN, m_kernelOpen);
    dilate(m_thresh, m_thresh, m_kernelJoin);

    const int count = connectedComponentsWithStats(m_thresh, m_labels, m_stats, m_centroids, 8, CV_32S);

    int largest = 0;
    for (int i = 1; i < count; i++)     //label 0 is the background
        largest = std::max(largest, m_stats.at<int>(i, CC_STAT_AREA));
    if (largest < MOTION_MIN_CONTOUR)
        return -1;

    //One object moving fast leaves two blobs in a frame difference: where it arrived and where it
    //left. When they are about the same size, "the largest" alternated between them from frame to
    //frame, and a clean sweep came out as a zigzag that failed the direction test. Both are taken,
    //weighted by area, and anything much smaller -- somebody behind the hand shifting their
    //weight -- still is not.
    double sum = 0, weight = 0;
    for (int i = 1; i < count; i++)
    {
        const int area = m_stats.at<int>(i, CC_STAT_AREA);
        if (area < MOTION_MIN_CONTOUR || area * MOTION_BLOB_RATIO < largest) continue;
        sum += m_centroids.at<double>(i, 0) * area;
        weight += area;
    }
    return cvRound(sum / weight);
}

/*! \brief The part of the frame difference that is a change of light, not of anything in it
    \return The shift to take out of the signed difference.
    \note The median of the strip medians whose magnitude is smallest, not the median of the whole
    \note band. A change of light moves every strip together, so any one of them measures it. A hand
    \note close to the lens, sweeping, changes more than half the band between two frames, and the
    \note whole band median was then the hand: subtracting it erased the hand and left the
    \note background as the "motion". The strips the hand is not in still read the light.
*/
int cMotionDetection::GlobalShift() const
{
    const int strip = m_signed.cols / MOTION_GLOBAL_STRIPS;
    int best = 0;
    bool first = true;
    for (int i = 0; i < MOTION_GLOBAL_STRIPS; i++)
    {
        const int m = median_s16(m_signed(Rect(i * strip, 0, strip, m_signed.rows)));
        if (first || abs(m) < abs(best))
            best = m;
        first = false;
    }
    return best;
}

/*! \brief Whether what is left after the compensation is a change of light anyway
    \note A light being switched on or a cloud crossing the sun, unevenly enough to get through
    \note the compensation. It used to be any frame pair with more than half the band changed,
    \note which is also exactly what a hand sweeping close to the lens looks like: the strongest
    \note and most deliberate gesture there is, thrown away as a light. Now every strip must have
    \note changed. A hand, however close, leaves some of the band alone; a light does not.
*/
bool cMotionDetection::IsLightChange() const
{
    const int strip = m_thresh.cols / MOTION_GLOBAL_STRIPS;
    const double area = static_cast<double>(strip) * m_thresh.rows;
    for (int i = 0; i < MOTION_GLOBAL_STRIPS; i++)
        if (countNonZero(m_thresh(Rect(i * strip, 0, strip, m_thresh.rows))) <= MOTION_GLOBAL_CHANGE * area)
            return false;
    return true;
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

/*! \brief Judges the path collected so far, without touching it.
    \param _why Receives why the verdict is what it is, for the log.
    \return MOTION_LEFT, MOTION_RIGHT, or MOTION_NONE when the path says nothing certain.

    \note First the whole path, two tests that must both pass. It has to have gone somewhere -- a
    \note net displacement of a quarter of the band -- and it has to have gone there without
    \note changing its mind, which is what rules out a wave: a wave ends up near where it started
    \note and spends half its steps disagreeing with its own net direction.
    \note Then, if that failed, the longest run in one direction inside it. A sweep is often not
    \note the whole episode: whoever made it is still in front of the clock, shifting about, and
    \note the episode only ends once the band has been still for a few frames. That tail used to
    \note vote on the sweep and outvote it. A run counts if it travelled as far as a sweep must and
    \note nothing before or after it went back over more than half of it -- a wave does exactly
    \note that, a fidget does not.
    \note Returning NONE rather than guessing is still the point. Undirected movement has already
    \note been reported as MOTION_ANY while the gesture was running, so nothing is lost by it.
*/
int cMotionDetection::JudgePath(const char **_why) const
{
    const size_t samples = m_path.size();
    if (samples < MOTION_MIN_SAMPLES)
    {
        *_why = "too short";
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
    int travel = net;
    if (abs(net) >= m_minTravel && steps > 0 && agree * 100 / steps >= MOTION_MONOTONIC_PERCENT)
        *_why = "whole path";
    else
    {
        //Longest run of steps that all agree; small steps go along with either direction.
        size_t bestFrom = 0, bestTo = 0, from = 0;
        int dir = 0;
        for (size_t i = 1; i < samples; i++)
        {
            const int step = m_path[i] - m_path[i - 1];
            if (abs(step) >= MOTION_STEP_MIN)
            {
                const int sign = step > 0 ? 1 : -1;
                if (dir != 0 && sign != dir)
                    from = i - 1;               //the run ends here and the next starts
                dir = sign;
            }
            if (abs(m_path[i] - m_path[from]) > abs(m_path[bestTo] - m_path[bestFrom]))
            {
                bestFrom = from;
                bestTo = i;
            }
        }
        travel = m_path[bestTo] - m_path[bestFrom];
        if (abs(travel) < m_minTravel || bestTo - bestFrom + 1 < MOTION_MIN_SAMPLES)
        {
            *_why = (abs(net) < m_minTravel) ? "did not travel" : "no direction";
            return MOTION_NONE;
        }
        //Went back over half of it, before or after: a wave, or the far leg of one.
        const int a = m_path[bestFrom], b = m_path[bestTo], half = abs(travel) / 2;
        const int up = travel > 0 ? 1 : -1;
        for (size_t i = 0; i < samples; i++)
        {
            if ((i > bestTo && (m_path[i] - b) * up < -half) ||
                (i < bestFrom && (m_path[i] - a) * up > half))
            {
                *_why = "wave";
                return MOTION_NONE;
            }
        }
        *_why = "run inside the path";
    }

    //Convention inherited from the original code: an object travelling to the right in the
    //image reports MOTION_LEFT. Kept deliberately -- the camera faces the room, so the image
    //is mirrored with respect to whoever is standing in front of it.
    return (travel > 0) ? MOTION_LEFT : MOTION_RIGHT;
}

/*! \brief Judges the path collected so far, logs the verdict and clears it.
    \note One line per episode that had anything in it: without them there is no telling, from
    \note the clock, whether a gesture that did nothing was never seen, seen too briefly, or
    \note seen and judged undirected.
*/
int cMotionDetection::CloseEpisode()
{
    const char *why = "";
    const int verdict = JudgePath(&why);
    if (m_path.size() >= MOTION_MIN_SAMPLES)
        printf("Motion episode: %zu frames, %d px net -> %s (%s)\n", m_path.size(),
               m_path.back() - m_path.front(),
               verdict == MOTION_LEFT ? "gesture LEFT" : verdict == MOTION_RIGHT ? "gesture RIGHT" : "no gesture",
               why);
    m_path.clear();
    return verdict;
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
    m_reported = false;
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
        if (++m_quiet >= MOTION_EPISODE_END_FRAMES)
        {
            m_reported = false;
            if (!m_path.empty())
                return CloseEpisode();
        }
        return MOTION_NONE;
    }

    m_sawMovement = true;
    m_quiet = 0;
    //The sweep was reported early, below; what is left of the same movement is its tail, and it
    //must not count again. It still says somebody is there.
    if (m_reported)
    {
        if (m_anyCooldown.IsTimeOut())
        {
            m_anyCooldown.SetTimeOut(MOTION_ANY_COOLDOWN_MS);
            return MOTION_ANY;
        }
        return MOTION_NONE;
    }
    m_path.push_back(center);

    //Past this length it is not a gesture, it is somebody moving about in front of the clock.
    //Judge what there is and start over, so the path cannot grow without bound.
    if (m_path.size() >= MOTION_PATH_MAX)
        return CloseEpisode();

    //Somebody who sweeps and then stays in front of the clock keeps the band busy, and the
    //episode would not end, or be judged, until they stood still or MOTION_PATH_MAX frames had
    //gone by. As long as a gesture could still be in progress, a sweep already traced is reported
    //as soon as it is there.
    if (m_path.size() == MOTION_GESTURE_FRAMES)
    {
        const char *why = "";
        if (JudgePath(&why) != MOTION_NONE)
        {
            m_reported = true;
            return CloseEpisode();
        }
    }

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
