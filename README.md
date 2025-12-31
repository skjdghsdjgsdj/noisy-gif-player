# Noisy GIF Player

## What is it?

It's a little portable device with a tiny screen and speaker to play animated GIFs, along with sound, from a microSD card.

![Hero image (actual product)](docs/img/hero.jpg)
![Hero image (render)](docs/img/hero.png)

## How do I build one?

You need:

* Some parts listed below
* Some soldering experience
* A 3D printer
* A computer:
  * Running macOS or Linux
  * With a microSD card reader
  * With FFmpeg installed

For Windows, you will need to modify the `convert.sh` script to make it compatible (PRs welcome!).

### Step 1: Buy parts

You need these parts:

| Part                                                                           | Purpose                                | Price (USD) |
|--------------------------------------------------------------------------------|----------------------------------------|-------------|
| [Adafruit ESP32-S3 Reverse TFT Feather](https://www.adafruit.com/product/5691) | The brains of the project, plus an LCD | $24.95      |
| [Adafruit SDIO breakout](https://www.adafruit.com/product/4682)                | Stores GIFs and WAVs                   | $3.50       |
| [Adafruit I2S MAX98357A breakout](https://www.adafruit.com/product/3006)       | Plays audio to a speaker               | $5.95       |
| [Mini oval speaker](https://www.adafruit.com/product/3923)                     | An itty bitty speaker                  | $1.95       |
| [Female headers for Feathers](https://www.adafruit.com/product/2886)           | Physical assembly                      | $0.95       |
| [400mAH battery for Feathers](https://www.adafruit.com/product/3898)           | The battery                            | $6.95       |

You'll need these supplies:

* [Solid core hookup wire](https://www.adafruit.com/product/1311)
* Some screws:
  * 2&times; M2.5x4 screws
  * 2&times; M2x4 screws
  * 4&times; [countersunk self-tapping M2x6 screws](https://www.amazon.com/dp/B09DB5SMCZ?th=1)
* A microSD card (almost any size)
* A paperclip
* A USB C cable

### Step 2: Print the enclosure

Print the following parts. Use 0.2mm or smaller layer height. Infill settings don't matter. The STLs are in the latest release download on GitHub.

* Print `Case.stl` with the screen opening on the print bed. Use paint-on supports for the four screw holes in the corners of the case.
* Print `Backplate.stl` with the two ribs facing upwards.
* Print `Button.stl` with the wider part of the button on the print bed.

If your printer doesn't bridge well, also add supports to the USB C hole in `Case.stl`.

### Step 3: Make a GIF

The repository includes a script called `convert.sh` that runs FFmpeg with very specific arguments to make GIFs and WAVs compatible with the board.

At least one GIF must be loaded onto the SD card for the project to function. The GIF follows a very specific format. A random GIF you find on the internet is unlikely to work. Here's how to make a compatible GIF and WAV.

First, prepare your environment. This only needs to be done once:

1. In a terminal, `cd` to the project's directory and run `chmod +x convert.sh` to make the conversion script executable.
2. Format your SD card as FAT32.
3. Create two directories in the root: `gifs` and `wavs`.
4. Copy `test.gif` to `gifs/` and `test.wav` to `wavs/`.

The resulting SD card structure should look like:

```
SD card root
 ├── gifs/
 │   └── test.gif
 └── wavs/
     └── test.wav
```

Now, convert a video into a compatible GIF and WAV format:

1. Download a video to your computer that you want to load. Pick something only a few seconds long. Let's assume it's called `test.mp4` and you stored it in the same directory as this project.
2. Open a terminal and `cd` to the project's directory.
4. Run `./convert.sh test.mp4`. This outputs `test.gif` and test.wav in the same directory.

### Step 4: Physical assembly

Read these instructions fully before actually starting! Everything is a very tight fit in the enclosure and the details matter.

Here's a logical diagram of how the components are connected, for reference. In particular, note the following:

* The Feather's `GND` pin is shared, but two separate `3V` pins are used
* The I2S amplifier aligns to `MI` through `TX`; the _first_ 3V pin gets wired to the I2S amplifier's power pin
* The SD breakout aligns to the _second_ 3V pin, the GND pin, and various GPIO pins up through `SCK`; all the SD breakout's connections are through headers, not wires
* `RST`, `MO`, and `DB` are unused on the row of 16 pins
* None of the pins on the row of 12 pins are used

![Fritzing diagram](docs/img/fritzing.png)

The male headers that come with the boards may not match the number of pins. You can twist apart the male headers at the perforations to get the right number of pins.

1. Solder headers to various boards. Be sure you get the pins right! You will not use all the headers!
   1. Solder the 16-pin female header to the Feather's row of 16 pins (`RST` through `DB`). **Do not solder the 12-pin female header.** The female portion of the header is on the opposite of the board as the LCD, like this:
   
      ![Feather with female headers](docs/img/feather.png)
      
   2. Solder a male header to the SD card breakout. The SD card slot faces up and the male header faces down, like this:
   
      ![SD card breakout with male headers](docs/img/sd.png)

   3. Solder a male header **with only three** pins to the I2S amplifier breakout. Solder the `LRC`, `BCLK`, and `DIN` pins like this. Note that in this picture, the screw terminals if included with your board aren't shown, but would be on the top with the headers facing down. The components are also on the top.

      ![I2S amplifier with male headers](docs/img/i2s.png)

2. If your I2S amplifier came with presoldered screw terminals, you'll need to remove them because they take up too much space. Gently twist and rock them back and forth a whole bunch until you can work them free, or if you're able, desolder the pins. Be careful and take your time.

3. Now you'll connect the speaker to the I2S amplifier.

   1. Cut the tiny male JST connector off the speaker. You'll need every bit of wire possible, so cut off the connector as close to the wires as possible.
   2. Strip about 3 mm from the red and black wires from the speaker. The wire is very thin and you won't have much to work with, so be careful!
   3. Solder the black wire to the `-` pin on the I2S amplifier. This is one of the pins that are now exposed after you removed the screw terminals.
   4. Solder the red wire to the `+` pin.

4. Lastly, you need to solder a couple wires to the I2S amplifier for power and ground. This isn't necessary for the SD breakout because the pins already line up. Once you're done, the `GAIN` and `SD` pins should be unoccupied, but the other pins all connected to either headers or wires.

   1. Cut a black wire about 5 cm long and strip both ends at about 3 mm. Solder one end to the `GND` pin on the I2S amplifier. Solder the other end to the `GND` pin on the SD breakout. This means that the `GND` pin on the SD breakout will be soldered to both a male header pin and the wire, so it'll be tricky to solder.
   2. Cut a red wire about 5 cm long. Strip one end to 3 mm, and the other to about 6 mm. Solder the 3mm end to the `VIN` pin of the I2S amplifier. Leave the other end free for now.

5. Install the parts into the enclosure:

   1. Place the tiny 3D printed button in its hole. The narrow end goes through the front of the case. The wider part keeps it in place, like this:

      ![Button in place](docs/img/button.png)

   2. Connect the battery to the Feather. Definitely from this point onwards be careful aligning things and poking around with screwdrivers so you don't short any components!
   
   3. Screw the Feather into place. Use the M2.5x4mm screws for the larger holes and M2x4mm screws for the smaller ones. Only tighten them enough to keep the board in place. Overtightening will easily strip the plastic and you'll need to reprint the case. Once done, test pressing the button to make sure it moves freely and actuates the reset button on the Feather itself.

      ![Feather in place](docs/img/feather-in-place.png)

   4. Insert your microSD card into the SD reader, then install the SD reader and I2S amplifier. Note the alignment. **Be sure to align them to the pins shown!** The first two pins closest to the USB C port should be empty, along with the last pin closest to the ESP32. When doing so, tuck the battery under the breakouts (the white box).

      ![Breakouts in place](docs/img/breakouts-in-place.png)
      ![Breakouts in place](docs/img/battery.png)

   5. Take the loose end of the red wire and put it in the second pin closest to the USB C port (`3V`) of the female header.

   6. Remove the white cover for the adhesive on the sticker, and shove it into place by the speaker hole with the grille facing outwards. It may be tricky to maneuver it around the battery connection.

      ![Speaker in place](docs/img/speaker.png)

6. You're done! Screw the backplate on with the ribs facing inward by the components using the self-tapping screws.

### Step 5: Software installation

1. Install the Arduino IDE and set it up for this specific board per [Adafruit's instructions](https://learn.adafruit.com/esp32-s3-reverse-tft-feather/arduino-ide-setup-2).
2. Open `software/Arduino/Arduino.ino` and install the following libraries via Sketch → Include Library → Manage Libraries. Install dependencies if prompted.
   1. "Adafruit GFX Library" by Adafruit
   2. "Adafruit ST7735 and ST7789 Library" by Adafruit
   3. "AnimatedGIF" by Larry Bank
3. Connect the board to your computer via a USB C cable.
4. Using a paperclip, press and hold D0 (top-left pinhole on the board when viewed with the USB port on the left). While holding D0, press the Reset button, then release D0.
5. Connect to the board in the Arduino IDE.
6. Select **Tools** → **Partition Scheme** and "Huge APP (3MB No OTA/1MB SPIFFS)" or a similar entry. This prevents a bootloader screen from appearing if you press the Reset button twice with a specific timing.
7. Compile and upload the sketch.

Press the Reset button. Your GIF and WAV should play! Once the GIF is done, the screen fades off and the board sleeps. Pressing the Reset button wakes it back up and a GIF/WAV play again.

## Usage

The physical 3D printed button presses the Reset button on the Feather, playing a random GIF. The code will avoid playing the same GIF twice  if more than one GIF is loaded.

The battery should last a long time, but it's not a good idea to let it discharge too deeply. Charge the board with a USB C cable periodically. It is safe to keep a USB cable connected even after the battery is done charging.

### Loading more GIFs

There are two ways to load a GIF and WAV.

The fastest way for a lot of GIFs and WAVs is directly via the SD card. Consider this option when you first build the project. Once you've build the project, it can be a pain to take it apart to get the SD card out.

The more convenient way is by a USB cable, but transfer speeds are much slower than using the SD card directly:

* Connect the project to your computer via USB C.
* Use a paperclip to hold down the D2 button. When viewed with the USB C port on the left, this is the bottom-left pinhole.
* While holding down D2, press Reset, then release D2.

This reboots the board into USB mass storage mode with a message appearing on the screen stating that USB is connected. A new drive will appear on your computer when connected by USB. The name will be the same as whatever the SD card was named when you formatted it. Once you're done managing the files, eject the drive from your computer, then press the Reset button.

All the GIFs and WAVs you load must be created using the `convert.sh` script! Don't use GIFs or WAVs you find on the internet because they will not likely be compatible with these specs. Always process all your files through `convert.sh`, even if the source itself is a GIF, to be sure it's compatible with the project.

### About GIF creation

The WAV for a GIF is optional, so if your source file has no sound, or you just don't want sound, delete the resulting `.wav` file.

`convert.sh` does several things:

* Forces the FPS to 15 by default
* Forces the resolution to exactly 240x135, which is the native resolution of the screen. GIFs are letterboxed or pillarboxed as necessary to keep the aspect ratio and to match this resolution.
* Forces a specific color palette
* Outputs a WAV file at 16 KHz mono

## Known issues

* **The OpenSCAD code is terrible and a lot of the math is wrong if you adjust some settings, like surface thickness.** Sure is. I needed to build this quickly so I hardcoded a _lot_ of things. I welcome any attempts to clean up or just completely redo the enclosure code.
* **Some of the Arduino code is also terrible.** Because it is largely and shamefully AI generated for the same reason of time constraints, plus C++ not being my speciality. At least it works.
* **The USB transfer speed is super slow.** That's a limitation of the board. I don't think there's any way to make it faster. If you want to transfer a lot of data, take out the SD card and put it in a card reader.
* **If I load a lot of GIFs, some don't show up in rotation.** The code artificially limits the list of GIFs to 64, but you can increase this.
* **There's no low battery warning.** Yes I should add that. There's an onboard I2C battery monitor that should work for this purpose; I just haven't programmed that in yet.
* **Audio drifts out of sync with longer GIFs.** Probably, yeah. There's no code to sync the two and instead the code relies on the GIF playing back smoothly and assumes the audio does the same.