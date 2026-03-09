---
description: >-
  The Hydrom supports several units for the tilt and the temperature. Here is
  explained how to change the unit.
---

# Customize units



!!! tip
    **Precondiion:**\
    To make this setting you must have access to the user interface. If this is not the case, see here how to do it:[access-to-the-user-interface.md](../services/access-to-the-user-interface.md)

## Step 1: Open the Menue

To access the menu bar you have to click on the three lines in the upper left corner. (See arrow)\
Then the menu bar will open.

![access the menu bar by clicking the three lines in the upper left corner](../assets/images/Folie4.png)

## Step 2: Select "Settings"

On this page, you can adjust various settings to personalize your experience with the device. Options you can customize include the interval at which the device collects and sends data, the units displayed on the main page, and the overhead detection feature. You can also enable temperature compensation to ensure accurate measurements. Take some time to explore the various settings available and make the necessary adjustments for your needs.

## Step 3: Change Units

![](../assets/images/Folie16.png)

### Tilt

* **Plato** The unit Plato (°P) is a unit of measure used to quantify the concentration of sugars in a solution. It is commonly used in the brewing industry to describe the concentration of wort, which is the liquid that is extracted from the mashing process during the production of beer. One degree Plato is equal to one gram of sucrose per 100 grams of solution. The Plato scale is widely used because it is more accurate than other methods of measuring the concentration of sugars, such as the Brix scale.
* **Specific Gravity** Specific gravity is a unit of measure used to compare the density of a substance to the density of a reference substance, typically water. It is commonly used in the brewing industry to measure the concentration of sugars in a solution, such as wort or beer. Specific gravity is expressed as a ratio, with the density of the substance being measured being the numerator and the density of the reference substance being the denominator. For example, if the specific gravity of a substance is 1.050, it means that it is 1.050 times denser than water, which has a specific gravity of 1.000. Specific gravity is an important measure for brewers because it allows them to determine the concentration of sugars and other dissolved solids in their wort or beer, which can affect the final alcohol content and flavor of the finished product.
* **Degree** The raw angle of the Hydrom to the horizontal is indicated in degrees. If the hydrometer is horizontal, "0°" is displayed. If the hydrometer is in the vertical plane, "90°" is displayed.

### Temperature

* **Celsius** The unit Celsius (°C) is the international standard temperature scale used for measuring temperatures, with zero set at 0 °C and the boiling point of water set at 100 °C.
* **Fahrenheit** Fahrenheit (°F) is a temperature scale used in the United States and some other countries, with the freezing point of water set at 32 °F and the boiling point at 212 °F.
* **Kelvin**. The unit Kelvin (K) is a temperature scale used in science and industrial applications and on which the absolute temperature scale is based. On the Kelvin scale, absolute zero is set at 0 K, which corresponds to the theoretical state of complete immobility of atoms and molecules. The Kelvin scale is often used in physics and chemistry because it is independent of the type of heat transfer and has no negative values.

!!! info
    The set units do not affect the services.

    This means that the data sent will always be consistent, no matter what unit is set.

### Save Settings

Saving the data is important because it is the only way to connect to the service after waking up from DeepSleep.

![Pressing the "save" button saves the settings.](../assets/images/Folie5.png)

You can check whether the saving was successful by looking at the settings file at http://hydrom001/settings.json/. to check if the save was successful. This file is the permanent memory of the Hydrom.

A second way to check the saving is to reload the page (all browsers offer this). If the properties are then reloaded, the hydrom has accepted them, otherwise the old settings are reloaded.
