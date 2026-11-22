
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "pico_lfs_safe.h"
#include "ssd1306.h"


//Red And Green LEDs and buzzer
#define RED_LED 10
#define GREEN_LED 11
#define BUZZER 13

// Keypad pins
const uint keypad_rows[4] = {6, 7, 8, 9};
const uint keypad_cols[3] = {3, 4, 5};

// OLED Display pins
#define OLED_CS 20
#define OLED_DC 16
#define OLED_RST 17
#define OLED_SCK 18
#define OLED_MOSI 19

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

// Keypad button
const char keys[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

// OLED display 
ssd1306_t display;

// Manager PIN
const char manager_pin[7] = "272665";


// Storage for the Safe!!!
// This information is saved in LittleFS.
// It will still be here after the Pico restarts.

#define SAFE_FILE "safe.dat"
#define SAFE_MAGIC 0x53414645

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t locked;
    uint8_t pin_set;
    uint8_t manager_only;
    char user_pin[7];
} safe_data_t;

safe_data_t safe_data;

// Count incorrect PIN attempts
int failed_attempts = 0;

//program states

typedef enum {
    WAIT_SETUP,
    SET_PIN,
    WAIT_LOCKED,
    ENTER_PIN,
    UNLOCKED,
    WAIT_MANAGER,
    ENTER_MANAGER
} safe_state_t;

safe_state_t current_state = WAIT_SETUP;

//PIN being entered
char entered_pin[7];
int pin_length = 0;

// Shared variable for Core 1
volatile bool core1_ready = false;

//OLED Display
void oled_init(){
    //initialize SPI0
    spi_init(spi0, 8000000);

    //configure SPI clock and data pins
    gpio_set_function(OLED_SCK, GPIO_FUNC_SPI);
    gpio_set_function(OLED_MOSI, GPIO_FUNC_SPI);

    //configure chip select
    gpio_init(OLED_CS);
    gpio_set_dir(OLED_CS, GPIO_OUT);
    gpio_put(OLED_CS, 0);

    //use the pico's 3.3v supply
    display.external_vcc = false;

    //initialize the 128x32 OLED display
    if(!ssd1306_init(
        &display,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        OLED_DC,
        OLED_RST,
        spi0)) 
    {
        printf("OLED initialization failed!\n");
        return;
    }
    //Clear display
    ssd1306_clear(&display);
    ssd1306_show(&display);

    printf("OLED initialized successfully!\n");
}
//display the two lines text
void show_message(const char *line1, const char *line2){
    //clear previous display contents 
    ssd1306_clear(&display);

    //display first line
    ssd1306_draw_string(&display, 0, 8, 1, line1);
    ssd1306_draw_string(&display, 0, 16, 1, line2);

    ssd1306_show(&display);
}

// LED and Buzzer

void components_init()
{
    gpio_init(RED_LED);
    gpio_set_dir(RED_LED, GPIO_OUT);

    gpio_init(GREEN_LED);
    gpio_set_dir(GREEN_LED, GPIO_OUT);

    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);

    gpio_put(RED_LED, 0);
    gpio_put(GREEN_LED, 0);
    gpio_put(BUZZER, 0);
}

void beep()
{
    gpio_put(BUZZER, 1);
    sleep_ms(150);
    gpio_put(BUZZER, 0);
}

void update_indicators()
{
    if (safe_data.locked) {
        gpio_put(RED_LED, 1);
        gpio_put(GREEN_LED, 0);
    }
    else {
        gpio_put(RED_LED, 0);
        gpio_put(GREEN_LED, 1);
    }
}

//Keypad functions
void keypad_init()
{
    // Set up rows as outputs
    for (int i = 0; i < 4; i++) {
        gpio_init(keypad_rows[i]);
        gpio_set_dir(keypad_rows[i], GPIO_OUT);
        gpio_put(keypad_rows[i], 1);
    }

    // Set up columns as inputs
    for (int i = 0; i < 3; i++) {
        gpio_init(keypad_cols[i]);
        gpio_set_dir(keypad_cols[i], GPIO_IN);
        gpio_pull_up(keypad_cols[i]);
    }
}

char get_key()
{
    for (int row = 0; row < 4; row++) {

        // Set all rows high
        for (int i = 0; i < 4; i++) {
            gpio_put(keypad_rows[i], 1);
        }

        // Set current row low
        gpio_put(keypad_rows[row], 0);

        sleep_us(5);

        // Check columns
        for (int col = 0; col < 3; col++) {
            if (gpio_get(keypad_cols[col]) == 0) {
                return keys[row][col];
            }
        }
    }

    return '\0';
}

void wait_for_key_release()
{
    while (get_key() != '\0') {
        sleep_ms(10);
    }
}

//Core 1: keypad scanning and input handling
void core1_entry()
{
    // Register Core 1 with the storage library.
    // This must happen before Core 0 uses flash storage.
    safe_fs_core1_init();

    // Initialize keypad on Core 1
    keypad_init();

    // Tell Core 0 that Core 1 is ready
    core1_ready = true;

    while (true) {
        char key = get_key();

        if (key != '\0') {

            // Simple debounce
            sleep_ms(30);

            if (get_key() == key) {

                // Send the key to Core 0
                multicore_fifo_push_blocking((uint32_t)key);

                // Do not read the same press repeatedly
                wait_for_key_release();
            }
        }

        sleep_ms(5);
    }
}

// storage functions
bool save_safe_data()
{
    int result = safe_fs_write(SAFE_FILE, &safe_data, sizeof(safe_data));

    if (result == 0) {
        printf("Safe information saved.\n");
        return true;
    }

    printf("Error saving safe information: %d\n", result);
    return false;
}

bool valid_pin(const char *pin)
{
    for (int i = 0; i < 6; i++) {
        if (pin[i] < '0' || pin[i] > '9') {
            return false;
        }
    }

    return pin[6] == '\0';
}

bool load_safe_data()
{
    int bytes_read = safe_fs_read(SAFE_FILE, &safe_data, sizeof(safe_data));

    // Check if the saved information is valid
    if (bytes_read == sizeof(safe_data) &&
        safe_data.magic == SAFE_MAGIC &&
        safe_data.version == 1 &&
        safe_data.locked <= 1 &&
        safe_data.pin_set <= 1 &&
        safe_data.manager_only <= 1 &&
        (!safe_data.pin_set || valid_pin(safe_data.user_pin))) {

        printf("Saved safe information loaded.\n");
        return true;
    }

    // First startup or no valid saved information
    printf("No saved safe information. Starting setup.\n");

    memset(&safe_data, 0, sizeof(safe_data));

    safe_data.magic = SAFE_MAGIC;
    safe_data.version = 1;
    safe_data.locked = 0;
    safe_data.pin_set = 0;
    safe_data.manager_only = 0;

    return save_safe_data();
}

// display safe
void show_home()
{
    update_indicators();

    if (safe_data.locked && safe_data.pin_set) {
        show_message("LOCKED - PIN SET",
                     safe_data.manager_only ? "MANAGER ONLY" : "PRESS * TO ENTER");
    }
    else if (safe_data.locked) {
        show_message("LOCKED - NO PIN", "PRESS * TO SET PIN");
    }
    else if (safe_data.pin_set) {
        show_message("UNLOCKED - PIN SET", "* CHANGE # LOCK");
    }
    else {
        show_message("UNLOCKED - NO PIN", "PRESS * TO SET PIN");
    }
}

void clear_pin_entry()
{
    memset(entered_pin, 0, sizeof(entered_pin));
    pin_length = 0;
}

void show_pin_entry(bool new_pin)
{
    char line2[22];

    if (new_pin) {
        strcpy(line2, "NEW PIN ");
    }
    else {
        strcpy(line2, "ENTER PIN ");
    }

    int length = strlen(line2);

    for (int i = 0; i < pin_length; i++) {
        line2[length + i] = '*';
    }

    line2[length + pin_length] = '\0';

    if (new_pin && pin_length == 6) {
        show_message(safe_data.pin_set ? "UNLOCKED - PIN SET" :
                                         "UNLOCKED - NO PIN",
                     "PRESS # TO SAVE");
    }
    else {
        show_message(safe_data.locked ? "LOCKED - PIN SET" :
                                        "UNLOCKED - NO PIN",
                     line2);
    }
}

//reset the safe functions
void reset_safe()
{
    // Manager PIN resets the safe
    memset(&safe_data, 0, sizeof(safe_data));

    safe_data.magic = SAFE_MAGIC;
    safe_data.version = 1;
    safe_data.locked = 0;
    safe_data.pin_set = 0;
    safe_data.manager_only = 0;

    failed_attempts = 0;
    clear_pin_entry();

    save_safe_data();

    current_state = WAIT_SETUP;

    beep();
    show_message("SAFE RESET", "NO PIN - UNLOCKED");
    update_indicators();

    printf("Manager PIN accepted. Safe reset.\n");
    sleep_ms(1200);

    show_home();
}

void finish_pin_creation()
{
    strcpy(safe_data.user_pin, entered_pin);

    safe_data.pin_set = 1;
    safe_data.locked = 1;
    safe_data.manager_only = 0;

    failed_attempts = 0;
    clear_pin_entry();

    if (!save_safe_data()) {
        show_message("SAVE ERROR", "CHECK STORAGE");
        return;
    }

    current_state = WAIT_LOCKED;

    beep();
    printf("New PIN saved. Safe locked.\n");

    show_home();
}

void unlock_safe()
{
    safe_data.locked = 0;
    safe_data.manager_only = 0;

    failed_attempts = 0;

    save_safe_data();

    current_state = UNLOCKED;

    beep();
    printf("Correct PIN. Safe unlocked.\n");

    show_message("PIN CORRECT", "SAFE UNLOCKED");
    update_indicators();

    sleep_ms(1000);
    show_home();
}

void check_user_pin()
{
    // Check manager PIN first
    if (strcmp(entered_pin, manager_pin) == 0) {
        reset_safe();
        return;
    }

    // Check the user's PIN
    if (strcmp(entered_pin, safe_data.user_pin) == 0) {
        unlock_safe();
        return;
    }

    // Incorrect PIN
    failed_attempts++;

    printf("Incorrect PIN. Attempts: %d\n", failed_attempts);

    clear_pin_entry();
    beep();

    if (failed_attempts >= 3) {
        safe_data.manager_only = 1;
        safe_data.locked = 1;

        save_safe_data();

        current_state = WAIT_MANAGER;

        show_message("LOCKED - PIN SET", "MANAGER PIN ONLY");
        printf("Safe is now manager-only.\n");
    }
    else {
        show_message("WRONG PIN", "TRY AGAIN");
        sleep_ms(1000);

        current_state = ENTER_PIN;
        show_pin_entry(false);
    }

    update_indicators();
}

//keypad Input

bool is_digit(char key)
{
    return key >= '0' && key <= '9';
}

void add_digit(char key)
{
    if (pin_length >= 6) {
        return;
    }

    entered_pin[pin_length] = key;
    pin_length++;

    entered_pin[pin_length] = '\0';
}

void handle_key(char key)
{
    printf("Key pressed: %c\n", key);

    
    // Starting setup: no PIN has been set

    if (current_state == WAIT_SETUP) {

        if (key == '*') {
            clear_pin_entry();
            current_state = SET_PIN;

            show_pin_entry(true);
            printf("Enter a new six-digit PIN.\n");
        }
    }

    //Safe is locked
    else if (current_state == WAIT_LOCKED) {

        if (key == '*') {
            clear_pin_entry();
            current_state = ENTER_PIN;

            show_pin_entry(false);
            printf("Enter your PIN.\n");
        }
    }


    // Safe is unlocked
    else if (current_state == UNLOCKED) {

        if (key == '#') {
            safe_data.locked = 1;

            if (save_safe_data()) {
                current_state = WAIT_LOCKED;
                printf("Safe locked again.\n");
                beep();
                show_home();
            }
        }
        else if (key == '*') {
            clear_pin_entry();
            current_state = SET_PIN;

            show_pin_entry(true);
            printf("Enter a new six-digit PIN.\n");
        }
    }

    // Creating or changing a PIN
    else if (current_state == SET_PIN) {

        if (is_digit(key)) {

            if (pin_length >= 6) {
                show_message("TOO MANY DIGITS", "START AGAIN");
                sleep_ms(1000);

                clear_pin_entry();

                current_state = safe_data.pin_set ?
                                UNLOCKED : WAIT_SETUP;

                show_home();
            }
            else {
                add_digit(key);

                // Manager PIN works during PIN setup too
                if (pin_length == 6 &&
                    strcmp(entered_pin, manager_pin) == 0) {
                    reset_safe();
                }
                else {
                    show_pin_entry(true);
                }
            }
        }
        else if (key == '#') {

            if (pin_length != 6) {
                show_message("TOO FEW DIGITS", "START AGAIN");
                sleep_ms(1000);

                clear_pin_entry();

                current_state = safe_data.pin_set ?
                                UNLOCKED : WAIT_SETUP;

                show_home();
            }
            else {
                finish_pin_creation();
            }
        }
    }

    //Entering the user's PIN
    else if (current_state == ENTER_PIN) {

        if (is_digit(key)) {
            add_digit(key);

            show_pin_entry(false);

            // Check the PIN as soon as six digits are entered
            if (pin_length == 6) {
                check_user_pin();
            }
        }
        else if (key == '#') {
            show_message("TOO FEW DIGITS", "START AGAIN");
            sleep_ms(1000);

            clear_pin_entry();

            current_state = WAIT_LOCKED;
            show_home();
        }
        else if (key == '*') {
            clear_pin_entry();
            show_pin_entry(false);
        }
    }

    //Safe is in manager-only mode
    else if (current_state == WAIT_MANAGER) {

        if (key == '*') {
            clear_pin_entry();
            current_state = ENTER_MANAGER;

            show_message("MANAGER ONLY", "ENTER MANAGER PIN");
        }
    }


    //User is entering manager PIN
    else if (current_state == ENTER_MANAGER) {

        if (is_digit(key)) {
            add_digit(key);

            char line2[22] = "MANAGER ";

            for (int i = 0; i < pin_length; i++) {
                line2[8 + i] = '*';
            }

            line2[8 + pin_length] = '\0';

            show_message("MANAGER ONLY", line2);

            if (pin_length == 6) {

                if (strcmp(entered_pin, manager_pin) == 0) {
                    reset_safe();
                }
                else {
                    clear_pin_entry();

                    show_message("WRONG MANAGER", "TRY AGAIN");
                    beep();
                    sleep_ms(1000);

                    current_state = ENTER_MANAGER;
                    show_message("MANAGER ONLY", "ENTER MANAGER PIN");
                }
            }
        }
        else if (key == '#') {
            clear_pin_entry();

            show_message("TOO FEW DIGITS", "TRY AGAIN");
            sleep_ms(1000);

            show_message("MANAGER ONLY", "ENTER MANAGER PIN");
        }
    }
}


// Main Program: Core0
int main()
{
    // Initialize Pico standard functions
    stdio_init_all();

    sleep_ms(2000);

    printf("Safe project starting!\n");

    // Initialize hardware
    components_init();
    oled_init();

    // Start keypad scanning on Core 1
    multicore_launch_core1(core1_entry);

    // Wait for Core 1 to register with the storage library
    while (!core1_ready) {
        tight_loop_contents();
    }

    // Initialize persistent storage on Core 0
    if (!safe_fs_init()) {
        printf("Storage initialization failed!\n");

        show_message("STORAGE ERROR", "CHECK PROJECT");

        while (true) {
            sleep_ms(1000);
        }
    }

    // Load saved information or create first-time setup
    if (!load_safe_data()) {
        printf("Could not save initial safe information!\n");

        show_message("STORAGE ERROR", "CHECK PROJECT");

        while (true) {
            sleep_ms(1000);
        }
    }

    //Choose the correct starting state
    if (safe_data.manager_only) {
        current_state = WAIT_MANAGER;
    }
    else if (safe_data.locked) {
        current_state = WAIT_LOCKED;
    }
    else if (safe_data.pin_set) {
        current_state = UNLOCKED;
    }
    else {
        current_state = WAIT_SETUP;
    }

    update_indicators();
    show_home();

    printf("Safe project ready!\n");

    // Core 0 receives keypad presses and handles the safe
    while (true) {

        if (multicore_fifo_rvalid()) {
            char key = (char)multicore_fifo_pop_blocking();

            handle_key(key);
        }

        sleep_ms(10);
    }

    return 0;
}
