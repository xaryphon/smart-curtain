import logging
import requests
from homeassistant.core import HomeAssistant
from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.components.sensor import SensorEntity, SensorDeviceClass, SensorStateClass
from homeassistant.const import CONF_URL
from .device import DeviceMode

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    device = config_entry.runtime_data
    lux = SmartCurtainSensor("Ambient Light", device)
    mode = SmartCurtainModeSensor("Current Mode", device)
    async_add_entities([lux, mode])

class SmartCurtainSensor(SensorEntity):
    _attr_device_class = SensorDeviceClass.ILLUMINANCE
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = "lx"
    _attr_should_poll = False

    def __init__(self, name, device):
        self._name = name
        self._device = device
        device._entities.append(self)

    @property
    def name(self):
        return self._name

    @property
    def native_value(self) -> float:
        return self._device.lux_current

    def update(self):
        pass

class SmartCurtainModeSensor(SensorEntity):
    _attr_device_class = SensorDeviceClass.ENUM
    _attr_options = [x.value for x in DeviceMode]
    _attr_should_poll = False

    def __init__(self, name, device):
        self._name = name
        self._device = device
        device._entities.append(self)

    @property
    def name(self):
        return self._name

    @property
    def native_value(self) -> str:
        return self._device.mode.value
