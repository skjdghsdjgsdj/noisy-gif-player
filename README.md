# Noisy GIF Player

## What is it?

It's a little portable device with a tiny screen and speaker to play animated GIFs, along with sound, from a microSD card.

## How do I build one?

### Parts

You need these parts:

* [Adafruit ESP32-S3 Reverse TFT Feather](https://www.adafruit.com/product/5691)
* [Adafruit SDIO breakout](https://www.adafruit.com/product/4682)
* [Adafruit I2S MAX98357A breakout](https://www.adafruit.com/product/3006)
* [Mini oval speaker](https://www.adafruit.com/product/3923)
* [Female headers for Feathers](https://www.adafruit.com/product/2886)
* [Solid core hookup wire](https://www.adafruit.com/product/1311)
* [400mAH battery for Feathers](https://www.adafruit.com/product/3898)
* Some screws:
  * 2&times; M2.5x4 screws
  * 2&times; M2x4 screws
  * 4&times; [countersunk self-tapping M2x6 screws](https://www.amazon.com/dp/B09DB5SMCZ?th=1)

You'll also need a 3D printer, soldering iron, and tiny screwdriver.

### Printing

Print the following parts. Use 0.2mm layer height and any infill you desire.

* Print `Case.stl` with the screen opening on the print bed. Use paint-on supports for the four screw holes in the corners of the case. You can do the same for the USB C port, but if your printer bridges well, you should be fine without them.
* Print `Backplate.stl` with the two ribs facing upwards.
* Print `Button.stl` with the wider part of the button on the print bed.