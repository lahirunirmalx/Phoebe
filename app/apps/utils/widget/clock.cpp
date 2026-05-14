/**
 * @file clock.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-26
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "widget.h"
#include <cmath>
#include <ArduinoJson.h>

using namespace widget;
using namespace widget_helper;

WidgetClock::WidgetClock(lv_obj_t* parent)
{
    // _lv_obj = lv_canvas_create(parent);
    // lv_obj_null_on_delete(&_lv_obj);

    _hour_hand = lv_line_create(parent);
    _minute_hand = lv_line_create(parent);
    _second_hand = lv_line_create(parent);
    lv_obj_null_on_delete(&_hour_hand);
    lv_obj_null_on_delete(&_minute_hand);
    lv_obj_null_on_delete(&_second_hand);

    lv_obj_set_style_line_rounded(_hour_hand, false, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(_minute_hand, false, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(_second_hand, false, LV_PART_MAIN);
}

WidgetClock::~WidgetClock()
{
    if (_hour_hand != NULL) {
        lv_obj_delete(_hour_hand);
    }
    if (_minute_hand != NULL) {
        lv_obj_delete(_minute_hand);
    }
    if (_second_hand != NULL) {
        lv_obj_delete(_second_hand);
    }
}

void WidgetClock::update()
{
    // Get the current time
    time_t now;
    struct tm* tm_info;
    time(&now);
    tm_info = localtime(&now);

    int hour = tm_info->tm_hour % 12; // 12-hour format
    int minute = tm_info->tm_min;
    int second = tm_info->tm_sec;

    // Compute hand positions
    float hour_angle = (hour + minute / 60.0) * 30 * (3.14159 / 180);    // 360 / 12 = 30
    float minute_angle = (minute + second / 60.0) * 6 * (3.14159 / 180); // 360 / 60 = 6
    float second_angle = second * 6 * (3.14159 / 180);                   // 360 / 60 = 6

    // Update hour hand
    _hour_points[0] = {centerX, centerY};
    _hour_points[1] = {centerX + (int)(hourHandLength * cos(hour_angle - 1.5708)),
                       centerY + (int)(hourHandLength * sin(hour_angle - 1.5708))};
    lv_line_set_points(_hour_hand, _hour_points, 2);
    lv_obj_set_style_line_width(_hour_hand, hourHandWidth, LV_PART_MAIN);
    lv_obj_set_style_line_color(_hour_hand, handColor, LV_PART_MAIN);

    // Update minute hand
    _minute_points[0] = {centerX, centerY};
    _minute_points[1] = {centerX + (int)(minHandLength * cos(minute_angle - 1.5708)),
                         centerY + (int)(minHandLength * sin(minute_angle - 1.5708))};
    lv_line_set_points(_minute_hand, _minute_points, 2);
    lv_obj_set_style_line_width(_minute_hand, minHandWidth, LV_PART_MAIN);
    lv_obj_set_style_line_color(_minute_hand, handColor, LV_PART_MAIN);

    // Update second hand
    _second_points[0] = {centerX, centerY};
    _second_points[1] = {centerX + (int)(secHandLength * cos(second_angle - 1.5708)),
                         centerY + (int)(secHandLength * sin(second_angle - 1.5708))};
    lv_line_set_points(_second_hand, _second_points, 2);
    lv_obj_set_style_line_width(_second_hand, secHandWidth, LV_PART_MAIN);
    lv_obj_set_style_line_color(_second_hand, handColor, LV_PART_MAIN);
}

bool WidgetClock::setStyle(const char* styleJson)
{
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, styleJson);
    if (error) {
        return false;
    }

    if (!doc["centerX"].isNull()) {
        centerX = doc["centerX"];
    }
    if (!doc["centerY"].isNull()) {
        centerY = doc["centerY"];
    }
    if (!doc["hourHandWidth"].isNull()) {
        hourHandWidth = doc["hourHandWidth"];
    }
    if (!doc["hourHandLength"].isNull()) {
        hourHandLength = doc["hourHandLength"];
    }
    if (!doc["minHandWidth"].isNull()) {
        minHandWidth = doc["minHandWidth"];
    }
    if (!doc["minHandLength"].isNull()) {
        minHandLength = doc["minHandLength"];
    }
    if (!doc["secHandWidth"].isNull()) {
        secHandWidth = doc["secHandWidth"];
    }
    if (!doc["secHandLength"].isNull()) {
        secHandLength = doc["secHandLength"];
    }
    if (!doc["handColor"].isNull()) {
        handColor = get_lv_color_by_string(doc["handColor"].as<std::string>().c_str());
    }

    return true;
}
