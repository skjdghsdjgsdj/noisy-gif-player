import os
import random
import struct
import time
import adafruit_st7789
import alarm
import audiobusio
import audiocore
import board
import busio
import digitalio
import displayio
import fourwire
import gifio
import microcontroller
import sdcardio
import storage
from alarm.pin import PinAlarm
from microcontroller import watchdog
from watchdog import WatchDogMode

# Ignore unsupported typehinting in CircuitPython; just for IDE's sake
try:
	# noinspection PyUnresolvedReferences
	from typing import Tuple, Optional, Final
except ImportError:
	pass

# Initialize watchdog timer early
watchdog.timeout = int(os.getenv("WATCHDOG_TIMEOUT_MS", 10000))
watchdog.mode = WatchDogMode.RESET

# Pin definitions
SD_CS_PIN = board.D11  # SD card chip select
LCD_CS_PIN = board.D5  # LCD chip select
LCD_DC_PIN = board.D6  # LCD data/command
BUTTON_PIN = board.D12  # Button input (active LOW to GND)
I2S_BCLK_PIN = board.RX  # I2S bit clock
I2S_LRC_PIN = board.TX  # I2S left/right clock
I2S_DIN_PIN = board.D10  # I2S data input

class Hardware:
	def __init__(self, preferred_spi_frequency: int):
		self.spi: Optional[busio.SPI] = None
		self.display: Optional[displayio.Display] = None
		self.i2s: Optional[audiobusio.I2SOut] = None
		self.sd: Optional[sdcardio.SDCard] = None
		self.vfs: Optional[storage.VfsFat] = None
		self.sd_cs_pin: Optional[microcontroller.Pin] = None
		self.button: Optional[Button] = None
		self.button_pin: Optional[microcontroller.Pin] = None
		self.init_spi(preferred_spi_frequency)

	def init_spi(self, preferred_frequency: int) -> None:
		self.spi = board.SPI()
		while not self.spi.try_lock():
			pass

		self.spi.configure(baudrate = preferred_frequency)
		actual_speed = self.spi.frequency
		self.spi.unlock()
		print(f"SPI set to {actual_speed // 1000000} MHz (requested {preferred_frequency // 1000000} MHz)")

	def init_sdcard(self, sd_cs_pin: microcontroller.Pin):
		print("Init SD card...", flush=True, end = "")
		self.sd_cs_pin = sd_cs_pin
		# noinspection PyTypeChecker
		self.sd = sdcardio.SDCard(
			bus = self.spi,
			cs = digitalio.DigitalInOut(self.sd_cs_pin),
			baudrate = self.spi.frequency
		)
		self.vfs = storage.VfsFat(self.sd)
		storage.mount(self.vfs, "/sd", readonly = True)

	def init_display(self,
					 dc_pin: microcontroller.Pin,
					 cs_pin: microcontroller.Pin,
					 width: int,
					 height: int,
					 rotation: int = 0):
		print("Init display...", flush=True, end = "")
		displayio.release_displays()
		display_bus = fourwire.FourWire(
			self.spi, command = dc_pin, chip_select = cs_pin, baudrate = self.spi.frequency
		)
		self.display = adafruit_st7789.ST7789(
			bus = display_bus,
			width = width,
			height = height,
			rotation = rotation
		)
		print("done")

	def init_audio(self,
				   bclk_pin: microcontroller.Pin,
				   ws_pin: microcontroller.Pin,
				   data_pin: microcontroller.Pin):
		print("Init audio...", flush=True, end = "")
		self.i2s = audiobusio.I2SOut(
			bit_clock = bclk_pin,
			word_select = ws_pin,
			data = data_pin
		)
		print("done")

	def init_button(self, button_pin: microcontroller.Pin) -> None:
		self.button_pin = button_pin
		self.button = Button(pin = self.button_pin)

class Button:
	def __init__(self, pin: microcontroller.Pin, debounce_time: float = 0.05):
		self.dio = digitalio.DigitalInOut(pin)
		self.dio.direction = digitalio.Direction.INPUT
		self.dio.pull = digitalio.Pull.UP

		self.debounce_time = debounce_time

	def wait_for_press(self, timeout: float = None) -> tuple[bool, float]:
		start = time.monotonic()

		# wait until the initial press
		while self.dio.value:
			time.sleep(self.debounce_time)
			watchdog.feed()

			if timeout and time.monotonic() - start > timeout:
				return False, 0

		# wait until the button is released
		start = time.monotonic()
		while not self.dio.value:
			time.sleep(self.debounce_time)
			watchdog.feed()

		return True, time.monotonic() - start

class MediaLibrary:
	def __init__(self, gif_dir: str, wav_dir: str):
		self.gif_dir = gif_dir
		self.wav_dir = wav_dir

	def get_random_media_pair(self, avoid_gif_path = None) -> tuple[str, Optional[str]]:
		all_files = os.listdir(self.gif_dir)
		gifs = [f for f in all_files if f.endswith(".gif") and not f.startswith(".")]
		if not gifs:
			raise RuntimeError(f"No GIF files found in {self.gif_dir}/")

		if avoid_gif_path and len(gifs) > 1:
			try:
				gifs.remove(avoid_gif_path)
			except ValueError as e:
				print(f"Couldn't remove {avoid_gif_path} from list; ignoring: {e}")

		gif_file = random.choice(gifs)
		gif_path = f"{self.gif_dir}/{gif_file}"

		base_name = gif_file[:-4]  # Remove .gif extension
		wav_path = f"{self.wav_dir}/{base_name}.wav"

		return gif_path, wav_path if os.path.exists(wav_path) else None

class MediaRenderer:
	def __init__(self, hardware: Hardware):
		self.hardware = hardware

	def play_wav(self, wav_path: str):
		# noinspection PyArgumentList
		self.hardware.i2s.play(audiocore.WaveFile(wav_path))

	def play_gif(self, gif_path: str) -> None:
		# suspend updates for direct writes to the LCD
		self.hardware.display.auto_refresh = False

		rendered_frames = 0
		with gifio.OnDiskGif(gif_path) as gif:
			# assume a fixed overhead time in loading frames and account for that in the delay between frames
			start = time.monotonic()
			next_delay = gif.next_frame()
			end = time.monotonic()
			overhead = end - start

			while True:
				time.sleep(max(0.0, next_delay - overhead))
				next_delay = gif.next_frame()

				# direct display writes adapted from
				# https://docs.circuitpython.org/en/latest/shared-bindings/gifio/#gifio.OnDiskGif
				bus = self.hardware.display.bus
				bus.send(42, struct.pack(">hh", 0, gif.bitmap.width - 1))
				bus.send(43, struct.pack(">hh", 0, gif.bitmap.width - 1))
				bus.send(44, gif.bitmap)

				watchdog.feed()

				rendered_frames += 1
				if rendered_frames >= gif.frame_count:
					print(f"Done playing {gif_path}: rendered {rendered_frames} frames")
					break

		self.hardware.display.auto_refresh = True

class App:
	def __init__(self, hardware: Hardware, renderer: MediaRenderer, library: MediaLibrary):
		self.hardware = hardware
		self.renderer = renderer
		self.library = library

	def enter_deep_sleep(self) -> None:
		pin_alarm = PinAlarm(pin = self.hardware.button_pin, value = False, pull = True)
		alarm.exit_and_deep_sleep_until_alarms(pin_alarm)

	def main_loop(self) -> None:
		last_gif_path = None
		long_hold_time = int(os.getenv("BUTTON_LONG_HOLD_SECONDS", 5))
		idle_timeout = int(os.getenv("IDLE_TIMEOUT_SECONDS", 60))
		while True:
			gif_path, wav_path = library.get_random_media_pair(last_gif_path)

			if wav_path:
				renderer.play_wav(wav_path)

			renderer.play_gif(gif_path)
			last_gif_path = gif_path

			was_pressed, hold_time = self.hardware.button.wait_for_press(timeout = idle_timeout)

			if not was_pressed or hold_time > long_hold_time:
				print(f"Idle timeout of {idle_timeout}s reached or button was held down, entering deep sleep")
				self.enter_deep_sleep()


hardware = Hardware(int(os.getenv("SPI_PREFERRED_SPEED_HZ", 40_000_000)))
hardware.init_sdcard(sd_cs_pin = LCD_CS_PIN) # don't init other SPI devices before the SD card or it won't work
hardware.init_display(
	dc_pin = LCD_DC_PIN,
	cs_pin = LCD_CS_PIN,
	width = int(os.getenv("LCD_WIDTH", 320)),
	height = int(os.getenv("LCD_HEIGHT", 170)),
	rotation = int(os.getenv("LCD_ROTATION", 0))
)
hardware.init_audio(bclk_pin = I2S_BCLK_PIN, ws_pin = I2S_LRC_PIN, data_pin = I2S_DIN_PIN)
hardware.init_button(BUTTON_PIN)

renderer = MediaRenderer(hardware)
library = MediaLibrary(
	gif_dir = os.getenv("GIFS_PATH", "/sd/gifs"),
	wav_dir = os.getenv("WAVS_PATH", "/sd/wavs")
)

App(
	hardware = hardware,
	renderer = renderer,
	library = library
).main_loop()