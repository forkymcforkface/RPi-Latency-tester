#include <stdio.h>
#include <gpiod.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <float.h> // For DBL_MAX

// --- CONFIGURATION ---
#define GPIO_CHIP_NAME "gpiochip0"
#define BUTTON_START_PIN 21
#define PHOTODIODE_END_PIN 20
#define TARGET_BTN_CODE BTN_SOUTH

// --- Auto-Detection Functions ---
void get_pi_model(char* buffer, size_t size) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp == NULL) {
        strncpy(buffer, "Unknown", size);
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "Model") || strstr(line, "Hardware")) {
            char *model_ptr = strchr(line, ':');
            if (model_ptr && *(model_ptr + 1) != '\0') {
                model_ptr += 2; // Skip ': '
                strncpy(buffer, model_ptr, size);
                buffer[strcspn(buffer, "\n")] = 0;
                fclose(fp);
                return;
            }
        }
    }
    fclose(fp);
    strncpy(buffer, "Unknown", size);
}

char* find_joystick_device() {
    FILE *fp;
    char line[256];
    char event_handler[32] = {0};
    bool is_joystick = false;
    static char device_path[64];

    fp = fopen("/proc/bus/input/devices", "r");
    if (fp == NULL) {
        perror("Failed to open /proc/bus/input/devices");
        return NULL;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '\n') {
            if (is_joystick && strlen(event_handler) > 0) {
                sprintf(device_path, "/dev/input/%s", event_handler);
                fclose(fp);
                return device_path;
            }
            is_joystick = false;
            memset(event_handler, 0, sizeof(event_handler));
            continue;
        }

        if (strncmp(line, "N: Name=", 8) == 0) {
            if (strstr(line, "Joystick") || strstr(line, "Gamepad")) {
                is_joystick = true;
            }
        } else if (strncmp(line, "H: Handlers=", 12) == 0) {
            if (strstr(line, "js")) {
                is_joystick = true;
            }
            char *ptr = strstr(line, "event");
            if (ptr) {
                sscanf(ptr, "%s", event_handler);
            }
        }
    }

    if (is_joystick && strlen(event_handler) > 0) {
        sprintf(device_path, "/dev/input/%s", event_handler);
        fclose(fp);
        return device_path;
    }

    fclose(fp);
    return NULL;
}

// Helper function to get a nanosecond timestamp
long long get_nanoseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <core_name>\n", argv[0]);
        fprintf(stderr, "Example: %s snes9x\n", argv[0]);
        return 1;
    }
    const char* core_name = argv[1];

    char pi_model[256];
    get_pi_model(pi_model, sizeof(pi_model));

    struct gpiod_chip *chip;
    struct gpiod_line *start_line, *end_line;
    struct input_event ev;
    int ev_fd;
    long long t0 = 0, t1 = 0, t2 = 0;
    char* evdev_device_path;

    // --- Statistics Variables ---
    int test_count = 0;
    double sum_hw = 0, min_hw = DBL_MAX, max_hw = 0;
    double sum_sw = 0, min_sw = DBL_MAX, max_sw = 0;
    double sum_total = 0, min_total = DBL_MAX, max_total = 0;


    evdev_device_path = find_joystick_device();
    if (evdev_device_path == NULL) {
        fprintf(stderr, "Could not find a Joystick or Gamepad device.\n");
        return 1;
    }
    printf("Detected controller at: %s\n", evdev_device_path);
    printf("Detected Pi Model: %s\n", pi_model);

    chip = gpiod_chip_open_by_name(GPIO_CHIP_NAME);
    if (!chip) {
        perror("Failed to open GPIO chip");
        return 1;
    }

    start_line = gpiod_chip_get_line(chip, BUTTON_START_PIN);
    end_line = gpiod_chip_get_line(chip, PHOTODIODE_END_PIN);
    if (!start_line || !end_line) {
        fprintf(stderr, "Failed to get GPIO lines\n");
        gpiod_chip_close(chip);
        return 1;
    }

    if (gpiod_line_request_rising_edge_events(start_line, "latency-tester") < 0 ||
        gpiod_line_request_rising_edge_events(end_line, "latency-tester") < 0) {
        fprintf(stderr, "Failed to request GPIO line events\n");
        gpiod_chip_close(chip);
        return 1;
    }

    ev_fd = open(evdev_device_path, O_RDONLY | O_NONBLOCK);
    if (ev_fd == -1) {
        fprintf(stderr, "Failed to open evdev device %s\n", evdev_device_path);
        gpiod_chip_close(chip);
        return 1;
    }

    printf("Latency Tester Initialized for core: %s\n", core_name);
    printf("Watching for button press on GPIO %d...\n\n", BUTTON_START_PIN);

    while (1) {
        t0 = 0; t1 = 0; t2 = 0;

        gpiod_line_event_wait(start_line, NULL);
        t0 = get_nanoseconds();

        struct gpiod_line_event gpio_ev;
        gpiod_line_event_read(start_line, &gpio_ev);

        printf("--- Test %d Started for %s ---\n", test_count + 1, core_name);
        printf("T0 (Physical Press): Captured\n");

        struct pollfd fds[2];
        fds[0].fd = ev_fd;
        fds[0].events = POLLIN;
        fds[1].fd = gpiod_line_event_get_fd(end_line);
        fds[1].events = POLLIN;

        int events_captured = 0;
        while (events_captured < 2) {
            int ret = poll(fds, 2, 2000); // 2-second timeout
            if (ret < 0) {
                perror("Poll error");
                break;
            } else if (ret == 0) {
                printf("Timeout waiting for events. Resetting.\n");
                break;
            }

            if (fds[0].revents & POLLIN && t1 == 0) {
                while (read(ev_fd, &ev, sizeof(ev)) > 0) {
                    if (ev.type == EV_KEY && ev.code == TARGET_BTN_CODE && ev.value == 1) {
                        t1 = get_nanoseconds();
                        printf("T1 (OS Event):       Captured\n");
                        events_captured++;
                    }
                }
            }

            if (fds[1].revents & POLLIN && t2 == 0) {
                gpiod_line_event_read(end_line, &gpio_ev);
                t2 = get_nanoseconds();
                printf("T2 (Photon Output):  Captured\n");
                events_captured++;
            }
        }

        if (t0 > 0 && t1 > 0 && t2 > 0) {
            test_count++;
            double hardware_ms = (t1 - t0) / 1000000.0;
            double software_ms = (t2 - t1) / 1000000.0;
            double total_ms = (t2 - t0) / 1000000.0;

            // Update stats
            sum_hw += hardware_ms;
            if (hardware_ms < min_hw) min_hw = hardware_ms;
            if (hardware_ms > max_hw) max_hw = hardware_ms;

            sum_sw += software_ms;
            if (software_ms < min_sw) min_sw = software_ms;
            if (software_ms > max_sw) max_sw = software_ms;

            sum_total += total_ms;
            if (total_ms < min_total) min_total = total_ms;
            if (total_ms > max_total) max_total = total_ms;


            printf("\n--- Results for Current Test ---\n");
            printf("Controller + Kernel Latency: %.3f ms\n", hardware_ms);
            printf("Software + Core Latency:     %.3f ms\n", software_ms);
            printf("Total End-to-End Latency:    %.3f ms\n", total_ms);

            printf("\n--- Session Statistics (Total Tests: %d) ---\n", test_count);
            printf("Category\t\tMin\t\tMax\t\tAverage\n");
            printf("------------------------------------------------------------------\n");
            printf("Controller+Kernel\t%.3f ms\t%.3f ms\t%.3f ms\n", min_hw, max_hw, sum_hw / test_count);
            printf("Software+Core\t\t%.3f ms\t%.3f ms\t%.3f ms\n", min_sw, max_sw, sum_sw / test_count);
            printf("Total End-to-End\t%.3f ms\t%.3f ms\t%.3f ms\n", min_total, max_total, sum_total / test_count);
            printf("------------------------------------------------------------------\n\n");
        } else {
            printf("Failed to capture all events. Resetting.\n\n");
        }
    }

    gpiod_line_release(start_line);
    gpiod_line_release(end_line);
    gpiod_chip_close(chip);
    close(ev_fd);
    return 0;
}
