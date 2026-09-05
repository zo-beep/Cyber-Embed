#ifndef __ANALYZER_UI_H__
#define __ANALYZER_UI_H__

#include "analyzer_types.h"
#include "wireless_analyzer.h"

void analyzer_dashboard();
void analyzer_wifi_section();
void analyzer_ble_section();
void analyzer_subghz_section();
void analyzer_nrf24_section();
void analyzer_overview_section();
void analyzer_live_monitor();
void analyzer_device_explorer();
void analyzer_device_details(const WirelessDevice& dev);
void analyzer_channel_map();
void analyzer_baseline_menu();
void analyzer_compare_view();
void analyzer_report_menu();

#endif // __ANALYZER_UI_H__

