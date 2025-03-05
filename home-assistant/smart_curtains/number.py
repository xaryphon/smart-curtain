import logging
import requests
from homeassistant.core import HomeAssistant
from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.components.number import NumberEntity, NumberDeviceClass
from homeassistant.const import CONF_URL

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    numbers = [SmartCurtainNumber(config_entry.data.get('name'), config_entry.data.get('url'), i) for i in range(24)]
    async_add_entities(numbers)

class SmartCurtainNumber(NumberEntity):
    _attr_device_class = NumberDeviceClass.ILLUMINANCE
    _attr_native_unit_of_measurement = "lx"
    _attr_mode = "box"
    _attr_native_step = 0.05
    _attr_native_max_value = 100000
    _attr_native_min_value = 0

    def __init__(self, name, url, i):
        self._name = f"{name} {i:02}h"
        self._url = url
        self._value = 0
        self._hour = i

    @property
    def name(self):
        return self._name

    @property
    def native_value(self) -> float:
        return self._value

    def set_native_value(self, value: float) -> None:
        try:
            response = requests.post(f"{self._url}/target/{value}")
            if response.status_code == 200:
                _LOGGER.info(f"Lux target changed to {value}.")
            else:
                _LOGGER.error(f"Failed to set lux target to {value}.")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error setting lux target to {value}: {e}")

    def update(self):
        try:
            response = requests.get(f"{self._url}/target")
            if response.status_code == 200:
                data = response.json()
                self._value = float(data["value"])
                _LOGGER.info(f"Updated state: {self._value}")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error fetching target state: {e}")
