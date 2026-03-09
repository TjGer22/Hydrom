---
description: >
  How to change the device name of your Hydrom in the web interface.
---

# Changing the Hydrom name



!!! danger
    ATTENTION! If you have entered incorrect Name for the Hydrom, you will not be able to connect to the Hydrom via existing Network. You will then need to use the Configuration-Wifi.

    If the setting is incorrect, you can reset the Hydrom as described here:

    [#reset-by-the-button](factory-reset.md#reset-by-the-button)

!!! info
    Changing the Hydrom name has the following effect:

    1. The user interface is not accessed via [http://hydrom001/](http://hydrom001) as described in the chapter [Access to the User Interface](../services/access-to-the-user-interface.md#connect-to-the-hydrom-via-the-hydroms-configuration-wifi), but now via http://\<own\_choosen\_name>/.
    2.  Some of the services described in the chapter "Connect Services" now send to a different address.

        However, this is mentioned separately in the respective instruction article.

!!! tip
    To make this setting you must have access to the user interface. If this is not the case, see here how to do it:

    [access-to-the-user-interface.md](../services/access-to-the-user-interface.md)

## Step 1: Open the Menue

To access the menu bar you have to click on the three lines in the upper left corner. (See arrow)\
Then the menu bar will open.

![access the menu bar by clicking the three lines in the upper left corner](../assets/images/Folie4.png)

## Step 2: Select "Settings"

On this page, you can adjust various settings to personalize your experience with the device. Options you can customize include the interval at which the device collects and sends data, the units displayed on the main page, and the overhead detection feature. You can also enable temperature compensation to ensure accurate measurements. Take some time to explore the various settings available and make the necessary adjustments for your needs.

## Step 3: Change device name

To change the device name, enter the new name under the "Devicename" lettering.\
By default, the device name is "Hydrom001".

![Changing the device name of the hydrometer](../assets/images/Folie15.png)

### Save Settings

Saving the data is important because it is the only way to connect to the service after waking up from DeepSleep.

![Pressing the "save" button saves the settings.](../assets/images/Folie5.png)

You can check whether the saving was successful by looking at the settings file at http://hydrom001/settings.json/. to check if the save was successful. This file is the permanent memory of the Hydrom.

A second way to check the saving is to reload the page (all browsers offer this). If the properties are then reloaded, the hydrom has accepted them, otherwise the old settings are reloaded.
