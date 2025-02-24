import logging
import requests
from homeassistant.core import HomeAssistant
from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.components.cover import CoverEntity, CoverDeviceClass, CoverEntityFeature
from homeassistant.const import CONF_URL, STATE_OPEN, STATE_CLOSED

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    cover = SmartCurtainCover(config_entry.data.get('name'), config_entry.data.get('url'))
    async_add_entities([cover])

class SmartCurtainCover(CoverEntity):
    _attr_device_class = CoverDeviceClass.CURTAIN
    _attr_supported_features = CoverEntityFeature.OPEN | CoverEntityFeature.CLOSE | CoverEntityFeature.SET_POSITION

    def __init__(self, name, url):
        self._name = name
        self._url = url
        self._state = 0
        self._direction = 0

    @property
    def name(self):
        return self._name

    @property
    def is_closed(self):
        return self._state == 0

    @property
    def is_closing(self):
        return self._direction < 0

    @property
    def is_opening(self):
        return self._direction > 0

    @property
    def current_cover_position(self):
        return self._state

    def open_cover(self, **kwargs):
        try:
            response = requests.post(f"{self._url}/open")
            if response.status_code == 200:
                _LOGGER.info("Curtains opened successfully.")
            else:
                _LOGGER.error("Failed to open curtains.")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error opening curtains: {e}")

    def close_cover(self, **kwargs):
        try:
            response = requests.post(f"{self._url}/close")
            if response.status_code == 200:
                _LOGGER.info("Curtains closed successfully.")
            else:
                _LOGGER.error("Failed to close curtains.")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error closing curtains: {e}")

    def set_cover_position(self, **kwargs):
        try:
            position = kwargs['position']
            response = requests.post(f"{self._url}/set_position/{position}")
            if response.status_code == 200:
                _LOGGER.info(f"Curtains moved to {position}.")
            else:
                _LOGGER.error(f"Failed to move curtains to position {position}.")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error moving curtains to position {position}: {e}")

    def update(self):
        try:
            response = requests.get(f"{self._url}/status")
            if response.status_code == 200:
                data = response.json()
                self._state = int(data["position"])
                self._direction = int(data["direction"])
                _LOGGER.info(f"Updated state: {self._state}")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error fetching curtain state: {e}")
