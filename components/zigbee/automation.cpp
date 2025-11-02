#include "automation.h"
#include <algorithm>
#include "esphome/core/log.h"

namespace esphome {
namespace zigbee {

/**
 * @brief sRGB to R'G'B' gamma correction as outlined in
 *        https://en.wikipedia.org/wiki/SRGB
 *
 * @param linear a component value in sRGB color space
 * @return float the corresponding component value in R'G'B' color space
 */
inline float gamma_correct(float linear) {
  if(linear < 0.0031308f) {
    return linear * 12.92f;
  } else {
    return (1.055f) * pow(linear, (1.0f / 2.4f)) - 0.055f;
  }
}

/* The following conversions operate on xyY with Y = 1.0, hence Y
   doesn't appear in the formulas. The coefficients originate from
   https://en.wikipedia.org/wiki/SRGB#Primaries
 */
float get_r_from_xy(float x, float y) {
  float z = 1.0f - x - y;
  float X = x / y;
  float Z = z / y;
  float r = X * 3.2406 - 1.5372f - Z * 0.4986f;
  return std::clamp(gamma_correct(r), 0.0f, 1.0f);
}

float get_g_from_xy(float x, float y) {
  float z = 1.0f - x - y;
  float X = x / y;
  float Z = z / y;
  float g = -X * 0.9689f + 1.8758f + Z * 0.0415f;
  return std::clamp(gamma_correct(g), 0.0f, 1.0f);
}

float get_b_from_xy(float x, float y) {
  float z = 1.0f - x - y;
  float X = x / y;
  float Z = z / y;

  float b = X * 0.0557f - 0.2040f + Z * 1.0570f;
  return std::clamp(gamma_correct(b), 0.0f, 1.0f);
}

#ifdef USE_LIGHT
void set_light_color(uint8_t ep, light::LightCall *call, uint16_t value, bool is_x) {
  static std::map<uint8_t, float> x;
  static std::map<uint8_t, float> y;
  if (is_x) {
    x[ep] = (float) value / 65536;
  } else {
    y[ep] = (float) value / 65536;
  }
  ESP_LOGD(TAG, "Set color, x: %f, y: %f", x[ep], y[ep]);
  call->set_rgb(get_r_from_xy(x[ep], y[ep]), get_g_from_xy(x[ep], y[ep]), get_b_from_xy(x[ep], y[ep]));
}
#endif

}  // namespace zigbee
}  // namespace esphome
