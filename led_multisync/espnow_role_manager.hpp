#ifndef ESPNOW_ROLE_MANAGER
#define ESPNOW_ROLE_MANAGER

#include <functional>


class EspNowRoleManager {
private:
    std::function<void(bool, bool)> _callback;
    bool _master;
    bool _slave;
    bool _master_subscribed;

public:
    EspNowRoleManager(std::function<void(bool, bool)>&& callback, bool master, bool slave) : _callback(std::move(callback)), _master(master), _slave(slave) {
    }

    void set_slave() {
        if (!this->is_slave()) {
            this->_slave = true;
            this->_master = false;
            this->_callback(this->_master, this->_slave);
            // roleChangeRequested = true;
            // newMasterRole = false;
        }
    }

    void set_master() {
        if (!this->is_master()) {
            this->_slave = false;
            this->_master = true;
            this->_callback(this->_master, this->_slave);
            // roleChangeRequested = true;
            // newMasterRole = true;
        }
    }

    bool is_slave() {
        return this->_slave;
    }

    bool is_master() {
        return this->_master;
    }

    void set_cloud_connected() {
        this->_master_subscribed = true;
    }

    bool is_cloud_connected() {
        return this->_master_subscribed;
    }
};

#endif