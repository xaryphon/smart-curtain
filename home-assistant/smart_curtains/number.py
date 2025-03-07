import logging
import requests
from homeassistant.core import HomeAssistant
from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.components.number import NumberEntity, NumberDeviceClass
from homeassistant.const import CONF_URL

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    name = config_entry.data.get('name')
    device = config_entry.runtime_data
    numbers = [SmartCurtainNumber(f" Static", device, None)]
    numbers += [SmartCurtainNumber(f"Hour {i:02}", device, i) for i in range(24)]
    async_add_entities(numbers)

class SmartCurtainNumber(NumberEntity):
    _attr_device_class = NumberDeviceClass.ILLUMINANCE
    _attr_native_unit_of_measurement = "lx"
    _attr_mode = "box"
    _attr_native_step = 0.05
    _attr_native_max_value = 100000
    _attr_native_min_value = 0
    _attr_should_poll = False

    def __init__(self, name, device, i):
        self._name = name
        self._device = device
        self._hour = i
        device._entities.append(self)

    @property
    def name(self):
        return self._name

    @property
    def native_value(self) -> float:
        if self._hour is None:
            return self._device.auto_static_target
        return self._device.auto_hourly_targets[self._hour]

    def set_native_value(self, value: float) -> None:
        if self._hour is None:
            self._device.send_settings({"auto_static": {"target": value}})
        else:
            new_targets = list(self._device.auto_hourly_targets)
            new_targets[self._hour] = value
            self._device.send_settings({"auto_hourly": {"targets": new_targets}})

    def update(self):
        pass
