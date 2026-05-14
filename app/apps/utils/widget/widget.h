/**
 * @file widget.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-25
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <cstdint>
#include <lvgl.h>
#include <memory>
#include <vector>
#include <functional>

namespace widget_helper {

lv_align_t get_lv_align_by_string(const char* align);
lv_color_t get_lv_color_by_string(const char* hexColor);
const lv_font_t* get_lv_font_by_string(const char* fontName);

} // namespace widget_helper

namespace widget {

/**
 * @brief Widget type
 *
 */
namespace WidgetType {
enum Type_t {
    Base = 0,
    Label,
    Img,
    Clock,
};
}

namespace InputEventType {
enum Type_t {
    None = 0,
    Hover,
    MouseLeave,
    Click,
};
}

class WidgetBase;

struct InputEvent_t {
    InputEventType::Type_t type = InputEventType::None;
    WidgetBase* target = nullptr;
};

/**
 * @brief Base widget
 *
 */
class WidgetBase {
public:
    WidgetBase() = default;
    WidgetBase(lv_obj_t* parent);
    virtual ~WidgetBase();

    // "lv_align_default" | "lv_align_top_left" | "lv_align_top_mid" | "lv_align_top_right"
    // "lv_align_bottom_left" | "lv_align_bottom_mid" | "lv_align_bottom_right"
    // "lv_align_left_mid" | "lv_align_right_mid" | "lv_align_center"
    void setAlign(const char* alignment);
    void setBgColor(const char* hexColor);
    void setPos(int32_t x, int32_t y);
    void setSize(int32_t w, int32_t h);
    void setScrollbarMode(lv_scrollbar_mode_t mode);
    void setRadius(int32_t value);
    void setBorderWidth(int32_t value);
    void setBorderColor(const char* hexColor);
    void setRotation(int32_t value);
    void setPadding(int32_t top, int32_t bottom, int32_t left, int32_t right);
    void setOutlineWidth(int32_t value);
    void setOutlineColor(const char* hexColor);
    void moveBackground();
    void moveForeground();
    void setHidden(bool hidden);
    virtual int32_t getX();
    virtual int32_t getX2();
    virtual int32_t getY();
    virtual int32_t getY2();
    virtual int32_t getWidth();
    virtual int32_t getHeight();

    lv_obj_t* get()
    {
        return _lv_obj;
    }

    void set(lv_obj_t* lvObj)
    {
        _lv_obj = lvObj;
    }

    virtual WidgetType::Type_t type()
    {
        return WidgetType::Base;
    }

    std::function<void(InputEvent_t)> onHover;
    std::function<void(InputEvent_t)> onMouseLeave;
    std::function<void(InputEvent_t)> onClick;

    void triggerInputEvent(InputEventType::Type_t type);

protected:
    lv_obj_t* _lv_obj = NULL;
};

/**
 * @brief Label widget
 *
 */
class WidgetLabel : public WidgetBase {
public:
    WidgetLabel() = default;
    WidgetLabel(lv_obj_t* parent);

    // "RajdhaniBold16" | "RajdhaniBold24" | "RajdhaniBold36" | "RajdhaniBold48"
    // "RajdhaniBold64" | "RajdhaniBold72" | "RajdhaniBold96" | "RajdhaniBold144"
    // "Zpix12"
    void setFont(const char* fontName);
    void setTextColor(const char* hexColor);
    void setText(const char* text);

    WidgetType::Type_t type() override
    {
        return WidgetType::Label;
    }
};

/**
 * @brief Image widget
 *
 */
class WidgetImg : public WidgetBase {
public:
    WidgetImg() = default;
    WidgetImg(lv_obj_t* parent) {}

    void setSrc(const char* imageSrc);

    WidgetType::Type_t type() override
    {
        return WidgetType::Img;
    }
};

/**
 * @brief Clock widget
 *
 */
class WidgetClock : public WidgetBase {
public:
    WidgetClock() = default;
    WidgetClock(lv_obj_t* parent);
    ~WidgetClock();

    int centerX = 50;
    int centerY = 50;
    int hourHandWidth = 5;
    int hourHandLength = 26;
    int minHandWidth = 4;
    int minHandLength = 40;
    int secHandWidth = 2;
    int secHandLength = 60;
    lv_color_t handColor = lv_color_black();

    // {
    //   "centerX": 50,
    //   "centerY": 50,
    //   "hourHandWidth": 5,
    //   "hourHandLength": 26,
    //   "minHandWidth": 4,
    //   "minHandLength": 40,
    //   "secHandWidth": 2,
    //   "secHandLength": 60,
    //   "handColor": "#000000"
    // }
    bool setStyle(const char* styleJson);
    void update();

    WidgetType::Type_t type() override
    {
        return WidgetType::Clock;
    }

private:
    lv_obj_t* _hour_hand = NULL;
    lv_obj_t* _minute_hand = NULL;
    lv_obj_t* _second_hand = NULL;
    lv_point_precise_t _hour_points[2];
    lv_point_precise_t _minute_points[2];
    lv_point_precise_t _second_points[2];
};

class WidgetMouse : public WidgetBase {
public:
    WidgetMouse() = default;
    WidgetMouse(lv_obj_t* parent);

    void addTarget(WidgetBase* targetWidget);
    void clearAllTargets();
    void show();
    void hide();
    void goNext();
    void goLast();
    void goTo(WidgetBase* targetWidget);
    void click();

    int getCurrentTargetIndex();
    WidgetBase* getCurrentTargetWidget();

    virtual void onShow();
    virtual void onHide();
    virtual void onGoTo(WidgetBase* targetWidget);

    bool goInLoop = true;

protected:
    int _current_target_index = -1;
    std::vector<WidgetBase*> _target_widget_list;
};

/**
 * @brief Widget factory
 *
 */
class WidgetFactory {
public:
    /**
     * @brief Create a widget and return its ID
     *
     * @param widgetType "base" | "label" | "img" | "clock"
     * @return int
     */
    int create(const char* widgetType);

    /**
     * @brief Destroy a widget
     *
     * @param widgetId widget ID
     */
    void destory(int widgetId);

    // Get the widget instance; returns nullptr if no widget matches the ID or the type is wrong
    WidgetBase* getBase(int widgetId);
    WidgetLabel* getLabel(int widgetId);
    WidgetImg* getImg(int widgetId);
    WidgetClock* getClock(int widgetId);

    /**
     * @brief Set the widget parent object
     *
     * @param widgetParent
     */
    void setWidgetParent(lv_obj_t* widgetParent)
    {
        _widget_parent = widgetParent;
    }

private:
    struct WidgetInfo_t {
        int id = -114514;
        std::unique_ptr<WidgetBase> widget;
    };

    std::vector<WidgetInfo_t> _widget_list;
    std::vector<int> _available_widget_id_list;
    int _next_widget_id = 0;
    lv_obj_t* _widget_parent = NULL;

    int get_next_widget_id();
};

} // namespace widget
