/**
 * @file app_launcher.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <mooncake.h>
#include <vector>

/**
 * @brief Launcher, comprising four main pages: watch face, widgets, notifications, and app menu
 *
 */
class AppLauncher : public mooncake::AppAbility {
public:
    AppLauncher();

    // Override lifecycle callbacks
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    int _current_page_index = 1;
    int _new_page_index = 1;
    std::vector<int> _page_id_list;

    void handle_page_change();
    void create_page_list();
    void destroy_page_list();
    void reset_lv_screen();
};
