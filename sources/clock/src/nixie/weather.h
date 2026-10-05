/*! \file weather.h */
#pragma once
#include <cstdint>
#include <ctime>
#include <string>
#include "../utils/cJSON.h"
#include "../utils/defines.h"

/*** Call example and the returned data:
 * 
 * https://api.weatherapi.com/v1/forecast.json?key=<SUA_CHAVE>&days=1&q=-23.5505,-46.6333
 *
 * A chave sai de weatherapi.com e e gravada em nixie.json, pela aba
 * "Localizacao" do site do relogio -- nunca aqui dentro.
 * 
 {
  "location": {
    "name": "Sao Paulo",
    "region": "Sao Paulo",
    "country": "Brazil",
    "lat": -23.55,
    "lon": -46.63,
    "tz_id": "America/Sao_Paulo",
    "localtime_epoch": 1750346624,
    "localtime": "2025-06-19 12:23"
  },
  "current": {
    "last_updated": "2025-06-19 12:15",
    "temp_c": 26.2,
    "is_day": 1
  },
  "forecast": {
    "forecastday": [
      {
        "date": "2025-06-19",
        "day": {
          "maxtemp_c": 27.9,
          "mintemp_c": 12.5,
          "avgtemp_c": 19.1
        }
      }
    ]
  }
}
 */

/*! \brief keeps the RGB data for the RGB LEDs */
struct sRGB{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

 /*! \brief Forecast data
     \note Tempertures in Celcius, cloud in percentage 
 */
 struct sTempertaureForecast
 {
    uint8_t current;          //!< Current temperature in Celcius
    uint8_t minimum;          //!< Minimum temperature in Celcius
    uint8_t maximum;          //!< Maximum temperature in Celcius
    uint8_t cloudPercentage;  //!< Cloud percentage
    sRGB    clouds;           //!< RGB color for the clouds
    
 };

/*! \brief Sunrise and sunset of the current day, in minutes past local midnight
    \note The forecast.json call this project makes comes back with the astro block stripped: the
    \note api key has field filtering turned on in the account, which is also why the hourly array
    \note only carries wet bulb temperatures. The times below come from astronomy.json instead,
    \note a separate endpoint that the same key reaches with the full payload.
*/
struct sAstro
{
    uint16_t sunrise;   //!< ASTRO_INVALID when the service gave no usable time
    uint16_t sunset;    //!< likewise; "Polar Day" and "Polar Night" land here
    time_t   updated;   //!< when it was fetched, zero meaning never
};

/*! \brief Turns a weatherapi "hh:mm AM" string into minutes past midnight
    \param _txt The string as the service writes it, for instance "05:48 AM".
    \return Minutes 0 to 1439, or ASTRO_INVALID when the string is not a clock time.
    \note Above the arctic circles the service answers "Polar Day" or "Polar Night" instead of a
    \note time, and the coordinates are editable from the web page, so that has to be survivable.
*/
uint16_t astro_to_minutes(const char *_txt);

/*! \class cWeatherReport
  *  \brief A class to fetch and manage weather reports.
  *
  *  \note This class uses the WeatherAPI to fetch weather data based on latitude and longitude.
  *  \note It provides methods to update and retrieve the current weather forecast.
*/
class cWeatherReport
{
    protected:
        char apikey[64]{};
        double latitud, longitud;
    sTempertaureForecast forecast{};
    sAstro astro{ASTRO_INVALID, ASTRO_INVALID, 0};
    void SetcloudToRGB(uint8_t cloudPercentage);
    void UpdateValues(cJSON *current, cJSON *day);
    void CheckClockAgainstApi(const cJSON *_json) const;
    std::string GetForescastFromAPI();
    public:
        cWeatherReport(double _latitud, double _longitud, const char *_apikey);
        ~cWeatherReport();

        bool SetLocation(double _latitud, double _longitud, const char *_apikey);

        const sTempertaureForecast *UpdateForecast();
        const sTempertaureForecast *GetForecast();

        const sAstro *UpdateAstro();
        const sAstro *GetAstro() const;
};
