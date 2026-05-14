/**
 * @file page.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include "../../utils/smooth_widget/smooth_widget.h"
#include <mooncake.h>
#include <memory>
#include <vector>

/**
 * @brief Launcher page base class; extends UIAbility with a sub-page state so sub-pages can take over button input events
 *
 */
class LauncherPageBase : public mooncake::UIAbility {
public:
    virtual void onEnterSubPage() {}
    virtual void onQuitSubPage() {}

    bool isOnSubPage()
    {
        return _is_on_sub_page;
    }

    void enterSubPage()
    {
        _is_on_sub_page = true;
        onEnterSubPage();
    }

    void quitSubPage()
    {
        _is_on_sub_page = false;
        onQuitSubPage();
    }

protected:
    bool _is_on_sub_page = false;
};

class LauncherPageWatchFace : public LauncherPageBase {
public:
    void onCreate() override;
    void onShow() override;
    void onForeground() override;
    void onBackground() override;
    void onHide() override;
    void onDestroy() override;

private:
    std::unique_ptr<smooth_widget::SmoothWidgetBase> _canvas;
    int _watch_face_ability_id = -1;
};

class LauncherPageNotification : public LauncherPageBase {
public:
    void onCreate() override;
    void onShow() override;
    void onForeground() override;
    void onBackground() override;
    void onHide() override;
    // void onDestroy() override {}

private:
    std::vector<std::unique_ptr<smooth_widget::SmoothWidgetBase>> _canvas_list;
};

class LauncherPageWidgets : public LauncherPageBase {
public:
    void onCreate() override;
    void onShow() override;
    void onForeground() override;
    void onBackground() override;
    void onHide() override;
    // void onDestroy() override {}
    void onEnterSubPage() override;
    void onQuitSubPage() override;

private:
    std::vector<std::unique_ptr<smooth_widget::SmoothWidgetBase>> _canvas_list;
    std::unique_ptr<smooth_widget::SmoothWidgetMouse> _mouse;
    int _widget_a_ability_id = -1;
    int _widget_b_ability_id = -1;
    bool _need_reload = false;

    void handle_show_widget_canvas();
    void handle_hide_widget_canvas();
    void handle_sub_page_input();
    void handle_create_launcher_widget(int canvasIndex);
    void handle_destroy_launcher_widget(int canvasIndex);
    void handle_config_widget_type();
};

class LauncherPageAppList : public LauncherPageBase {
public:
    // void onCreate() override {}
    void onShow() override;
    void onForeground() override;
    void onBackground() override;
    void onHide() override;
    // void onDestroy() override {}

private:
    std::unique_ptr<smooth_widget::SmoothWidgetBase> _canvas;
};
