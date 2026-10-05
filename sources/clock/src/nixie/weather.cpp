/*! \file weather.cpp */
#include "weather.h"
#include <cstring>
#include <cstdio>
#include <ctime>
#include <curl/curl.h>
#include <sstream>
#include <cmath>
#include "../logger/logger.h"

/*! \brief A write callback for CURL to retrieve the data
    *  \param contents Pointer to the data received from the server.
    *  \param size Size of each element in the data.
    *  \param nmemb Number of elements in the data.
    *  \param output Pointer to a string where the data will be appended.
    *  \return The total size of the data written.
*/
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *output)
{
    size_t totalSize = size * nmemb;
    output->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

/*! \brief Constructor for cWeatherReport
    *  \param _latitud Latitude for the weather report.
    *  \param _longitud Longitude for the weather report.
    *  \param _apikey API key for the weather service.
    *
    *  \note Initializes the weather report with the given latitude, longitude, and API key.
*/
cWeatherReport::cWeatherReport(double _latitud, double _longitud, const char *_apikey)
{
    strncpy(apikey, _apikey, sizeof(apikey) - 1);
    apikey[sizeof(apikey) - 1] = '\0';
    latitud = _latitud;
    longitud = _longitud;
    forecast = {0, 0, 0};
}

cWeatherReport::~cWeatherReport() = default;

/*! \brief Points the report at another location or key
    *  \return True when something changed, meaning the caller should fetch again.
    *  \note The sunrise and sunset of the old place are dropped: until the next fetch the fixed
    *  \note window is a better guess than another city's sun. The temperature is kept, since a
    *  \note stale reading beats a bargraph sitting on zero.
*/
bool cWeatherReport::SetLocation(double _latitud, double _longitud, const char *_apikey)
{
    if(latitud == _latitud && longitud == _longitud && strncmp(apikey, _apikey, sizeof(apikey)) == 0)
        return false;

    strncpy(apikey, _apikey, sizeof(apikey) - 1);
    apikey[sizeof(apikey) - 1] = '\0';
    latitud = _latitud;
    longitud = _longitud;
    astro = {ASTRO_INVALID, ASTRO_INVALID, 0};
    return true;
}

/*! \brief Get the current weather forecast
    *  \return Pointer to the current weather forecast.
*/
const sTempertaureForecast *cWeatherReport::GetForecast()
{
    return &forecast;
}

/*! \brief Set the RGB color based on cloud percentage
    *  \param cloudPercentage Percentage of cloud cover (0-100).
    *
    *  \note This function calculates the RGB color based on the cloud percentage.
    *  \note It uses a linear interpolation between clear and cloudy colors.
*/
void cWeatherReport::SetcloudToRGB(uint8_t cloudPercentage)
{
    if (cloudPercentage > 100) cloudPercentage = 100;
    float t = cloudPercentage / 100.0f;

    constexpr sRGB colorClear = { 0, 255, 255 };
    constexpr sRGB colorCloudy = { 10, 20, 150 };
    
    forecast.clouds.r = static_cast<uint8_t>(colorClear.r * (1.0f - t) + colorCloudy.r * t);
    forecast.clouds.g = static_cast<uint8_t>(colorClear.g * (1.0f - t) + colorCloudy.g * t);
    forecast.clouds.b = static_cast<uint8_t>(colorClear.b * (1.0f - t) + colorCloudy.b * t);
    glogger->debug("Forecast RGB = [{:d}, {:d}, {:d}]", forecast.clouds.r, forecast.clouds.g, forecast.clouds.b);
}

const sTempertaureForecast *cWeatherReport::UpdateForecast()
{
    std::string response = GetForescastFromAPI();
    if (response.empty())
        return nullptr;

    try
    {
        cJSON *json = cJSON_Parse(response.c_str());
        if(json == nullptr)
        {
            LOGGER_ERROR("Json parsing error");
            return nullptr;
        }

        cJSON *current = cJSON_GetObjectItem(json, "current");
        cJSON *forecastday = cJSON_GetObjectItem(json, "forecast");
        forecastday = cJSON_GetObjectItem(forecastday, "forecastday");
        forecastday = cJSON_GetArrayItem(forecastday, 0);
        cJSON *day = cJSON_GetObjectItem(forecastday, "day");

        UpdateValues(current, day);

        cJSON_Delete(json);
        glogger->debug("Forecast: Min: {:d}°C, Max: {:d}°C, Current: {:d}°C. Clouds: {:d}%", 
            forecast.minimum, forecast.maximum, forecast.current, forecast.cloudPercentage);
        return &forecast;
    }
    catch (const std::exception &e)
    {
        glogger->error("Weather: Error processing JSON: {:s}", e.what());
        return nullptr;
    }
    
    return nullptr;
}

/*! \brief Performs a GET and hands back the body
    *  \param _url The address to fetch.
    *  \return The response body, or an empty string when the request failed.
    *  \note Shared by the forecast and the astronomy calls, which hit different endpoints of the
    *  \note same service and so need exactly the same handle setup.
*/
static std::string http_get(const std::string &_url)
{
    CURL *curl = curl_easy_init();
    std::string response;

    if (curl == nullptr)
    {
        LOGGER_ERROR("Error initializing curl");
        return "";
    }

    curl_easy_setopt(curl, CURLOPT_URL, _url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    //the pi has no business hanging on a dead link: the caller just keeps the previous values
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        LOGGER_ERROR("Error during request");
        return "";
    }
    return response;
}

/*! \brief Fetches the weather forecast from the API
    *  \return A string containing the JSON response from the weather API.
    *  \note It returns an empty string if there is an error during the request.
*/
std::string cWeatherReport::GetForescastFromAPI()
{
    //No key configured is a normal state now that it comes from nixie.json rather than from the
    //binary: a clock that has never been through the web page has none. Spending a request and
    //twenty seconds of timeout to be told so by the service helps nobody.
    if(apikey[0] == 0)
    {
        LOGGER_ERROR("No weather API key configured -- set it in the web page, under Localizacao");
        return "";
    }

    std::ostringstream url;
    url << "https://api.weatherapi.com/v1/forecast.json?key=" << apikey
        << "&q=" << latitud << "," << longitud << "&days=1&aqi=no&alerts=no";

    return http_get(url.str());
}

uint16_t astro_to_minutes(const char *_txt)
{
    int hour = 0, minute = 0;
    char meridiem[3] = {0};

    if(_txt == nullptr)
        return ASTRO_INVALID;
    if(sscanf(_txt, "%d:%d %2s", &hour, &minute, meridiem) != 3)
        return ASTRO_INVALID;
    if(hour < 1 || hour > 12 || minute < 0 || minute > 59)
        return ASTRO_INVALID;

    //12 AM is midnight and 12 PM is noon, so the twelve folds to zero before the PM offset
    if(hour == 12)
        hour = 0;
    if(meridiem[0] == 'P' || meridiem[0] == 'p')
        hour += 12;

    return static_cast<uint16_t>(hour * 60 + minute);
}

/*! \brief Warns when our clock disagrees with the local time the service reports
    *  \param _json The parsed response, which carries location.localtime.
    *  \note The sunrise and sunset we store are in the local time of the configured coordinates,
    *  \note and they get compared against localtime(). Nothing in this project ever applies the
    *  \note timezone from the config file, so that comparison only holds while the operating
    *  \note system timezone is right. This catches it when it is not, and catches coordinates
    *  \note left in the wrong hemisphere too.
*/
void cWeatherReport::CheckClockAgainstApi(const cJSON *_json) const
{
    const cJSON *location = cJSON_GetObjectItem(_json, "location");
    const cJSON *txt = cJSON_GetObjectItem(location, "localtime");
    if(!cJSON_IsString(txt) || txt->valuestring == nullptr)
        return;

    int year = 0, month = 0, day = 0, hour = 0, minute = 0;
    if(sscanf(txt->valuestring, "%d-%d-%d %d:%d", &year, &month, &day, &hour, &minute) != 5)
        return;

    const time_t now = time(nullptr);
    const struct tm *tm = localtime(&now);
    if(tm == nullptr)
        return;

    int drift = (hour * 60 + minute) - (tm->tm_hour * 60 + tm->tm_min);
    //the two readings can sit on opposite sides of midnight, so fold into +/- twelve hours
    if(drift >  720) drift -= 1440;
    if(drift < -720) drift += 1440;

    if(drift > ASTRO_CLOCK_DRIFT_WARN || drift < -ASTRO_CLOCK_DRIFT_WARN)
        glogger->warn("Astro: our clock is {:d} min off the service local time "
                      "(we say {:02d}:{:02d}, service says {:02d}:{:02d}). "
                      "Check the timezone on the pi and the configured coordinates.",
                      drift, tm->tm_hour, tm->tm_min, hour, minute);
}

/*! \brief Fetches sunrise and sunset for today and stores them
    *  \return The stored astro data, or nullptr when nothing usable came back.
    *  \note Uses astronomy.json rather than the astro block of forecast.json, which this key
    *  \note does not receive. Once a day is plenty: the times move about a minute a day.
*/
const sAstro *cWeatherReport::UpdateAstro()
{
    if(apikey[0] == 0)
        return nullptr;     //see GetForescastFromAPI(); the caller falls back to the fixed hours

    std::ostringstream url;
    url << "https://api.weatherapi.com/v1/astronomy.json?key=" << apikey
        << "&q=" << latitud << "," << longitud;

    const std::string response = http_get(url.str());
    if(response.empty())
        return nullptr;

    cJSON *json = cJSON_Parse(response.c_str());
    if(json == nullptr)
    {
        LOGGER_ERROR("Astro: json parsing error");
        return nullptr;
    }

    const cJSON *node = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "astronomy"), "astro");
    const cJSON *sunrise = cJSON_GetObjectItem(node, "sunrise");
    const cJSON *sunset  = cJSON_GetObjectItem(node, "sunset");

    if(cJSON_IsString(sunrise) && cJSON_IsString(sunset))
    {
        const uint16_t rise = astro_to_minutes(sunrise->valuestring);
        const uint16_t set  = astro_to_minutes(sunset->valuestring);

        if(rise != ASTRO_INVALID && set != ASTRO_INVALID && rise < set)
        {
            astro.sunrise = rise;
            astro.sunset  = set;
            astro.updated = time(nullptr);
            glogger->debug("Astro: sunrise {:02d}:{:02d}, sunset {:02d}:{:02d}",
                           rise / 60, rise % 60, set / 60, set % 60);
        }
        else
            //"Polar Day" and "Polar Night" come through here, and so does anything unreadable.
            //The previous values are left alone: they age out on their own if nothing replaces them.
            glogger->debug("Astro: no usable sunrise/sunset ('{:s}' / '{:s}')",
                           sunrise->valuestring, sunset->valuestring);
    }
    else
        LOGGER_ERROR("Astro: response carried no astro block");

    CheckClockAgainstApi(json);
    cJSON_Delete(json);
    return GetAstro();
}

/*! \brief Hands back the stored sunrise and sunset, or nullptr when they cannot be trusted
    *  \note Stale data still beats the fixed fallback window by a wide margin: the sun moves
    *  \note about a minute a day, while the fixed window runs up to an hour and a half off. So
    *  \note the data is only dropped after a long outage, when the season itself has moved on.
*/
const sAstro *cWeatherReport::GetAstro() const
{
    if(astro.updated == 0 || astro.sunrise == ASTRO_INVALID || astro.sunset == ASTRO_INVALID)
        return nullptr;

    //a negative age means the clock stepped backwards, which is just the first ntp sync after
    //a boot. The data itself is still good, so only an age that runs forward disqualifies it.
    if(difftime(time(nullptr), astro.updated) > ASTRO_MAX_AGE_HOURS * 3600.0)
        return nullptr;

    return &astro;
}

/*! \brief Converts a temperature in Celsius to the uint8_t used by the forecast struct
    *  \param _celsius The temperature in degrees Celsius.
    *  \return The temperature clamped to the 0..99 range.
    *  \note The Nixie display has no minus sign and only two tubes per temperature field,
    *  \note so negative values are capped at zero instead of wrapping around uint8_t.
*/
static uint8_t temperature_to_u8(double _celsius)
{
    const double rounded = round(_celsius);
    if(rounded < 0.0)  return 0;
    if(rounded > 99.0) return 99;
    return static_cast<uint8_t>(rounded);
}

/*! \brief  Updates the forecast values
    *  \param current Pointer to the current weather data in JSON format.
    *  \param day Pointer to the forecast day data in JSON format.
    *
    *  \note This function extracts temperature and cloud data from the JSON objects
    *  \note and updates the forecast structure accordingly.
    *  \note Temperatures are clamped to 0..99 by temperature_to_u8().
*/
void cWeatherReport::UpdateValues(cJSON *current, cJSON *day)
{
    if (current == nullptr || day == nullptr)
        return;

    cJSON *temp_c = cJSON_GetObjectItem(current, "temp_c");
    cJSON *clouds = cJSON_GetObjectItem(current, "cloud");
    cJSON *maxtemp_c = cJSON_GetObjectItem(day, "maxtemp_c");
    cJSON *mintemp_c = cJSON_GetObjectItem(day, "mintemp_c");

    if (temp_c && maxtemp_c && mintemp_c && clouds) {
        forecast.current = temperature_to_u8(temp_c->valuedouble);
        uint8_t cloud = static_cast<uint8_t>(round(clouds->valuedouble));
        forecast.cloudPercentage = cloud;
        SetcloudToRGB(cloud);
        forecast.maximum = temperature_to_u8(maxtemp_c->valuedouble);
        forecast.minimum = temperature_to_u8(mintemp_c->valuedouble);
    }
}