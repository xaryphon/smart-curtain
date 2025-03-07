import logging
import requests
from homeassistant.core import HomeAssistant
from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.components.cover import CoverEntity, CoverDeviceClass, CoverEntityFeature
from homeassistant.const import CONF_URL, STATE_OPEN, STATE_CLOSED

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    device = config_entry.runtime_data
    cover = SmartCurtainCover("Curtains", device)
    async_add_entities([cover])

class SmartCurtainCover(CoverEntity):
    _attr_device_class = CoverDeviceClass.CURTAIN
    _attr_supported_features = CoverEntityFeature.OPEN | CoverEntityFeature.CLOSE | CoverEntityFeature.SET_POSITION
    _attr_should_poll = False

    def __init__(self, name, device):
        self._name = name
        self._device = device
        device._entities.append(self)

    @property
    def name(self):
        return self._name

    @property
    def is_closed(self):
        return self._device.motor_current == 0

    @property
    def is_closing(self):
        return self._device.motor_target < self._device.motor_current

    @property
    def is_opening(self):
        return self._device.motor_target > self._device.motor_current

    @property
    def current_cover_position(self):
        return self._device.motor_current

    def open_cover(self, **kwargs):
        self._device.send_settings({"wanted_mode": "manual", "manual": {"target": 0}})

    def close_cover(self, **kwargs):
        self._device.send_settings({"wanted_mode": "manual", "manual": {"target": 100}})

    def set_cover_position(self, **kwargs):
        position = int(kwargs['position'])
        self._device.send_settings({"wanted_mode": "manual", "manual": {"target": 100 - position}})
