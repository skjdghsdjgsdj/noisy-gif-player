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
import terminalio
from adafruit_display_text.label import Label
from alarm.pin import PinAlarm
from microcontroller import watchdog
from watchdog import WatchDogMode

# Try importing typing for PyCharm/IDE support, ignore on CircuitPython
try:
	# noinspection PyUnresolvedReferences
	from typing import Tuple, Optional, Final
except ImportError:
	pass

# Load settings from settings.toml with defaults
WATCHDOG_TIMEOUT_MS: Final[int] = int(os.getenv("WATCHDOG_TIMEOUT_MS", 10000))
SPI_PREFERRED_SPEED_HZ: Final[int] = int(os.getenv("SPI_PREFERRED_SPEED_HZ", 40_000_000))
SPI_FALLBACK_SPEED_HZ: Final[int] = int(os.getenv("SPI_FALLBACK_SPEED_HZ", 25_000_000))
LCD_WIDTH: Final[int] = int(os.getenv("LCD_WIDTH", 320))
LCD_HEIGHT: Final[int] = int(os.getenv("LCD_HEIGHT", 170))
LCD_ROTATION: Final[int] = int(os.getenv("LCD_ROTATION", 0))
GIFS_PATH: Final[str] = os.getenv("GIFS_PATH", "/sd/gifs")
WAVS_PATH: Final[str] = os.getenv("WAVS_PATH", "/sd/wavs")
USB_MODE_HOLD_SECONDS: Final[int] = int(os.getenv("USB_MODE_HOLD_SECONDS", 5))
IDLE_TIMEOUT_SECONDS: Final[int] = int(os.getenv("IDLE_TIMEOUT_SECONDS", 60))

# Initialize watchdog timer early
watchdog.timeout = WATCHDOG_TIMEOUT_MS
watchdog.mode = WatchDogMode.RESET
print(f"Watchdog timer started with {WATCHDOG_TIMEOUT_MS}ms timeout.")

# Pin definitions (constants)
SD_CS_PIN = board.D11  # SD card chip select
LCD_CS_PIN = board.D5  # LCD chip select
LCD_DC_PIN = board.D6  # LCD data/command
BUTTON_PIN = board.D12  # Button input (active LOW to GND)
I2S_BCLK_PIN = board.RX  # I2S bit clock
I2S_LRC_PIN = board.TX  # I2S left/right clock
I2S_DIN_PIN = board.D10  # I2S data input

# Global state tracking
usb_mode_active = False
current_display_group = None

class Hardware:
	def __init__(self, *spi_frequencies: int):
		self.spi = None
		self.init_spi(*spi_frequencies)

	def init_spi(self, *frequencies: int) -> None:
		"""Configure SPI bus, trying frequencies in order until acceptable actual frequency achieved."""
		if not frequencies:
			raise ValueError("At least one frequency must be provided")

		self.spi = board.SPI()
		while not self.spi.try_lock():
			pass

		requested_speed = frequencies[0]
		actual_speed = 0

		for freq in frequencies:
			self.spi.configure(baudrate = freq)
			actual_speed = self.spi.frequency
			if actual_speed < freq:
				print(f"Requested SPI {freq // 1000000} MHz but got {actual_speed // 1000000} MHz; trying fallback")
			requested_speed = freq
			if actual_speed >= freq or freq == frequencies[-1]:
				break

		self.spi.unlock()
		print(f"SPI requested {requested_speed // 1000000} MHz, actual {actual_speed // 1000000} MHz")

	def init_sdcard(self, sd_cs_pin: microcontroller.Pin):
		# noinspection PyTypeChecker
		sd_card = sdcardio.SDCard(self.spi, digitalio.DigitalInOut(sd_cs_pin))
		vfs = storage.VfsFat(sd_card)
		storage.mount(vfs, "/sd")

hardware = Hardware(SPI_PREFERRED_SPEED_HZ, SPI_FALLBACK_SPEED_HZ)

print("Setting up LCD display...")
displayio.release_displays()
display_bus = fourwire.FourWire(
	board.SPI(), command = LCD_DC_PIN, chip_select = LCD_CS_PIN, baudrate = board.SPI().frequency
)
display = adafruit_st7789.ST7789(display_bus, width = LCD_WIDTH, height = LCD_HEIGHT, rotation = LCD_ROTATION)
watchdog.feed()
print(f"LCD display initialized ({LCD_WIDTH}x{LCD_HEIGHT}).")

print("Setting up I2S audio output...")
i2s = audiobusio.I2SOut(bit_clock = I2S_BCLK_PIN, word_select = I2S_LRC_PIN, data = I2S_DIN_PIN)
watchdog.feed()
print("I2S audio output configured.")

class Button:
	def __init__(self, pin: microcontroller.Pin, debounce_time: float = 0.01):
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

def enter_usb_mode() -> None:
	"""Enter USB file transfer mode - make /sd card accessible via USB."""
	print("Entering USB file transfer mode - /sd accessible...")

	font = terminalio.FONT

	root_group = displayio.Group()
	center_x = display.width // 2
	line_height = font.bitmap.height + 5
	start_y = (display.height - (2 * line_height)) // 2

	root_group.append(Label(
		font = terminalio.FONT,
		text = "Ready for USB transfers",
		x = center_x,
		y = start_y,
		color = 0xFFFFFF,
		anchor_point = (0.5, 0.5)
	))

	root_group.append(Label(
		font = terminalio.FONT,
		text = "Press button when done and ejected",
		x = center_x,
		y = start_y + line_height,
		color = 0xFFFFFF,
		anchor_point = (0.5, 0.5)
	))

	display.root_group = root_group

	# Make /sd available for USB access
	storage.umount("/sd")
	print("/sd unmounted for USB access")


def exit_usb_mode():
	"""Exit USB mode, remount /sd for CircuitPython use."""
	print("Exiting USB mode, remounting /sd...")

	# Remount SD card at /sd for app use
	cs_sd = digitalio.DigitalInOut(SD_CS_PIN)
	# noinspection PyTypeChecker
	sd_card = sdcardio.SDCard(board.SPI(), cs_sd)
	vfs = storage.VfsFat(sd_card)
	storage.mount(vfs, "/sd")
	print("/sd remounted for CircuitPython use")

class MediaLibrary:
	def __init__(self, gif_dir: str, wav_dir: str):
		self.gif_dir = gif_dir
		self.wav_dir = wav_dir

	def get_random_media_pair(self, avoid_gif_path = None) -> tuple[str, Optional[str]]:
		"""Get random GIF and corresponding WAV if it exists, ignoring hidden files."""
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
	def __init__(self, display: displayio.Display, i2s: audiobusio.I2SOut):
		self.display = display
		self.i2s = i2s

	def play_wav(self, wav_path: str):
		# noinspection PyArgumentList
		self.i2s.play(audiocore.WaveFile(wav_path), loop = False)

	def play_gif(self, gif_path: str) -> None:
		# suspend updates for direct writes to the LCD
		self.display.auto_refresh = False

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
				display.bus.send(42, struct.pack(">hh", 0, gif.bitmap.width - 1))
				display.bus.send(43, struct.pack(">hh", 0, gif.bitmap.width - 1))
				display_bus.send(44, gif.bitmap)

				rendered_frames += 1
				if rendered_frames >= gif.frame_count:
					break

		self.display.auto_refresh = True


def enter_deep_sleep():
	"""Enter deep sleep, wake on button press (D12 LOW)."""
	print("Entering deep sleep, waiting for button press...")
	pin_alarm = PinAlarm(pin = BUTTON_PIN, value = False, pull = True)
	alarm.exit_and_deep_sleep_until_alarms(pin_alarm)


button = Button(pin = BUTTON_PIN)
renderer = MediaRenderer(display = display, i2s = i2s)
library = MediaLibrary(gif_dir = GIFS_PATH, wav_dir = WAVS_PATH)

# Single main loop
print(f"System ready - SD mounted at /sd (LCD: {LCD_WIDTH}x{LCD_HEIGHT}, idle timeout: {IDLE_TIMEOUT_SECONDS}s)")
last_gif_path = None

while True:
	# Play random media immediately on startup/wake
	gif_path, wav_path = library.get_random_media_pair(last_gif_path)

	if wav_path:
		renderer.play_wav(wav_path)

	renderer.play_gif(gif_path)
	last_gif_path = gif_path  # Update after successful play

	was_pressed, hold_time = button.wait_for_press(timeout = IDLE_TIMEOUT_SECONDS)

	if not was_pressed:
		print(f"Idle timeout of {IDLE_TIMEOUT_SECONDS}s reached, entering deep sleep")
		enter_deep_sleep()

	if hold_time >= USB_MODE_HOLD_SECONDS:
		print(f"Button held for {hold_time}s, entering USB mode")
		enter_usb_mode()
		button.wait_for_press()
		exit_usb_mode()