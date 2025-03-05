import logging
import requests
from homeassistant.core import HomeAssistant
from homeassistant.config_entries import ConfigEntry
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.components.sensor import SensorEntity, SensorDeviceClass, SensorStateClass
from homeassistant.const import CONF_URL

_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, config_entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    cover = SmartCurtainSensor(config_entry.data.get('name'), config_entry.data.get('url'))
    async_add_entities([cover])

class SmartCurtainSensor(SensorEntity):
    _attr_device_class = SensorDeviceClass.ILLUMINANCE
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = "lx"

    def __init__(self, name, url):
        self._name = name
        self._url = url
        self._value = 0

    @property
    def name(self):
        return self._name

    @property
    def native_value(self) -> float:
        return self._value

    def update(self):
        try:
            response = requests.get(f"{self._url}/als")
            if response.status_code == 200:
                data = response.json()
                self._value = float(data["value"])
                _LOGGER.info(f"Updated state: {self._value}")
        except requests.exceptions.RequestException as e:
            _LOGGER.error(f"Error fetching sensor state: {e}")
