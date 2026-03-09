---
description: >
  Troubleshooting guide for when the Hydrom web interface cannot be reached from a browser.
---

# Hydrom Web Interface Not Reachable

Can't access the Hydrom web interface? Follow this troubleshooting guide to get back online.

## Quick Diagnosis

### Step 1: Check Configuration Mode

Is your Hydrom in configuration mode (AP mode)?

**Look for:** A **green LED** on the device

- **Green LED = Yes** — The Hydrom is in configuration mode
- **Blue/red LED or no light = No** — The Hydrom is in normal mode

**If in configuration mode:**

1. Connect to the temporary Wi-Fi network named **hydrom_XXXXX** (where XXXXX is your device ID)
2. Open your browser and go to: **http://192.168.2.1**
3. You should see the Hydrom configuration interface
4. Complete the Wi-Fi setup if you haven't already

### Step 2: Are You on the Right Network?

If the Hydrom is NOT in configuration mode, verify you're connected to the correct network:

**To access the normal web interface:**

1. **Connect to your home/brewery Wi-Fi network** — The same network your Hydrom joined
2. Open your browser and enter the Hydrom's IP address in the address bar

![Network settings showing IP address](../assets/images/Folie35.png)

**Don't know the Hydrom's IP address?**

Look for it in your router's connected devices or network settings (typically shown in a green box on your router's admin interface).

### Step 3: Home Network Access

Once you know the Hydrom's IP address:

1. **On the same Wi-Fi network** as the Hydrom
2. Open a web browser (Chrome, Firefox, Safari, Edge, etc.)
3. Type the IP address in the address bar: **http://192.168.X.X**
4. Press Enter

You should see the Hydrom dashboard.

!!! info
    Always use **http://** not **https://** — Hydrom is accessed via unencrypted HTTP

### Step 4: Browser Shows "Not Secure"

If your browser displays a security warning like "Not Secure," "Your connection is not private," or shows a lock icon:

**This is normal** — Hydrom uses HTTP (not HTTPS) for the local web interface.

**To proceed:**

1. Click **Advanced** or **More details** (varies by browser)
2. Look for an option like **"Proceed anyway"** or **"Accept the risk and continue"**
3. Click it to access the interface

![Proceed past browser warning](../assets/images/Folie34.png)

**Why is this secure?**
- Hydrom communicates only on your local Wi-Fi network
- No sensitive data passes through the internet
- This is standard for local network devices

## Still Can't Access?

If you've tried all steps above and still can't reach the web interface:

### Try a Soft Reset

1. Find the **reset button** on your Hydrom (small button, often recessed)
2. Press and hold for 3-5 seconds
3. Release and wait 30 seconds for the device to restart
4. Look for the green LED (configuration mode)
5. Try accessing via **http://192.168.2.1** again

### Check These Common Issues

**Is Wi-Fi working?**
- Other devices connected to the network?
- Can you access other network resources?

**Is the device powered?**
- Check the LED indicator (should show some light)
- Is the battery charged? Try charging via USB-C for 15 minutes and try again

**Firewall or network restrictions?**
- Some enterprise networks block device access
- Try connecting to a personal mobile hotspot to test
- Contact your network administrator

**Browser cache issues?**
- Clear your browser cache and cookies
- Try a different browser
- Try an incognito/private browsing window

## Contact Support

If none of these steps work:

**Email:** support@hydrom.io

**Include in your message:**
- Hydrom device serial number (on device or in documentation)
- Your IP address or network range (e.g., 192.168.1.X)
- LED color you're seeing (green/blue/red/none)
- What browser you're using
- Any error messages exactly as shown

Our support team will help you get back online quickly.

---

**Related guides:**
- [Wi-Fi Setup →](../services/wifi-setup.md)
- [Service Not Receiving Data →](service-no-data.md)
