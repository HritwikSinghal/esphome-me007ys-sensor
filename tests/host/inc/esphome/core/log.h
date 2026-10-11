#pragma once
#include <cstdio>
#define ESP_LOGD(tag, ...) do { if (esphome_log_on) { printf("  [D] "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define ESP_LOGW(tag, ...) do { if (esphome_log_on) { printf("  [W] "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define ESP_LOGCONFIG(tag, ...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
extern bool esphome_log_on;
