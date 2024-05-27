#ifndef ESPNOW_ROLE_MANAGER
#define ESPNOW_ROLE_MANAGER
/*
Singleton Class
*/

#include <functional>


class EspNowRoleManager {
private:
    std::function<void(bool, bool)> _callback;
    bool _master;
    bool _slave;
    bool _master_subscribed;
    bool _should_update;
    bool update_required;
    EspNowRoleManager(std::function<void(bool, bool)>&& callback, bool master, bool slave) {
    }
    EspNowRoleManager(const EspNowRoleManager& role_manager) = delete;
    EspNowRoleManager& operator=(const EspNowRoleManager& role_manager) = delete;

public:
    static EspNowRoleManager& get_instance() {
        static EspNowRoleManager instance(nullptr, false, false);
        return instance;
    }

    static void init(std::function<void(bool, bool)>&& callback, bool master, bool slave) {
        EspNowRoleManager& instance = get_instance();
        instance._callback = std::move(callback);
        instance._master = master;
        instance._slave = slave;
    }

    void set_slave() {
        if (!is_slave()) {
            _slave = true;
            _master = false;
            _callback(_master, _slave);
            // roleChangeRequested = true;
            // newMasterRole = false;
        }
    }

    void set_master() {
        if (!is_master()) {
            _slave = false;
            _master = true;
            _callback(_master, _slave);
            // roleChangeRequested = true;
            // newMasterRole = true;
        }
    }

    bool is_slave() {
        return _slave;
    }

    bool is_master() {
        return _master;
    }

    void set_cloud_connected() {
        _master_subscribed = true;
    }

    bool is_cloud_connected() {
        return _master_subscribed;
    }

    void set_update_required(bool state) {
        _should_update = state;
    }

    bool is_update_required() {
        return _should_update;
    }
};

#endif