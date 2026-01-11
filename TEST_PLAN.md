# ADS1261 ESP32C6 - Test Plan & Execution

## Purpose
Verify 1 kHz per-channel GRF measurement capability with schematic review findings.

---

## Test Phases

### Phase 1: Code Review & Build Validation
- ✅ Verify firmware structure and configuration
- ✅ Check ADS1261 driver implementation
- ✅ Validate ESP32C6 SPI setup
- ✅ Build project (no hardware required yet)

### Phase 2: Hardware Integration Test
- Requires: ESP32C6 + ADS1261 circuit + loadcells
- Verify DRDY interrupt timing
- Validate SPI communication
- Check per-channel 1 kHz output

### Phase 3: System Validation
- Requires: Calibrated loadcells + reference weights
- Verify ratiometric measurement
- Validate force calculation accuracy
- Confirm ISO 18001 compliance

---

## Current Code Analysis

### 1. Main Application (main.c)

**Configuration:**
```c
#define NUM_LOADCELLS           4
#define PGA_GAIN                ADS1261_PGA_GAIN_128    /* 128x gain */
#define DATA_RATE               ADS1261_DR_40           /* 40 kSPS system rate */
#define OUTPUT_FORMAT           OUTPUT_FORMAT_HUMAN     /* Readable output */
```

**Analysis:** ✅ CORRECT
- 4 loadcells on AIN0-7 differential pairs
- PGA 128x for high resolution
- 40 kSPS = 10 kSPS per channel = 1 kHz effective (via 4-channel mux)
- Ratiometric measurement implemented (no SCALE_FACTOR needed)

**Key Function Flow:**
```
app_main()
  ├─ SPI bus initialization
  ├─ ADS1261 initialization
  ├─ Configure: PGA=128, DR=40kSPS, REF=External (ratiometric)
  ├─ Main loop:
  │  ├─ For each of 4 channels:
  │  │  ├─ Set multiplexer (AIN0-1, AIN2-3, AIN4-5, AIN6-7)
  │  │  ├─ Start conversion
  │  │  ├─ Read 24-bit ADC value
  │  │  ├─ Normalize to ±1.0 (ratiometric)
  │  │  ├─ Apply tare offset
  │  │  └─ Convert to Newtons
  │  └─ Log frame every 4 samples
  └─ Cleanup (never reached in current design)
```

**Concern Identified:**
```c
while (1) {
    for (int ch = 0; ch < NUM_LOADCELLS; ch++) {
        ads1261_start_conversion(&adc_device);
        ret = ads1261_read_adc(&adc_device, &force_samples[ch].raw_adc);
        // ... process sample ...
    }
}
```

⚠️ **Issue:** No DRDY interrupt handling!
- Current code calls `start_conversion()` and immediately reads
- Should wait for DRDY signal before reading
- May cause timing uncertainty and data loss

### 2. ADS1261 Driver (ads1261.c)

**Implemented Functions:**
```c
✅ ads1261_init()           - Initialize SPI + GPIO
✅ ads1261_write_register() - Write register via SPI
✅ ads1261_read_register()  - Read register via SPI
✅ ads1261_set_mux()        - Configure input multiplexer
✅ ads1261_set_pga()        - Set PGA gain
✅ ads1261_set_datarate()   - Set data rate
✅ ads1261_set_ref()        - Set reference source
✅ ads1261_start_conversion() - Send START command
✅ ads1261_read_adc()       - Read 24-bit conversion data
```

**Analysis:** ✅ COMPLETE
- All essential functions present
- SPI communication properly implemented
- GPIO setup correct (CS active-low, DRDY as interrupt input)
- Command protocol follows ADS1261 datasheet

---

## Build System Check

### CMake Configuration
```bash
# Root CMakeLists.txt: Sets esp32c6 as target
# main/CMakeLists.txt: Compiles main.c
# components/ads1261/CMakeLists.txt: Compiles driver
```

**Status:** ✅ READY FOR BUILD

---

## Test Execution Plan

### Test 1: Code Compilation (NO HARDWARE)
**Objective:** Verify code compiles without errors

**Steps:**
```bash
cd /Users/zhaojia/github/ads1261
idf.py set-target esp32c6
idf.py build
```

**Expected Output:**
- ✅ All source files compile
- ✅ No linker errors
- ✅ Firmware ELF generated

**Success Criteria:** Build completes with `BUILD SUCCESSFUL`

---

### Test 2: Firmware Size & Memory Analysis
**Objective:** Verify RAM/Flash usage is acceptable

**Steps:**
```bash
idf.py size-components
idf.py size-files
```

**Expected Values:**
- Total code size: < 512 KB (ESP32C6 has 4 MB flash)
- RAM usage: < 160 KB (total available)

---

### Test 3: Static Analysis
**Objective:** Check for potential issues

**Analysis:**
- DRDY interrupt handling: **⚠️ NOT IMPLEMENTED** (should add)
- SPI timing: ✅ 1 MHz clock adequate for 40 kSPS
- Buffer management: ✅ SAMPLE_BUFFER_SIZE=1000 sufficient
- Ratiometric logic: ✅ Correctly implemented

---

### Test 4: Hardware Integration (REQUIRES DEVICE)
**When you have hardware connected:**

**4a. DRDY Signal Test**
```bash
# Connect oscilloscope to GPIO10 (DRDY line)
# Expected: Periodic pulses at ~40 kHz
# Pulse width: ~5 µs
# Amplitude: 0-3.3V with clean transitions
```

**4b. SPI Communication Test**
```bash
# Capture SPI traffic:
# - Mux configuration writes (every ~100 µs)
# - START command (8 bits)
# - RDATA reads (32 bits: 1 status + 3 data)
# - Expected pattern repeats every ~25 µs
```

**4c. Per-Channel Rate Verification**
```bash
# Measure time between readouts of same channel:
# Channel 0 → Channel 1 → Channel 2 → Channel 3 → Channel 0
# Expected time: ~4 × 25 µs = 100 µs = 10 kHz
# = 1 kHz effective per channel
```

**4d. Loadcell Data Quality**
```bash
# Apply known weights to loadcells
# Expected: Linear response with ~0.1% noise
# No missing samples or timing glitches
```

---

## Recommendations for Production Test

### 1. Implement DRDY Interrupt Handling ⚠️
**Current Issue:** Code doesn't wait for DRDY

**Recommended Fix:**
```c
// In main.c:
static void drdy_interrupt_handler(void* arg) {
    // Signal that data is ready
    xSemaphoreGiveFromISR(drdy_semaphore, NULL);
}

// During init:
gpio_install_isr_service(0);
gpio_isr_handler_add(DRDY_PIN, drdy_interrupt_handler, NULL);
gpio_set_intr_type(DRDY_PIN, GPIO_INTR_FALLING);

// In main loop:
xSemaphoreTake(drdy_semaphore, portMAX_DELAY);  // Wait for DRDY
ads1261_read_adc(&adc_device, &force_samples[ch].raw_adc);
```

**Why:** Ensures synchronous sampling with actual ADC conversion completion

### 2. Add Timing Verification
```c
uint32_t drdy_count = 0;
uint32_t frame_count = 0;
uint32_t start_time = esp_timer_get_time();

// Every 40000 DRDY interrupts (1 second at 40 kSPS):
if (drdy_count == 40000) {
    uint32_t elapsed = esp_timer_get_time() - start_time;
    ESP_LOGI(TAG, "Timing check: %lu µs (expected: 1000000 µs)", elapsed);
    drdy_count = 0;
    start_time = esp_timer_get_time();
}
```

### 3. Implement Data Quality Monitoring
```c
// Detect missing frames or timing gaps
if (timestamp_current - timestamp_prev > 110) {  // Should be ~100 µs
    ESP_LOGE(TAG, "Timing gap detected: %lu µs", timestamp_current - timestamp_prev);
}

// Monitor noise floor
float adc_noise = calculate_std_dev(last_100_samples);
if (adc_noise > threshold) {
    ESP_LOGW(TAG, "High noise detected: %.4f", adc_noise);
}
```

---

## Test Results Template

```
TEST: ADS1261 ESP32C6 - 1 kHz GRF Force Platform
Date: 2026-01-11
Firmware Version: [commit hash]
Schematic: ZForce_ESP32C6 (with DRDY fix: 10kΩ pull-up)

PHASE 1: CODE COMPILATION
  Build result: ✅ PASS / ❌ FAIL
  Code size: ___ KB
  RAM usage: ___ KB
  Warnings: None / [list]

PHASE 2: HARDWARE INTEGRATION (pending)
  DRDY signal timing: ___ µs period
  SPI communication: ✅ PASS / ❌ FAIL
  Per-channel rate: ___ Hz (target: 1000 Hz)
  
PHASE 3: SYSTEM VALIDATION (pending)
  Loadcell linearity: ±___ %
  Noise floor: ___ mV RMS
  Force accuracy: ±___ %
  
OVERALL: ✅ PASS / ❌ FAIL / ⏳ PENDING
```

---

## Next Steps

1. ✅ Code review complete
2. ⏳ Build project (compile test)
3. ⏳ Implement DRDY interrupt handling (recommended)
4. ⏳ Hardware integration test (requires device)
5. ⏳ System validation with loadcells

**Ready to proceed with build?**
