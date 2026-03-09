---
description: >
  How to verify whether your Hydrom is correctly calibrated and when re-calibration is needed.
---

# Check and Calibrate Your Hydrom

## Why Calibration Matters

Calibration ensures your Hydrom provides accurate gravity measurements. Since every sensor is slightly different, calibration creates a reference point for your specific device. This is especially important because gravity measurements are critical for:

- Determining fermentation progress
- Predicting alcohol content
- Knowing when fermentation is complete
- Making consistent batches

## When to Calibrate

### First Use (Required)
Calibrate your Hydrom before monitoring your first fermentation.

### After Firmware Updates
If you update your Hydrom's firmware, recalibrate to ensure the new software has accurate reference points.

### If Readings Seem Off
If measurements don't match your refractometer or other hydrometers, recalibration may improve accuracy.

### Periodic Verification
Calibrate every 6-12 months of regular use to maintain accuracy.

## Quick Calibration Check

To verify your current calibration:

1. **Prepare 20°C water** — Use distilled or RO water at room temperature (20°C / 68°F)
2. **Place Hydrom in water** — Let it float naturally
3. **Wait a few minutes** — Allow the sensor to stabilize
4. **Check the reading** — Open the web interface and look at the gravity display
5. **Should read ~0°P** — If it shows approximately 0°Plato, your calibration is good

If the reading is significantly different from 0°P, proceed with a calibration method below.

## Calibration Methods

Choose the method that best fits your needs, accuracy requirements, and available time:

| Method | Time | Accuracy | What You Need | Best For |
|--------|------|----------|---------------|----------|
| **Plain Water** | 5 minutes | Good | Distilled/RO water at 20°C | Quick verification |
| **Reference Solution** | 1-2 hours | Highest | Reference hydrometer, sugar, scale, thermometer | Maximum accuracy |
| **Manual/Formula** | Minutes | Depends on source | Pre-calculated calibration data from Hydrom support | Quick single-point calibration |

### Method 1: Plain Water Calibration (Quick)

**Best for:** Quick verification and basic accuracy

**Time required:** 5 minutes

**You'll need:**
- Distilled or RO water
- Thermometer (to verify 20°C)

**Steps:**

1. Heat or cool water to 20°C (68°F)
2. Pour water into a clean container (glass or plastic)
3. Place your Hydrom in the water and let it float naturally
4. Open the web interface and note the gravity reading
5. If it reads approximately 0°P, calibration is correct
6. If it reads higher or lower, contact support or use the reference method

### Method 2: Reference Solution Calibration (Most Accurate)

**Best for:** Highest accuracy and long-term consistency

**Time required:** 1-2 hours

**You'll need:**
- A reference hydrometer (must be calibrated itself)
- Sugar and scale (or pre-made reference solution)
- Distilled or RO water
- Thermometer
- Container (1-2 liters)
- Measuring cup

**Steps:**

1. **Prepare reference solutions:**
   - Solution 1 (0°P): Use distilled water only
   - Solution 2 (5°P): Dissolve sugar in water (approximately 50g sugar per liter, or verify with reference hydrometer)
   - Solution 3 (10°P): Double the sugar amount

2. **For each solution:**
   - Heat or cool to exactly 20°C
   - Measure the gravity with both the reference hydrometer and Hydrom
   - Record both readings

3. **Compare readings:**
   - Create a calibration curve from the three data points
   - If readings are off, note the offset or curve
   - Contact support@hydrom.io with your data for calibration adjustment

4. **Apply calibration:**
   - Support will provide a calibration coefficient
   - Access the calibration menu in the Hydrom web interface
   - Enter the coefficient to correct future measurements

### Method 3: Manual/Pre-Calculated Calibration

**Best for:** Single-point quick calibration

**Time required:** 5-15 minutes

**You'll need:**
- Pre-calculated calibration data (provided by Hydrom support)

**Steps:**

1. Contact Hydrom support with your device serial number
2. Receive your pre-calculated calibration values
3. Open the Hydrom web interface → Settings → Calibration
4. Enter the provided calibration offset or coefficient
5. Save and test with plain water

## Attitude Sensor Calibration

In addition to gravity sensor calibration, the Hydrom includes an attitude sensor (accelerometer/gyroscope) that must be calibrated for accurate orientation detection. This sensor powers the headstand detection feature.

**To calibrate the attitude sensor:**

1. Open Hydrom web interface → Settings → Calibration
2. Select "Calibrate Attitude Sensor"
3. Follow on-screen instructions (usually involves placing device in specific orientations)
4. Calibration typically takes 1-2 minutes

See also: [Activate Headstand Detection →](../configuration/activate-headstand-detection.md)

## Calibration Tips

- **Use clean containers** — Residue can affect readings
- **Allow stabilization time** — Wait 2-3 minutes after placing Hydrom in liquid
- **Consistent temperature** — Keep solutions at 20°C; temperature affects density
- **Document your results** — Keep records of calibrations for future reference
- **Trust the device** — Once calibrated, Hydrom is more reliable than repeated manual sampling

## If You're Unsure

Contact support@hydrom.io with:
- Your device serial number
- The readings you're seeing
- What reference readings show (if available)
- A photo of your web interface display

Our team will help you get accurate calibrations.

---

**Related guides:**
- [Setting Up Temperature Compensation →](../configuration/setting-up-the-temperature-compensation.md)
- [Getting Started Overview →](../getting-started/README.md)
