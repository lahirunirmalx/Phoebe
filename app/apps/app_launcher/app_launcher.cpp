/**
 * @file app_launcher.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "app_launcher.h"
#include "view/page.h"
#include <mooncake.h>
#include <memory>
#include <mooncake_log.h>
#include <hal/hal.h>
#include <lvgl.h>

using namespace mooncake;

#define PAGE_INDEX_NOTIFICATION 0
#define PAGE_INDEX_WATCH_FACE   1
#define PAGE_INDEX_WIDGETS      2
#define PAGE_INDEX_APP_LIST     3

#define _tag (getAppInfo().name)

AppLauncher::AppLauncher()
{
    setAppInfo().name = "AppLauncher";
}

void AppLauncher::onCreate()
{
    mclog::tagInfo(_tag, "on create");
    open();
}

void AppLauncher::onOpen()
{
    mclog::tagInfo(_tag, "on open");
    reset_lv_screen();
    create_page_list();
}

void AppLauncher::onRunning()
{
    // Refresh button states
    HAL::BtnUpdate();
    handle_page_change();
}

void AppLauncher::onClose()
{
    mclog::tagInfo(_tag, "on close");
    destroy_page_list();
}

void AppLauncher::handle_page_change()
{
    auto current_page_ability =
        GetMooncake().getExtensionInstance<LauncherPageBase>(_page_id_list[_current_page_index]);

    if (current_page_ability->isOnSubPage()) {
        return;
    }

    _new_page_index = _current_page_index;

    // Go up
    if (HAL::BtnUp().wasClicked()) {
        _new_page_index--;
        if (_new_page_index < 0) {
            _new_page_index = 0;
            // TODO feedback
        }
    }

    // Go down
    else if (HAL::BtnDown().wasClicked()) {
        _new_page_index++;
        if (_new_page_index >= _page_id_list.size()) {
            _new_page_index = _page_id_list.size() - 1;
            // TODO feedback
        }
    }

    // Go home (watch face)
    if (HAL::BtnPower().wasClicked()) {
        _new_page_index = PAGE_INDEX_WATCH_FACE;
    }

    // Go sub page
    else if (HAL::BtnOk().wasClicked()) {
        current_page_ability->enterSubPage();
    }

    // If page change
    if (_new_page_index != _current_page_index) {
        // mclog::info("{}", _new_page_index);

        // Hide current
        GetMooncake().extensionManager()->hideUIAbility(_page_id_list[_current_page_index]);
        // Show new
        GetMooncake().extensionManager()->showUIAbility(_page_id_list[_new_page_index]);
        // Update index
        _current_page_index = _new_page_index;
    }

    if (HAL::BtnPower().wasHold()) {
        HAL::SysCtrl().powerOff();
    }
}

void AppLauncher::create_page_list()
{
    if (!_page_id_list.empty()) {
        destroy_page_list();
    }

    // Create page instances
    _page_id_list.resize(4);
    _page_id_list[PAGE_INDEX_NOTIFICATION] =
        GetMooncake().createExtension(std::make_unique<LauncherPageNotification>());
    _page_id_list[PAGE_INDEX_WATCH_FACE] = GetMooncake().createExtension(std::make_unique<LauncherPageWatchFace>());
    _page_id_list[PAGE_INDEX_WIDGETS] = GetMooncake().createExtension(std::make_unique<LauncherPageWidgets>());
    _page_id_list[PAGE_INDEX_APP_LIST] = GetMooncake().createExtension(std::make_unique<LauncherPageAppList>());

    GetMooncake().extensionManager()->showUIAbility(_page_id_list[1]);
}

void AppLauncher::destroy_page_list()
{
    for (auto& page_ability_id : _page_id_list) {
        GetMooncake().destroyExtension(page_ability_id);
    }
    _page_id_list.clear();
}

void AppLauncher::reset_lv_screen()
{
    lv_obj_clean(lv_screen_active());
    // lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xAFAFAC), 0);
    lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);
}
