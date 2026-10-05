/*! \file defines.h */
#pragma once

#define TEST_MODE                  //define this to enable test mode

#define CAMERA_VERSION             "1.0.0"

//Resolution we ask the driver for. It is a request, not a promise: 320x200 is an unusual mode
//and plenty of UVC cameras will hand back the nearest one they do support instead. The camera
//reads the size of the first frame it actually receives and derives everything from that, so
//a substitution no longer silently moves the detection band out from under the detectors.
#define FRAME_WIDTH                 320
#define FRAME_HEIGHT                200

//Face detection.
//scaleFactor 1.1 with minNeighbors 2 is the most expensive and the noisiest combination there
//is. Measured on the band this runs on, 1.2 with 3 neighbours costs a little over half as much
//and reports fewer false positives, which is what pays for the consecutive frame confirmation
//below -- a confirmation the old code described in a comment but never actually performed,
//because the count it compared against was 1.
//These three are defaults: the web page sets them (detection.face_* in the config).
#define FACE_SCALE_FACTOR_DEFAULT   1.2
#define FACE_MIN_NEIGHBORS_DEFAULT  3
#define FACE_MIN_CONSECUTIVE_DEFAULT 2  //runs in a row before a face wakes the clock
//Face detection looks at the whole frame, not the middle half it used to get. Cutting the
//frame down was hiding two different people: anyone approaching from either side, who was
//outside the horizontal half, and anyone standing close, whose head is taller than the 100
//pixel band and so could not fit a detection window at all. Whole frames cost about four
//times a band, which is paid for by running the cascade on one frame in N rather than on
//every one: motion detection is cheap, runs every frame and is what wakes the clock
//immediately, so the cascade only has to confirm within a fraction of a second.
//...and by only running it at full cadence while the motion detector is actually seeing
//something. An empty room is the normal state of a clock on a shelf, and a whole frame of
//cascade windows several times a second to confirm that nobody is there is the single most
//expensive thing this program could be doing. Motion costs a blur and a diff, says "somebody
//is moving" long before the cascade could, and is what opens the active window below.
#define FACE_DETECT_EVERY_N         3   //one frame in this many, just after movement
#define FACE_IDLE_EVERY_N           15  //...and one in this many once the room has gone still
#define FACE_ACTIVE_WINDOW_MS       5000 //how long movement keeps the fast cadence going
//While a gesture is actually being traced, the cascade stands down completely. A sweep lasts
//two to four tenths of a second, which is three to six frames, and the path needs at least
//three points to be judged at all -- so those frames are worth far more spent on sampling the
//gesture than on a face that will still be there when it ends. Arming the fast cadence on
//movement and then spending it during the gesture was the wrong way round: it cut the sample
//rate exactly when resolution mattered most.
#define FACE_SUSPEND_DURING_GESTURE 1

//Motion detection.
//Direction is decided once, when the gesture is over, from the whole path it traced -- not
//accumulated frame by frame as it used to be. Summing per frame deltas cannot tell a sweep
//from a wave: a wave contains both directions in equal measure, so whichever half happened to
//win by the required score fired, which is a coin toss. Judged as a path, a wave simply fails
//the monotonicity test and is reported as undirected presence, which is the honest answer.
//Measured on synthetic gestures, this also recovers the slow sweep that the old per frame
//deadband threw away entirely.
#define MOTION_MIN_CONTOUR          100     //contour area below this is sensor noise
#define MOTION_GLOBAL_CHANGE        0.5     //fraction of the band that means a light changed
#define MOTION_PRESENCE_FRAMES      3       //frames of movement that mean "someone is there"
#define MOTION_ANY_COOLDOWN_MS      2000    //...and how often that may be reported
#define MOTION_WAKE_SECONDS         10      //how long undirected motion alone keeps the tubes lit
#define MOTION_GESTURE_SECONDS      15      //...and how much a deliberate sweep is worth
#define MOTION_EPISODE_END_FRAMES   3       //quiet frames that close a gesture
#define MOTION_MIN_SAMPLES          3       //points a path needs before it can be judged
#define MOTION_MIN_TRAVEL_PERCENT   25      //net displacement, as a percentage of band width
#define MOTION_MONOTONIC_PERCENT    70      //...and how much of the path must agree with it
#define MOTION_STEP_MIN             4       //px; smaller steps do not vote on monotonicity
//A gesture is short. Past this the episode is somebody milling about in front of the clock,
//which must not go on suppressing the face cascade (see FACE_SUSPEND_DURING_GESTURE above).
#define MOTION_GESTURE_FRAMES       12      //frames a path may claim to still be a gesture
#define MOTION_PATH_MAX             45      //...and the hard cap before it is closed anyway

#define NETWORK_CHECK_FILE          "/tmp/network_mode"
#define FACE_STATUS_FILE            "/tmp/camera_face.json" //what the face search really does, for the web page
#define SSID_CHECK_FILE             "/tmp/wifi.txt"
//The cascades the web page can pick from. The config stores the name, never a path.
#define CASCADE_LBP_IMPROVED_FILE   "./lbpcascade_frontalface_improved.xml"
#define CASCADE_LBP_FILE            "./lbpcascade_frontalface.xml"
#define CASCADE_HAAR_FILE           "./haarcascade_frontalface_default.xml"

//configuration paramenters
//#define EXPORT_FACE_JPG             //to export jpg files with face detected

//cathode regeneration ("slot machine")
//Nixie cathodes that are seldom lit get poisoned and stop glowing properly.
//The routine cycles 0..9 on a single tube at full brightness to burn the deposit off.
#define REGEN_TUBE_DEFAULT          6   //tube to regenerate when none is given, as the user numbers them
#define REGEN_TUBE_MIN              1   //tube numbering used by the web page: 1 is the leftmost...
#define REGEN_TUBE_MAX              6   //...and 6 the seconds units, the rightmost one
//The routine never sleeps: it advances one digit per one second tick, so the pace comes from
//the tick itself. Raise this to hold each digit longer; never go back to sleeping inside the
//state handler, or the ticks pile up in the queue while it blocks.
#define REGEN_TICKS_PER_DIGIT       1   //one second ticks each digit stays lit
#define REGEN_TUBES                 6   //tubes on the display

//Automatic night time regeneration.
//This one is prevention, not repair, and that is why it looks nothing like the manual routine
//above: all six tubes at once, at the brightness the user configured, for a few short passes in
//the small hours. Overdriving healthy tubes every single night would spend the life we are
//trying to save, so the overcurrent stays where it belongs, in the on demand repair mode.
//A full 0..9 sweep of every cathode on every tube takes ten seconds, which is what makes it
//cheap enough to run several times a night instead of once for a long while.
#define REGEN_NIGHT_HOUR_DEFAULT    2   //default first session, and the old "hour" key's fallback
#define REGEN_NIGHT_SESSIONS        3   //default sessions, one hour apart: 02:00, 03:00 and 04:00
#define REGEN_SESSIONS_MAX          6   //preventive, not repair: more than this spends tube life
#define REGEN_NIGHT_CYCLES          5   //full 0..9 sweeps per session, so about 50s each

//Daytime gate for the scheduled wake ups.
//The tubes only announce the hour while the sun is up. Face and motion are never gated: if
//someone is standing in front of the clock at three in the morning, they want to see the time.
#define DAY_START_DEFAULT           7   //fallback window, whole hours, both ends inclusive
#define DAY_END_DEFAULT             18
//The weather service answers "Polar Day" or "Polar Night" above the arctic circles, and the
//coordinates are editable from the web page, so a non numeric answer has to be survivable.
#define ASTRO_INVALID               0xFFFF
//Stale sunrise times still beat the fixed window: the sun moves about a minute a day, while the
//fixed window is up to an hour and a half off. So we only drop the data after a long outage,
//when the season itself may have moved on.
#define ASTRO_MAX_AGE_HOURS         168 //a week
#define ASTRO_REFRESH_HOUR          0   //when the daily sunrise/sunset fetch happens...
#define ASTRO_REFRESH_MIN           30  //...kept away from the xx:56 forecast call
#define ASTRO_CLOCK_DRIFT_WARN      20  //minutes of disagreement with the service before we complain


//Ambient light dimming.
//The camera measures the room and the tubes follow it down at night. The reading used to be
//taken after an exposure compensation and a CLAHE pass -- two steps whose whole purpose is to
//remove the absolute light level the reading was trying to report -- and the 21..75 band it was
//scaled against had been calibrated by hand on whatever survived that. Both steps are gone, the
//mean is now taken on the raw frame, and these two ends replace that band: a starting point for
//an indoor room, not a measurement. The camera logs the raw value next to the scaled one on
//every update, so the real ends can be read off the clock itself and set here.
#define BRIGHTNESS_RAW_DARK         10  //raw mean that reads as 0...
#define BRIGHTNESS_RAW_BRIGHT       140 //...and the one that reads as 100
//The gap between the two thresholds below has to stay wider than the sensor noise, or the tubes
//change brightness every time a reading is sent.
#define BRIGHTNESS_DIM_BELOW        15  //dim the tubes under this reading...
#define BRIGHTNESS_RESTORE_ABOVE    35  //...and only bring them back over this one
#define BRIGHTNESS_DIM_PERCENT      30  //what dimmed means, as a percentage of full scale
#define BRIGHTNESS_MIN_DELTA        3   //points a reading must move before it is worth resending
#define BRIGHTNESS_WINDOW_SECONDS   1   //seconds of frames the median filter runs over...
#define BRIGHTNESS_WINDOW_MAX       60  //...capped, in case the configured frame rate is nonsense

//signal values
//0xC000 flag for error signals
#define SIG_ERROR                   0x8000
#define SIG_CONFIG                  0x4000
#define SIG_NO_CAMERA               0x0001
#define SIG_NO_WIFI                 0x0002

//0x1000 flag for CGI signals
#define SIG_CGI                     0x1000
#define SIG_CGI_RELOAD              0x1001  
#define SIG_CGI_VU_MIN              0x1002
#define SIG_CGI_VU_MAX              0x1004
#define SIG_CGI_REBOOT              0x1008
#define SIG_CGI_EXIT                0x1010
#define SIG_CGI_WIFI                0x1020
#define SIG_CGI_BRIGHNESS           0x1040
//The low nibble of SIG_CGI_REGEN_ON carries the tube to work on, 1 to 6, so the page can pick one.
//Masking with SIG_CGI_REGEN_MASK is what tells it apart from the other CGI signals.
#define SIG_CGI_REGEN_ON            0x1080  //start the cathode regeneration routine
#define SIG_CGI_REGEN_MASK          0xFFF0  //isolates SIG_CGI_REGEN_ON from the tube number
#define SIG_CGI_REGEN_OFF           0x1100  //stop the cathode regeneration routine

//0x0000 flag for face/motion signals
#define SIG_FACE_DETECTED           0x0004
#define SIG_MOTION_DETECTED_LEFT    0x0008
#define SIG_MOTION_DETECTED_RIGHT   0x0010
#define SIG_MOTION_DETECTED_ANY     0x0020

//0x0100 flag for signals with values up to 0xff
#define SIG_BRIGHTNESS              0x0100  //values from 0x00 to 0x64 (0 - 100)
//0x2000 flags for network detection
#define SIG_NETWORK_CONNECTED       0x2001  //we're connected to a wifi network
#define SIG_NETWORK_HOTSPOT         0x2002  //we're a hotspot

#define SIG_NONE                    0x0000  
#define SIG_TIME_CHANGED            0x0001 

//eof defines.h