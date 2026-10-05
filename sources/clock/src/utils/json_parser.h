/*! \file json_parser.h */
#pragma once
#include "cJSON.h"
#include "structs.h"

cJSON *parse_jsonfile();
void free_jsonfile();
bool save_json();

void load_camera_config(sCameraConfig *_cameraconfig);
void load_nixie_config(sNixieConfig *_nixieconfig);
void load_ntp_config(sNtpSettings *_ntpconfig);
void load_location(sLocation *_location);
void load_wifi_config(sWifiConfig *_wificonfig);
void load_schedule_config(sScheduleConfig *_schedule);
void load_regen_config(sRegenConfig *_regen);
bool parse_hhmm(const char *_text, int *_minutes);
const char *face_cascade_name(enumFaceCascade _cascade);
bool face_cascade_from_name(const char *_name, enumFaceCascade *_cascade);
//eof json_parser.h