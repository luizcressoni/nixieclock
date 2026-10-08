/*! \file face_detection.h*/
#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include "../utils/structs.h"

using namespace cv;

/*! \class cFaceDetection
    \brief Class for face detection using OpenCV's Classifier.
    
    This class encapsulates the functionality for detecting faces in video frames captured from a camera.
    It uses an OpenCV's pre-trained model for face detection.

    \note The class no longer owns a VideoCapture. It is handed the grayscale band the main loop
    \note already captured, so the one frame serves both detectors.
*/
class cFaceDetection
{
    protected:
    sCameraConfig *m_cameraconfig; // Reference to the camera configuration
    CascadeClassifier m_faceCascade;
    std::vector<Rect> m_faces;
    Size m_minFace;
    Size m_maxFace;
    int m_consecutive{};
    Rect m_lastFace;                //!< the face the consecutive count is following
    Mat m_equalized;                //!< scratch for the equalised copy, reused across frames
    int m_window{};                 //!< the cascade's own window, in pixels
    std::vector<int> m_sizes;       //!< the face sizes the search really tries
    bool m_adjusted{};              //!< the configured range had none of them and was widened

    public:
    explicit cFaceDetection(sCameraConfig *_cameraconfig);
    ~cFaceDetection();

    bool Init(const Size &_band);
    bool Detect(const Mat &_gray, uint8_t _brightness);
    void Reset() { m_consecutive = 0; }

    int GetWindow() const                     { return m_window; }
    const std::vector<int> &GetSizes() const  { return m_sizes; }
    bool WasAdjusted() const                  { return m_adjusted; }
};

//eof face_detection.h
