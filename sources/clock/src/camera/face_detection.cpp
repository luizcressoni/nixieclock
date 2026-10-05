/*! \file face_detection.cpp */
#include <unistd.h>
#include <algorithm>
#include <vector>
#include "face_detection.h"
#include "../utils/defines.h"

using namespace std;

#ifdef EXPORT_FACE_JPG
static int face_number = 0;
#endif

/*! \brief The face sizes detectMultiScale actually tries, smallest first
    \param _window The cascade's own window, the first size tried.
    \param _scale The scale factor; each size is the previous one times this.
    \param _limit The shorter side of the frame; nothing larger fits.
    \note The same loop OpenCV runs (cascadedetect.cpp): sizes in between are never searched, so a
    \note configured range that falls between two of these finds no face, ever, and says nothing.
*/
static vector<int> reachable_sizes(int _window, double _scale, int _limit)
{
    vector<int> sizes;
    for(double factor = 1; ; factor *= _scale)
    {
        const int size = cvRound(_window * factor);
        if(size > _limit)
            break;
        if(sizes.empty() || sizes.back() != size)
            sizes.push_back(size);
    }
    return sizes;
}

/*! \brief Constructor for cFaceDetection class.
  \param _cameraconfig Pointer to the camera configuration structure.
*/
cFaceDetection::cFaceDetection(sCameraConfig *_cameraconfig): m_consecutive(0) {
    m_cameraconfig = _cameraconfig;
}

/*! \brief Loads the classifier and works out the window sizes to search for.
  \param _band Size of the grayscale band the detector will be given.
  \return True if the classifier loaded and the configured sizes can actually be searched for.

  \note This is where the bug that kept face detection from ever working lived. The sizes were
  \note built as Size(faceSizeMin, faceSizeMax), putting the minimum in the width and the
  \note maximum in the height of a single rectangle -- which is not what minSize means. With
  \note the shipped configuration that asked detectMultiScale for windows at least 110 pixels
  \note tall inside a band 100 pixels tall. No window of that size exists in that image, every
  \note scale was skipped, and the detector returned empty on every frame it was ever given.
  \note minSize is square, maxSize is a separate argument, and both are now clamped to the band
  \note so a bad value on the web page can slow detection down but can never silence it again.
*/
bool cFaceDetection::Init(const Size &_band)
{
    m_sizes.clear();
    m_adjusted = false;
    m_window = 0;

    const char *file = CASCADE_LBP_IMPROVED_FILE;
    if(m_cameraconfig->faceCascade == FACE_CASCADE_LBP)
        file = CASCADE_LBP_FILE;
    else if(m_cameraconfig->faceCascade == FACE_CASCADE_HAAR)
        file = CASCADE_HAAR_FILE;

    if(!m_faceCascade.load(file))
    {
        printf("Face detection: failed to load '%s'\n", file);
        return false;
    }

    //Nothing smaller than the cascade's own training window can ever be found, whatever the
    //configuration asks for. The improved LBP cascade in use is 45x45, not the usual 24x24.
    const Size window = m_faceCascade.getOriginalWindowSize();
    const int floor_px = std::max(window.width, window.height);
    const int limit = std::min(_band.width, _band.height);
    m_window = floor_px;

    if(floor_px > limit)
    {
        printf("Face detection: cascade window is %dx%d but the band is only %dx%d\n",
               window.width, window.height, _band.width, _band.height);
        return false;
    }

    int lo = std::max(m_cameraconfig->faceSizeMin, floor_px);
    lo = std::min(lo, limit);

    int hi = m_cameraconfig->faceSizeMax;
    if(hi < lo)
        hi = limit;
    hi = std::min(hi, limit);

    //A range holding none of the sizes the search tries would leave face detection running and
    //finding nothing, forever. It is widened to the nearest size that is tried, and said so.
    const vector<int> all = reachable_sizes(floor_px, m_cameraconfig->faceScaleFactor, limit);
    for(const int size : all)
        if(size >= lo && size <= hi)
            m_sizes.push_back(size);
    if(m_sizes.empty())
    {
        int below = -1, above = -1;
        for(const int size : all)
        {
            if(size < lo)
                below = size;
            else if(size > hi && above < 0)
                above = size;
        }
        const bool use_below = above < 0 || (below >= 0 && lo - below <= above - hi);
        const int pick = use_below ? below : above;
        printf("Face detection: no size between %d and %d px is ever searched at scale %.2f; "
               "widened to %d px\n", lo, hi, m_cameraconfig->faceScaleFactor, pick);
        if(use_below)
            lo = pick;
        else
            hi = pick;
        m_sizes.push_back(pick);
        m_adjusted = true;
    }

    m_minFace = Size(lo, lo);
    m_maxFace = Size(hi, hi);

    printf("Face detection: %s, band %dx%d, cascade window %dx%d, searching %d to %d px, "
           "scale %.2f, %d neighbours, %d frames in a row\n",
           file, _band.width, _band.height, window.width, window.height, m_sizes.front(), m_sizes.back(),
           m_cameraconfig->faceScaleFactor, m_cameraconfig->faceMinNeighbors,
           m_cameraconfig->faceMinConsecutive);
    return true;
}

cFaceDetection::~cFaceDetection() = default;

/*! \brief Looks for a face in a frame the caller already captured.
  \param _gray Grayscale detection band.
  \return True when a face has been confirmed, false otherwise.

  \note The temporal confirmation the old comment described never ran: min_consecutive was 1,
  \note so a single frame the cascade fired on was enough and every false positive woke the
  \note clock. It is a real count now, which is what makes the lower minNeighbors affordable.
*/
bool cFaceDetection::Detect(const Mat &_gray)
{
    m_faceCascade.detectMultiScale(_gray, m_faces, m_cameraconfig->faceScaleFactor,
                                   m_cameraconfig->faceMinNeighbors,
                                   0 | CASCADE_SCALE_IMAGE, m_minFace, m_maxFace);

    if(m_faces.empty())
    {
        m_consecutive = 0;
        return false;
    }

    if(++m_consecutive < m_cameraconfig->faceMinConsecutive)
        return false;

    printf("Face detected!\n");
    m_consecutive = 0;

#ifdef EXPORT_FACE_JPG
    {
        Mat marked = _gray.clone();
        for (size_t i = 0; i < m_faces.size(); i++) {
           rectangle(marked, m_faces[i], Scalar(255, 0, 0), 1);
        }
        char name[32];
        sprintf(name, "/tmp/face_%d.jpg", face_number++);
        imwrite(name, marked);
    }
#endif
    return true;
}

//eof face_detection.cpp
