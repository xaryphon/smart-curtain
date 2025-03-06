import logging
import requests
from homeassistant.core import HomeAssistant
from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.components.button import ButtonEntity
from homeassistant.const import CONF_URL

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    device = config_entry.runtime_data
    button = SmartCurtainButton(config_entry.data.get('name'), device)
    async_add_entities([button])

class SmartCurtainButton(ButtonEntity):
    def __init__(self, name, device):
        self._name = name + " Calibrate"
        self._device = device

    @property
    def name(self):
        return self._name

    def press(self) -> None:
        self._device.calibrate()

    def update(self):
        pass
