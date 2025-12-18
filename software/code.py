import os
import random
import struct
import time

import alarm
import audiobusio
import audiocore
import board
import busio
import digitalio
import displayio
import gifio
import microcontroller
import sdcardio
import sdioio
import storage
import supervisor
from alarm.pin import PinAlarm
from espidf import IDFError
from microcontroller import watchdog
from watchdog import WatchDogMode

# Ignore unsupported typehinting in CircuitPython; just for IDE's sake
try:
	# noinspection PyUnresolvedReferences
	from typing import Tuple, Optional, Final
except ImportError:
	pass

# Disable auto-reboot on code change
supervisor.runtime.autoreload = False

# Initialize watchdog timer early
timeout = int(os.getenv("WATCHDOG_TIMEOUT_S", 10))
if supervisor.runtime.usb_connected:
	print("Watchdog timer is disabled because USB is connected")
elif timeout > 0:
	try:
		watchdog.timeout = timeout
		watchdog.mode = WatchDogMode.RESET
	except IDFError as e:
		print(f"Warning: failed to set up watchdog with {timeout}s timeout: {e}")
else:
	print("Warning: watchdog timer is disabled! A manual reset will be needed if there's a crash")

# Pin definitions
I2S_LRC_PIN = board.D5 # I2S left/right clock
I2S_BCLK_PIN = board.D6 # I2S bit clock
I2S_DIN_PIN = board.D9 # I2S data input
SDIO_CLK_PIN = board.A0
SDIO_CMD_PIN = board.A2
SDIO_DET_PIN = board.SCK
SDIO_DATA_PINS = [board.A1, board.A4, board.A5, board.A3]
BUTTON_PIN = board.D1

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

	def init_spi(self, preferred_frequency: int = 0) -> None:
		self.spi = board.SPI()

		if preferred_frequency > 0:
			while not self.spi.try_lock():
				pass

			self.spi.configure(baudrate = preferred_frequency)
			actual_speed = self.spi.frequency
			self.spi.unlock()
			print(f"SPI set to {actual_speed // 1000000} MHz (requested {preferred_frequency // 1000000} MHz)")
		else:
			actual_speed = self.spi.frequency
			print(f"SPI set to {actual_speed // 1000000} MHz (no requested speed provided)")

	def init_sdcard(self, clk_pin: microcontroller.Pin, cmd_pin: microcontroller.Pin, data_pins: list[microcontroller.Pin]) -> None:
		self.sd = None

		while True:
			try:
				print("Init SD card...", end = "")
				self.sd = sdioio.SDCard(
					clock = clk_pin,
					command = cmd_pin,
					data = data_pins,
					frequency = 40000000
				)
				print("device loaded, mounting...", end = "")
				self.vfs = storage.VfsFat(self.sd)
				storage.mount(self.vfs, "/sd", readonly = True)
				print("SD card mounted")
				break
			except Exception as e:
				print(f"failed, retrying: {e}")
				time.sleep(0.1)

	def init_display(self):
		self.display = board.DISPLAY

		# suspend updates for direct writes to the LCD
		self.display.auto_refresh = False

		print("done")

	def init_audio(self,
				   bclk_pin: microcontroller.Pin,
				   ws_pin: microcontroller.Pin,
				   data_pin: microcontroller.Pin):
		print("Init audio...", end = "")
		self.i2s = audiobusio.I2SOut(
			bit_clock = bclk_pin,
			word_select = ws_pin,
			data = data_pin
		)
		print("done")

	def init_button(self, button_pin: microcontroller.Pin) -> None:
		print("Init button...", end = "")
		self.button_pin = button_pin
		self.button = Button(pin = self.button_pin)
		print("done")

class Button:
	def __init__(self, pin: microcontroller.Pin, debounce_time: float = 0.05):
		self.dio = digitalio.DigitalInOut(pin)
		self.dio.direction = digitalio.Direction.INPUT
		self.dio.pull = digitalio.Pull.DOWN

		self.debounce_time = debounce_time

	def wait_for_press(self, timeout: float = None) -> tuple[bool, float]:
		start = time.monotonic()

		# wait until the initial press
		while not self.dio.value:
			time.sleep(self.debounce_time)
			watchdog.feed()

			if timeout and time.monotonic() - start > timeout:
				return False, 0

		# wait until the button is released
		start = time.monotonic()
		while self.dio.value:
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

		wav_exists = False
		# noinspection PyBroadException
		try:
			os.stat(wav_path)
			wav_exists = True
		except Exception:
			pass

		return gif_path, wav_path if wav_exists else None

class MediaRenderer:
	def __init__(self, hardware: Hardware):
		self.hardware = hardware

	def play_wav(self, wav_path: str) -> None:
		print(f"Playing {wav_path}")
		# noinspection PyArgumentList
		self.hardware.i2s.play(audiocore.WaveFile(wav_path))

	def play(self, gif_path: str, wav_path: Optional[str]) -> None:
		print(f"Rendering {gif_path}")

		try:
			print(f"Loading {gif_path}...", end = "")
			gif = gifio.OnDiskGif(gif_path)
			print("done")
		except Exception as e:
			print(f"Failed to load {gif_path}: {e}")
			return

		rendered_frames = 0
		deficit_frames = 0
		try:
			overhead = 0
			next_delay = 0

			packed_width = 0
			packed_height = 0

			x_offset = 40
			y_offset = 51

			is_first_frame = True

			while True:
				delay = max(0.0, next_delay - overhead)
				time.sleep(delay)

				if packed_width == 0 or packed_height == 0:
					packed_width = struct.pack(">hh", x_offset, gif.bitmap.width - 1 + x_offset)
					packed_height = struct.pack(">hh", y_offset, gif.bitmap.height - 1 + y_offset)
				elif delay <= 0.01:
					print(f"Warning: Not delaying between frames; overhead deficit of {int(abs(overhead - next_delay) * 1000)}ms")
					deficit_frames += 1

				start = time.monotonic()
				next_delay = gif.next_frame()

				# direct display writes adapted from
				# https://docs.circuitpython.org/en/latest/shared-bindings/gifio/#gifio.OnDiskGif
				self.hardware.display.bus.send(42, packed_width)
				self.hardware.display.bus.send(43, packed_height)
				self.hardware.display.bus.send(44, gif.bitmap)

				watchdog.feed()

				overhead = time.monotonic() - start

				rendered_frames += 1
				if rendered_frames >= gif.frame_count:
					print(f"Done playing {gif_path}: rendered {rendered_frames} frames")
					if deficit_frames > 0:
						print(f"Warning: {int(deficit_frames / rendered_frames * 100)}% of frames had no deliberate delay")
					break

				if is_first_frame:
					if wav_path:
						self.play_wav(wav_path)
					is_first_frame = False
		finally:
			gif.deinit()

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
			print("Picking random media...", end = "")
			gif_path, wav_path = library.get_random_media_pair(last_gif_path)
			print("done")

			renderer.play(gif_path, wav_path)
			last_gif_path = gif_path

			print("Waiting for button press or timeout")
			was_pressed, hold_time = self.hardware.button.wait_for_press(timeout = idle_timeout)
			print("Timeout reached" if not was_pressed else "Button held" if hold_time > long_hold_time else "Button pressed")

			if not was_pressed or hold_time > long_hold_time:
				print(f"Entering deep sleep")
				self.enter_deep_sleep()


hardware = Hardware(int(os.getenv("SPI_PREFERRED_SPEED_HZ", 80_000_000)))
hardware.init_sdcard(
	clk_pin = SDIO_CLK_PIN,
	cmd_pin = SDIO_CMD_PIN,
	data_pins = SDIO_DATA_PINS
)
hardware.init_display()
hardware.init_audio(bclk_pin = I2S_BCLK_PIN, ws_pin = I2S_LRC_PIN, data_pin = I2S_DIN_PIN)
hardware.init_button(button_pin = BUTTON_PIN)

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