/*! \file main.cpp*/
#include <unistd.h>
#include "capture.h"
#include "face_detection.h"
#include "motion_detection.h"
#include "../utils/signals.h"
#include "../utils/defines.h"
#include "../utils/json_parser.h"
#include "../utils/median.h"
#include "../utils/ctimer.h"
#include <thread>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <memory>

using namespace std;
using namespace std::chrono;

/*! \brief Reports an error to the clock every few seconds until a signal says otherwise
    \return SIG_CGI_EXIT or SIG_CGI_RELOAD, whichever arrived.
    \note The camera used to sit here forever, deaf: it could not be stopped short of SIGKILL,
    \note and a configuration fix from the web page never reached it.
*/
static int report_until_signal(cSignal &_signal, int _value, const char *_why)
{
    printf("%s\n", _why);
    while(true)
    {
        _signal.Send(NIXIE_SIGNAL, _value);
        for(int i = 0; i < 50; i++)
        {
            int value = 0;
            if(poll_signal(&value) && (value == SIG_CGI_EXIT || value == SIG_CGI_RELOAD))
                return value;
            usleep(100 * 1000);
        }
    }
}

/*! \brief Tells the web page what the face search is really doing
    \param _state "on", "off" or "error".
    \param _face The detector, when it initialised; null otherwise.
    \note The configured sizes go through clamps the page cannot see: the frame the driver really
    \note delivers, the cascade's own window, and the handful of discrete sizes the search tries.
    \note A setting that collides with any of them used to fail in silence. This is how it shows.
*/
static void write_face_status(const char *_state, const sCameraConfig &_config, int _width, int _height,
                              const cFaceDetection *_face)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state", _state);
    cJSON_AddNumberToObject(root, "time", static_cast<double>(time(nullptr)));
    cJSON_AddNumberToObject(root, "frame_width", _width);
    cJSON_AddNumberToObject(root, "frame_height", _height);
    cJSON_AddStringToObject(root, "cascade", face_cascade_name(_config.faceCascade));
    cJSON_AddNumberToObject(root, "requested_min", _config.faceSizeMin);
    cJSON_AddNumberToObject(root, "requested_max", _config.faceSizeMax);
    if(_face != nullptr)
    {
        cJSON_AddNumberToObject(root, "window", _face->GetWindow());
        cJSON *sizes = cJSON_AddArrayToObject(root, "sizes");
        for(const int size : _face->GetSizes())
            cJSON_AddItemToArray(sizes, cJSON_CreateNumber(size));
        cJSON_AddBoolToObject(root, "adjusted", _face->WasAdjusted());
    }

    //renamed into place, so the CGI never reads half a file
    if(char *text = cJSON_PrintUnformatted(root))
    {
        if(FILE *f = fopen(FACE_STATUS_FILE ".tmp", "w"))
        {
            const bool written = fputs(text, f) >= 0;
            if(fclose(f) == 0 && written)
                rename(FACE_STATUS_FILE ".tmp", FACE_STATUS_FILE);
        }
        cJSON_free(text);
    }
    cJSON_Delete(root);
}

/*! \brief Runs the detectors until a signal asks for a reload or an exit
    \param cap The open camera.
    \param NixieSignal Where detections are reported.
    \param cameraConfig The configuration in force; the detectors keep a pointer to it.
    \param width Frame width, as delivered by the driver.
    \param height Frame height, likewise.
    \return SIG_CGI_RELOAD or SIG_CGI_EXIT.
*/
static int run_detection(VideoCapture &cap, cSignal &NixieSignal, sCameraConfig &cameraConfig,
                         const int width, const int height)
{
    //Motion only needs the band across the middle of the frame: the ceiling and the floor
    //contribute noise and nothing else. Face detection gets the whole frame -- the middle half
    //it used to get was hiding anyone approaching from the side, and anyone close enough for
    //their head to be taller than the band, which is to say anyone actually reading the clock.
    const Rect motionRoi(0, height / 4, width, height / 2);
    const Size frame(width, height);

    const bool wantFace   = (cameraConfig.detection_model == CAM_DETECT_FACE ||
                             cameraConfig.detection_model == CAM_DETECT_BOTH);
    const bool wantMotion = (cameraConfig.detection_model == CAM_DETECT_MOTION ||
                             cameraConfig.detection_model == CAM_DETECT_BOTH);

    uint8_t oldbrightnessValue = 0xff;
    //A second of frames. The old three sample window spanned two tenths of a second at the
    //default frame rate, which let the sensor noise through to the clock almost untouched.
    //Sizing it off the configured fps keeps that one second whatever the rate is set to.
    int brightnessWindow = cameraConfig.fps * BRIGHTNESS_WINDOW_SECONDS;
    if(brightnessWindow < 1)
        brightnessWindow = 1;   //a zero sized window would read past the end of an empty buffer
    if(brightnessWindow > BRIGHTNESS_WINDOW_MAX)
        brightnessWindow = BRIGHTNESS_WINDOW_MAX;
    cMedianFilter brightness(brightnessWindow);
    cTimer  timer;
    timer.SetTimeOut(1);

    auto motionDetection = std::make_unique<cMotionDetection>(&cameraConfig);
    auto FaceDetection = std::make_unique<cFaceDetection>(&cameraConfig);

    motionDetection->Init(Size(motionRoi.width, motionRoi.height));
    //a reload can fix this one: switching the page to motion only takes the cascade out of it
    if (wantFace && !FaceDetection->Init(frame))
    {
        write_face_status("error", cameraConfig, width, height, nullptr);
        return report_until_signal(NixieSignal, SIG_ERROR + SIG_CONFIG + SIG_FACE_DETECTED,
                                   "Ops, face detection init failed");
    }
    write_face_status(wantFace ? "on" : "off", cameraConfig, width, height,
                      wantFace ? FaceDetection.get() : nullptr);

    printf("Detectors: face %s, motion %s\n", wantFace ? "on" : "off", wantMotion ? "on" : "off");

    const double desired_fps = cameraConfig.fps;
    const milliseconds target_frame_time(static_cast<int>(1000.0/desired_fps));

    Mat gray;   //kept across iterations so cvtColor reuses the buffer instead of reallocating
    int faceTick = 0;
    cTimer faceActive;  //armed by movement; while it runs, the cascade uses the fast cadence

    while(true){
        int signal_value = 0;
        while(poll_signal(&signal_value))
            if(signal_value == SIG_CGI_EXIT || signal_value == SIG_CGI_RELOAD)
                return signal_value;

        auto frame_start = std::chrono::steady_clock::now();

        //One capture per pass, shared. Face detection took one frame and motion detection took
        //two or three more, so a single pass of this loop blocked on three grabs -- two hundred
        //milliseconds at the configured rate, before any detection work had started.
        uint8_t rawBrightness = 0;
        if(!capture_frame(cap, gray, &rawBrightness))
        {
            sleep(1);
            continue;
        }

        //Motion first, and deliberately so: it costs a blur and a difference, it is what wakes
        //the clock immediately, and what it sees decides whether paying for the cascade on this
        //frame is worth it at all.
        if(wantMotion)
        {
            //A view, not a copy: it shares the pixels of the frame already captured.
            const Mat band = gray(motionRoi);
            switch(motionDetection->Check(band))
            {
                case MOTION_LEFT:
                    NixieSignal.Send(NIXIE_SIGNAL, SIG_MOTION_DETECTED_LEFT);
                    break;
                case MOTION_RIGHT:
                    NixieSignal.Send(NIXIE_SIGNAL, SIG_MOTION_DETECTED_RIGHT);
                    break;
                case MOTION_ANY:
                    NixieSignal.Send(NIXIE_SIGNAL, SIG_MOTION_DETECTED_ANY);
                    break;
                case MOTION_NONE:
                    break;
            }
            if(motionDetection->SawMovement())
                faceActive.SetTimeOut(FACE_ACTIVE_WINDOW_MS);
        }

        //The cascade reads the whole frame, which is four times the band the old code gave it,
        //so it runs on one frame in N instead of on every one. N is small just after movement
        //and large once the room has gone still, which is where a shelf clock spends nearly
        //all of its life. With motion switched off there is nothing to gate on.
        //While a gesture is actually being traced it stands down altogether: a sweep is three
        //to six frames, the path needs three points to be judged at all, and a frame spent in
        //the cascade is a frame the gesture was not sampled in. Arming the fast cadence on
        //movement and then spending it during the gesture was exactly backwards.
#if FACE_SUSPEND_DURING_GESTURE
        const bool tracing = wantMotion && motionDetection->GestureInProgress();
#else
        const bool tracing = false;
#endif
        if(wantFace && !tracing)
        {
            const bool active = !wantMotion || !faceActive.IsTimeOut();
            if(++faceTick >= (active ? FACE_DETECT_EVERY_N : FACE_IDLE_EVERY_N))
            {
                faceTick = 0;
                if(FaceDetection->Detect(gray))
                    NixieSignal.Send(NIXIE_SIGNAL, SIG_FACE_DETECTED);
            }
        }

        //Filter the raw level, then scale: the median belongs on the quantity the sensor
        //actually produced, not on one that has already been clipped to the ends of a range.
        const uint8_t rawFiltered = brightness.Update(rawBrightness);
        const uint8_t brightnessValue = scale_brightness(rawFiltered);

        //A single point of movement was enough to resend before, which meant the clock was fed a
        //fresh reading every five seconds forever, noise and all. Only real movement travels now.
        const int delta = static_cast<int>(brightnessValue) - static_cast<int>(oldbrightnessValue);
        if((delta >= BRIGHTNESS_MIN_DELTA || delta <= -BRIGHTNESS_MIN_DELTA) && timer.IsTimeOut())
        {
            timer.SetTimeOut(5000);
            oldbrightnessValue = brightnessValue;
            //The raw value goes in the log so the two ends of the scale in defines.h can be
            //calibrated against the room the clock actually stands in.
            printf("Brightness: raw %u -> %u%%\n", rawFiltered, brightnessValue);
            NixieSignal.Send(NIXIE_SIGNAL, SIG_BRIGHTNESS + brightnessValue);
        }

        auto frame_end = std::chrono::steady_clock::now();
        auto frame_duration = duration_cast<milliseconds>(frame_end - frame_start);

        if (frame_duration < target_frame_time) {
            auto wait_time = target_frame_time - frame_duration;
            this_thread::sleep_for(wait_time);
        }
    }
}

int main()
{
    //Before any thread exists, so that the signal thread is the only one that ever sees them.
    block_signals();
    start_signal_thread();

    printf("Starting camera version %s...\n", CAMERA_VERSION);
    printf("Opening camera device\n");
    VideoCapture cap;
    cap.open(0, cv::CAP_V4L2);

    cSignal NixieSignal("nixie");

    //Nothing to reload without a camera: only an exit request ends these two.
    while (!cap.isOpened())
        if(report_until_signal(NixieSignal, SIG_ERROR + SIG_NO_CAMERA, "Error opening video device") == SIG_CGI_EXIT)
            return 0;

    printf("Camera looks good, let's continue\n");
    cap.set(CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
    cap.set(CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
    //V4L2 keeps a queue of frames by default, and cap >> hands back the oldest one in it. For a
    //clock that is supposed to light up when somebody arrives, that is a quarter of a second of
    //pure latency between the room and the detector, every frame.
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

    //Ask the first frame how big it really is rather than trusting the request above. The
    //detection band and the thresholds are all derived from this, so a driver that quietly
    //substituted 320x240 for the 320x200 we asked for used to move the band without saying so.
    Mat probe;
    for(int i = 0; i < 10 && probe.empty(); i++)
        cap >> probe;
    while(probe.empty())
        if(report_until_signal(NixieSignal, SIG_ERROR + SIG_NO_CAMERA, "Camera opened but never produced a frame") == SIG_CGI_EXIT)
            return 0;

    const int width  = probe.cols;
    const int height = probe.rows;
    printf("Camera delivering %dx%d (asked for %dx%d)\n", width, height, FRAME_WIDTH, FRAME_HEIGHT);

    //The web page sends SIG_CGI_RELOAD after saving the detection settings. The camera stays
    //open across a reload; everything built from the configuration is built again.
    while(true)
    {
        sCameraConfig cameraConfig{};
        load_camera_config(&cameraConfig);
        free_jsonfile();
        cap.set(cv::CAP_PROP_FPS, cameraConfig.fps);

        if(run_detection(cap, NixieSignal, cameraConfig, width, height) == SIG_CGI_EXIT)
            break;
        printf("Reloading the configuration\n");
    }

    cap.release();
    return 0;
}

//eof main.cpp
