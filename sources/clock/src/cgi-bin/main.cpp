/*! \file main.cpp CGI backend of the clock's web page
    \note Runs as root, behind a hotspot with no password. Every field is checked against a
    \note range or a character set before it gets anywhere near the config file or the system.
*/

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../utils/json_parser.h"
#include "parse_stdin.h"

#include "../utils/defines.h"
#include "../utils/signals.h"

#define ZONEINFO_DIR            "/usr/share/zoneinfo/"
#define TIMESYNCD_DROPIN_DIR    "/etc/systemd/timesyncd.conf.d"
#define TIMESYNCD_DROPIN        TIMESYNCD_DROPIN_DIR "/nixie.conf"
#define TIMEDATECTL             "/usr/bin/timedatectl"
#define SYSTEMCTL               "/bin/systemctl"

cJSON *mJson = nullptr;

//! What the page shows in its alert box
static char gMessage[256] = "";

/*! \brief A signal held back until the config is safely on disk */
struct sPendingSignal
{
    const char *process;
    int value;
};
static sPendingSignal gSignals[8];
static int gSignalCount = 0;

static void queue_signal(const char *_process, int _value)
{
    if(gSignalCount < static_cast<int>(sizeof(gSignals) / sizeof(gSignals[0])))
        gSignals[gSignalCount++] = {_process, _value};
}

/*! \brief Sets the message the page will show */
static void say(const char *_format, ...)
{
    va_list args;
    va_start(args, _format);
    vsnprintf(gMessage, sizeof(gMessage), _format, args);
    va_end(args);
}

/*! \brief Records why a form was refused
    \return Always false, so a check can end with return fail(...).
*/
static bool fail(const char *_format, ...)
{
    va_list args;
    va_start(args, _format);
    vsnprintf(gMessage, sizeof(gMessage), _format, args);
    va_end(args);
    return false;
}

// ------------------------------------------------------------------ json helpers

/*! \brief Finds an object inside the config, creating it when the file predates it */
static cJSON *ensure_object(cJSON *_parent, const char *_key)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(_parent, _key);
    if(item == nullptr)
        item = cJSON_AddObjectToObject(_parent, _key);
    return item;
}

/*! \brief Stores an item under a key, replacing in place or adding when the key is new
    \note Replacing keeps the key where it was, so the file on disk does not reshuffle on save.
*/
static void put(cJSON *_json, const char *_key, cJSON *_item)
{
    if(cJSON_GetObjectItemCaseSensitive(_json, _key) != nullptr)
        cJSON_ReplaceItemInObjectCaseSensitive(_json, _key, _item);
    else
        cJSON_AddItemToObject(_json, _key, _item);
}

static void set_number(cJSON *_json, const char *_key, double _value)        { put(_json, _key, cJSON_CreateNumber(_value)); }
static void set_bool(cJSON *_json, const char *_key, bool _value)            { put(_json, _key, cJSON_CreateBool(_value)); }
static void set_string(cJSON *_json, const char *_key, const char *_value)   { put(_json, _key, cJSON_CreateString(_value)); }

// ------------------------------------------------------------------ field readers

/*! \brief Reads a whole number field, refusing anything that is not one inside the range */
static bool read_int_field(const char *_field, const char *_label, long _min, long _max, int *_out)
{
    const char *value = get_field_value(_field);
    char *end = nullptr;
    errno = 0;
    const long parsed = strtol(value, &end, 10);
    while(isspace(static_cast<unsigned char>(*end)))
        end++;
    if(end == value || *end != '\0' || errno == ERANGE || parsed < _min || parsed > _max)
        return fail("%s: enter a whole number from %ld to %ld.", _label, _min, _max);
    *_out = static_cast<int>(parsed);
    return true;
}

/*! \brief Reads a decimal field, accepting a comma as the decimal separator */
static bool read_double_field(const char *_field, const char *_label, double _min, double _max, double *_out)
{
    char value[64];
    snprintf(value, sizeof(value), "%s", get_field_value(_field));
    //"-23,55" is how half of the people setting this up will type it
    for(char *c = value; *c; c++)
        if(*c == ',')
            *c = '.';

    char *end = nullptr;
    errno = 0;
    const double parsed = strtod(value, &end);
    while(isspace(static_cast<unsigned char>(*end)))
        end++;
    if(end == value || *end != '\0' || errno == ERANGE || !std::isfinite(parsed) || parsed < _min || parsed > _max)
        return fail("%s: enter a number from %g to %g.", _label, _min, _max);
    *_out = parsed;
    return true;
}

/*! \brief Reads a text field whose length, in bytes, must fall inside a range */
static bool read_text_field(const char *_field, const char *_label, size_t _min, size_t _max,
                            char *_out, size_t _size)
{
    const char *value = get_field_value(_field);
    const size_t length = strlen(value);
    if(length < _min || length > _max || length >= _size)
    {
        if(_min == 0)
            return fail("%s: at most %zu characters.", _label, _max);
        return fail("%s: %zu to %zu characters.", _label, _min, _max);
    }
    memcpy(_out, value, length + 1);
    return true;
}

// ------------------------------------------------------------------ character sets

/*! \brief Tells whether a string can go between quotes in wpa_supplicant.conf
    \note Same rule as save_wifiConfig() in the clock: a quote, a backslash or a line break would
    \note let the rest of the string be read as configuration lines of its own.
*/
static bool is_wpa_safe(const char *_text)
{
    for(const unsigned char *c = reinterpret_cast<const unsigned char *>(_text); *c; c++)
        if(*c < 0x20 || *c == 0x7f || *c == '"' || *c == '\\')
            return false;
    return true;
}

/*! \brief Letters and digits only, which is what a weatherapi.com key is made of */
static bool is_alnum(const char *_text)
{
    for(const char *c = _text; *c; c++)
        if(!isalnum(static_cast<unsigned char>(*c)))
            return false;
    return true;
}

/*! \brief A host name or an address: letters, digits, dots, dashes and the colons of IPv6 */
static bool is_host(const char *_text)
{
    for(const char *c = _text; *c; c++)
        if(!isalnum(static_cast<unsigned char>(*c)) && *c != '.' && *c != '-' && *c != ':')
            return false;
    return _text[0] != '\0' && _text[0] != '-';
}

/*! \brief Tells whether a name is a timezone installed on this system, such as America/Sao_Paulo */
static bool is_timezone(const char *_name)
{
    if(_name[0] == '\0' || _name[0] == '/' || strstr(_name, "..") != nullptr)
        return false;
    for(const char *c = _name; *c; c++)
        if(!isalnum(static_cast<unsigned char>(*c)) && strchr("_+-/", *c) == nullptr)
            return false;

    char path[256];
    snprintf(path, sizeof(path), ZONEINFO_DIR "%s", _name);
    struct stat st{};
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

// ------------------------------------------------------------------ system

/*! \brief Runs a program without a shell and waits for it
    \return True when it exited with status zero.
    \note No shell on purpose: some of the arguments come from the web page.
*/
static bool run(const char *const _argv[])
{
    fflush(stdout);
    const pid_t pid = fork();
    if(pid < 0)
        return false;
    if(pid == 0)
    {
        //whatever the program prints must not end up in the HTTP response
        const int devnull = open("/dev/null", O_WRONLY);
        if(devnull >= 0)
        {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
        }
        execv(_argv[0], const_cast<char *const *>(_argv));
        _exit(127);
    }
    int status = 0;
    while(waitpid(pid, &status, 0) < 0)
        if(errno != EINTR)
            return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/*! \brief The zone /etc/localtime points at, such as America/Sao_Paulo */
static bool current_timezone(char *_out, size_t _size)
{
    char target[256];
    const ssize_t n = readlink("/etc/localtime", target, sizeof(target) - 1);
    if(n <= 0)
        return false;
    target[n] = 0;
    const char *zone = strstr(target, "zoneinfo/");
    if(zone == nullptr)
        return false;
    snprintf(_out, _size, "%s", zone + strlen("zoneinfo/"));
    return true;
}

/*! \brief Hands the NTP servers to systemd-timesyncd
    \return False when the drop-in could not be written or the service would not restart.
    \note The clock reads the system time and nothing else, so this file is the only place where
    \note the servers on the page ever had any effect. Left alone when it already says the same.
*/
static bool apply_ntp_servers(const char _servers[4][64])
{
    char content[512];
    int used = snprintf(content, sizeof(content),
                        "# Written by the clock's web page (nixie.cgi). Edits here are overwritten.\n"
                        "[Time]\nNTP=");
    for(int i = 0; i < 4; i++)
        if(_servers[i][0] != '\0')
            used += snprintf(content + used, sizeof(content) - used, "%s ", _servers[i]);
    content[used - 1] = '\n';   //the trailing space becomes the end of the line

    if(FILE *f = fopen(TIMESYNCD_DROPIN, "r"))
    {
        char current[512] = {0};
        const size_t n = fread(current, 1, sizeof(current) - 1, f);
        fclose(f);
        current[n] = 0;
        if(strcmp(current, content) == 0)
            return true;
    }

    if(mkdir(TIMESYNCD_DROPIN_DIR, 0755) != 0 && errno != EEXIST)
        return false;
    FILE *f = fopen(TIMESYNCD_DROPIN ".tmp", "w");
    if(f == nullptr)
        return false;
    const bool written = fputs(content, f) >= 0;
    if(fclose(f) != 0 || !written || rename(TIMESYNCD_DROPIN ".tmp", TIMESYNCD_DROPIN) != 0)
        return false;

    const char *const restart[] = {SYSTEMCTL, "restart", "systemd-timesyncd", nullptr};
    if(run(restart))
        return true;
    //gone, so the next save does not find it identical and skip the restart that never happened
    unlink(TIMESYNCD_DROPIN);
    return false;
}

// ------------------------------------------------------------------ forms

static bool process_maps()
{
    double latitude = 0, longitude = 0;
    char apikey[64];
    if(!read_double_field("latitude", "Latitude", -90, 90, &latitude) ||
       !read_double_field("longitude", "Longitude", -180, 180, &longitude) ||
       !read_text_field("apikey", "API key", 0, 63, apikey, sizeof(apikey)))
        return false;
    if(!is_alnum(apikey))
        return fail("API key: letters and digits only.");

    cJSON *j = ensure_object(mJson, "maps");
    set_number(j, "latitude", latitude);
    set_number(j, "longitude", longitude);

    //The page never receives the stored key, so an empty box means "keep it". Removing the key,
    //which also switches the forecast off, takes the checkbox.
    if(strcmp(get_field_value("apikey_clear"), "1") == 0)
        set_string(j, "apikey", "");
    else if(apikey[0] != '\0')
        set_string(j, "apikey", apikey);
    else if(cJSON_GetObjectItemCaseSensitive(j, "apikey") == nullptr)
        set_string(j, "apikey", "");

    queue_signal("nixie", SIG_CGI_RELOAD);
    return true;
}

static bool process_wifi()
{
    char ssid[33], password[64], confirm[64];
    if(!read_text_field("wifiSSID", "Network name", 1, 32, ssid, sizeof(ssid)) ||
       !read_text_field("wifiPassword", "Password", 8, 63, password, sizeof(password)) ||
       !read_text_field("confirmPassword", "Password confirmation", 8, 63, confirm, sizeof(confirm)))
        return false;
    if(strcmp(password, confirm) != 0)
        return fail("Passwords do not match.");
    if(!is_wpa_safe(ssid) || !is_wpa_safe(password))
        return fail("Quotes, backslashes and line breaks are not allowed in the network name or password.");

    sWifiConfig current{};
    load_wifi_config(&current);
    if(strcmp(current.ssid, ssid) == 0 && strcmp(current.password, password) == 0)
        return fail("Nothing changed: this network and password are already stored.");

    cJSON *j = ensure_object(mJson, "wifi");
    set_string(j, "ssid", ssid);
    set_string(j, "password", password);

    queue_signal("nixie", SIG_CGI_WIFI);
    say("Configuration saved. The clock will restart and look for network %s.", ssid);
    return true;
}

static bool process_ntp()
{
    char timezone[64];
    if(!read_text_field("timezone", "Time zone", 1, 63, timezone, sizeof(timezone)))
        return false;
    if(!is_timezone(timezone))
        return fail("Unknown time zone: %s.", timezone);

    char servers[4][64];
    int count = 0;
    for(int i = 0; i < 4; i++)
    {
        char field[24], label[32];
        snprintf(field, sizeof(field), "server%d", i);
        snprintf(label, sizeof(label), "Servidor %d", i + 1);
        if(!read_text_field(field, label, 0, 63, servers[i], sizeof(servers[i])))
            return false;
        if(servers[i][0] == '\0')
            continue;
        if(!is_host(servers[i]))
            return fail("%s: letters, digits, dots and dashes only.", label);
        count++;
    }
    if(count == 0)
        return fail("Enter at least one NTP server.");

    char current[64] = "";
    current_timezone(current, sizeof(current));
    if(strcmp(current, timezone) != 0)
    {
        const char *const argv[] = {TIMEDATECTL, "set-timezone", timezone, nullptr};
        if(!run(argv))
            return fail("Could not change the time zone to %s.", timezone);
    }
    if(!apply_ntp_servers(servers))
        return fail("Could not configure the servers in systemd-timesyncd.");

    cJSON *j = ensure_object(mJson, "ntp");
    for(int i = 0; i < 4; i++)
    {
        char key[24];
        snprintf(key, sizeof(key), "server_%d", i);
        set_string(j, key, servers[i]);
    }
    //never read by anything: the timezone lives in the system, and nothing ever used the format
    cJSON_DeleteItemFromObjectCaseSensitive(j, "zone");
    cJSON_DeleteItemFromObjectCaseSensitive(j, "formato");

    //the clock rereads /etc/localtime on reload
    queue_signal("nixie", SIG_CGI_RELOAD);
    return true;
}

static bool process_detection()
{
    const char *model = get_field_value("model");
    if(strcmp(model, "face") != 0 && strcmp(model, "motion") != 0 && strcmp(model, "both") != 0)
        return fail("Choose face, motion or both.");
    char model_copy[16];
    snprintf(model_copy, sizeof(model_copy), "%s", model);

    enumFaceCascade cascade = FACE_CASCADE_LBP_IMPROVED;
    if(!face_cascade_from_name(get_field_value("face_cascade"), &cascade))
        return fail("Choose one of the face detectors in the list.");

    int on_timeout = 0, motion_timeout = 0, threshold = 0, face_size_min = 0, face_size_max = 0, fps = 0;
    int face_min_neighbors = 0, face_min_consecutive = 0;
    double face_scale_factor = 0;
    if(!read_int_field("on_timeout", "Display time", 5, 3600, &on_timeout) ||
       !read_int_field("motion_timeout", "Display time after motion", 5, 3600, &motion_timeout) ||
       !read_int_field("threshold", "Motion threshold", 1, 255, &threshold) ||
       !read_double_field("face_scale_factor", "Face: scale factor", 1.05, 2.0, &face_scale_factor) ||
       !read_int_field("face_min_neighbors", "Face: minimum neighbors", 1, 10, &face_min_neighbors) ||
       !read_int_field("face_min_consecutive", "Face: consecutive frames", 1, 10, &face_min_consecutive) ||
       !read_int_field("face_size_min", "Face: minimum size", 1, 480, &face_size_min) ||
       !read_int_field("face_size_max", "Face: maximum size", 1, 480, &face_size_max) ||
       !read_int_field("fps", "Frames per second", 1, 30, &fps))
        return false;
    if(face_size_min > face_size_max)
        return fail("The minimum face size cannot exceed the maximum.");
    //on_timeout is the ceiling on any stretch the tubes stay lit, so a longer value would be
    //cut short without saying so
    if(motion_timeout > on_timeout)
        return fail("The display time after motion cannot exceed the display time.");

    cJSON *j = ensure_object(mJson, "detection");
    set_string(j, "model", model_copy);
    set_number(j, "on_timeout", on_timeout);
    set_number(j, "motion_timeout", motion_timeout);
    set_number(j, "threshold", threshold);
    set_string(j, "face_cascade", face_cascade_name(cascade));
    set_number(j, "face_scale_factor", face_scale_factor);
    set_number(j, "face_min_neighbors", face_min_neighbors);
    set_number(j, "face_min_consecutive", face_min_consecutive);
    set_number(j, "face_size_min", face_size_min);
    set_number(j, "face_size_max", face_size_max);
    set_number(j, "fps", fps);
    //on the page for years, read by nothing: the camera never looked at any of the three
    cJSON_DeleteItemFromObjectCaseSensitive(j, "blur");
    cJSON_DeleteItemFromObjectCaseSensitive(j, "percentage");
    cJSON_DeleteItemFromObjectCaseSensitive(j, "maxpercent");

    queue_signal("nixie", SIG_CGI_RELOAD);     //on_timeout, motion_timeout
    queue_signal("camera", SIG_CGI_RELOAD);    //everything else
    return true;
}

/*! \brief Saves brightness and bargraph limits
    \note One signal per value that changed. A single one used to carry whichever was checked
    \note last, so changing both bargraph ends at once only ever applied one of them.
*/
static bool process_hardware()
{
    int vu_min = 0, vu_max = 0, brightness = 0;
    if(!read_int_field("brightness", "Brightness", 1, 100, &brightness) ||
       !read_int_field("vu_min", "Bargraph minimum", 0, 255, &vu_min) ||
       !read_int_field("vu_max", "Bargraph maximum", 0, 255, &vu_max))
        return false;
    if(vu_min >= vu_max)
        return fail("The bargraph minimum must be lower than the maximum.");

    sNixieConfig current{};
    load_nixie_config(&current);

    cJSON *j = ensure_object(mJson, "hardware");
    set_number(j, "brightness", brightness);
    set_number(j, "vu_min", vu_min);
    set_number(j, "vu_max", vu_max);

    //the last one sent is the one left on the tubes, and the bargraph is what is being calibrated
    if(current.brightness != brightness)
        queue_signal("nixie", SIG_CGI_BRIGHNESS);
    if(current.vu_max != vu_max)
        queue_signal("nixie", SIG_CGI_VU_MAX);
    if(current.vu_min != vu_min)
        queue_signal("nixie", SIG_CGI_VU_MIN);
    return true;
}

/*! \brief Saves the daytime window and the automatic regeneration settings
    \note load_schedule_config() and load_regen_config() check the same ranges again on the way
    \note in: a day window that makes no sense would leave the clock dark for good.
*/
static bool process_schedule()
{
    int use_astro = 0, day_start = 0, day_end = 0, gate_half_hour = 0;
    int regen_enabled = 0, regen_count = 0, regen_cycles = 0;
    if(!read_int_field("use_astro", "How to tell it is daytime", 0, 1, &use_astro) ||
       !read_int_field("day_start", "First hour", 0, 23, &day_start) ||
       !read_int_field("day_end", "Last hour", 0, 23, &day_end) ||
       !read_int_field("gate_half_hour", "Date on the half hour", 0, 1, &gate_half_hour) ||
       !read_int_field("regen_enabled", "Automatic regeneration", 0, 1, &regen_enabled) ||
       !read_int_field("regen_count", "Sessions per day", 1, REGEN_SESSIONS_MAX, &regen_count) ||
       !read_int_field("regen_cycles", "Sweeps per session", 1, 60, &regen_cycles))
        return false;
    if(day_start > day_end)
        return fail("The first hour of the day cannot come after the last one.");

    //only the first regen_count boxes count: the page keeps the others, hidden, for later
    int minutes[REGEN_SESSIONS_MAX];
    for(int i = 0; i < regen_count; i++)
    {
        char field[24];
        snprintf(field, sizeof(field), "regen_time_%d", i);
        if(!parse_hhmm(get_field_value(field), &minutes[i]))
            return fail("Session %d time: use HH:MM, from 00:00 to 23:59.", i + 1);
        for(int k = 0; k < i; k++)
            if(minutes[k] == minutes[i])
                return fail("Two sessions at the same time: %02d:%02d.", minutes[i] / 60, minutes[i] % 60);
    }
    std::sort(minutes, minutes + regen_count);

    cJSON *schedule = ensure_object(mJson, "schedule");
    cJSON *regen = ensure_object(mJson, "regen");
    if(schedule == nullptr || regen == nullptr)
        return fail("An error occurred");

    set_bool  (schedule, "use_astro",      use_astro != 0);
    set_number(schedule, "day_start",      day_start);
    set_number(schedule, "day_end",        day_end);
    set_bool  (schedule, "gate_half_hour", gate_half_hour != 0);

    cJSON *times = cJSON_CreateArray();
    for(int i = 0; i < regen_count; i++)
    {
        char text[16];
        snprintf(text, sizeof(text), "%02d:%02d", minutes[i] / 60, minutes[i] % 60);
        cJSON_AddItemToArray(times, cJSON_CreateString(text));
    }
    set_bool  (regen, "enabled", regen_enabled != 0);
    put       (regen, "times",   times);
    set_number(regen, "cycles",  regen_cycles);
    //the old format: a first hour plus a count, one hour apart. "times" replaces both.
    cJSON_DeleteItemFromObjectCaseSensitive(regen, "hour");
    cJSON_DeleteItemFromObjectCaseSensitive(regen, "sessions");

    queue_signal("nixie", SIG_CGI_RELOAD);
    return true;
}

/*! \brief Starts or stops the cathode regeneration routine
    \note The tube travels in the low nibble of SIG_CGI_REGEN_ON.
    \note Nothing is written to the config file: the routine is a transient maintenance mode.
*/
static void process_regen()
{
    const char *action = get_field_value("action");
    if(strcmp(action, "stop") == 0)
    {
        queue_signal("nixie", SIG_CGI_REGEN_OFF);
        say("Regeneration stopped");
        return;
    }
    if(strcmp(action, "start") != 0)
    {
        say("Unknown action");
        return;
    }

    int tube = 0;
    if(!read_int_field("tube", "Tube", REGEN_TUBE_MIN, REGEN_TUBE_MAX, &tube))
        return;
    queue_signal("nixie", SIG_CGI_REGEN_ON | tube);
    say("Regeneration started on tube %d", tube);
}

static void process_Log_file()
{
    FILE *f = fopen("/tmp/nixie.txt", "r");
    if (!f) {
        say("Could not open the log");
        return;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), f))
        printf("%s", buffer);
    fclose(f);
    gMessage[0] = 0;
}

// ------------------------------------------------------------------ GET

/*! \brief What the camera reported about its face search, or null when it has said nothing
    \note Written by the camera at every start and reload (FACE_STATUS_FILE). It is the only way
    \note the page can know the frame the driver really delivers and the sizes really searched.
*/
static cJSON *read_face_status()
{
    FILE *f = fopen(FACE_STATUS_FILE, "r");
    if(f == nullptr)
        return nullptr;
    char text[2048];
    const size_t n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[n] = 0;
    cJSON *status = cJSON_Parse(text);
    if(!cJSON_IsObject(status))
    {
        cJSON_Delete(status);
        return nullptr;
    }
    return status;
}

static const char *model_name(enumCameraDetectModel _model)
{
    switch(_model)
    {
        case CAM_DETECT_FACE:   return "face";
        case CAM_DETECT_MOTION: return "motion";
        case CAM_DETECT_NONE:   return "none";
        default:                return "both";
    }
}

/*! \brief Prints the configuration the page fills its forms from
    \note Built from the loaders, so a block missing from an old file comes out with the same
    \note defaults the clock is running with. The Wi-Fi password and the weather key never leave:
    \note the page only learns whether a key is set. nixie.json itself is not served (lighttpd.conf).
*/
static void print_config()
{
    sWifiConfig wifi{};         load_wifi_config(&wifi);
    sLocation location{};       load_location(&location);
    sNtpSettings ntp{};         load_ntp_config(&ntp);
    sCameraConfig camera{};     load_camera_config(&camera);
    sNixieConfig hardware{};    load_nixie_config(&hardware);
    sScheduleConfig schedule{}; load_schedule_config(&schedule);
    sRegenConfig regen{};       load_regen_config(&regen);

    cJSON *root = cJSON_CreateObject();

    cJSON *j = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddStringToObject(j, "ssid", wifi.ssid);

    j = cJSON_AddObjectToObject(root, "maps");
    cJSON_AddNumberToObject(j, "latitude", location.latitude);
    cJSON_AddNumberToObject(j, "longitude", location.longitude);
    cJSON_AddBoolToObject(j, "apikey_set", location.apikey[0] != '\0');

    j = cJSON_AddObjectToObject(root, "ntp");
    char timezone[64] = "";
    current_timezone(timezone, sizeof(timezone));
    cJSON_AddStringToObject(j, "timezone", timezone);
    for(int i = 0; i < 4; i++)
    {
        char key[24];
        snprintf(key, sizeof(key), "server_%d", i);
        cJSON_AddStringToObject(j, key, ntp.ntp_server[i]);
    }

    j = cJSON_AddObjectToObject(root, "detection");
    cJSON_AddStringToObject(j, "model", model_name(camera.detection_model));
    cJSON_AddNumberToObject(j, "on_timeout", camera.on_timeout);
    cJSON_AddNumberToObject(j, "motion_timeout", camera.motion_timeout);
    cJSON_AddNumberToObject(j, "threshold", camera.threshold);
    cJSON_AddStringToObject(j, "face_cascade", face_cascade_name(camera.faceCascade));
    cJSON_AddNumberToObject(j, "face_scale_factor", camera.faceScaleFactor);
    cJSON_AddNumberToObject(j, "face_min_neighbors", camera.faceMinNeighbors);
    cJSON_AddNumberToObject(j, "face_min_consecutive", camera.faceMinConsecutive);
    cJSON_AddNumberToObject(j, "face_size_min", camera.faceSizeMin);
    cJSON_AddNumberToObject(j, "face_size_max", camera.faceSizeMax);
    cJSON_AddNumberToObject(j, "fps", camera.fps);
    if(cJSON *status = read_face_status())
        cJSON_AddItemToObject(j, "status", status);

    j = cJSON_AddObjectToObject(root, "hardware");
    cJSON_AddNumberToObject(j, "brightness", hardware.brightness);
    cJSON_AddNumberToObject(j, "vu_min", hardware.vu_min);
    cJSON_AddNumberToObject(j, "vu_max", hardware.vu_max);

    j = cJSON_AddObjectToObject(root, "schedule");
    cJSON_AddBoolToObject(j, "use_astro", schedule.use_astro);
    cJSON_AddNumberToObject(j, "day_start", schedule.day_start);
    cJSON_AddNumberToObject(j, "day_end", schedule.day_end);
    cJSON_AddBoolToObject(j, "gate_half_hour", schedule.gate_half_hour);

    j = cJSON_AddObjectToObject(root, "regen");
    cJSON_AddBoolToObject(j, "enabled", regen.enabled);
    cJSON_AddNumberToObject(j, "count", regen.count);
    cJSON *times = cJSON_AddArrayToObject(j, "times");
    for(int i = 0; i < regen.count; i++)
    {
        char text[16];
        snprintf(text, sizeof(text), "%02d:%02d", regen.minutes[i] / 60, regen.minutes[i] % 60);
        cJSON_AddItemToArray(times, cJSON_CreateString(text));
    }
    cJSON_AddNumberToObject(j, "cycles", regen.cycles);

    if(char *text = cJSON_PrintUnformatted(root))
    {
        printf("%s\n", text);
        cJSON_free(text);
    }
    cJSON_Delete(root);
}

/*! \brief Prints the timezones this system knows, for the select box on the NTP page */
static void print_timezones()
{
    cJSON *list = cJSON_CreateArray();
    if(FILE *p = popen(TIMEDATECTL " list-timezones", "r"))
    {
        char line[128];
        while(fgets(line, sizeof(line), p))
        {
            line[strcspn(line, "\r\n")] = 0;
            if(line[0] != '\0')
                cJSON_AddItemToArray(list, cJSON_CreateString(line));
        }
        pclose(p);
    }
    //without timedatectl the page still has to be able to show, and keep, the current zone
    char current[64];
    if(cJSON_GetArraySize(list) == 0 && current_timezone(current, sizeof(current)))
        cJSON_AddItemToArray(list, cJSON_CreateString(current));

    if(char *text = cJSON_PrintUnformatted(list))
    {
        printf("%s\n", text);
        cJSON_free(text);
    }
    cJSON_Delete(list);
}

static void process_get()
{
    const char *query = getenv("QUERY_STRING");
    if(query != nullptr && strcmp(query, "get=config") == 0)
    {
        printf("Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n\r\n");
        print_config();
    }
    else if(query != nullptr && strcmp(query, "get=timezones") == 0)
    {
        printf("Content-Type: application/json; charset=utf-8\r\n\r\n");
        print_timezones();
    }
    else
        printf("Status: 400 Bad Request\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nUnknown request\n");
}

// ------------------------------------------------------------------ main

int main() {
    const char *method = getenv("REQUEST_METHOD");
    if(method != nullptr && strcmp(method, "GET") == 0)
    {
        process_get();
        free_jsonfile();
        return 0;
    }

    printf("Content-Type: text/plain; charset=utf-8\r\n\r\n");

    mJson = parse_jsonfile();
    if(mJson == nullptr)
    {
        printf("Could not read the clock configuration\n");
        return 0;
    }
    if(parse_stdin() == nullptr)
    {
        printf("Invalid request\n");
        free_jsonfile();
        return 0;
    }

    bool save = false;
    const char *form = get_form_name();
    if(strcmp(form, "maps") == 0)
        save = process_maps();
    else if(strcmp(form, "hardware") == 0)
        save = process_hardware();
    else if(strcmp(form, "ntp") == 0)
        save = process_ntp();
    else if(strcmp(form, "detection") == 0)
        save = process_detection();
    else if(strcmp(form, "wifi") == 0)
        save = process_wifi();
    else if(strcmp(form, "schedule") == 0)
        save = process_schedule();
    else if(strcmp(form, "regen") == 0)
        process_regen();    //maintenance mode, nothing to save
    else if(strcmp(form, "logs") == 0)
        process_Log_file();
    else
        say("Unknown form");

    if(save)
    {
        if(save_json())
        {
            if(gMessage[0] == '\0')
                say("Configuration saved");
        }
        else
        {
            say("Could not save the configuration.");
            gSignalCount = 0;   //nothing changed on disk, so there is nothing to tell anyone
        }
    }
    free_buffer();
    free_jsonfile();

    if(gMessage[0] != '\0')
        printf("%s\n", gMessage);

    for(int i = 0; i < gSignalCount; i++)
    {
        cSignal target(gSignals[i].process);
        target.Send(NIXIE_SIGNAL, gSignals[i].value);
    }
    return 0;
}

//eof main.cpp
