/*! \file json_parser.cpp */
#include "json_parser.h"
#include "structs.h"
#include "defines.h"
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <cstring>

cJSON *gJson = nullptr;
#define JSONFILENAME "/home/pi/www/json/nixie.json"

long get_file_size(const char *filename)
{
    struct stat stat_buf{};
    const int rc = stat(filename, &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

cJSON *parse_jsonfile()
{
    const long filesize = get_file_size(JSONFILENAME);
    if(filesize <= 0)
    {
        fprintf(stderr, "File not found\n");
        return gJson;
    }
    if(FILE *f = fopen(JSONFILENAME, "rb"))
    {
        auto buffer = new char[filesize];
        long sizeread = fread(buffer, 1, filesize, f);
        //the buffer carries no terminator, so the length has to travel with it
        if(sizeread == filesize)
            gJson = cJSON_ParseWithLength(buffer, filesize);
        else
            fprintf(stderr, "Wrong size %ld != %ld\n", sizeread, filesize);
        delete []buffer;
        fclose(f);
    }
    else
        fprintf(stderr, "File not found\n");
    return gJson;
}

void free_jsonfile()
{
    if(gJson)
    {
        cJSON_Delete(gJson);
        gJson = nullptr;
    }
}


/*! \brief Reads an integer from a json object, leaving the default alone when it is missing
    \note Every loader goes through these: a key missing from an older config file, or one edited
    \note by hand, must fall back to the default instead of taking the whole process down.
*/
static void read_int(const cJSON *_json, const char *_key, int *_target)
{
    const cJSON *item = cJSON_GetObjectItem(_json, _key);
    if(cJSON_IsNumber(item))
        *_target = item->valueint;
}

/*! \brief Reads a floating point number from a json object, leaving the default when missing */
static void read_double(const cJSON *_json, const char *_key, double *_target)
{
    const cJSON *item = cJSON_GetObjectItem(_json, _key);
    if(cJSON_IsNumber(item))
        *_target = item->valuedouble;
}

/*! \brief Reads a boolean from a json object, accepting both true/false and 0/1 */
static void read_bool(const cJSON *_json, const char *_key, bool *_target)
{
    const cJSON *item = cJSON_GetObjectItem(_json, _key);
    if(cJSON_IsBool(item))
        *_target = cJSON_IsTrue(item);
    else if(cJSON_IsNumber(item))
        *_target = item->valueint != 0;
}

/*! \brief Copies a string from a json object into a fixed buffer, truncating to fit
    \note Leaves the target alone when the key is missing or is not a string.
*/
static void read_string(const cJSON *_json, const char *_key, char *_target, size_t _size)
{
    const cJSON *item = cJSON_GetObjectItem(_json, _key);
    if(cJSON_IsString(item) && item->valuestring != nullptr)
    {
        strncpy(_target, item->valuestring, _size - 1);
        _target[_size - 1] = 0;
    }
}

/*! \brief Returns the root of the config file, parsing it on first use */
static const cJSON *config_root()
{
    return gJson != nullptr ? gJson : parse_jsonfile();
}

//! The names the config and the web page use for each cascade
static const struct { enumFaceCascade cascade; const char *name; } gCascadeNames[] = {
    {FACE_CASCADE_LBP_IMPROVED, "lbp_improved"},
    {FACE_CASCADE_LBP,          "lbp"},
    {FACE_CASCADE_HAAR,         "haar"},
};

/*! \brief The config name of a cascade, such as "lbp_improved" */
const char *face_cascade_name(enumFaceCascade _cascade)
{
    for(const auto &entry : gCascadeNames)
        if(entry.cascade == _cascade)
            return entry.name;
    return gCascadeNames[0].name;
}

/*! \brief Turns a config name into a cascade
    \return False when the name is not one of the known cascades.
*/
bool face_cascade_from_name(const char *_name, enumFaceCascade *_cascade)
{
    for(const auto &entry : gCascadeNames)
        if(strcmp(entry.name, _name) == 0)
        {
            *_cascade = entry.cascade;
            return true;
        }
    return false;
}

void load_camera_config(sCameraConfig *_cameraconfig)
{
    _cameraconfig->detection_model = CAM_DETECT_BOTH;
    _cameraconfig->threshold = 20;
    _cameraconfig->faceCascade = FACE_CASCADE_LBP_IMPROVED;
    _cameraconfig->faceScaleFactor = FACE_SCALE_FACTOR_DEFAULT;
    _cameraconfig->faceMinNeighbors = FACE_MIN_NEIGHBORS_DEFAULT;
    _cameraconfig->faceMinConsecutive = FACE_MIN_CONSECUTIVE_DEFAULT;
    _cameraconfig->faceSizeMin = 45;
    _cameraconfig->faceSizeMax = 200;   //the frame height: no upper limit in practice
    _cameraconfig->on_timeout = 90;
    _cameraconfig->fps = 15;

    const cJSON *jsonCamera = cJSON_GetObjectItem(config_root(), "detection");
    if(jsonCamera == nullptr)
        return;

    char detection_model[16] = "both";
    read_string(jsonCamera, "model", detection_model, sizeof(detection_model));
    if(strcmp(detection_model, "face") == 0)
        _cameraconfig->detection_model = CAM_DETECT_FACE;
    else if(strcmp(detection_model, "motion") == 0)
        _cameraconfig->detection_model = CAM_DETECT_MOTION;
    else if(strcmp(detection_model, "none") == 0)
        _cameraconfig->detection_model = CAM_DETECT_NONE;
    else
        //Anything unrecognised means both, not nothing. A clock that notices no one is
        //worse than one that works a little harder, and a configuration written before
        //"both" existed must not be read as an instruction to switch the camera off.
        _cameraconfig->detection_model = CAM_DETECT_BOTH;

    char cascade[16] = "";
    read_string(jsonCamera, "face_cascade", cascade, sizeof(cascade));
    face_cascade_from_name(cascade, &_cameraconfig->faceCascade);   //unknown keeps the default

    read_int(jsonCamera, "threshold",     &_cameraconfig->threshold);
    read_double(jsonCamera, "face_scale_factor",  &_cameraconfig->faceScaleFactor);
    read_int(jsonCamera, "face_min_neighbors",    &_cameraconfig->faceMinNeighbors);
    read_int(jsonCamera, "face_min_consecutive",  &_cameraconfig->faceMinConsecutive);
    read_int(jsonCamera, "face_size_min", &_cameraconfig->faceSizeMin);
    read_int(jsonCamera, "face_size_max", &_cameraconfig->faceSizeMax);
    read_int(jsonCamera, "on_timeout",    &_cameraconfig->on_timeout);
    read_int(jsonCamera, "fps",           &_cameraconfig->fps);

    //the frame loop divides by the frame rate
    if(_cameraconfig->fps < 1 || _cameraconfig->fps > 30)
        _cameraconfig->fps = 15;
    //detectMultiScale asserts on a factor of 1 or less, which would take the camera down
    if(!(_cameraconfig->faceScaleFactor >= 1.05 && _cameraconfig->faceScaleFactor <= 2.0))
        _cameraconfig->faceScaleFactor = FACE_SCALE_FACTOR_DEFAULT;
    if(_cameraconfig->faceMinNeighbors < 1 || _cameraconfig->faceMinNeighbors > 10)
        _cameraconfig->faceMinNeighbors = FACE_MIN_NEIGHBORS_DEFAULT;
    if(_cameraconfig->faceMinConsecutive < 1 || _cameraconfig->faceMinConsecutive > 10)
        _cameraconfig->faceMinConsecutive = FACE_MIN_CONSECUTIVE_DEFAULT;
}

void load_nixie_config(sNixieConfig *_nixieconfig)
{
    _nixieconfig->vu_min = 24;
    _nixieconfig->vu_max = 62;
    _nixieconfig->brightness = 100;

    if(const cJSON *jsonNixie = cJSON_GetObjectItem(config_root(), "hardware"); jsonNixie != nullptr)
    {
        read_int(jsonNixie, "vu_min",     &_nixieconfig->vu_min);
        read_int(jsonNixie, "vu_max",     &_nixieconfig->vu_max);
        read_int(jsonNixie, "brightness", &_nixieconfig->brightness);
    }
}

void load_ntp_config(sNtpSettings *_ntpconfig)
{
    for(int i = 0; i < 4; i++)
        snprintf(_ntpconfig->ntp_server[i], sizeof(_ntpconfig->ntp_server[i]), "%d.pool.ntp.org", i);

    if(const cJSON *jsonNtp = cJSON_GetObjectItem(config_root(), "ntp"); jsonNtp != nullptr)
    {
        for(int i = 0; i < 4; i++)
        {
            char key[24];
            snprintf(key, sizeof(key), "server_%d", i);
            read_string(jsonNtp, key, _ntpconfig->ntp_server[i], sizeof(_ntpconfig->ntp_server[i]));
        }
    }
}

void load_location(sLocation *_location)
{
    //Zero/zero de proposito: sem um "maps" no arquivo nao ha coordenada nenhuma
    //para adivinhar, e qualquer default util seria a casa de alguem. Com apikey
    //vazia tambem nao sai requisicao, entao isso nunca chega a virar uma consulta.
    _location->latitude = 0.0;
    _location->longitude = 0.0;
    _location->apikey[0] = 0;

    if(const cJSON *jsonLocation = cJSON_GetObjectItem(config_root(), "maps"); jsonLocation != nullptr)
    {
        read_double(jsonLocation, "latitude",  &_location->latitude);
        read_double(jsonLocation, "longitude", &_location->longitude);
        read_string(jsonLocation, "apikey", _location->apikey, sizeof(_location->apikey));
    }
}

void load_wifi_config(sWifiConfig *_wificonfig)
{
    _wificonfig->ssid[0] = 0;
    _wificonfig->password[0] = 0;

    if(const cJSON *jsonWifi = cJSON_GetObjectItem(config_root(), "wifi"); jsonWifi != nullptr)
    {
        read_string(jsonWifi, "ssid",     _wificonfig->ssid,     sizeof(_wificonfig->ssid));
        read_string(jsonWifi, "password", _wificonfig->password, sizeof(_wificonfig->password));
    }
}

void load_schedule_config(sScheduleConfig *_schedule)
{
    const cJSON *json = config_root();

    _schedule->use_astro      = true;
    _schedule->day_start      = DAY_START_DEFAULT;
    _schedule->day_end        = DAY_END_DEFAULT;
    _schedule->gate_half_hour = true;

    if(json != nullptr)
    {
        if(const cJSON *j = cJSON_GetObjectItem(json, "schedule"); j != nullptr)
        {
            read_bool(j, "use_astro",      &_schedule->use_astro);
            read_int (j, "day_start",      &_schedule->day_start);
            read_int (j, "day_end",        &_schedule->day_end);
            read_bool(j, "gate_half_hour", &_schedule->gate_half_hour);
        }
    }

    //a window that makes no sense would silence the clock for good, so it never survives the load
    if(_schedule->day_start < 0 || _schedule->day_start > 23 ||
       _schedule->day_end   < 0 || _schedule->day_end   > 23 ||
       _schedule->day_start > _schedule->day_end)
    {
        _schedule->day_start = DAY_START_DEFAULT;
        _schedule->day_end   = DAY_END_DEFAULT;
    }
}

/*! \brief Reads a time of day written as HH:MM
    \param _text The text, such as "02:30". Exactly two digits, a colon and two digits.
    \param _minutes Receives the minutes past midnight, 0 to 1439.
    \return False when the text is not a valid time of day.
*/
bool parse_hhmm(const char *_text, int *_minutes)
{
    if(_text == nullptr || strlen(_text) != 5 || _text[2] != ':')
        return false;
    for(int i : {0, 1, 3, 4})
        if(_text[i] < '0' || _text[i] > '9')
            return false;
    const int hour   = (_text[0] - '0') * 10 + (_text[1] - '0');
    const int minute = (_text[3] - '0') * 10 + (_text[4] - '0');
    if(hour > 23 || minute > 59)
        return false;
    *_minutes = hour * 60 + minute;
    return true;
}

/*! \brief Loads the automatic regeneration schedule
    \note Sessions used to be a first hour plus a count, one hour apart. A file in that format is
    \note read into the same list of times the page now writes, so an old clock keeps its schedule.
*/
void load_regen_config(sRegenConfig *_regen)
{
    const cJSON *json = config_root();

    int hour     = REGEN_NIGHT_HOUR_DEFAULT;
    int sessions = REGEN_NIGHT_SESSIONS;
    _regen->enabled = true;
    _regen->count   = 0;
    _regen->cycles  = REGEN_NIGHT_CYCLES;

    if(const cJSON *j = cJSON_GetObjectItem(json, "regen"); j != nullptr)
    {
        read_bool(j, "enabled", &_regen->enabled);
        read_int (j, "cycles",  &_regen->cycles);

        //a time that does not parse, or repeats an earlier one, is dropped rather than guessed at
        const cJSON *time = nullptr;
        cJSON_ArrayForEach(time, cJSON_GetObjectItem(j, "times"))
        {
            int minutes = 0;
            if(_regen->count >= REGEN_SESSIONS_MAX || !cJSON_IsString(time) || !parse_hhmm(time->valuestring, &minutes))
                continue;
            bool repeated = false;
            for(int i = 0; i < _regen->count; i++)
                repeated = repeated || _regen->minutes[i] == minutes;
            if(!repeated)
                _regen->minutes[_regen->count++] = minutes;
        }

        read_int(j, "hour",     &hour);
        read_int(j, "sessions", &sessions);
    }

    //no usable list: the old first hour plus count, or the defaults when there is neither
    if(_regen->count == 0)
    {
        if(hour < 0 || hour > 23)
            hour = REGEN_NIGHT_HOUR_DEFAULT;
        if(sessions < 1 || sessions > REGEN_SESSIONS_MAX)
            sessions = REGEN_NIGHT_SESSIONS;
        for(int i = 0; i < sessions; i++)
            _regen->minutes[_regen->count++] = ((hour + i) % 24) * 60;
    }
    std::sort(_regen->minutes, _regen->minutes + _regen->count);

    if(_regen->cycles < 1 || _regen->cycles > 60)
        _regen->cycles = REGEN_NIGHT_CYCLES;
}

/*! \brief Writes the config back to disk
    \return False when nothing was written.
    \note Written to a temporary file and renamed over the real one: a power cut in the middle
    \note of the write leaves the old config in place instead of half of the new one.
*/
bool save_json()
{
    if(gJson == nullptr)
        return false;

    char *text = cJSON_Print(gJson);
    if(text == nullptr)
        return false;

    const char *tmpname = JSONFILENAME ".tmp";
    bool ok = false;
    if(FILE *f = fopen(tmpname, "wb"))
    {
        //the CGI runs as root; the file must stay pi's, or nobody can edit it by hand afterwards
        struct stat st{};
        if(stat(JSONFILENAME, &st) == 0)
        {
            if(fchown(fileno(f), st.st_uid, st.st_gid) != 0) {}
            if(fchmod(fileno(f), st.st_mode & 07777) != 0) {}
        }
        ok = fprintf(f, "%s\n", text) > 0;
        ok = (fflush(f) == 0) && ok;
        ok = (fsync(fileno(f)) == 0) && ok;
        ok = (fclose(f) == 0) && ok;
        ok = ok && rename(tmpname, JSONFILENAME) == 0;
        if(!ok)
            remove(tmpname);
    }
    cJSON_free(text);
    return ok;
}
//eof json_parser.cpp