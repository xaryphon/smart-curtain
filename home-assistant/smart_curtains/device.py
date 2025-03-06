import logging
import requests
from homeassistant.core import HomeAssistant
from enum import Enum
from typing import Tuple

_LOGGER = logging.getLogger(__name__)

HourlyTargets = Tuple[float, float, float, float, float, float,
                      float, float, float, float, float, float,
                      float, float, float, float, float, float,
                      float, float, float, float, float, float]

class DeviceMode(Enum):
    CALIBRATING = "calibrating"
    MANUAL = "manual"
    AUTO_STATIC = "auto_static"
    AUTO_HOURLY = "auto_hourly"

class Device:
    def __init__(self, hass: HomeAssistant, url: str):
        self._hass = hass
        self._url = url
        self._mode = DeviceMode.CALIBRATING
        self._motor_target = 0
        self._motor_current = 0
        self._lux_target = 0
        self._lux_current = 0
        self._wanted_mode = DeviceMode.MANUAL
        self._manual_target = 0
        self._auto_static_target = 0
        self._auto_hourly_targets = [0 for _ in range(24)]

    def update_from_data(self, json: dict):
        mode = json.get("mode")
        if mode is not None:
            self._mode = DeviceMode(mode)
        motor = json.get("motor", {})
        self._motor_target = motor.get("target", self._motor_target)
        self._motor_current = motor.get("current", self._motor_current)
        lux = json.get("lux", {})
        self._lux_target = lux.get("target", self._lux_target)
        self._lux_current = lux.get("current", self._lux_current)

        mode = json.get("wanted_mode")
        if mode is not None:
            self._wanted_mode = DeviceMode(mode)
        manual = json.get("manual", {})
        self._manual_target = json.get("target", self._manual_target)
        auto_static = json.get("auto_static", {})
        self._auto_static_target = auto_static.get("target", self._auto_static_target)
        auto_hourly = json.get("auto_hourly", {})
        self._auto_hourly_targets = auto_hourly.get("targets", self._auto_hourly_targets)

    @property
    def mode(self) -> DeviceMode:
        return self._mode

    @property
    def motor_target(self) -> int:
        return self._motor_target

    @property
    def motor_current(self) -> int:
        return self._motor_current

    @property
    def lux_target(self) -> float:
        return self._lux_target

    @property
    def lux_current(self) -> float:
        return self._lux_current

    @property
    def wanted_mode(self) -> DeviceMode:
        return self._wanted_mode

    @property
    def manual_target(self) -> int:
        return self._manual_target

    @property
    def auto_static_target(self) -> float:
        return self._auto_static_target

    @property
    def auto_hourly_targets(self) -> HourlyTargets:
        return self._auto_hourly_targets

    @property
    def unique_id(self) -> str:
        return self._unique_id

    def query_status(self):
        try:
            response = requests.get(f"{self._url}/status/full")
            if response.status_code == 200:
                self.update_from_data(response.json())
            else:
                _LOGGER.error(f"Failed to query device status")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error trying to query device status: {e}")

    def send_settings(self, data: dict):
        try:
            response = requests.post(f"{self._url}/settings", json=data)
            if response.status_code == 200:
                _LOGGER.info(f"Successfully sent new settings: {data}")
                self.update_from_data(response.json())
            else:
                _LOGGER.error(f"Failed to send new settings: {data}")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error sending new settings: {data}: {e}")

    def calibrate(self):
        try:
            response = requests.post(f"{self._url}/calibrate")
            if response.status_code == 202:
                _LOGGER.info(f"Successfully sent calibration command")
            else:
                _LOGGER.error(f"Failed to send calibration command")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error sending calibration command: {e}")
