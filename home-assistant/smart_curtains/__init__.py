from __future__ import annotations

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers import discovery
from homeassistant.helpers.typing import ConfigType
from homeassistant.const import Platform
from .cover import SmartCurtainCover
from .device import Device

PLATFORMS = [Platform.COVER, Platform.SENSOR, Platform.NUMBER, Platform.SELECT, Platform.BUTTON]

type DeviceConfigEntry = ConfigEntry[Device]

async def async_setup_entry(hass: HomeAssistant, entry: DeviceConfigEntry) -> bool:
    entry.runtime_data = Device(hass, entry.data["url"])
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    return True

async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    return await hass.config_entries.async_unload_platforms(entry, PLATFORMS)

# https://github.com/home-assistant/example-custom-config/blob/master/custom_components/detailed_hello_world_push
# https://developers.home-assistant.io/docs/core/entity/cover/
