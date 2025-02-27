import logging
import requests
from homeassistant.core import HomeAssistant
from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.components.switch import SwitchEntity, SwitchDeviceClass
from homeassistant.const import CONF_URL

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    switch = SmartCurtainSwitch(config_entry.data.get('name'), config_entry.data.get('url'))
    async_add_entities([switch])

class SmartCurtainSwitch(SwitchEntity):
    _attr_device_class = SwitchDeviceClass.SWITCH

    def __init__(self, name, url):
        self._name = f"{name}: Automatic Mode"
        self._url = url
        self._is_on = False

    @property
    def name(self):
        return self._name

    @property
    def is_on(self) -> bool:
        return self._is_on

    def turn_on(self, **kwargs) -> None:
        try:
            response = requests.post(f"{self._url}/mode/automatic")
            if response.status_code == 200:
                _LOGGER.info(f"Automatic mode enabled.")
                _is_on = True
            else:
                _LOGGER.error(f"Failed to enable automatic mode.")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error enabling automatic mode: {e}")

    def turn_off(self, **kwargs) -> None:
        try:
            response = requests.post(f"{self._url}/mode/manual")
            if response.status_code == 200:
                _LOGGER.info(f"Automatic mode disabled.")
                _is_on = False
            else:
                _LOGGER.error(f"Failed to disable automatic mode.")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error disabling automatic mode: {e}")

    def update(self):
        try:
            response = requests.get(f"{self._url}/mode")
            if response.status_code == 200:
                data = response.json()
                self._is_on = data["mode"] == 'automatic'
                _LOGGER.info(f"Updated state: {self._is_on}")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error fetching auto state: {e}")
