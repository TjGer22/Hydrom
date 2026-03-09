---
description: >-
  Ubidots is a cloud service that accepts, stores and visualizes measurement
  data from internet-enabled devices.
cover: >-
  https://files.startupranking.com/startup/thumb/6796_043b01ef967da629abc843eca1f2a477906da07a_ubidots_l.png
coverY: 0
---

# Connect the Hydrom to Ubidots

!!! tip
    **Prerequisite:**

    1. **Access to the user interface**\
       To make this setting you must have access to the user interface. If this is not the case, see here how to do it:[access-to-the-user-interface.md](access-to-the-user-interface.md)
    2. **Ubidots user account available**\
       An account was created with the cloud provider ubidots. At the time of writing, there is a free account for non-commercial use.\
       https://industrial.ubidots.com/accounts/signup\_industrial/

<figure><img src="../assets/images/Ubidots2.gif" alt=""><figcaption></figcaption></figure>

## Add the hydrom to Ubidots

### Navigate to the Devicemanager

The device management is located at the top center. Here a new device must be added

![](../assets/images/Folie27.png)

### Add Blank Device

It is really important that you enter the device name here. E.g. "Hydrom001" in the default configuration

![Choose Device Name](../assets/images/Folie28.png)

### Copy the token

After adding the device, the device must be opened and then the token must be copied (see red marking).

![Copy Ubidots Token](../assets/images/Folie30.png)

## Insert Service settings in Hydrom

Now open the user interface of the Hydrom, in the best case the Hydrom is already in the network and can be easily accessed via Webpage.\
If there are questions about how to get to the user interface, the following page is recommended: [access-to-the-user-interface.md](access-to-the-user-interface.md)

Otherwise the Hydrom must still be started.

## Step 1: Open the Menue

To access the menu bar you have to click on the three lines in the upper left corner. (See arrow)\
Then the menu bar will open.

![access the menu bar by clicking the three lines in the upper left corner](../assets/images/Folie4.png)

## Step 2: Select "Service"

The "Services" page is a resource that allows users to select and configure various interfaces and cloud services for use with their system. On this page, users can choose from three different interfaces - HTTP, MQTT, and TCP - and several cloud services, and then activate and parameterize these options to suit their needs. This page is a useful tool for those who want to connect their system to the cloud or other external services and customize the way in which data is transmitted and received. With the options available on the "Services" page, users can tailor their system to meet their specific requirements and ensure that it is able to communicate effectively with other devices and systems.

## Step 3: Activate Ubidots and change the settings

On the "Service" page, the Ubidots service must be activated.

![Activating the Ubidots interface and entering the token](../assets/images/Folie44.png)

### additional settings

In order not to consume too much power, we would turn Bluetooth off when it is not needed. It was documented here:

[Enable Bluetooth on the Hydrom](bluetooth/README.md)

In order for the Hydrom to transmit the data reliably, a DeepSleep time must be set. This is documented on this page:

[enable-deepsleep.md](../configuration/enable-deepsleep.md)

### Save Settings

Saving the data is important because it is the only way to connect to the service after waking up from DeepSleep.

![Pressing the "save" button saves the settings.](../assets/images/Folie5.png)

You can check whether the saving was successful by looking at the settings file at http://hydrom001/settings.json/. to check if the save was successful. This file is the permanent memory of the Hydrom.

A second way to check the saving is to reload the page (all modern browsers offer this Feature). If the properties are then reloaded, the hydrom has accepted them, otherwise the old settings are reloaded.

### Send Test-message

By activating this button, you can check whether the Hydrom has a connection to the service and to the individual parts of the service. If you now click on "Save", the set services are executed once and the page is reloaded. On the new page you can see whether the servers are accessible and the ports are open.

![Sending a test message can greatly simplify the process of checking the connection.](../assets/images/Folie6.png)

### activate DeepSleep

Activating DeepSleep is mandatory for the hydrom to be able to send data. If DeepSleep is activated, the hydrom wakes up after the set time and sends the data to the set service. Afterwards, the hydrom goes back to sleep and waits for the new service.

[enable-deepsleep.md](../configuration/enable-deepsleep.md)

!!! tip
    To ensure that the device sends measured values to the Service in the set interval as intended, the Hydrom must be put into deep sleep.

    There are two ways to put the Hydrom into deep sleep.

    Turning the _**power switch off and on**_ is the easiest way to put the Hydrom into deep sleep.

    The alternative way is to put it to sleep _**via the UI**_. To do this, open the navigation bar and go to the "DeepSleep" tab.
