/**
 * @file Calibration_MPU.cpp
 * @author TjGer22
 * @brief IMU offset-calibration routine for the MPU6050.
 * @date 2026
 *
 * @details
 * Implements the Calibration_MPU class. Runs the MPU6050 offset
 * calibration procedure and persists the results via
 * HydromConfiguration.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define __PGMSPACE_H_ true
#include "Calibration_MPU.h"

#include "Arduino.h"
#include "DeviceManager.h"
#include "HydromConfiguration.h"
#include "I2Cdev.h"
#include "MPU6050.h"
#include "SensorManager.h"
#include "Util.h"
#include "Wire.h"

#define DEBUG_PRINT(p, ...) Serial.print(p, ##__VA_ARGS__)
#define DEBUG_PRINTF(p, ...) Serial.printf(p, ##__VA_ARGS__)

MPU6050 accelgyro;
int N = 10000;
Calibration_MPU calibrator_mpu;
/** @brief Default constructor. */
Calibration_MPU::Calibration_MPU(void) {
}

/**
 * @brief Forces the header to print on the next serial output cycle
 *        by setting the line counter above the threshold.
 */
void Calibration_MPU::ForceHeader() {
    LinesOut = 99;
}

/**
 * @brief Collects N raw IMU samples and stores their smoothed (averaged)
 *        values in the Smoothed[] array for all 6 axes.
 */
void Calibration_MPU::GetSmoothed() {
    int16_t RawValue[6];
    int i;
    long Sums[6];
    for(i = iAx; i <= iGz; i++) {
        Sums[i] = 0;
    }
    //    unsigned long Start = micros();
    for(i = 1; i <= N; i++) {    // get sums
        accelgyro.getMotion6(&RawValue[iAx], &RawValue[iAy], &RawValue[iAz], &RawValue[iGx], &RawValue[iGy], &RawValue[iGz]);
        if((i % 500) == 0)
            delayMicroseconds(usDelay);
        for(int j = iAx; j <= iGz; j++) {
            Sums[j] = Sums[j] + RawValue[j];
        }

    }    // get sums
    for(i = iAx; i <= iGz; i++)
        Smoothed[i] = (Sums[i] + N / 2) / N;
}    // GetSmoothed

/**
 * @brief Wires up I2C, initialises the MPU6050 and runs the first
 *        coarse calibration passes (6×accel + 6×gyro, then 4×1 fine passes).
 *
 * @param l_MPU_SDA_PIN   I2C SDA GPIO pin.
 * @param l_MPU_SCL_PIN   I2C SCL GPIO pin.
 * @param l_MPU_SET_CLOCK I2C clock frequency in Hz.
 */
void Calibration_MPU::Initialize(int8_t l_MPU_SDA_PIN, int8_t l_MPU_SCL_PIN, uint32_t l_MPU_SET_CLOCK) {
    Device.LED_FlipFlop();
    //initial Ofset
    accelgyro.setXGyroOffset(-21);
accelgyro.setYGyroOffset(-17);
accelgyro.setZGyroOffset(-58);
accelgyro.setXAccelOffset(0);
accelgyro.setYAccelOffset(0);
accelgyro.setZAccelOffset(0);



    Print_Info(4, Hydrom.current_Log_Level, "Initialize MPU");
    Device.setMPUstate(HIGH);
    delay(500);
    Wire.begin(l_MPU_SDA_PIN, l_MPU_SCL_PIN);
    Wire.setClock(l_MPU_SET_CLOCK);
    // initialize device
    accelgyro.initialize();
    Print_Info(4, Hydrom.current_Log_Level, "MPU with Device ID "+String(accelgyro.getDeviceID())+" was Initialized");
    if(accelgyro.testConnection()){
        Print_Info(3, Hydrom.current_Log_Level, "MPU6050 connection successful");
    } else{
        Print_Error("MPU6050 connection failed");
    }

Device.LED_FlipFlop();
    accelgyro.CalibrateAccel(6);
   Print_Info(4, Hydrom.current_Log_Level, "First Set of Calibration");
   Device.LED_FlipFlop();
    accelgyro.CalibrateGyro(6);
    Print_Info(4, Hydrom.current_Log_Level, "First Set of Calibration");
    Device.LED_FlipFlop();
    accelgyro.CalibrateAccel(1);
    Device.LED_FlipFlop();
    accelgyro.CalibrateGyro(1);
    Device.LED_FlipFlop();
    accelgyro.CalibrateAccel(1);
    Device.LED_FlipFlop();
    accelgyro.CalibrateGyro(1);
    Device.LED_FlipFlop();
    accelgyro.CalibrateAccel(1);
    Device.LED_FlipFlop();
    accelgyro.CalibrateGyro(1);
    Device.LED_FlipFlop();
    accelgyro.CalibrateAccel(1);
    Device.LED_FlipFlop();
    accelgyro.CalibrateGyro(1);
}    // Initialize

/**
 * @brief Writes all 6 offset values (3×accel + 3×gyro) to the MPU6050.
 *
 * @param TheOffsets Array of 6 integer offsets: [Ax, Ay, Az, Gx, Gy, Gz].
 */
void Calibration_MPU::SetOffsets(int TheOffsets[6]) {
    accelgyro.setXAccelOffset(TheOffsets[iAx]);
    accelgyro.setYAccelOffset(TheOffsets[iAy]);
    accelgyro.setZAccelOffset(TheOffsets[iAz]);
    accelgyro.setXGyroOffset(TheOffsets[iGx]);
    accelgyro.setYGyroOffset(TheOffsets[iGy]);
    accelgyro.setZGyroOffset(TheOffsets[iGz]);
}    // SetOffsets

/**
 * @brief Performs a binary search to narrow the offset brackets around
 *        the target value for each of the 6 IMU axes.
 *        Switches from fast to slow averaging once all brackets are narrow.
 */
void Calibration_MPU::PullBracketsIn() {
    Print_Info(5, Hydrom.current_Log_Level, "Start with PullBracketsIn");
    boolean AllBracketsNarrow;
    boolean StillWorking;
    int NewOffset[6];
    AllBracketsNarrow = false;
    ForceHeader();
    StillWorking = true;
    while(StillWorking) {
        StillWorking = false;
        if(AllBracketsNarrow && (N == NFast)) {
            SetAveraging(NSlow);
        } else {
            AllBracketsNarrow = true;
        }    // tentative
        for(int i = iAx; i <= iGz; i++) {
            Device.LED_FlipFlop();
            if(HighOffset[i] <= (LowOffset[i] + 1)) {
                NewOffset[i] = LowOffset[i];
            } else {    // binary search
                StillWorking = true;
                NewOffset[i] = (LowOffset[i] + HighOffset[i]) / 2;
                if(HighOffset[i] > (LowOffset[i] + 10)) {
                    AllBracketsNarrow = false;
                }
            }    // binary search
        }
        SetOffsets(NewOffset);
        GetSmoothed();
        for(int i = iAx; i <= iGz; i++) {    // closing in
        Device.LED_FlipFlop();
            if(Smoothed[i] > Target[i]) {    // use lower half
                HighOffset[i] = NewOffset[i];
                HighValue[i]  = Smoothed[i];
            }         // use lower half
            else {    // use upper half
                LowOffset[i] = NewOffset[i];
                LowValue[i]  = Smoothed[i];
            }    // use upper half
        }        // closing in
    }            // still working
    Print_Info(5, Hydrom.current_Log_Level, "End with PullBracketsIn");
}    // PullBracketsIn

/**
 * @brief Expands the offset search brackets until the target value lies
 *        between the low and high offsets for every axis.
 */
void Calibration_MPU::PullBracketsOut() {
    Print_Info(5, Hydrom.current_Log_Level, "start PullBracketsOut");
    boolean Done = false;
    int NextLowOffset[6];
    int NextHighOffset[6];
    ForceHeader();
    while(!Done) {
        Done = true;
        SetOffsets(LowOffset);
        GetSmoothed();
        for(int i = iAx; i <= iGz; i++) {    // got low values
            LowValue[i] = Smoothed[i];
            if(LowValue[i] >= Target[i]) {
                Done             = false;
                NextLowOffset[i] = LowOffset[i] - 1000;
            } else {
                NextLowOffset[i] = LowOffset[i];
            }
        }    // got low values
        SetOffsets(HighOffset);
        GetSmoothed();
        for(int i = iAx; i <= iGz; i++) {    // got high values
            HighValue[i] = Smoothed[i];
            if(HighValue[i] <= Target[i]) {
                Done              = false;
                NextHighOffset[i] = HighOffset[i] + 1000;
            } else {
                NextHighOffset[i] = HighOffset[i];
            }
        }    // got high values
        for(int i = iAx; i <= iGz; i++) {
            LowOffset[i]  = NextLowOffset[i];     // had to wait until ShowProgress done
            HighOffset[i] = NextHighOffset[i];    // ..
        }
    }    // keep going
    Print_Info(5, Hydrom.current_Log_Level, "end PullBracketsOut");
}    // PullBracketsOut

/**
 * @brief Runs the full offset calibration sequence and persists the
 *        results into the global Hydrom configuration struct.
 *
 * @param l_MPU_SDA_PIN   I2C SDA GPIO pin.
 * @param l_MPU_SCL_PIN   I2C SCL GPIO pin.
 * @param l_MPU_SET_CLOCK I2C clock frequency in Hz.
 */
void Calibration_MPU::start(int8_t l_MPU_SDA_PIN, int8_t l_MPU_SCL_PIN, uint32_t l_MPU_SET_CLOCK) {
    sensormanager.stop();
    Print_Info(4, Hydrom.current_Log_Level, "STARTING MPU Calibration\n");

    Initialize(l_MPU_SDA_PIN, l_MPU_SCL_PIN, l_MPU_SET_CLOCK);
    Print_Info(4, Hydrom.current_Log_Level, "MPU initialization completed\n");

    for(int i = iAx; i <= iGz; i++) {    // set targets and initial guesses
        Target[i]     = 0;               // must fix for ZAccel
        HighOffset[i] = 0;
        LowOffset[i]  = 0;
    }    // set targets and initial guesses
    Target[iAz] = 16384;
    SetAveraging(NFast);
Device.LED_FlipFlop();
    Print_Info(4, Hydrom.current_Log_Level, "Start with PullBracketsOut");
    PullBracketsOut();
Device.LED_FlipFlop();
    PullBracketsIn();
Device.LED_FlipFlop();
    Print_Info(4, Hydrom.current_Log_Level, "Start with overwriting the values");
    if(Hydrom.accelOffset[0] != accelgyro.getXAccelOffset()) {
        Print_Info(4, Hydrom.current_Log_Level, "Accelx diff: ");
        DEBUG_PRINT(String(Hydrom.accelOffset[0] - accelgyro.getXAccelOffset()));
        Hydrom.accelOffset[0] = accelgyro.getXAccelOffset();
    } else {
        Print_Info(4, Hydrom.current_Log_Level, "Accelx has no differenz");
    }
    if(Hydrom.accelOffset[1] != accelgyro.getYAccelOffset()) {
        Print_Info(4, Hydrom.current_Log_Level, "Accely diff: ");
        DEBUG_PRINT(String(Hydrom.accelOffset[1] - accelgyro.getYAccelOffset()));
        Hydrom.accelOffset[1] = accelgyro.getYAccelOffset();
    } else {
        Print_Info(4, Hydrom.current_Log_Level, "Accely has no differenz");
    }
    if(Hydrom.accelOffset[2] != accelgyro.getZAccelOffset()) {
        Print_Info(4, Hydrom.current_Log_Level, "Accelz diff: ");
        DEBUG_PRINT(String(Hydrom.accelOffset[1] - accelgyro.getZAccelOffset()));
        Hydrom.accelOffset[2] = accelgyro.getZAccelOffset();
    } else {
        Print_Info(4, Hydrom.current_Log_Level, "Accelz has no differenz");
    }

    if(Hydrom.gyroOffset[0] != accelgyro.getXGyroOffset()) {
        Print_Info(4, Hydrom.current_Log_Level, "Gyrox diff: ");
        DEBUG_PRINT(String(Hydrom.gyroOffset[0] - accelgyro.getXGyroOffset()));
        Hydrom.gyroOffset[0] = accelgyro.getXGyroOffset();
    } else {
        Print_Info(4, Hydrom.current_Log_Level, "Gyrox has no differenz");
    }
    if(Hydrom.gyroOffset[1] != accelgyro.getYAccelOffset()) {
        Print_Info(4, Hydrom.current_Log_Level, "Gyroy diff: ");
        DEBUG_PRINT(String(Hydrom.gyroOffset[1] - accelgyro.getYGyroOffset()));
        Hydrom.gyroOffset[1] = accelgyro.getYGyroOffset();
    } else {
        Print_Info(4, Hydrom.current_Log_Level, "Gyroy has no differenz");
    }
    if(Hydrom.gyroOffset[2] != accelgyro.getZGyroOffset()) {
        Print_Info(4, Hydrom.current_Log_Level, "Gyroz diff: ");
        DEBUG_PRINT(String(Hydrom.gyroOffset[1] - accelgyro.getZGyroOffset()));
        Hydrom.gyroOffset[2] = accelgyro.getZGyroOffset();
    } else {
        Print_Info(4, Hydrom.current_Log_Level, "Gyroz has no differenz");
    }

    Wire.endTransmission();
    Print_Info(4, Hydrom.current_Log_Level, "MPU calibration completed");
    Device.ConstantGREEN();
}    // setup
/**
 * @brief Sets the number of samples used for the smoothing average.
 *
 * @param NewN Desired sample count (e.g. NFast=1000 or NSlow=10000).
 */
void Calibration_MPU::SetAveraging(int NewN) {
    N = NewN;
}    // SetAveraging

/**
 * @brief Returns whether the IMU has been calibrated in this session.
 *
 * @return true  Calibration completed.
 * @return false Calibration has not run yet.
 */
boolean Calibration_MPU::get_state_was_calibrated(void) {
    return was_calibrated;
}
/**
 * @brief Sets the calibrated flag.
 *
 * @param l_new_state true = mark as calibrated, false = clear the flag.
 */
void Calibration_MPU::set_state_was_calibrated(bool l_new_state) {
    was_calibrated = l_new_state;
}