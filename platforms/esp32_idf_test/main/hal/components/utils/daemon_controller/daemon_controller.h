/**
 * @file daemon_controller.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2024-10-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>

/**
 * @brief Daemon manager that simplifies thread control and data exchange
 *
 */
class DaemonController_t {
protected:
    struct Data_t {
        std::mutex mutex;
        bool time_2_go = false;
        bool is_daemon_exist = false;
    };
    Data_t _data;

public:
    virtual ~DaemonController_t() = default;

    Data_t& Borrow()
    {
        _data.mutex.lock();
        return _data;
    }

    void Return()
    {
        _data.mutex.unlock();
    }

    void SendKillSignal()
    {
        Borrow().time_2_go = true;
        Return();
    }

    bool CheckKillSignal()
    {
        bool ret = Borrow().time_2_go;
        Return();
        return ret;
    }

    void CreateDaemon()
    {
        Borrow().is_daemon_exist = true;
        _data.time_2_go = false;
        Return();
    }

    void DestoryDaemon()
    {
        Borrow().is_daemon_exist = false;
        Return();
    }

    bool IsDaemonExist()
    {
        bool ret = Borrow().is_daemon_exist;
        Return();
        return ret;
    }

    void SendKillSignalAndWait(const TickType_t xTicksToDelay = pdMS_TO_TICKS(20))
    {
        SendKillSignal();
        while (1) {
            vTaskDelay(xTicksToDelay);
            if (!IsDaemonExist())
                break;
        }
    }
};
