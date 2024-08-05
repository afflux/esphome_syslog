#include "syslog_component.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif
/*
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include "esphome/core/version.h"
*/

namespace esphome {
namespace syslog {

static const char *TAG = "syslog";

//https://github.com/arcao/Syslog/blob/master/src/Syslog.h#L37-44
//https://github.com/esphome/esphome/blob/5c86f332b269fd3e4bffcbdf3359a021419effdd/esphome/core/log.h#L19-26
static const uint8_t esphome_to_syslog_log_levels[] = {0, 3, 4, 6, 5, 7, 7, 7};

SyslogComponent::SyslogComponent() {
    this->settings_.client_id = App.get_name();
}

void SyslogComponent::setup() {

    this->server_socklen = socket::set_sockaddr((struct sockaddr *)&this->server, sizeof(this->server),
                                                this->settings_.address, this->settings_.port);
    if (!this->server_socklen) {
        ESP_LOGW(TAG, "Failed to parse server IP address '%s'", this->settings_.address.c_str());
        return;
    }
    this->socket_ = socket::socket(this->server.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (!this->socket_) {
        ESP_LOGW(TAG, "Failed to create UDP socket");
        return;
    }

    this->log(ESPHOME_LOG_LEVEL_INFO , "syslog", "Syslog started");
    ESP_LOGI(TAG, "Started");

    #ifdef USE_LOGGER
    if (logger::global_logger != nullptr) {
        logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
            if(!this->enable_logger || (level > this->settings_.min_log_level)) return;
            if(this->strip_colors) { //Strips the "033[0;xxx" at the beginning and the "#033[0m" at the end of log messages
                std::string org_msg(message);
                this->log(level, tag, org_msg.substr(7, org_msg.size() -7 -4));
            } else {
                this->log(level, tag, message);
            }
        });
    }
    #endif
}

void SyslogComponent::loop() {
}

void SyslogComponent::log(uint8_t level, const std::string &tag, const std::string &payload) {
    level = level > 7 ? 7 : level;

    if (!this->socket_) {
        ESP_LOGW(TAG, "Tried to send \"%s\"@\"%s\" with level %d but socket isn't connected", tag.c_str(), payload.c_str(), level);
        return;
    }

    int pri = esphome_to_syslog_log_levels[level];
    std::string buf = str_sprintf("<%d>1 - %s %s - - - \xEF\xBB\xBF%s",
                                  pri, this->settings_.client_id.c_str(),
                                  tag.c_str(), payload.c_str());
    if (this->socket_->sendto(buf.c_str(), buf.length(), 0, (struct sockaddr *)&this->server, this->server_socklen) < 0) {
        ESP_LOGW(TAG, "Tried to send \"%s\"@\"%s\" with level %d but but failed for an unknown reason", tag.c_str(), payload.c_str(), level);
    }
}

float SyslogComponent::get_setup_priority() const {
    return setup_priority::AFTER_WIFI;
}

}  // namespace syslog
}  // namespace esphome
