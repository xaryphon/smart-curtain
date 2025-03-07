import logging
import requests
from homeassistant.core import HomeAssistant
from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.components.select import SelectEntity
from homeassistant.const import CONF_URL
from .device import DeviceMode

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    device = config_entry.runtime_data
    select = SmartCurtainSelect("Mode", device)
    async_add_entities([select])

class SmartCurtainSelect(SelectEntity):
    _attr_options = [x.value for x in (DeviceMode.MANUAL, DeviceMode.AUTO_STATIC, DeviceMode.AUTO_HOURLY)]
    _attr_should_poll = False

    def __init__(self, name, device):
        self._name = name
        self._device = device
        device._entities.append(self)

    @property
    def name(self):
        return self._name

    @property
    def current_option(self) -> str:
        return self._device.wanted_mode.value

    def select_option(self, option: str) -> None:
        self._device.send_settings({"wanted_mode": option})

    def update(self):
        pass
